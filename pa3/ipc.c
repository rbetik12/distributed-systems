#include "ipc.h"
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "Utils.h"

extern local_id currentLocalID;

int send(void *self, local_id dst, const Message *msg)
{
    IPCInfo *ipcInfo = (IPCInfo *) self;
    struct ProcessInfo currentProcess = ipcInfo->process[currentLocalID];

    //Checks if we send message to ourselves
    if (dst == currentLocalID)
    {
        Log(Debug, "Process %d trying to send message to himself!\n", 1, getpid());
        return -1;
    }
    ssize_t messageSize = sizeof(Message) - (MAX_PAYLOAD_LEN - msg->s_header.s_payload_len);
    ssize_t writeAmount;

    while ((writeAmount = write(currentProcess.pipe[dst][1], msg, messageSize)) == -1)
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
    IPCInfo *ipcInfo = (IPCInfo *) self;
    struct ProcessInfo *processInfo = &ipcInfo->process[from];

    //Checks if we are trying to receive message from ourselves
    if (from == currentLocalID)
    {
        Log(Debug, "Process %d trying to receive message from himself!\n", 1, getpid());
        return -1;
    }

    ssize_t readAmount = read(processInfo->pipe[currentLocalID][0], &msg->s_header, sizeof(msg->s_header));

    if (readAmount != sizeof(msg->s_header))
    {
        if (readAmount == -1)
        {
            if (errno == EAGAIN)
            {
                return EAGAIN;
            }
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
    IPCInfo *ipcInfo = (IPCInfo *) self;
    int status = 0;
    for (int8_t processIndex = 0; processIndex < ipcInfo->processAmount; processIndex++)
    {
        if (currentLocalID == processIndex)
        {
            continue;
        }
        status = send(ipcInfo, processIndex, msg);
        if (status != 0)
        {
            return -1;
        }
    }

    return 0;
}

int receive_any(void *self, Message *msg)
{
    IPCInfo *ipcInfo = (IPCInfo *) self;
    int status = 0;
    for (int8_t processIndex = 0; processIndex < ipcInfo->processAmount; processIndex++)
    {
        if (currentLocalID == processIndex)
        {
            continue;
        }
        status = receive(ipcInfo, processIndex, msg);
        switch (status)
        {
            case EAGAIN:
                continue;
            default:
                return status;
        }
    }

    return EAGAIN;
}

