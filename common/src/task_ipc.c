#include "task_ipc.h"
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

/* Create/open queue with flags (O_RDONLY, O_WRONLY, O_NONBLOCK) */
mqd_t create_queue(const char *name, int flags)
{
    struct mq_attr attr = {
        .mq_flags   = (flags & O_NONBLOCK) ? O_NONBLOCK : 0,
        .mq_maxmsg  = 10,
        .mq_msgsize = sizeof(ipc_msg_t),
        .mq_curmsgs = 0
    };

    mqd_t qd = mq_open(name, flags | O_CREAT, 0666, &attr);
    if (qd == (mqd_t)-1) {
        perror("mq_open");
        exit(EXIT_FAILURE);
    }
    return qd;
}

/* Send a message */
int send_message(mqd_t qd, const ipc_msg_t *msg)
{
    return mq_send(qd, (const char *)msg, sizeof(*msg), 0);
}

/* Receive a message (blocking or non-blocking depending on queue flags) */
int receive_message(mqd_t qd, ipc_msg_t *msg)
{
    ssize_t ret = mq_receive(qd, (char *)msg, sizeof(*msg), NULL);
    if (ret == -1) {
        if (errno == EAGAIN) {
            return 0; /* no message available for non-blocking */
        }
        perror("mq_receive");
        return -1;
    }
    return 1; /* message received */
}
