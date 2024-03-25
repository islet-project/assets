#define _POSIX_SOURCE

#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <string.h>
#include <kvm/util.h>
#include <sys/select.h>

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h> 
#include <socket.h>
#include <pthread.h>

#define PATH_MAX      4096	/* chars in a path name including nul */

typedef enum Error
{
    ERROR_PEER_LIST_EMPTY = -1,
    ERROR_PEER_NOT_MATCHED = -2,
} Error;


static bool push_back(Client* client, Peer new_peer) {
    if (client->peer_cnt >= PEER_LIST_MAX) {
        return false;
    }
	pr_debug("[VM_ID:%d] push_back peer.vm_id %d, peer.eventfd %d",
            client->vm_id, new_peer.vm_id, new_peer.eventfd);
    client->peers[client->peer_cnt++] = new_peer;
    return true;
}

static int search_peer_idx(Client *client, int vm_id) {
	pr_debug("[VM_ID:%d] search_peer_idx start, peer_cnt %d", client->vm_id, client->peer_cnt);
    for (int i = 0; i < client->peer_cnt; i++) {
		pr_debug("[VM_ID:%d] peer[%d].vm_id: %d", client->vm_id, i, client->peers[i].vm_id);
		pr_debug("[VM_ID:%d] peer[%d].eventfd: %d", client->vm_id, i, client->peers[i].eventfd);
        if (vm_id == client->peers[i].vm_id) {
            return i;
        }
    }
    return -1;
}

static int search_peer(Client* client, int vm_id) {
    int idx = search_peer_idx(client, vm_id);
    return idx;
}

static int remove_peer(Client* client, int idx) {
    Peer empty_peer = {-1, -1};

    if (client->peer_cnt == 0) {
        return ERROR_PEER_LIST_EMPTY;
    }

    if (idx < 0) {
        return ERROR_PEER_NOT_MATCHED;
    }

    client->peers[idx] = client->peers[--client->peer_cnt];
    client->peers[client->peer_cnt] = empty_peer;
    return true;
}

/* read message from the unix socket */
static int read_one_msg(int sock_fd, int64_t *vm_id, int *fd) {
    int64_t ret;
    struct msghdr msg;
    struct iovec iov[1];
    union {
        struct cmsghdr cmsg;
        char control[CMSG_SPACE(sizeof(int))];
    } msg_control;
    struct cmsghdr *cmsg;

    iov[0].iov_base = vm_id;
    iov[0].iov_len = sizeof(vm_id);

    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_control = &msg_control;
    msg.msg_controllen = sizeof(msg_control);

    ret = recvmsg(sock_fd, &msg, 0);
    if (ret < 0) {
        pr_debug("cannot read message: %s", strerror(errno));
        return -1;
    }
    if (ret == 0) {
        pr_debug("lost connection to server");
        return -1;
    }
	
	// this code is only needed for other platform support between VM and host
    //*vm_id = GINT64_FROM_LE(*vm_id);
    *fd = -1;

    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_len != CMSG_LEN(sizeof(int)) ||
            cmsg->cmsg_level != SOL_SOCKET ||
            cmsg->cmsg_type != SCM_RIGHTS) {
            continue;
        }

        memcpy(fd, CMSG_DATA(cmsg), sizeof(*fd));
    }

    return 0;
}

static int connect_socket(const char* socket_path) {
    struct sigaction sa;
    struct sockaddr_un s_un;
	int ret;
    int sock_fd = -1;

    /* Ignore SIGPIPE, see this link for more info:
     * http://www.mail-archive.com/libevent-users@monkey.org/msg01606.html */
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    if (sigemptyset(&sa.sa_mask) == -1 ||
        sigaction(SIGPIPE, &sa, 0) == -1) {
        perror("failed to ignore SIGPIPE; sigaction");
        return -1;
    }

    pr_debug("connect to client %s", socket_path);

    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        pr_debug("cannot create socket: %s", strerror(errno));
        return -1;
    }

    s_un.sun_family = AF_UNIX;
    ret = snprintf(s_un.sun_path, sizeof(s_un.sun_path), "%s", socket_path);
    if (ret < 0 || (uint64_t)ret >= sizeof(s_un.sun_path)) {
        pr_debug("could not copy unix socket path");
        goto err_close;
    }

    if (connect(sock_fd, (struct sockaddr *)&s_un, sizeof(s_un)) < 0) {
        pr_debug("cannot connect to %s: %s", s_un.sun_path,
                             strerror(errno));
        goto err_close;
    }

    return sock_fd;

err_close:
    close(sock_fd);
    return -1;
}

