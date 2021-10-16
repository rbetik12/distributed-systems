#include "IOMisc.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "common.h"
#include "ipc.h"

static FILE *eventLogFile = NULL;
static FILE *pipesLogFile = NULL;

void InitLog()
{
    eventLogFile = fopen(events_log, "a");
    pipesLogFile = fopen(pipes_log, "a");
}

void ShutdownLog()
{
    fclose(eventLogFile);
    fclose(pipesLogFile);
}

const char *MessageTypeToStr(MessageType type)
{
    switch (type)
    {
        case STARTED:
            return "STARTED";
        case DONE:
            return "DONE";
        case ACK:
            return "ACK";
        case STOP:
            return "STOP";
        case TRANSFER:
            return "TRANSFER";
        case BALANCE_HISTORY:
            return "BALANCE_HISTORY";
        case CS_REQUEST:
            return "CS_REQUEST";
        case CS_REPLY:
            return "CS_REPLY";
        case CS_RELEASE:
            return "CS_RELEASE";
    }

    return "UNKNOWN";
}

void Log(enum LogType type, const char *format, int argsAmount, ...)
{
    va_list valist;
    va_start(valist, argsAmount);

    switch (type)
    {
        case Event:
        {
            vprintf(format, valist);
            char logStr[256];
            memset(logStr, 0, sizeof(logStr));
            vsnprintf(logStr, 256, format, valist);
            fwrite(logStr, sizeof(char), strlen(logStr), eventLogFile);
            break;
        }
        case Pipe:
        {
            vfprintf(pipesLogFile, format, valist);
            break;
        }
        case MessageInfo:
        {
            Message *message = va_arg(valist, Message*);
            printf("Message info:\n");
            printf("\ts_magic: 0x%X\n", message->s_header.s_magic);
            printf("\ts_payload_len: %d\n", message->s_header.s_payload_len);
            printf("\ts_type: %s\n", MessageTypeToStr(message->s_header.s_type));
            printf("\ts_local_time: %d\n", message->s_header.s_local_time);
            printf("\ts_payload: %s\n", message->s_payload);
            break;
        }
        default:
        {
            vprintf(format, valist);
            break;
        }
    }

    va_end(valist);
}
