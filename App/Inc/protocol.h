#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    CMD_INVALID = 0,
    CMD_MODE,
    CMD_DRIVE,
    CMD_SPEED,
    CMD_PID,
    CMD_STOP,
    CMD_CLEAR,
    CMD_STATUS,
    CMD_HELP,
    CMD_MOVE,
    CMD_TURN,
    CMD_CANCEL,
    CMD_ODOM,
    CMD_RESET_ODOM,
    CMD_SAVE,
    CMD_LOAD,
    CMD_DEFAULTS,
    CMD_CONFIG,
    CMD_WHEEL,
    CMD_RAMP,
    CMD_LINE_GAIN,
    CMD_BATTERY,
    CMD_STALL,
    CMD_DIAG,
    CMD_LOG,
    CMD_VERSION,
    CMD_QUEUE_MOVE,
    CMD_QUEUE_TURN,
    CMD_RUN_ROUTE,
    CMD_CLEAR_ROUTE,
    CMD_ROUTE
} CommandType;

typedef struct {
    CommandType type;
    int mode;
    float a;
    float b;
    float c;
} ParsedCommand;

bool Protocol_Parse(char *line, ParsedCommand *command);
const char *Protocol_Help(void);

#endif
