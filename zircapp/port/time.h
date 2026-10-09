/* ZircApp port shim for <time.h>.
 *
 * The CodeOS userspace freestanding libc (kernel/userspace/include/time.h)
 * provides time()/gettimeofday() but not ctime(), which Zircon's apps/clock.c
 * calls.  This shim is found before the real header (the port directory is
 * first on the include path) and re-exports it with ctime() added.
 *
 * ctime() is modelled on the Zircon/qt6 sysroot's: "Www Mmm dd hh:mm:ss yyyy".
 * The value is whatever time() returns, which in Zircon is the kernel clock in
 * seconds -- it is not necessarily a Unix epoch, and the app only displays it.
 */
#ifndef ZIRCAPP_PORT_TIME_H
#define ZIRCAPP_PORT_TIME_H

#include_next <time.h>

static inline char *ctime(const time_t *t) {
    static char buf[32];
    static const char *wd[7] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };
    static const char *mo[12] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    long secs = t ? (long)*t : 0;
    long days = secs / 86400;
    long rem = secs % 86400;
    if (rem < 0) { rem += 86400; days -= 1; }

    /* civil-from-days (Howard Hinnant's algorithm), 1970-01-01 = day 0. */
    long z = days + 719468;
    long era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long y = (long)yoe + era * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    unsigned d = doy - (153 * mp + 2) / 5 + 1;
    unsigned m = (mp < 10 ? mp + 3 : mp - 9);
    if (m <= 2) y++;

    int p = 0;
    int wday = (int)((days % 7 + 11) % 7); /* 1970-01-01 was a Thursday */
    for (int i = 0; i < 3; i++) buf[p++] = wd[wday][i];
    buf[p++] = ' ';
    for (int i = 0; i < 3; i++) buf[p++] = mo[(m - 1) % 12][i];
    buf[p++] = ' ';
    buf[p++] = (char)('0' + (d / 10) % 10);
    buf[p++] = (char)('0' + d % 10);
    buf[p++] = ' ';
    buf[p++] = (char)('0' + (rem / 3600) / 10);
    buf[p++] = (char)('0' + (rem / 3600) % 10);
    buf[p++] = ':';
    buf[p++] = (char)('0' + (rem / 60) % 60 / 10);
    buf[p++] = (char)('0' + (rem / 60) % 60 % 10);
    buf[p++] = ':';
    buf[p++] = (char)('0' + (rem % 60) / 10);
    buf[p++] = (char)('0' + (rem % 60) % 10);
    buf[p++] = ' ';
    /* year, right-aligned in 4 columns */
    char ybuf[8];
    int yn = 0;
    if (y <= 0) { ybuf[yn++] = '0'; }
    else {
        while (y > 0 && yn < 8) { ybuf[yn++] = (char)('0' + y % 10); y /= 10; }
    }
    while (yn < 4) ybuf[yn++] = '0';
    for (int i = yn - 1; i >= 0; i--) buf[p++] = ybuf[i];
    buf[p++] = '\n';
    buf[p] = 0;
    return buf;
}

#endif
