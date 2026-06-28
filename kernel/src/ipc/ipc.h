#ifndef IPC_H
#define IPC_H

#include <stdint.h>

int ipcSend(uint64_t destPid, const uint64_t* data);
int ipcSendPid(uint64_t destPid, uint64_t senderPid, const uint64_t* data);
int ipcRecv(uint64_t* data, uint64_t* senderPid);

#endif
