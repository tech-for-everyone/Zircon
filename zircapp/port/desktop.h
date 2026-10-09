/* Host-port shim for Zircon's `gui` headers.
 *
 * Zircon's gui/windows.h and gui/widgets.h both `#include "desktop.h"`,
 * which in the Zircon build resolves to kernel/kernel/desktop.h and pulls
 * in kernel types.  The CodeOS userspace ZircApp host only needs the
 * window/widget declarations (gui_window_*, gui_widget_*), so this shim
 * stands in for desktop.h and keeps the port free of kernel headers. */
#ifndef ZIRCAPP_PORT_DESKTOP_H
#define ZIRCAPP_PORT_DESKTOP_H
/* deliberately empty — no kernel types are required by gui/windows.h */
#endif
