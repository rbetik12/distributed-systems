#include "ipc.h"
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include "IOMisc.h"

extern local_id currentLocalID;

int send(void *self, local_id dst, const Message *msg)
{
    struct IOInfo *ioInfo = (struct IOInfo *) self;
    struct ProcessInfo currentProcess = ioInfo->process[currentLocalID];

    //Checks if we send message to ourselves
    if (dst == currentLocalID)
    {
        Log(Debug, "Process %d trying to send message to himself!\n", 1, getpid());
        return -1;
    }
    ssize_t messageSize = sizeof(Message) - (MAX_PAYLOAD_LEN - msg->s_header.s_payload_len);
    ssize_t writeAmount;

    //TODO Make real nonblock. If error is EAGAIN return to caller with this error
    while((writeAmount = write(currentProcess.pipe[dst][1], msg, messageSize)) == -1)
    {
        if (errno != EAGAIN)
        {
            break;
        }
    }

    if (writeAmount != messageSize)
    {
        Log(Debug, "Process %d didn't send message to process with local id: %d!\n", 2, getpid(), dst);
        if (writeAmount == -1)
        {
            Log(Debug, "Process %d didn't send message to process with local id: %d! Error occured: %s\n", 3,
                getpid(), dst, strerror(errno));
        }
        return -1;
    }

    return 0;
}

int receive(void *self, local_id from, Message *msg)
{
    struct IOInfo *ioInfo = (struct IOInfo *) self;
    struct ProcessInfo *processInfo = &ioInfo->process[from];

    //Checks if we are trying to receive message from ourselves
    if (from == currentLocalID)
    {
        Log(Debug, "Process %d trying to receive message from himself!\n", 1, getpid());
        return -1;
    }

    ssize_t readAmount;
    //TODO Make real nonblock. If error is EAGAIN return to caller with this error
    while ((readAmount = read(processInfo->pipe[currentLocalID][0], &msg->s_header, sizeof(msg->s_header))) == -1)
    {
        if (errno != EAGAIN)
        {
            break;
        }
    }

    if (readAmount != sizeof(msg->s_header))
    {
        if (readAmount == -1)
        {
            Log(Debug, "Process %d didn't receive message header from process with local id: %d! Error occured: %s\n",
                3, getpid(), from, strerror(errno));
        }
        return -1;
    }

    if (msg->s_header.s_magic != MESSAGE_MAGIC)
    {
        Log(Debug, "Process %d received message with incorrect magiс number!\n", currentLocalID);
        return -1;
    }

    readAmount = read(processInfo->pipe[currentLocalID][0], msg->s_payload, msg->s_header.s_payload_len);

    if (readAmount != msg->s_header.s_payload_len)
    {
        Log(Debug, "Process %d didn't receive message payload from process with local id: %d!\n", 2, getpid(), from);
        if (readAmount == -1)
        {
            Log(Debug,
                "Process %d didn't receive message payload from process with local id: %d!\n Error occured: %s\n", 3,
                getpid(), from, strerror(errno));
        }
        return -1;
    }

    return 0;
}

int send_multicast(void *self, const Message *msg)
{
    struct IOInfo *ioInfo = (struct IOInfo *) self;
    int status = 0;
    for (int8_t processIndex = 0; processIndex < ioInfo->processAmount; processIndex++)
    {
        if (currentLocalID == processIndex)
        {
            continue;
        }
        status = send(ioInfo, processIndex, msg);
        if (status != 0)
        {
            return -1;
        }
    }

    return 0;
}

