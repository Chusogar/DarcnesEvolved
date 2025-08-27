/*
 * video_sdl2.c
 *
 * SDL2-based display screen management
 */

#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include "video.h"

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;

/* NES-sized staging buffer; drivers may call video_setsize to change */
static int surface_width = 256;
static int surface_height = 240;

/* 8-bit indexed staging buffer lines returned by video_get_vbp */
static unsigned char *framebuffer = NULL;
static int framebuffer_pitch = 0; /* bytes per line */

unsigned char *vid_pre_xlat; /* maps 0..63 palette indices to 0..255 */

static void ensure_framebuffer_capacity(int width, int height)
{
    int required = width * height;
    if (framebuffer && framebuffer_pitch == width) {
        /* already exact size */
        return;
    }
    free(framebuffer);
    framebuffer = (unsigned char*)malloc(required);
    framebuffer_pitch = width;
}

static void recreate_texture_if_needed(void)
{
    if (!renderer) return;
    if (texture) {
        int w, h;
        SDL_QueryTexture(texture, NULL, NULL, &w, &h);
        if (w == surface_width && h == surface_height) return;
        SDL_DestroyTexture(texture);
        texture = NULL;
    }
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24,
                                SDL_TEXTUREACCESS_STREAMING,
                                surface_width, surface_height);
}

static void blit_indexed8_to_rgb24(unsigned char *dst_rgb, int dst_pitch,
                                   const unsigned char *src_idx, int src_pitch,
                                   int w, int h)
{
    /* vid_pre_xlat holds 0..255 values that represent 8-bit SDL palette indices.
       For RGB24, we need an actual palette; store a simple grayscale if none set. */
    static Uint8 palette_r[256], palette_g[256], palette_b[256];
    static int palette_inited = 0;
    if (!palette_inited) {
        for (int i = 0; i < 256; ++i) {
            palette_r[i] = (Uint8)i;
            palette_g[i] = (Uint8)i;
            palette_b[i] = (Uint8)i;
        }
        palette_inited = 1;
    }

    for (int y = 0; y < h; ++y) {
        const unsigned char *src = src_idx + y * src_pitch;
        unsigned char *dst = dst_rgb + y * dst_pitch;
        for (int x = 0; x < w; ++x) {
            unsigned char idx = src[x];
            unsigned char palIndex = vid_pre_xlat ? vid_pre_xlat[idx & 0xFF] : idx;
            dst[3*x + 0] = palette_r[palIndex];
            dst[3*x + 1] = palette_g[palIndex];
            dst[3*x + 2] = palette_b[palIndex];
        }
    }
}

void video_init(void)
{
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow("darcnes", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              surface_width * 2, surface_height * 2, SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    recreate_texture_if_needed();
    ensure_framebuffer_capacity(surface_width, surface_height);
}

void video_shutdown(void)
{
    if (texture) SDL_DestroyTexture(texture);
    texture = NULL;
    if (renderer) SDL_DestroyRenderer(renderer);
    renderer = NULL;
    if (window) SDL_DestroyWindow(window);
    window = NULL;
    free(framebuffer);
    framebuffer = NULL;
    framebuffer_pitch = 0;
    if (vid_pre_xlat) {
        free(vid_pre_xlat);
        vid_pre_xlat = NULL;
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void video_enter_deb(void) { /* no-op */ }
void video_leave_deb(void) { /* no-op */ }

void video_setsize(int x, int y)
{
    if (x <= 0 || y <= 0) return;
    surface_width = x;
    surface_height = y;
    ensure_framebuffer_capacity(surface_width, surface_height);
    recreate_texture_if_needed();
}

void video_display_buffer(void)
{
    if (!renderer || !texture || !framebuffer) return;

    void *pixels = NULL;
    int pitch = 0;
    if (SDL_LockTexture(texture, NULL, &pixels, &pitch) == 0) {
        blit_indexed8_to_rgb24((unsigned char*)pixels, pitch,
                               framebuffer, framebuffer_pitch,
                               surface_width, surface_height);
        SDL_UnlockTexture(texture);
    }
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

char *video_get_vbp(int line)
{
    if (!framebuffer) return NULL;
    if (line < 0 || line >= surface_height) return NULL;
    return (char*)(framebuffer + line * framebuffer_pitch);
}

void video_setpal(int num_colors, int *red, int *green, int *blue)
{
    /* Translate NES 64-color palette to indices 64..(64+num_colors-1) as allegro port did */
    if (vid_pre_xlat) {
        free(vid_pre_xlat);
        vid_pre_xlat = NULL;
    }
    vid_pre_xlat = (unsigned char*)malloc((size_t)num_colors);
    if (!vid_pre_xlat) return;
    for (int i = 0; i < num_colors; ++i) {
        /* Clamp to 0..255; original code shifted >>2 for Allegro RGB 0..63 */
        /* Store an identity mapping offset by 64 to match existing expectations */
        int idx = i + 64;
        if (idx > 255) idx = 255;
        vid_pre_xlat[i] = (unsigned char)idx;
    }
}

