#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <string.h>
#include "IOMisc.h"
#include "ipc.h"

struct IOInfo ioInfo;
const int8_t processAmount = 4;
local_id currentLocalID = 0;
pid_t currentPID = 0;
pid_t parentPID = 0;

bool RunChildProcess()
{
    currentPID = getpid();

    // Mark parent process as non-zero process to determine it
    ioInfo.process[0].pid = -1;
    InitIO(&currentLocalID, &ioInfo);
    if (close(ioInfo.process[0].pipe[currentLocalID][0]))
    {
        perror("Close");
    }
    ioInfo.process[0].pipe[currentLocalID][0] = 0;

    if (currentLocalID == 1)
    {
        Message message;
        InitMessage(&message);
        SendString(ioInfo, 2, "Hello from process 1!", &message);
    } else if (currentLocalID == 2)
    {
        Message message;
        receive(&ioInfo, 1, &message);
        Log(MessageInfo, NULL, 1, &message);
    }
    else if (currentLocalID == 3)
    {
        Message message;
        InitMessage(&message);
        SendString(ioInfo, PARENT_ID, "Hello Parent process from process 3!", &message);
    }

    return true;
}

int main()
{
    parentPID = getpid();
    currentPID = parentPID;
    memset(&ioInfo, 0, sizeof(ioInfo));
    ioInfo.processAmount = processAmount;

    for (int processIndex = 0; processIndex < ioInfo.processAmount; processIndex++)
    {
        ioInfo.process[processIndex].pid = -1;
        for (int childProcessPipeIndex = 0; childProcessPipeIndex < ioInfo.processAmount; childProcessPipeIndex++)
        {
            if (pipe(ioInfo.process[processIndex].pipe[childProcessPipeIndex]) == -1)
            {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
        }
    }

    for (int i = 1; i < ioInfo.processAmount; i++)
    {
        ioInfo.process[i].pid = fork();
        switch (ioInfo.process[i].pid)
        {
            case -1:
            {
                perror("fork");
                exit(EXIT_FAILURE);
            }
            case 0:
            {
                bool result = RunChildProcess();
                exit(!result);
            }
            default:
            {
                continue;
            }
        }
    }

    ioInfo.process[0].pid = 0;
    InitIO(&currentLocalID, &ioInfo);

    Message message;
    receive(&ioInfo, 3, &message);
    Log(MessageInfo, NULL, 1, &message);

    for (int i = 0; i < ioInfo.processAmount; i++)
    {
        wait(&ioInfo.process[i].pid);
    }

    return 0;
}
