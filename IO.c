#include "IOMisc.h"
#include <stdio.h>
#include <string.h>

int SendString(struct IOInfo ioInfo, local_id destination, const char *string, Message *message)
{
    snprintf(message->s_payload, MAX_PAYLOAD_LEN, "%s", string);
    message->s_header.s_payload_len = strlen(string) + 1;

    return send(&ioInfo, destination, message);
}

void InitMessage(Message *message)
{
    memset(message, 0, sizeof(Message));
    message->s_header.s_magic = MESSAGE_MAGIC;
}