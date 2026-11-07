/* Zircon Move On — stage the phone's user files for CodeOS transfer. */
#include "moveon_syscalls.h"
#include <zircapp.h>
#include <windows.h>
#include <string.h>
#include <stdio.h>

#define MOVEON_SOURCE "/root/root/home"
#define MOVEON_PARENT "/root/root/home/Move On"
#define MOVEON_PATH_MAX 256
#define MOVEON_READDIR_MAX (33 * 512)
#define MOVEON_MAX_DEPTH 16
#define MOVEON_MAX_ENTRIES 500
#define MOVEON_MAX_FILE_SIZE 65536

static int entry_count;
static char status_text[160] = "Ready to prepare a safe export.";

const char *moveon_status(void) {
    return status_text;
}

static size_t bounded_length(const char *text, size_t max_length) {
    size_t length = 0;
    while (length < max_length && text[length])
        length++;
    return length;
}

static int join_path(char *out, const char *parent, const char *name) {
    size_t parent_len = strlen(parent);
    size_t name_len = strlen(name);
    int separator = parent_len > 0 && parent[parent_len - 1] != '/';

    if (parent_len + (size_t)separator + name_len >= MOVEON_PATH_MAX)
        return -1;
    memcpy(out, parent, parent_len);
    if (separator)
        out[parent_len++] = '/';
    memcpy(out + parent_len, name, name_len + 1);
    return 0;
}

static int valid_name(const char *name, size_t length) {
    if (length == 0 || (length == 1 && name[0] == '.') ||
        (length == 2 && name[0] == '.' && name[1] == '.') ||
        name[length - 1] == ' ' || name[length - 1] == '.')
        return 0;
    for (size_t i = 0; i < length; i++) {
        unsigned char ch = (unsigned char)name[i];
        if (ch < 32 || ch == 127 || ch == '/' || ch == '\\' || ch == ':')
            return 0;
    }
    return 1;
}

static int is_output_dir_name(const char *name, size_t length) {
    static const char output_name[] = "Move On";
    if (length != sizeof(output_name) - 1)
        return 0;
    for (size_t i = 0; i < length; i++)
        if (name[i] != output_name[i])
            return 0;
    return 1;
}

static int copy_file(const char *source, const char *target, int expected_size) {
    char buffer[1024];
    int source_fd = sys_open(source, O_RDONLY);
    if (source_fd < 0)
        return -1;
    int target_fd = sys_open(target, O_WRONLY | O_CREAT);
    if (target_fd < 0) {
        sys_close(source_fd);
        return -1;
    }

    int total = 0;
    for (;;) {
        int bytes = sys_read(source_fd, buffer, sizeof(buffer));
        if (bytes < 0) {
            total = -1;
            break;
        }
        if (bytes == 0)
            break;
        int offset = 0;
        while (offset < bytes) {
            int written = sys_pwrite(target_fd, buffer + offset, bytes - offset);
            if (written <= 0) {
                total = -1;
                break;
            }
            offset += written;
        }
        if (total < 0)
            break;
        total += bytes;
    }

    int source_close_result = sys_close(source_fd);
    int target_close_result = sys_close(target_fd);
    if (source_close_result < 0 || target_close_result < 0)
        return -1;
    return total == expected_size ? 0 : -1;
}

static int walk_tree(const char *path, const char *target, int depth, int copy) {
    stat_t info;
    if (depth > MOVEON_MAX_DEPTH || sys_stat(path, &info) < 0)
        return -1;
    if (depth > 0 && ++entry_count > MOVEON_MAX_ENTRIES)
        return -1;

    if (!info.is_dir) {
        if (info.size < 0 || info.size > MOVEON_MAX_FILE_SIZE)
            return -1;
        return copy ? copy_file(path, target, info.size) : 0;
    }

    char names[MOVEON_READDIR_MAX];
    int bytes = sys_readdir(path, names, sizeof(names));
    if (bytes < 0 || bytes >= (int)sizeof(names))
        return -1;

    for (int offset = 0; offset < bytes;) {
        size_t remaining = (size_t)(bytes - offset);
        size_t length = bounded_length(names + offset, remaining);
        if (length == remaining)
            return -1;
        const char *name = names + offset;
        offset += (int)length + 1;
        if (depth == 0 && is_output_dir_name(name, length))
            continue;
        if ((length == 1 && name[0] == '.') ||
            (length == 2 && name[0] == '.' && name[1] == '.'))
            continue;
        if (!valid_name(name, length))
            return -1;

        char child[MOVEON_PATH_MAX];
        if (join_path(child, path, name) < 0)
            return -1;
        char target_child[MOVEON_PATH_MAX];
        if (join_path(target_child, target, name) < 0)
            return -1;
        stat_t child_info;
        if (sys_stat(child, &child_info) < 0)
            return -1;
        if (copy && child_info.is_dir && sys_mkdir(target_child) < 0)
            return -1;
        if (walk_tree(child, target_child, depth + 1, copy) < 0)
            return -1;
    }
    return 0;
}

