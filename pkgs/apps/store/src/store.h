/* Zircon Store — the app catalogue for the Zircon phone OS.
 *
 * WHAT THIS IS
 *
 * Zircon ships every app inside the image (they are linked into zircond),
 * so there is nothing to download and nothing to unpack.  What a phone user
 * actually wants from a store is a *catalogue*: what is available, what it
 * does, how big it is, and which of them are on my home screen.
 *
 * That is what this package is.  It owns:
 *
 *   - the catalogue (name, version, description, category, size), and
 *   - the installed set -- the apps that appear on the Home grid.
 *
 * "Installing" an app means putting it back on the Home grid; "removing" it
 * means taking it off.  The compositor's Home screen asks this package which
 * apps to show, so the two are never out of step: there is one owner of the
 * installed set, not two.
 *
 * WHY THE STATE IS NOT PERSISTED
 *
 * The Zircon image filesystem is the initramfs: it is rebuilt from the
 * source tree on every build and lives in RAM at boot.  There is no writable
 * persistent storage, so a file written to it would be lost at the next
 * reboot -- persisting the state would look like it worked and then quietly
 * forget.  The installed set therefore lives for the session, and resets on
 * reboot.  When Zircon grows a writable volume, this is the one place that
 * needs to change.
 */
#ifndef ZIRCON_STORE_H
#define ZIRCON_STORE_H

#include <zircapp.h>

#define STORE_MAX_ITEMS 32
#define STORE_NAME_MAX  24
#define STORE_VER_MAX   12
#define STORE_DESC_MAX  56
#define STORE_CAT_MAX   16

typedef struct {
    const char *name;      /* must match the zircapp registry name */
    const char *version;
    const char *desc;
    const char *category;
    unsigned    size_kb;
} store_item_t;

/* Catalogue access. */
int                 store_count(void);
const store_item_t *store_item(int idx);
int                 store_index_of(const char *name);

/* Installed set: 1 = shown on the Home grid, 0 = removed by the user. */
int store_is_installed(const char *name);
int store_set_installed(const char *name, int installed);

extern zircapp_app_t store_app;

#endif /* ZIRCON_STORE_H */
