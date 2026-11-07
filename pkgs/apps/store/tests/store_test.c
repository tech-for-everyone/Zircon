/* store_test.c — host test for the Zircon app store.
 *
 * The store's screen needs a framebuffer, so its *logic* is tested here
 * instead: the catalogue and the installed set are plain C, and they are
 * what the Home grid and the Store screen both read.  A bug here would show
 * up as a phone whose home screen disagrees with its own store, which no
 * screenshot would explain.
 *
 * Build and run:
 *   cc -std=c11 -Wall -Wextra -I pkgs/apps/store/src \
 *      -I pkgs/core/base/zircapp/include \
 *      -o /tmp/store-test pkgs/apps/store/tests/store_test.c \
 *      pkgs/apps/store/src/store.c && /tmp/store-test
 */
#include <stdio.h>
#include <string.h>

#include "store.h"

static int failures;

#define CHECK(cond, what)                                                      \
    do {                                                                       \
        if (cond) {                                                            \
            printf("ok  : %s\n", what);                                        \
        } else {                                                               \
            printf("FAIL: %s\n", what);                                        \
            failures++;                                                        \
        }                                                                      \
    } while (0)

int main(void) {
    /* The catalogue. */
    CHECK(store_count() == 16, "the catalogue has 16 apps");
    const store_item_t *first = store_item(0);
    CHECK(first != 0, "item 0 exists");
    CHECK(first && first->name && first->name[0], "item 0 has a name");
    CHECK(first && first->version && first->version[0], "item 0 has a version");
    CHECK(first && first->desc && first->desc[0], "item 0 has a description");
    CHECK(first && first->category && first->category[0], "item 0 has a category");
    CHECK(first && first->size_kb > 0, "item 0 has a size");
    CHECK(store_item(-1) == 0, "a negative index is not an item");
    CHECK(store_item(store_count()) == 0, "an index past the end is not an item");
    CHECK(store_category_count() == 7, "the catalogue exposes category filters");
    CHECK(strcmp(store_category(0), "All") == 0, "the first category shows all apps");
    CHECK(store_category(-1) == 0, "an invalid category is rejected");
    CHECK(store_filtered_count("notes", "All") == 1, "search matches app names case-insensitively");
    CHECK(store_filtered_count("device", "Tools") == 1, "search matches descriptions inside a category");
    CHECK(store_filtered_count("", "Phone") == 2, "category filtering returns phone apps");
    CHECK(store_filtered_index(0, "notes", "All") == store_index_of("Notes"),
          "filtered positions map back to catalogue items");
    CHECK(store_filtered_index(1, "notes", "All") == -1, "filtered indexes reject overflow");
    CHECK(store_filtered_count("", "Dev") == 1, "developer tools have a category filter");

    /* Every app the store lists must be a name the registry knows, or the
     * store would offer something the Home screen cannot open.  The names
     * here are the ones home.c registers (pkgs/apps/home/src/home.c). */
    static const char *kRegistryNames[] = {
        "Kall", "SMS", "Notifications", "Settings", "About", "Clock",
        "Calculator", "Weather", "Notes", "Files", "Camera", "Gallery",
        "Music", "Browser", "Terminal", "Move On",
    };
    int listed = 0;
    for (unsigned i = 0; i < sizeof(kRegistryNames) / sizeof(kRegistryNames[0]); i++) {
        if (store_index_of(kRegistryNames[i]) >= 0) listed++;
    }
    CHECK(listed == store_count(), "every catalogue entry is a registered app");

    /* The installed set. */
    CHECK(store_is_installed("Clock") == 1, "everything starts installed");
    CHECK(store_set_installed("Clock", 0) == 1, "removing an app reports a change");
    CHECK(store_is_installed("Clock") == 0, "a removed app is not installed");
    CHECK(store_set_installed("Clock", 0) == 0, "removing it again reports no change");
    CHECK(store_set_installed("Clock", 1) == 1, "installing reports a change");
    CHECK(store_is_installed("Clock") == 1, "an installed app is installed");

    /* An app the catalogue does not know about is not the store's business:
     * it must stay visible (Home and Store are such apps -- there would be
     * no way back to the grid otherwise) and it must not be changeable. */
    CHECK(store_is_installed("NoSuchApp") == 1, "an unmanaged app stays visible");
    CHECK(store_set_installed("NoSuchApp", 0) == -1, "an unmanaged app cannot be removed");

    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all store checks passed\n");
    return 0;
}
