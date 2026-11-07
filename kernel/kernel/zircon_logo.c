#include "zircon_logo.h"

#include <stdint.h>

#include "../arch/x86_64/fb.h"
#include "kprintf.h"

#define LOGO_SOURCE_SIZE 192u
#define LOGO_SOURCE_BYTES (LOGO_SOURCE_SIZE * LOGO_SOURCE_SIZE * 2u)
#define LOGO_MAX_DISPLAY_SIZE 320u
#define SCREEN_BACKGROUND 0x000000u
#define LOGO_FOREGROUND 0x00cdd6f4u

extern const uint8_t _binary_zircon_logo_boot_rgb565_start[];
extern const uint8_t _binary_zircon_logo_boot_rgb565_end[];

void zircon_logo_show(void)
{
    uint32_t screen_w = fb_getwidth();
    uint32_t screen_h = fb_getheight();
    uintptr_t asset_size = (uintptr_t)_binary_zircon_logo_boot_rgb565_end -
                           (uintptr_t)_binary_zircon_logo_boot_rgb565_start;
    if (!fb_available() || !screen_w || !screen_h)
        return;
    if (asset_size < LOGO_SOURCE_BYTES) {
        kprintf("zircon: startup logo asset is truncated\n");
        return;
    }

    uint32_t size = screen_h / 3;
    if (size > LOGO_MAX_DISPLAY_SIZE)
        size = LOGO_MAX_DISPLAY_SIZE;
    if (size > screen_w)
        size = screen_w;
    if (!size)
        return;

    uint32_t left = (screen_w - size) / 2;
    uint32_t top = (screen_h - size) / 2 - (screen_h >= size + 120 ? 60 : 0);
    for (uint32_t y = 0; y < size; y++) {
        uint32_t sy = (y * LOGO_SOURCE_SIZE) / size;
        for (uint32_t x = 0; x < size; x++) {
            uint32_t sx = (x * LOGO_SOURCE_SIZE) / size;
            uint32_t index = (sy * LOGO_SOURCE_SIZE + sx) * 2;
            uint16_t pixel = (uint16_t)_binary_zircon_logo_boot_rgb565_start[index] |
                             (uint16_t)_binary_zircon_logo_boot_rgb565_start[index + 1] << 8;
            uint32_t red = ((pixel >> 11) & 0x1f) * 255 / 31;
            uint32_t green = ((pixel >> 5) & 0x3f) * 255 / 63;
            uint32_t blue = (pixel & 0x1f) * 255 / 31;
            fb_putpixel(left + x, top + y, (red << 16) | (green << 8) | blue);
        }
    }

    const char *label = "Zircon OS";
    int label_w = fb_text_width(label);
    uint32_t label_x = label_w > 0 && (uint32_t)label_w < screen_w
                           ? (screen_w - (uint32_t)label_w) / 2
                           : 0;
    fb_drawstr_px(label_x, top + size + 20,
                  label, LOGO_FOREGROUND, SCREEN_BACKGROUND);
}
