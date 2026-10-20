/* PixelMan stubs for Zircon - minimal fb drawing functions */

#include "pixelman.h"
#include "../arch/x86_64/fb.h"
#include "kprintf.h"

/* NOTE: fb_drawstr_px is NOT stubbed here.
 *
 * arch/fb.cpp is on the Zircon link line (it is the framebuffer
 * implementation the kernel calls into: fb_init, fb_fill, fb_get_pitch, the
 * cursor entry points), and it defines fb_drawstr_px.  Defining it here as
 * well made the link fail with "multiple definition of fb_drawstr_px".  The
 * real one is the better one anyway: this stub was a no-op, so a Zircon build
 * that linked against it would have drawn no console text at all.
 */

/* pm_composite_rect - used by fb.cpp */
void pm_composite_rect(uint32_t *buf, int stride, pm_rect_t clip,
                       int x, int y, int w, int h, uint32_t color) {
    (void)buf; (void)stride; (void)clip; (void)x; (void)y;
    (void)w; (void)h; (void)color;
    /* No-op for Zircon */
}

/* pm_composite_glyph - used by fb.cpp */
void pm_composite_glyph(uint32_t *buf, int stride, pm_rect_t clip,
                        int x, int y, const uint8_t *glyph, int gw, int gh,
                        uint32_t fg, uint32_t bg) {
    (void)buf; (void)stride; (void)clip; (void)x; (void)y;
    (void)glyph; (void)gw; (void)gh; (void)fg; (void)bg;
}

/* ── Image objects ──
 *
 * The X server (kernel/xserver.cpp) allocates a pixelman image per window
 * buffer.  Those calls became live when the syscall dispatcher did -- the
 * window syscalls reach xs_ensure_window_buffer() -- and Zircon has no
 * PixelMan, so the allocation fails and the X server has to cope without a
 * buffer.
 *
 * The signatures are the ones in pixelman.h, which is the Zircon copy of
 * the header and the one everything in this tree compiles against. */
pixelman_image_t *pixelman_image_create_bits(pixelman_format_code_t format,
                                              int width, int height,
                                              uint32_t *bits, int stride) {
    (void)format; (void)width; (void)height; (void)bits; (void)stride;
    return 0;
}

void *pixelman_image_get_data(pixelman_image_t *img) {
    (void)img;
    return 0;
}

int pixelman_image_get_stride(pixelman_image_t *img) {
    (void)img;
    return 0;
}

int pixelman_image_get_width(pixelman_image_t *img) {
    (void)img;
    return 0;
}

int pixelman_image_get_height(pixelman_image_t *img) {
    (void)img;
    return 0;
}