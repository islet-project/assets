#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdbool.h>
#include <sys/shm.h>

static int cloak_shm_id = -1;
static char *cloak_shm = NULL;

int send_msg(const void *msg, size_t size, int type, bool app_to_gw);
int receive_msg(void *msg, size_t size, int in_type, int *out_type, bool app_from_gw);
int receive_msg_nowait(void *msg, size_t size, int in_type, int *out_type, bool app_from_gw);

int write_to_shm(char *src, unsigned int size);
int read_from_shm(char *dst, unsigned int size);
void *get_shm(void);
//bool queue_exist(bool app_to_gw);

#define CLOAK_MSG_ID_APP_TO_GW (8765)
#define CLOAK_MSG_ID_GW_TO_APP (8766)

#define CLOAK_MSG_TYPE_P9 (2)
#define CLOAK_MSG_TYPE_NET_TX (3)
#define CLOAK_MSG_TYPE_NET_RX (4)
#define CLOAK_MSG_TYPE_NET_RX_NUM_BUFFERS (5)

#define SHM_SIZE (2 * 1024 * 1024)

/*
#define CLOAK_MSG_ID_NET_APP_TO_GW (8767)
#define CLOAK_MSG_ID_NET_GW_TO_APP (8768)
*/

/*
struct cloak_msg_conf {
    unsigned long type;
    int key_app_to_gw;
    int key_gw_to_app;
    int desc_app_to_gw;
    int desc_gw_to_app;
};

struct cloak_msg_conf msg_confs[4] = {
    [0] = { .type = 0, .key_app_to_gw = -1, .key_gw_to_app = -1, .desc_app_to_gw = -1, .desc_gw_to_app = -1 },
    [1] = { .type = 0, .key_app_to_gw = -1, .key_gw_to_app = -1, .desc_app_to_gw = -1, .desc_gw_to_app = -1 },
    [CLOAK_MSG_TYPE_P9] = { .type = CLOAK_MSG_TYPE_P9, .key_app_to_gw = CLOAK_MSG_ID_P9_APP_TO_GW, .key_gw_to_app = CLOAK_MSG_ID_P9_GW_TO_APP, .desc_app_to_gw = -1, .desc_gw_to_app = -1 },
    [CLOAK_MSG_TYPE_NET] = { .type = CLOAK_MSG_TYPE_NET, .key_app_to_gw = CLOAK_MSG_ID_NET_APP_TO_GW, .key_gw_to_app = CLOAK_MSG_ID_NET_GW_TO_APP, .desc_app_to_gw = -1, .desc_gw_to_app = -1 },
}; */

struct cloak_msgbuf {
    unsigned long mtype;
    char buf[32];
};

static int id_app_to_gw = -1;
static int id_gw_to_app = -1;

/*
bool queue_exist(bool app_to_gw)
{
    int cloak_msg_id = -1;

    cloak_msg_id = msgget(app_to_gw ? (key_t)CLOAK_MSG_ID_APP_TO_GW : (key_t)CLOAK_MSG_ID_GW_TO_APP, 0);
    if (cloak_msg_id < 0) {
        return false;
    } else {
        return true;
    }
} */

int write_to_shm(char *src, unsigned int size)
{
    if (cloak_shm_id < 0) {
        key_t key = 9876;
        key = ftok(".", 'A');
        cloak_shm_id = shmget(key, SHM_SIZE, IPC_CREAT | 0666);
        if (cloak_shm_id < 0) {
            printf("shmget error: %d\n", cloak_shm_id);
            return -1;
        }
        cloak_shm = (char*)shmat(cloak_shm_id, NULL, 0);
    }

    if (size >= SHM_SIZE) {
        printf("shm size too big\n");
        return -1;
    }

    memcpy(cloak_shm, src, size);
    return 0;
}

int read_from_shm(char *dst, unsigned int size)
{
    if (cloak_shm_id < 0) {
        key_t key = 9876;
        key = ftok(".", 'A');
        cloak_shm_id = shmget(key, SHM_SIZE, IPC_CREAT | 0666);
        if (cloak_shm_id < 0) {
            printf("shmget error: %d\n", cloak_shm_id);
            return -1;
        }
        cloak_shm = (char*)shmat(cloak_shm_id, NULL, 0);
    }

    if (size >= SHM_SIZE) {
        printf("shm size too big\n");
        return -1;
    }

    memcpy(dst, cloak_shm, size);
    return 0;
}

