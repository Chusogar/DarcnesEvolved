/*
 * ui_sdl2.c
 *
 * Minimal SDL2 UI entry to replace X/Motif UI for portability.
 */

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "ui.h"
#include "video.h"
#include "system.h"

shutdown_t dn_shutdown;

static void (*timeslice_proc)(void *) = NULL;
static void *timeslice_user = NULL;

struct joypad *ui_joypad;
struct keypad *ui_keypad;

/* Keyboard hook compatibility for keyboard_x.c */
typedef void (*keyhook)(void *display, void *event);
static keyhook ui_keyhook;
void ui_set_keyboard_hook(keyhook hook) { ui_keyhook = hook; }
/* Minimal msx/sc3k hook shims */
void sc3k_keyboard_event(u8 code) { (void)code; }
void msx_keyboard_event(u8 code) { (void)code; }

void deb_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

void set_timeslice(void (*proc)(void *), void *data)
{
    timeslice_proc = proc;
    timeslice_user = data;
}

void unset_timeslice(void)
{
    timeslice_proc = NULL;
    timeslice_user = NULL;
}

int ui_register_joypad(struct joypad *pad)
{
    if (ui_joypad) return 0;
    ui_joypad = pad;
    return 1;
}

void ui_update_joypad(struct joypad *pad)
{
    (void)pad;
}

int keypad_register(struct keypad *pad)
{
    if (ui_keypad) return 0;
    ui_keypad = pad;
    return 1;
}

void keypad_update(struct keypad *pad)
{
    (void)pad;
}

/* Some drivers expect kb_init from platform keyboard layer */
void kb_init(void) {}

static void pump_input(void)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            if (dn_shutdown) dn_shutdown();
            exit(0);
        }
        if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
            if (ui_keyhook) ui_keyhook(NULL, (void *)&e);
            /* Map basic arrows and Z/X to joypad if present */
            if (ui_joypad) {
                int down = (e.type == SDL_KEYDOWN);
                switch (e.key.keysym.sym) {
                    case SDLK_UP:    if (down) ui_joypad->data |= 0x10; else ui_joypad->data &= ~0x10; break;
                    case SDLK_DOWN:  if (down) ui_joypad->data |= 0x20; else ui_joypad->data &= ~0x20; break;
                    case SDLK_LEFT:  if (down) ui_joypad->data |= 0x40; else ui_joypad->data &= ~0x40; break;
                    case SDLK_RIGHT: if (down) ui_joypad->data |= 0x80; else ui_joypad->data &= ~0x80; break;
                    case SDLK_z:     if (down) ui_joypad->data |= 0x01; else ui_joypad->data &= ~0x01; break;
                    case SDLK_x:     if (down) ui_joypad->data |= 0x02; else ui_joypad->data &= ~0x02; break;
                    case SDLK_RETURN:if (down) ui_joypad->data |= 0x08; else ui_joypad->data &= ~0x08; break;
                    case SDLK_RSHIFT:if (down) ui_joypad->data |= 0x04; else ui_joypad->data &= ~0x04; break;
                }
            }
        }
    }
}

int main(int argc, char *argv[])
{
    extern int nes_psg_quality;
    rom_file romfile = NULL;
    int system_type = ST_NONE;

    nes_psg_quality = 0; /* no sound for minimal port */

    for (int i = 1; i < argc; i++) {
#ifdef _WIN32
#define strcasecmp _stricmp
#endif
        if (!strcasecmp(argv[i], "--nosound")) {
            nes_psg_quality = 0;
        } else if (!strncasecmp(argv[i], "--system=", 9)) {
            system_type = parse_system_name(argv[i] + 9);
        } else if (romfile) {
            printf("rom file \"%s\" already loaded, ignoring \"%s\"\n", romfile->filename, argv[i]);
        } else if ((romfile = read_romimage(argv[i]))) {
            if (system_type == ST_NONE) system_type = guess_system(romfile);
        } else {
            printf("error loading rom file \"%s\"\n", argv[i]);
        }
    }

    if (system_type == ST_NONE) {
        printf("no system specified or unable to guess system.\n");
        return 1;
    }
    global_system_type = system_type;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    activate_system(global_system_type, romfile);

    /* Provide a reasonable default size; drivers will call video_setsize */
    video_setsize(320, 240);

    for (;;) {
        pump_input();
        if (timeslice_proc) timeslice_proc(timeslice_user);
        SDL_Delay(1);
    }

    return 0;
}

