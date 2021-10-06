#include "IOMisc.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include "common.h"
#include "ipc.h"

static FILE *eventLogFile = NULL;
static FILE *pipesLogFile = NULL;
static bool isLogInitialized = false;

void InitLog()
{
    eventLogFile = fopen(events_log, "w");
    pipesLogFile = fopen(pipes_log, "w");
    isLogInitialized = true;
}

void Shutdown()
{
    fclose(eventLogFile);
    fclose(pipesLogFile);
}

const char* MessageTypeToStr(MessageType type)
{
    switch (type)
    {
        case STARTED:
            return "STARTED";
        case DONE:
            return "DONE";
        case ACK:
            break;
        case STOP:
            break;
        case TRANSFER:
            break;
        case BALANCE_HISTORY:
            break;
        case CS_REQUEST:
            break;
        case CS_REPLY:
            break;
        case CS_RELEASE:
            break;
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
