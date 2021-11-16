#pragma once

#include <unistd.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include "ipc.h"

#define CLOSE_PIPE(PIPE_ARRAY, PIPE_INDEX) do {\
                                                if (PIPE_ARRAY[PIPE_INDEX] != 0) \
                                                {                              \
                                                    if (close(PIPE_ARRAY[PIPE_INDEX])) \
                                                    {                           \
                                                        perror("Close in shutdown");\
                                                    } \
                                                    PIPE_ARRAY[PIPE_INDEX] = 0;    \
                                                }  \
                                            }while(0)\

/*
 * How it works:
 * Setup: 3 process and on parent process
 * Each process has its own instance of ProcessInfo struct. On the side of the process his pipes are write-only.
 * On the side of other process pipes of the process are read-only.
 * Example:
 * We are process 1.
 * We have 2 opened pipes:
 * - 1 -> 2
 * - 1 -> 3
 * They are write-only.
 * Other processes has this kind of pipes:
 * 2:
 * - 2 -> 1 - read-only
 * - 2 -> 3 - closed
 * 3:
 * - 3 -> 1 - read-only
 * - 3 -> 2 - closed
 * The same rule applies to other processes.
 */

struct ProcessInfo
{
    pid_t pid;
    int pipe[MAX_PROCESS_ID][2];
};

typedef struct IPCInfo
{
    timestamp_t currentLamportTime;
    uint8_t processAmount;
    struct ProcessInfo process[MAX_PROCESS_ID];
} IPCInfo;

enum LogType
{
    Pipe,
    Event,
    Debug,
    MessageInfo,
};

////////////////////////////// IO ///////////////////////////////////

void InitIO(local_id *currentProcessId, IPCInfo *ioInfo);

int InitIOParent(IPCInfo* ioInfo);

int InitIONonBlocking(IPCInfo* ioInfo);

void ShutdownIO(IPCInfo *ioInfo);

int ReceiveAll(IPCInfo* ioInfo, local_id currentLocalID);

////////////////////////////// Logging //////////////////////////////

void InitLog();

void Log(enum LogType type, const char *format, int argsAmount, ...);

void ShutdownLog();

////////////////////////////// Messages /////////////////////////////

void InitMessage(Message *message, MessageType type);

void CopyToMessage(Message *message, void* data, size_t dataSize);

void WriteFormatStringToMessage(Message *message, const char *format, int argsAmount, ...);
