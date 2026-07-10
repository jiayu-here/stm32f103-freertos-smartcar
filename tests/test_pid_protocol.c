#include "pid.h"
#include "protocol.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void test_pid(void)
{
    PIDController pid;
    PID_Init(&pid, 1.0f, 0.5f, 0.0f, 100.0f, 10.0f);
    assert(fabsf(PID_Update(&pid, 10.0f, 0.0f, 0.1f) - 10.5f) < 0.001f);
    for (int i = 0; i < 100; ++i) (void)PID_Update(&pid, 1000.0f, 0.0f, 0.1f);
    assert(fabsf(pid.integral - 10.0f) < 0.001f);
    assert(fabsf(PID_Update(&pid, 1000.0f, 0.0f, 0.1f) - 100.0f) < 0.001f);
    PID_Reset(&pid);
    assert(pid.integral == 0.0f && pid.previous_error == 0.0f);
}

static void expect_command(const char *text, CommandType type)
{
    char line[80];
    ParsedCommand command;
    strcpy(line, text);
    assert(Protocol_Parse(line, &command));
    assert(command.type == type);
}

static void test_protocol(void)
{
    char invalid[] = "DRIVE 20";
    char invalid_nan[] = "MOVE NAN";
    expect_command("mode fusion", CMD_MODE);
    expect_command("DRIVE -80 80", CMD_DRIVE);
    expect_command("PID 0.6 0.35 0.005", CMD_PID);
    expect_command("CLEAR", CMD_CLEAR);
    expect_command("MOVE 120", CMD_MOVE);
    expect_command("TURN -90", CMD_TURN);
    expect_command("WHEEL 6.5 14.5 780", CMD_WHEEL);
    expect_command("BATTERY 6.6 6.9 4.0303", CMD_BATTERY);
    expect_command("STALL 75 4 1000", CMD_STALL);
    expect_command("SAVE", CMD_SAVE);
    expect_command("DIAG", CMD_DIAG);
    expect_command("QMOVE 50", CMD_QUEUE_MOVE);
    expect_command("QTURN 90", CMD_QUEUE_TURN);
    expect_command("RUN", CMD_RUN_ROUTE);
    expect_command("ROUTE", CMD_ROUTE);
    assert(!Protocol_Parse(invalid, &(ParsedCommand){0}));
    assert(!Protocol_Parse(invalid_nan, &(ParsedCommand){0}));
}

int main(void)
{
    test_pid();
    test_protocol();
    puts("PASS: PID and protocol host tests");
    return 0;
}
