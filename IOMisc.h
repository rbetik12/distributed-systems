#pragma once

#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include "ipc.h"

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
    int pipe[11][2];
};

struct IOInfo
{
    int8_t processAmount;
    struct ProcessInfo process[11];
};

enum LogType
{
    Pipe,
    Event,
    Debug,
    MessageInfo,
};

void Log(enum LogType type, const char *format, int argsAmount, ...);
int SendString(struct IOInfo ioInfo, local_id destination, const char* string, Message* message);
void WriteString(const char* string, Message* message);
void WriteFormatString(Message* message, const char* format, int argsAmount, ...);
void InitMessage(Message *message);
void InitIO(local_id* currentProcessId, struct IOInfo* ioInfo);
void ShutdownIO(struct IOInfo* ioInfo);
void Shutdown();
void InitLog();
