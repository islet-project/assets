#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdbool.h>

int send_msg(const void *msg, size_t size, bool app_to_gw);
int receive_msg(void *msg, size_t size, bool app_from_gw);
bool queue_exist(bool app_to_gw);

#define CLOAK_MSG_ID_APP_TO_GW (8765)
#define CLOAK_MSG_ID_GW_TO_APP (8766)
#define CLOAK_MSG_TYPE (2)

static int cloak_msg_id_app_to_gw = -1;
static int cloak_msg_id_gw_to_app = -1;

struct cloak_msgbuf {
    unsigned long mtype;
    char buf[32];
};

bool queue_exist(bool app_to_gw)
{
    int cloak_msg_id = -1;

    cloak_msg_id = msgget(app_to_gw ? (key_t)CLOAK_MSG_ID_APP_TO_GW : (key_t)CLOAK_MSG_ID_GW_TO_APP, 0);
    if (cloak_msg_id < 0) {
        return false;
    } else {
        return true;
    }
}

int send_msg(const void *msg, size_t size, bool app_to_gw)
{
    // app_to_gw == true: app sends a message to gw
    // app_to_gw == false: gw sends a message to app

    int cloak_msg_id = -1;
    int res;
    struct cloak_msgbuf buf;

    if (app_to_gw) {
        cloak_msg_id = cloak_msg_id_app_to_gw;
    } else {
        cloak_msg_id = cloak_msg_id_gw_to_app;
    }

    if (cloak_msg_id < 0) {
        cloak_msg_id = msgget(app_to_gw ? (key_t)CLOAK_MSG_ID_APP_TO_GW : (key_t)CLOAK_MSG_ID_GW_TO_APP, IPC_CREAT | 0666);
        if (cloak_msg_id < 0) {
            printf("send_msg: msgget error\n");
            return -1;
        }

        if (app_to_gw) {
            cloak_msg_id_app_to_gw = cloak_msg_id;
        } else {
            cloak_msg_id_gw_to_app = cloak_msg_id;
        }

        printf("send_msg-%d, id: %d\n", app_to_gw ? 1 : 0, cloak_msg_id);
    }

    if (size >= 32) {
        printf("send_msg: size too big\n");
        return -1;
    }
    buf.mtype = CLOAK_MSG_TYPE;
    memcpy(buf.buf, msg, size);

    res = msgsnd(cloak_msg_id, &buf, size, 0);
    if (res < 0) {
        printf("send_msg: msgsnd error: %d\n", cloak_msg_id);
    }
    return res;
}

int receive_msg(void *msg, size_t size, bool app_from_gw)
{
    // app_from_gw == true: app receives a message from gw (equivalent of app_to_gw == false)
    // app_from_gw == false: gw receives a message from app (equivalent of app_to_gw == true)

    int cloak_msg_id = -1;
    int res;
    struct cloak_msgbuf buf;

    if (app_from_gw) {
        cloak_msg_id = cloak_msg_id_gw_to_app;
    } else {
        cloak_msg_id = cloak_msg_id_app_to_gw;
    }

    if (cloak_msg_id < 0) {
        cloak_msg_id = msgget(app_from_gw ? (key_t)CLOAK_MSG_ID_GW_TO_APP : (key_t)CLOAK_MSG_ID_APP_TO_GW, IPC_CREAT | 0666);
        if (cloak_msg_id < 0) {
            printf("receive_msg: msgget error\n");
            return -1;
        }

        if (app_from_gw) {
            cloak_msg_id_gw_to_app = cloak_msg_id;
        } else {
            cloak_msg_id_app_to_gw = cloak_msg_id;
        }

        printf("receive_msg-%d, id: %d\n", app_from_gw ? 1 : 0, cloak_msg_id);
    }

    if (size >= 32) {
        printf("receive_msg: size too big\n");
        return -1;
    }

    res = msgrcv(cloak_msg_id, &buf, size, 0, 0);
    if (res < 0) {
        printf("receive_msg: msgrcv error: %d\n", cloak_msg_id);
        return res;
    }
    memcpy(msg, buf.buf, size);
    return res;
}
