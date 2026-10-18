/* Host-test shim for CodeOS userspace <unistd.h>.
 *
 * The Phase-2 host (pkgs/core/zircapp/src/zircapp.c) includes "unistd.h"
 * for the window-bridge calls.  The kernel headers define those as
 * `static inline` syscall wrappers; on the build machine we cannot run
 * `int $0x80`, so this shim declares the same three entry points as
 * ordinary functions and the test provides them.  Only the calls the
 * host actually makes are declared -- nothing here should grow into a
 * general libc.
 */
#ifndef ZIRCAPP_HOST_TEST_UNISTD_H
#define ZIRCAPP_HOST_TEST_UNISTD_H

#include <stddef.h>

int sys_read(int fd, void *buf, int count);
int sys_pwrite(int fd, const void *buf, int count);
int sys_wm_attach(void);
int sys_get_info(int cmd, char *buf, int max_len);

#endif /* ZIRCAPP_HOST_TEST_UNISTD_H */
