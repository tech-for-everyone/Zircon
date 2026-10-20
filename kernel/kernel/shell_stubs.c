/* Shell-related stubs for Zircon kernel */

#include "shell.h"
#include "shell_stubs.h"

void shell_exec_done(void) {
    /* No-op for Zircon - container exec not supported */
}

void shell_init(void) { }
void shell_run(void) { }
int shell_execute(const char *cmd) {
    (void)cmd;
    return 0;
}
void shell_exec_cmd(const char *cmd) {
    (void)cmd;
}
void shell_selftest(void) { }
int shell_readln(char *buf, int max) {
    (void)buf; (void)max; return 0;
}
int shell_kbhit(void) { return 0; }
