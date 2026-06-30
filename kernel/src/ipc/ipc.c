#include "ipc.h"
#include "scheduler/scheduler.h"
#include "memory/memutils.h"
#include "debug.h"

int ipcSend(uint64_t destPid, const uint64_t* data) {
    Task* dest = findTask(destPid);
    if (!dest || dest->state == TASK_DEAD) return -1;

    Mailbox* mb = &dest->mailbox;
    uint32_t next = (mb->head + 1) % IPC_MAX_MSG;

    if (next == mb->tail) return -1;

    memcpy((void*)mb->msgs[mb->head].data, data, IPC_MSG_SIZE);
    mb->msgs[mb->head].senderPid = 0;
    mb->head = next;

    schedulerWake(destPid);
    return 0;
}

int ipcRecv(uint64_t* data, uint64_t* senderPid) {
    Task* self = findTask(0);
    if (!self) return -1;

    Mailbox* mb = &self->mailbox;
    if (mb->head == mb->tail) return -1;

    uint32_t idx = mb->tail;
    if (data) memcpy((void*)data, mb->msgs[idx].data, IPC_MSG_SIZE);
    if (senderPid) *senderPid = mb->msgs[idx].senderPid;
    mb->tail = (mb->tail + 1) % IPC_MAX_MSG;

    return 0;
}

int ipcRecvFrom(uint64_t* data, uint64_t* senderPid, uint64_t expectedSender) {
    Task* self = findTask(0);
    if (!self) return -1;
    Mailbox* mb = &self->mailbox;
    if (mb->head == mb->tail) return -1;

    // Search for a message from expectedSender
    uint32_t t = mb->tail;
    while (t != mb->head && mb->msgs[t].senderPid != expectedSender)
        t = (t + 1) % IPC_MAX_MSG;
    if (t == mb->head) return -1;

    // Found matching message at position t
    if (data) memcpy((void*)data, mb->msgs[t].data, IPC_MSG_SIZE);
    if (senderPid) *senderPid = mb->msgs[t].senderPid;

    // Remove it by shifting subsequent messages back
    uint32_t next = (t + 1) % IPC_MAX_MSG;
    while (next != mb->head) {
        mb->msgs[t] = mb->msgs[next];
        t = next;
        next = (next + 1) % IPC_MAX_MSG;
    }
    // head moves back by one
    mb->head = (mb->head - 1 + IPC_MAX_MSG) % IPC_MAX_MSG;

    return 0;
}

int ipcSendPid(uint64_t destPid, uint64_t senderPid, const uint64_t* data) {
    Task* dest = findTask(destPid);
    if (!dest || dest->state == TASK_DEAD) return -1;

    Mailbox* mb = &dest->mailbox;
    uint32_t next = (mb->head + 1) % IPC_MAX_MSG;

    if (next == mb->tail) return -1;

    memcpy((void*)mb->msgs[mb->head].data, data, IPC_MSG_SIZE);
    mb->msgs[mb->head].senderPid = senderPid;
    mb->head = next;

    schedulerWake(destPid);
    return 0;
}
