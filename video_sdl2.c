/*
 * video_sdl2.c
 *
 * SDL2 video backend implementing video.h
 */

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "video.h"
#include "ui.h"

static SDL_Window *sdl_window = NULL;
static SDL_Renderer *sdl_renderer = NULL;
static SDL_Texture *sdl_texture = NULL;

static int buffer_width = 0;
static int buffer_height = 0;
static int texture_pitch = 0;
static Uint32 *texture_pixels = NULL;
static uint8_t *index_buffer = NULL;

unsigned char *vid_pre_xlat;

typedef void ((*xlatfunc)(void *, int));
static xlatfunc vidxlate = NULL;

static Uint32 *xlatepal32 = NULL;

static void vidxlate32(void *dest, int size)
{
    unsigned char *src = (unsigned char *)dest;
    Uint32 *dst = (Uint32 *)dest;
    for (int i = size - 1; i >= 0; --i) {
        dst[i] = xlatepal32[src[i]];
    }
}

static void ensure_window(int width, int height)
{
    if (!sdl_window) {
        SDL_CreateWindowAndRenderer(width, height, SDL_WINDOW_SHOWN, &sdl_window, &sdl_renderer);
    } else {
        SDL_SetWindowSize(sdl_window, width, height);
    }
}

void video_setsize(int x, int y)
{
    buffer_width = x;
    buffer_height = y;

    ensure_window(x, y);

    if (sdl_texture) {
        SDL_DestroyTexture(sdl_texture);
        sdl_texture = NULL;
    }

    sdl_texture = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_STREAMING, x, y);

    if (index_buffer) {
        free(index_buffer);
        index_buffer = NULL;
    }
    index_buffer = (uint8_t *)malloc((size_t)x * (size_t)y);
}

static void video_allocate_if_needed(void)
{
    if (!sdl_window || !sdl_renderer) {
        ensure_window(buffer_width ? buffer_width : 320, buffer_height ? buffer_height : 240);
    }
}

char *video_get_vbp(int line)
{
    if (!index_buffer) return NULL;
    return (char *)(index_buffer + (size_t)line * (size_t)buffer_width);
}

void video_display_buffer(void)
{
    if (!sdl_texture) return;

    int ok = SDL_LockTexture(sdl_texture, NULL, (void **)&texture_pixels, &texture_pitch);
    if (ok != 0) return;

    if (vidxlate && index_buffer) {
        /* Translate from 8-bit indices to ARGB32 into the texture */
        vidxlate((void *)index_buffer, buffer_width * buffer_height);
        /* After vidxlate, index_buffer holds indices; we need to copy mapped pixels into texture */
        /* Our vidxlate32 currently writes ARGB directly back into the same buffer; adapt by writing into texture */
        Uint32 *dst = texture_pixels;
        for (int y = 0; y < buffer_height; ++y) {
            Uint32 *row = (Uint32 *)((uint8_t *)dst + (size_t)y * (size_t)texture_pitch);
            for (int x = 0; x < buffer_width; ++x) {
                unsigned char idx = index_buffer[(size_t)y * (size_t)buffer_width + x];
                row[x] = xlatepal32 ? xlatepal32[idx] : 0xFF000000;
            }
        }
    }

    SDL_UnlockTexture(sdl_texture);

    SDL_RenderClear(sdl_renderer);
    SDL_RenderCopy(sdl_renderer, sdl_texture, NULL, NULL);
    SDL_RenderPresent(sdl_renderer);
}

void video_run(void)
{
    /* No-op: UI mainloop drives rendering */
}

void video_enter_deb(void) { }
void video_leave_deb(void) { }

static void init_xlate32_internal(int colors, int *red, int *green, int *blue)
{
    if (xlatepal32) free(xlatepal32);
    xlatepal32 = (Uint32 *)malloc(colors * sizeof(Uint32));
    if (!xlatepal32) return;

    if (vid_pre_xlat) free(vid_pre_xlat);
    vid_pre_xlat = (unsigned char *)malloc(colors);
    if (!vid_pre_xlat) return;

    for (int i = 0; i < colors; ++i) {
        vid_pre_xlat[i] = (unsigned char)i;
        Uint8 r = red[i] >> 8;
        Uint8 g = green[i] >> 8;
        Uint8 b = blue[i] >> 8;
        xlatepal32[i] = 0xFF000000 | (r << 16) | (g << 8) | b;
    }

    vidxlate = vidxlate32;
}

void video_setpal(int num_colors, int *red, int *green, int *blue)
{
    video_allocate_if_needed();
    init_xlate32_internal(num_colors, red, green, blue);
}

/* Stubs for legacy Apple2 references from X backend */
int joy_zero_zone_apple = 127;
int get_mouse_position_x(void) { return 0; }
int get_mouse_position_y(void) { return 0; }
int get_mouse_clicked(void) { return 0; }
void beep(int v) { (void)v; }

/* Some modules call this periodically; noop for SDL2 */
void video_events(void) {}

