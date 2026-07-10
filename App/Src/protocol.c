#include "protocol.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static char *next_token(char **cursor)
{
    char *start = *cursor;

    while (*start == ' ' || *start == '\t') {
        ++start;
    }
    if (*start == '\0') {
        *cursor = start;
        return NULL;
    }

    *cursor = start;
    while (**cursor != '\0' && **cursor != ' ' && **cursor != '\t') {
        **cursor = (char)toupper((unsigned char)**cursor);
        ++(*cursor);
    }
    if (**cursor != '\0') {
        **cursor = '\0';
        ++(*cursor);
    }
    return start;
}

static bool parse_float(char *token, float *value)
{
    char *end;
    if (token == NULL) {
        return false;
    }
    *value = strtof(token, &end);
    return end != token && *end == '\0' && isfinite(*value);
}

bool Protocol_Parse(char *line, ParsedCommand *command)
{
    char *cursor = line;
    char *verb = next_token(&cursor);
    char *token;

    memset(command, 0, sizeof(*command));
    if (verb == NULL) {
        return false;
    }

    if (strcmp(verb, "STOP") == 0 || strcmp(verb, "ESTOP") == 0) {
        command->type = CMD_STOP;
        return next_token(&cursor) == NULL;
    }
    if (strcmp(verb, "CLEAR") == 0) {
        command->type = CMD_CLEAR;
        return next_token(&cursor) == NULL;
    }
    if (strcmp(verb, "STATUS") == 0) {
        command->type = CMD_STATUS;
        return next_token(&cursor) == NULL;
    }
    if (strcmp(verb, "HELP") == 0) {
        command->type = CMD_HELP;
        return next_token(&cursor) == NULL;
    }
    if (strcmp(verb, "CANCEL") == 0) command->type = CMD_CANCEL;
    else if (strcmp(verb, "ODOM") == 0) command->type = CMD_ODOM;
    else if (strcmp(verb, "RESETODOM") == 0) command->type = CMD_RESET_ODOM;
    else if (strcmp(verb, "SAVE") == 0) command->type = CMD_SAVE;
    else if (strcmp(verb, "LOAD") == 0) command->type = CMD_LOAD;
    else if (strcmp(verb, "DEFAULTS") == 0) command->type = CMD_DEFAULTS;
    else if (strcmp(verb, "CONFIG") == 0) command->type = CMD_CONFIG;
    else if (strcmp(verb, "DIAG") == 0) command->type = CMD_DIAG;
    else if (strcmp(verb, "LOG") == 0) command->type = CMD_LOG;
    else if (strcmp(verb, "VERSION") == 0) command->type = CMD_VERSION;
    else if (strcmp(verb, "RUN") == 0) command->type = CMD_RUN_ROUTE;
    else if (strcmp(verb, "CLEARROUTE") == 0) command->type = CMD_CLEAR_ROUTE;
    else if (strcmp(verb, "ROUTE") == 0) command->type = CMD_ROUTE;
    if (command->type != CMD_INVALID) return next_token(&cursor) == NULL;
    if (strcmp(verb, "MODE") == 0) {
        token = next_token(&cursor);
        if (token == NULL || next_token(&cursor) != NULL) {
            return false;
        }
        command->type = CMD_MODE;
        if (strcmp(token, "MANUAL") == 0) command->mode = 0;
        else if (strcmp(token, "TRACK") == 0) command->mode = 1;
        else if (strcmp(token, "AVOID") == 0) command->mode = 2;
        else if (strcmp(token, "FUSION") == 0) command->mode = 3;
        else if (strcmp(token, "CRUISE") == 0) command->mode = 4;
        else return false;
        return true;
    }
    if (strcmp(verb, "DRIVE") == 0) {
        command->type = CMD_DRIVE;
        if (!parse_float(next_token(&cursor), &command->a) ||
            !parse_float(next_token(&cursor), &command->b)) {
            return false;
        }
        return next_token(&cursor) == NULL;
    }
    if (strcmp(verb, "SPEED") == 0) {
        command->type = CMD_SPEED;
        if (!parse_float(next_token(&cursor), &command->a)) {
            return false;
        }
        return next_token(&cursor) == NULL;
    }
    if (strcmp(verb, "PID") == 0) {
        command->type = CMD_PID;
        if (!parse_float(next_token(&cursor), &command->a) ||
            !parse_float(next_token(&cursor), &command->b) ||
            !parse_float(next_token(&cursor), &command->c)) {
            return false;
        }
        return next_token(&cursor) == NULL;
    }
    if (strcmp(verb, "MOVE") == 0 || strcmp(verb, "TURN") == 0 ||
        strcmp(verb, "QMOVE") == 0 || strcmp(verb, "QTURN") == 0 ||
        strcmp(verb, "RAMP") == 0 || strcmp(verb, "LINEGAIN") == 0) {
        if (strcmp(verb, "MOVE") == 0) command->type = CMD_MOVE;
        else if (strcmp(verb, "TURN") == 0) command->type = CMD_TURN;
        else if (strcmp(verb, "QMOVE") == 0) command->type = CMD_QUEUE_MOVE;
        else if (strcmp(verb, "QTURN") == 0) command->type = CMD_QUEUE_TURN;
        else if (strcmp(verb, "RAMP") == 0) command->type = CMD_RAMP;
        else command->type = CMD_LINE_GAIN;
        if (!parse_float(next_token(&cursor), &command->a)) return false;
        return next_token(&cursor) == NULL;
    }
    if (strcmp(verb, "WHEEL") == 0 || strcmp(verb, "BATTERY") == 0 ||
        strcmp(verb, "STALL") == 0) {
        if (strcmp(verb, "WHEEL") == 0) command->type = CMD_WHEEL;
        else if (strcmp(verb, "BATTERY") == 0) command->type = CMD_BATTERY;
        else command->type = CMD_STALL;
        if (!parse_float(next_token(&cursor), &command->a) ||
            !parse_float(next_token(&cursor), &command->b) ||
            !parse_float(next_token(&cursor), &command->c)) return false;
        return next_token(&cursor) == NULL;
    }
    return false;
}

const char *Protocol_Help(void)
{
    return "MODE MANUAL|TRACK|AVOID|FUSION|CRUISE\r\n"
           "DRIVE <left_rpm> <right_rpm>\r\n"
           "SPEED <rpm> | PID <kp> <ki> <kd>\r\n"
           "MOVE <cm> | TURN <deg> | CANCEL | ODOM | RESETODOM\r\n"
           "QMOVE <cm> | QTURN <deg> | ROUTE | RUN | CLEARROUTE\r\n"
           "WHEEL <diam_cm> <track_cm> <cpr> | RAMP <rpm_s>\r\n"
           "LINEGAIN <gain> | BATTERY <low> <recover> <ratio>\r\n"
           "STALL <pwm_pct> <rpm> <ms>\r\n"
           "SAVE | LOAD | DEFAULTS | CONFIG | DIAG | LOG | VERSION\r\n"
           "STOP | CLEAR | STATUS | HELP\r\n";
}
