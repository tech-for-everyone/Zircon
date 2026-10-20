/* Minimal shell stubs for Zircon */
#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>

/* Declarations - implementations in shell_stubs.c */
void shell_init(void);
void shell_run(void);
int shell_execute(const char *cmd);
void shell_exec_cmd(const char *cmd);  /* For script.c compatibility */
void shell_selftest(void);
int shell_readln(char *buf, int max);
int shell_kbhit(void);
void shell_exec_done(void);

#endif
