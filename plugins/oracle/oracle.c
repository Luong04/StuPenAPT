#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include "arc_plugin_interface.h"

static char *mapping[] =
{
    "OracleTelnet:oracle_telnet",
    "OracleFTP:oracle_ftp",
    "OracleSSH:oracle_ssh"
};

const char *tool_name(void)
{
    return "Oracle";
}

char **tool_interface(size_t *num_caps)
{
    *num_caps = sizeof(mapping)/sizeof(char*);
    return mapping;
}
