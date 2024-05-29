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
#include <channel.h>
#include <pthread.h>
#include <kvm/ioeventfd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <kvm/kvm.h>
#include <asm/realm.h>

#define PATH_MAX      4096	/* chars in a path name including nul */

static Client *client;

typedef enum Error
{
    ERROR_PEER_LIST_EMPTY = -1,
    ERROR_PEER_NOT_MATCHED = -2,
} Error;


static bool push_back(Client* client, Peer new_peer) {
    if (client->peer_cnt >= PEER_LIST_MAX) {
        return false;
    }
	ch_syslog("[ID:%d] push_back peer.id %d, peer.eventfd %d",
            client->id, new_peer.id, new_peer.eventfd);
    client->peers[client->peer_cnt++] = new_peer;
    return true;
}

static int search_peer_idx(Client *client, int id) {
	ch_syslog("[ID:%d] search_peer_idx start, peer_cnt %d", client->id, client->peer_cnt);
    for (int i = 0; i < client->peer_cnt; i++) {
		ch_syslog("[ID:%d] peer[%d].id: %d", client->id, i, client->peers[i].id);
		ch_syslog("[ID:%d] peer[%d].eventfd: %d", client->id, i, client->peers[i].eventfd);
        if (id == client->peers[i].id) {
            return i;
        }
    }
    return -1;
}

static int search_peer(Client* client, int id) {
    int idx = search_peer_idx(client, id);
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
static int read_one_msg(int sock_fd, int64_t *id, int *fd) {
    int64_t ret;
    struct msghdr msg;
    struct iovec iov[1];
    union {
        struct cmsghdr cmsg;
        char control[CMSG_SPACE(sizeof(int))];
    } msg_control;
    struct cmsghdr *cmsg;

    iov[0].iov_base = id;
    iov[0].iov_len = sizeof(id);

    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_control = &msg_control;
    msg.msg_controllen = sizeof(msg_control);

    ret = recvmsg(sock_fd, &msg, 0);
    if (ret < 0) {
        ch_syslog("cannot read message: %s", strerror(errno));
        return -1;
    }
    if (ret == 0) {
        ch_syslog("lost connection to server");
        return -1;
    }
	
	// this code is only needed for other platform support between VM and host
    //*id = GINT64_FROM_LE(*id);
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

    ch_syslog("connect to client %s", socket_path);

    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        ch_syslog("cannot create socket: %s", strerror(errno));
        return -1;
    }

    s_un.sun_family = AF_UNIX;
    ret = snprintf(s_un.sun_path, sizeof(s_un.sun_path), "%s", socket_path);
    if (ret < 0 || (uint64_t)ret >= sizeof(s_un.sun_path)) {
        ch_syslog("could not copy unix socket path");
        goto err_close;
    }

    if (connect(sock_fd, (struct sockaddr *)&s_un, sizeof(s_un)) < 0) {
        ch_syslog("cannot connect to %s: %s", s_un.sun_path,
                             strerror(errno));
        goto err_close;
    }

    return sock_fd;

err_close:
    close(sock_fd);
    return -1;
}

static int set_ioeventfd(Client* client, int eventfd, int peer_id) {
	int ret;
	struct ioevent ioevent;

	ioevent = (struct ioevent){
		.io_addr = client->mmio_addr,
		.io_len = sizeof(int),
		.fn_kvm = client->kvm,
		.fd = eventfd,
		.datamatch = peer_id,
	};

	ret = ioeventfd__add_event(&ioevent, 0);
	if (ret) {
		ch_syslog("[ID:%d] ioeventfd__add_event fail(%d) for eventfd: %d, peer_id: %d",
				client->id, ret, eventfd, peer_id);
		return ret;
	}
	ch_syslog("[ID:%d] set_ioeventfd done. io_addr 0x%llx, io_len %d, fd %d, datamatch %lld",
			client->id, ioevent.io_addr, ioevent.io_len, ioevent.fd, ioevent.datamatch);
	return 0;
}

static int recv_initial_msg(Client* client) {
    int fd, ret = 0;
    int64_t id;

    /* receive client's id and eventfd */
    if (read_one_msg(client->sock_fd, &id, &fd) < 0 || id < 0 || fd < 0) {
        ch_syslog("cannot read client's id & eventfd from server");
        return -1;
    }
    client->id = id;
    client->eventfd = fd;
    ch_syslog("[ID:%d] client->id = %d, client->eventfd = %d",
			client->id, client->id, client->eventfd);

    /* receive eventfd manager's shm_id */
    if (read_one_msg(client->sock_fd, &id, &fd) < 0 || id <= 0 || fd != -1) {
        ch_syslog("[ID:%d] cannot read Eventfd Manager's shm_id", client->id);
        return -1;
    }
    client->shm_id = (int)id;
    ch_syslog("[ID:%d] client->shm_id = %d", client->id, client->shm_id);

	ret = set_ioeventfd(client, client->shm_alloc_efd, SHM_ALLOC_EFD_ID);

    return ret;
}