void *get_shm(void)
{
    if (cloak_shm_id < 0) {
        key_t key = 9876;
        key = ftok(".", 'A');
        cloak_shm_id = shmget(key, SHM_SIZE, IPC_CREAT | 0666);
        if (cloak_shm_id < 0) {
            printf("shmget error: %d\n", cloak_shm_id);
            return NULL;
        }
        cloak_shm = (char*)shmat(cloak_shm_id, NULL, 0);
    }

    return (void *)cloak_shm;
}

int send_msg(const void *msg, size_t size, int type, bool app_to_gw)
{
    // app_to_gw == true: app sends a message to gw
    // app_to_gw == false: gw sends a message to app

    int cloak_msg_id = -1;
    int res;
    struct cloak_msgbuf buf;

    if (app_to_gw) {
        cloak_msg_id = id_app_to_gw;
    } else {
        cloak_msg_id = id_gw_to_app;
    }

    if (cloak_msg_id < 0) {
        cloak_msg_id = msgget(app_to_gw ? (key_t)(CLOAK_MSG_ID_APP_TO_GW) : (key_t)(CLOAK_MSG_ID_GW_TO_APP), IPC_CREAT | 0666);
        if (cloak_msg_id < 0) {
            printf("send_msg: msgget error\n");
            return -1;
        }

        if (app_to_gw) {
            id_app_to_gw = cloak_msg_id;
        } else {
            id_gw_to_app = cloak_msg_id;
        }

        printf("msgq created. type: %d, app_to_gw: %d, id: %d\n", type, app_to_gw ? 1 : 0, cloak_msg_id);
    }

    if (size >= sizeof(buf.buf)) {
        printf("send_msg: size too big: %d\n", size);
        return -1;
    }
    buf.mtype = (unsigned long)type;
    memcpy(buf.buf, msg, size);

    res = msgsnd(cloak_msg_id, &buf, size, 0);
    if (res < 0) {
        printf("send_msg: msgsnd error: %d\n", cloak_msg_id);
    }
    return res;
}

int __receive_msg(void *msg, size_t size, int in_type, int *out_type, bool app_from_gw, bool nowait)
{
    // app_from_gw == true: app receives a message from gw (equivalent of app_to_gw == false)
    // app_from_gw == false: gw receives a message from app (equivalent of app_to_gw == true)

    int cloak_msg_id = -1;
    int res;
    struct cloak_msgbuf buf;
    long msgtyp = 0;

    if (app_from_gw) {
        cloak_msg_id = id_gw_to_app;
    } else {
        cloak_msg_id = id_app_to_gw;
    }

    if (cloak_msg_id < 0) {
        cloak_msg_id = msgget(app_from_gw ? (key_t)(CLOAK_MSG_ID_GW_TO_APP) : (key_t)(CLOAK_MSG_ID_APP_TO_GW), IPC_CREAT | 0666);
        if (cloak_msg_id < 0) {
            printf("receive_msg: msgget error\n");
            return -1;
        }

        if (app_from_gw) {
            id_gw_to_app = cloak_msg_id;
        } else {
            id_app_to_gw = cloak_msg_id;
        }

        printf("msgq created. type: %d, app_from_gw: %d, id: %d\n", in_type, app_from_gw ? 1 : 0, cloak_msg_id);
    }

    if (size >= sizeof(buf.buf)) {
        printf("receive_msg: size too big: %d\n", size);
        return -1;
    }

    if (in_type > 0)
        msgtyp = in_type;

    res = msgrcv(cloak_msg_id, &buf, size, msgtyp, nowait ? IPC_NOWAIT : 0);
    if (res < 0) {
        if (nowait == false)
            printf("receive_msg: msgrcv error: %d\n", cloak_msg_id);
        return res;
    }
    memcpy(msg, buf.buf, size);
    *out_type = (int)buf.mtype;
    return res;
}

// wait
int receive_msg(void *msg, size_t size, int in_type, int *out_type, bool app_from_gw)
{
    __receive_msg(msg, size, in_type, out_type, app_from_gw, false);
}

// no_wait
int receive_msg_nowait(void *msg, size_t size, int in_type, int *out_type, bool app_from_gw)
{
    __receive_msg(msg, size, in_type, out_type, app_from_gw, true);
}
