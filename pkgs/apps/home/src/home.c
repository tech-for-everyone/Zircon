/* Static app registration for the compositor-owned Zircon shell. */
#include <zircapp.h>
#include <kall.h>
#include <sms.h>
#include <settings.h>
#include <camera.h>
#include <clock.h>
#include <calculator.h>
#include <music.h>
#include <gallery.h>
#include <browser.h>
#include <notes.h>
#include <files.h>
#include <terminal.h>
#include <weather.h>
#include <about.h>
#include <notifications.h>
#include <store.h>
#include <moveon.h>
#include <stdio.h>

static int apps_registered = 0;

void zircon_apps_register(void) {
    if (apps_registered)
        return;
    zircapp_register(&kall_app);
    zircapp_register(&sms_app);
    zircapp_register(&settings_app);
    zircapp_register(&camera_app);
    zircapp_register(&clock_app);
    zircapp_register(&calc_app);
    zircapp_register(&music_app);
    zircapp_register(&gallery_app);
    zircapp_register(&browser_app);
    zircapp_register(&notes_app);
    zircapp_register(&files_app);
    zircapp_register(&terminal_app);
    zircapp_register(&weather_app);
    zircapp_register(&about_app);
    zircapp_register(&notifications_app);
    zircapp_register(&moveon_app);
    zircapp_register(&store_app);
    apps_registered = 1;
    printf("zircon: registered %d ZircApp modules\n", zircapp_count());
}