static int recv_initial_msg(int c_sock_fd, int* c_vm_id, int* c_eventfd, int* c_hc_eventfd) {
    int fd;
    int64_t vm_id;

    /* receive client's vm_id and eventfd */
    if (read_one_msg(c_sock_fd, &vm_id, &fd) < 0 || vm_id < 0 || fd < 0) {
        pr_debug("cannot read client's vm_id & eventfd from server");
        return -1;
    }
    *c_vm_id = vm_id;
    *c_eventfd = fd;
    pr_debug("client_vm_id = %d, client_eventfd = %d", *c_vm_id, *c_eventfd);

    /* receive host channel's eventfd */
    if (read_one_msg(c_sock_fd, &vm_id, &fd) < 0 || vm_id != -1 || fd < 0) {
        pr_debug("cannot read host channel eventfd from server");
        return -1;
    }
    *c_hc_eventfd = fd;
    pr_debug("host channel eventfd = %d", *c_hc_eventfd);

    return 0;
}

Client* get_client(const char *socket_path) {
    int ret;
    Client* client = calloc(1, sizeof(Client));
    memset(client, 0, sizeof(Client));

    client->sock_fd = connect_socket(socket_path);
    if (client->sock_fd < 0) {
        free(client);
        return NULL;
    }

	pr_debug("client->sock_fd = %d", client->sock_fd);

    ret = recv_initial_msg(client->sock_fd, &client->vm_id, &client->eventfd, &client->hc_eventfd);
    if (ret < 0) {
        goto err_close;
    }

	pr_debug("[VM_ID:%d] client addr %p", client->vm_id, client);

    memset(client->peers, -1, sizeof(Peer) * PEER_LIST_MAX);

    client->initialized = true;
    return client;

err_close:
    close(client->sock_fd);
    free(client);
    return NULL;
}

/* handle message coming from server (new peer, new vectors) */
static int handle_eventfd_manager_msg(Client* client) {
    Peer new_peer = {};
    int64_t peer_id = -1;
    int ret, fd = -1, peer_idx;

    ret = read_one_msg(client->sock_fd, &peer_id, &fd);
    if (ret < 0) {
        pr_debug("failed to read one message %d", ret);
        goto err;
    }

    if (peer_id < 0) {
        pr_debug("invalid peer_id %ld", peer_id);
        goto err;
    }

	pr_debug("[VM_ID:%d] recv a peer_id: %ld", client->vm_id, peer_id);
    peer_idx = search_peer(client, peer_id);

    /* delete peer */
    if (fd == -1) {
        if (peer_idx < 0) {
            pr_debug("receive delete for invalid peer_id: %ld", peer_id);
            goto err;
        }

        pr_debug("delete peer id = %ld", peer_id);
        remove_peer(client, peer_idx);
        return 0;
    }

    /* new peer */
    if (peer_idx < 0) {
        new_peer.vm_id = peer_id;
        new_peer.eventfd = fd;

        ret = push_back(client, new_peer);
        if (!ret) {
            pr_debug("peer list is full %d", client->peer_cnt);
            goto err;
        }

		pr_debug("[VM_ID:%d] a new peer is added. peer_id: %d", client->vm_id, client->peers[client->peer_cnt-1].vm_id);
    } else {
        pr_debug("The peer_id %ld is already exist", peer_id);
        goto err;
    }

    return 0;
err:
    return -1;
}

/* read and handle new messages on the given fd_set */
static int handle_fds(Client* client, fd_set *fds, int maxfd) {
    int ret = -1;

    if (client->sock_fd < maxfd && FD_ISSET(client->sock_fd, fds)) {
        ret = handle_eventfd_manager_msg(client);
    } else {
        pr_debug("invalid event. client->sock_fd %d, maxfd %d", client->sock_fd, maxfd);
        return ret;
    }

    if (ret < 0 && errno != EINTR) {
        pr_debug("handle_eventfd_manager_msg() failed %d %s", ret, strerror(errno));
        return ret;
    }

    return ret;
}

void *poll_events(void *c_ptr) {
    fd_set fds;
    int ret, maxfd = 0;
    Client* client = c_ptr;

	pr_debug("Start poll_events()");
    while (1) {
        FD_ZERO(&fds);
        FD_SET(client->sock_fd, &fds);
        if (client->sock_fd >= maxfd) {
            maxfd = client->sock_fd + 1;
        }

        ret = select(maxfd, &fds, NULL, NULL, NULL);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "select error: %s\n", strerror(errno));
            break;
        }
        if (ret == 0) { // timeout
            continue;
        }

        ret = handle_fds(client, &fds, maxfd);
        if (ret < 0) {
            fprintf(stderr, "handle_fds() failed %d\n", ret);
            break;
        }
    }

    pr_debug("close all fd & free client");
    client_close(client);
    free(client);

    pthread_exit(NULL);
    return NULL;
}

void client_close(Client* client) {
    for (int i = 0; i < client->peer_cnt; i++) {
        close(client->peers[i].eventfd);
    }

    close(client->sock_fd);
    client->sock_fd = -1;

    close(client->eventfd);
    client->eventfd = -1;

    close(client->hc_eventfd);
    client->hc_eventfd= -1;
}
