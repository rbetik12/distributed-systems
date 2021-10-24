#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include "IOMisc.h"
#include "ipc.h"
#include "pa2345.h"

struct IOInfo ioInfo;
local_id currentLocalID = 0;
pid_t currentPID = 0;
pid_t parentPID = 0;

void ProcessTransfer(Message *message)
{
    TransferOrder order;
    memcpy(&order, message->s_payload, sizeof(order));

    if (currentLocalID == order.s_src)
    {
        ioInfo.process[currentLocalID].balance -= order.s_amount;
        send(&ioInfo, order.s_dst, message);
    }
    else if (currentLocalID == order.s_dst)
    {
        ioInfo.process[currentLocalID].balance += order.s_amount;
        Message ackMessage;
        InitMessage(&ackMessage, ACK, get_physical_time);

        send(&ioInfo, PARENT_ID, &ackMessage);
    }
}

bool RunChildProcess()
{
    currentPID = getpid();

    // Mark parent process as non-zero process to determine it
    ioInfo.process[0].pid = -1;
    InitIO(&currentLocalID, &ioInfo);

    //Start
    Message message;
    InitMessage(&message, STARTED, get_physical_time);
    WriteFormatString(&message, log_started_fmt, 5, message.s_header.s_local_time, currentLocalID, currentPID, parentPID,
                      ioInfo.process[currentLocalID].balance);
    Log(Event, message.s_payload, 0);
    send_multicast(&ioInfo, &message);
    ReceiveAll(ioInfo, currentLocalID);
    //Start end

    bool isRunning = true;
    while (isRunning)
    {
        InitMessage(&message, STARTED, get_physical_time);
        int retVal = receive_any(&ioInfo, &message);
        if (retVal == EAGAIN)
        {
            continue;
        }
        switch (message.s_header.s_type)
        {
            case STOP:
            {
                Log(Debug, "Process with local id (%d) received STOP message\n", 1, currentLocalID);
                isRunning = false;
                break;
            }
            case TRANSFER:
            {
                ProcessTransfer(&message);
                break;
            }
        }
    }
    //Done
    InitMessage(&message, DONE, get_physical_time);
    WriteFormatString(&message, log_done_fmt, 3, message.s_header.s_local_time, currentLocalID,
                      ioInfo.process[currentLocalID].balance);
    Log(Event, message.s_payload, 0);
    send_multicast(&ioInfo, &message);
    ReceiveAll(ioInfo, currentLocalID);
    //Done end

    ShutdownIO(&ioInfo);

    return true;
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        exit(EXIT_FAILURE);
    }
    InitLog();
    parentPID = getpid();
    currentPID = parentPID;
    memset(&ioInfo, 0, sizeof(ioInfo));
    ioInfo.processAmount = strtol(argv[2], NULL, 10) + 1;

    for (int i = 1; i < ioInfo.processAmount; i++)
    {
        ioInfo.process[i].balance = strtol(argv[2 + i], NULL, 10);
    }

    InitIOParent(&ioInfo);
    InitIONonBlocking(&ioInfo);

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

    //Receive started
    ReceiveAll(ioInfo, currentLocalID);

//    bank_robbery(&ioInfo, ioInfo.processAmount);
    transfer(&ioInfo, 1, 2, 1);

    Message message;
    InitMessage(&message, STOP, get_physical_time);
    send_multicast(&ioInfo, &message);

    // Receive done
    ReceiveAll(ioInfo, currentLocalID);

    for (int i = 0; i < ioInfo.processAmount; i++)
    {
        wait(&ioInfo.process[i].pid);
    }

    ShutdownIO(&ioInfo);
    ShutdownLog();

    return 0;
}