bool is_valid_shm_id(Client* client, int shm_id) {
    return client->shm_id == shm_id;
}

Client* get_client(const char *socket_path, uint32_t mmio_addr, struct kvm* kvm) {
    int ret;

    if (client)
        return client;

    client = calloc(1, sizeof(Client));

	/* create eventfd */
    ret = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (ret < 0) {
        ch_syslog("Cannot create eventfd %s\n", strerror(errno));
        goto err_free;
    }
	client->shm_alloc_efd = ret;

    client->sock_fd = connect_socket(socket_path);
    if (client->sock_fd < 0) {
        free(client);
        return NULL;
    }

    client->mmio_addr = mmio_addr;
    client->kvm = kvm;

    ch_syslog("client->sock_fd = %d", client->sock_fd);

    ret = recv_initial_msg(client);
    if (ret < 0) {
        goto err_close;
    }

	ch_syslog("[ID:%d] client addr %p", client->id, client);

    memset(client->peers, -1, sizeof(Peer) * PEER_LIST_MAX);

    client->initialized = true;
    return client;

err_close:
    close(client->sock_fd);
err_free:
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
        ch_syslog("failed to read one message %d", ret);
        goto err;
    }

    if (peer_id < 0 || peer_id == client->id) {
        ch_syslog("invalid peer_id %ld", peer_id);
        goto err;
    }

	ch_syslog("[ID:%d] recv a peer_id: %ld", client->id, peer_id);
    peer_idx = search_peer(client, peer_id);

    /* delete peer */
    if (fd == -1) {
        if (peer_idx < 0) {
            ch_syslog("receive delete for invalid peer_id: %ld", peer_id);
            goto err;
        }

        ch_syslog("delete peer id = %ld", peer_id);
        remove_peer(client, peer_idx);
        return 0;
    }

    /* new peer */
    if (peer_idx < 0) {
        new_peer.id = peer_id;
        new_peer.eventfd = fd;

        ret = push_back(client, new_peer);
        if (!ret) {
            ch_syslog("peer list is full %d", client->peer_cnt);
            goto err;
        }
		ch_syslog("[ID:%d] a new peer is added. peer_id: %d",
				client->id, client->peers[client->peer_cnt-1].id);

		ret = set_ioeventfd(client, new_peer.eventfd, new_peer.id);
		if (ret) {
			goto err;
		}
    } else {
        ch_syslog("[ID:%d] The peer_id %ld is already exist", client->id, peer_id);
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
		if (ret < 0 && errno != EINTR) {
			ch_syslog("handle_eventfd_manager_msg() failed %d %s", ret, strerror(errno));
			return ret;
		}
    } 

	if (client->shm_alloc_efd < maxfd && FD_ISSET(client->shm_alloc_efd, fds)) {
		u64 efd_cnt;
		int len;

		len = read(client->shm_alloc_efd, &efd_cnt, sizeof(efd_cnt));
		if (len == -1) {
			ch_syslog("%s read failed on shm_alloc_efd %d", __func__, strerror(errno));
		}
		ch_syslog("%s shm_alloc_efd cnt: %d", __func__, efd_cnt);

		ret = allocate_shm_after_realm_activate(client->kvm);
	}

    return ret;
}

void *poll_events(void *c_ptr) {
    fd_set fds;
    int ret, maxfd = 0;
    Client* client = c_ptr;

	ch_syslog("[ID:%d] Start poll_events()", client->id);

    while (1) {
        FD_ZERO(&fds);
        FD_SET(client->shm_alloc_efd, &fds);
        FD_SET(client->sock_fd, &fds);

        if (client->sock_fd >= maxfd) {
            maxfd = client->sock_fd + 1;
		} else if (client->shm_alloc_efd >= maxfd) {
            maxfd = client->shm_alloc_efd + 1;
		}

        ret = select(maxfd, &fds, NULL, NULL, NULL);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "[ID:%d] select error: %s\n", client->id, strerror(errno));
            break;
        }
        if (ret == 0) { // timeout
            continue;
        }

        ret = handle_fds(client, &fds, maxfd);
        if (ret < 0) {
            fprintf(stderr, "[ID:%d] handle_fds() failed %d\n", client->id, ret);
            break;
        }
    }

    ch_syslog("[ID:%d] close all fd & free client", client->id);
    close_client(client);
    free(client);

    pthread_exit(NULL);
    return NULL;
}

void close_client(Client* client) {
    ch_syslog("[ID:%d] close_client() start", client->id);
    for (int i = 0; i < client->peer_cnt; i++) {
        close(client->peers[i].eventfd);
    }

    close(client->sock_fd);
    client->sock_fd = -1;

    close(client->eventfd);
    client->eventfd = -1;

    close(client->shm_alloc_efd);
    client->shm_alloc_efd = -1;

    free(client);
}
