/* Shell stubs header for Zircon */
#ifndef SHELL_STUBS_H
#define SHELL_STUBS_H

#include <stdint.h>

void shell_exec_done(void);
int shell_execute(const char *cmd);
void shell_call(const char *cmd);

#endif