static int write_info(const char *export_path) {
    static const char info[] =
        "Zircon Move On export\n"
        "Copy this folder to a host computer, then run:\n"
        "python3 Zircon/scripts/move_on.py export <this-folder> --output zircon-data.moveon\n"
        "Transfer the .moveon bundle to CodeOS and run:\n"
        "python3 Zircon/scripts/move_on.py import zircon-data.moveon --destination <new-folder>\n";
    char path[MOVEON_PATH_MAX];
    if (join_path(path, export_path, "MOVE-ON-INFO.txt") < 0)
        return -1;
    int fd = sys_open(path, O_WRONLY | O_CREAT);
    if (fd < 0)
        return -1;
    int result = sys_pwrite(fd, info, sizeof(info) - 1);
    int close_result = sys_close(fd);
    return result == (int)sizeof(info) - 1 && close_result == 0 ? 0 : -1;
}

static int create_export(char *export_path) {
    stat_t info;
    if (sys_stat(MOVEON_SOURCE, &info) < 0 || !info.is_dir) {
        printf("Move On: cannot access %s\n", MOVEON_SOURCE);
        return -1;
    }

    entry_count = 0;
    if (walk_tree(MOVEON_SOURCE, "", 0, 0) < 0) {
        printf("Move On: data exceeds supported path, entry, or 64 KiB file limits.\n");
        return -1;
    }
    int entries = entry_count;

    if (sys_mkdir(MOVEON_PARENT) < 0 &&
        (sys_stat(MOVEON_PARENT, &info) < 0 || !info.is_dir)) {
        printf("Move On: cannot create export folder.\n");
        return -1;
    }

    int created = 0;
    for (int index = 1; index <= 999; index++) {
        char name[32];
        snprintf(name, sizeof(name), "export-%03d", index);
        if (join_path(export_path, MOVEON_PARENT, name) < 0)
            return -1;
        if (sys_stat(export_path, &info) == 0)
            continue;
        if (sys_mkdir(export_path) < 0) {
            printf("Move On: cannot create %s\n", export_path);
            return -1;
        }
        created = 1;
        break;
    }
    if (!created) {
        printf("Move On: all export slots are in use.\n");
        return -1;
    }

    entry_count = 0;
    if (walk_tree(MOVEON_SOURCE, export_path, 0, 1) < 0 ||
        write_info(export_path) < 0) {
        printf("Move On: export failed; partial data remains at %s\n", export_path);
        return -1;
    }
    printf("Move On: exported %d entries to %s\n", entries, export_path);
    return 0;
}

static void moveon_init(struct zircapp_app *app) {
    app->win_id = gui_window_create(0, 0, zircapp_phone_get_width(),
                                    zircapp_phone_get_height(), "Move On");
    printf("Move On: tap Prepare export to stage a Zircon data export.\n");
    printf("Move On: host tool: python3 Zircon/scripts/move_on.py\n");
}

static void moveon_event(struct zircapp_app *app, const zircapp_event_t *ev) {
    (void)app;
    if (ev->type != ZIRCAPP_EVENT_TOUCH_DOWN || ev->touch_count <= 0)
        return;
    int width = zircapp_phone_get_width();
    int height = zircapp_phone_get_height();
    if (ev->touch[0].x < 20 || ev->touch[0].x >= width - 20 ||
        ev->touch[0].y < height / 2 - 25 || ev->touch[0].y >= height / 2 + 25)
        return;
    char export_path[MOVEON_PATH_MAX];
    printf("Move On: preparing user data...\n");
    if (create_export(export_path) == 0)
        snprintf(status_text, sizeof(status_text), "Ready: copy %s to a host.", export_path);
    else
        snprintf(status_text, sizeof(status_text), "Export failed. See the system log for details.");
}

static void moveon_stop(struct zircapp_app *app) {
    gui_window_close(app->win_id);
}

zircapp_app_t moveon_app = {
    .name = "Move On",
    .init = moveon_init,
    .event = moveon_event,
    .stop = moveon_stop,
    .state = ZIRCAPP_APP_STOPPED,
    .win_id = -1,
};
