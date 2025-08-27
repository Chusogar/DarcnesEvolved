/*
 * ui_sdl2.c
 *
 * Minimal SDL2 user interface (CLI + event/timeslice loop)
 */

#include <stdio.h>
#include <stdarg.h>
#include <SDL.h>
#include "ui.h"
#include "system.h"
#include "video.h"
#include "tool.h"

/* single joypad supported */
struct joypad *ui_joypad;
shutdown_t dn_shutdown;

/* local forward decls to satisfy C99 (not in public video.h interface) */
void video_init(void);
void video_shutdown(void);

static void video_events(void)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT:
            system_flags |= F_QUIT;
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP: {
            int pressed = (e.type == SDL_KEYDOWN);
            if (e.key.keysym.sym == SDLK_ESCAPE) {
                if (pressed) system_flags |= F_QUIT; else system_flags &= ~F_QUIT;
            }
            if (ui_joypad) {
                /* Map arrow keys and A/S/[/{ to buttons as allegro backend */
                switch (e.key.keysym.sym) {
                case SDLK_UP:
                    if (pressed) ui_joypad->data |= ui_joypad->button_template->buttons[0];
                    else ui_joypad->data &= ~ui_joypad->button_template->buttons[0];
                    break;
                case SDLK_DOWN:
                    if (pressed) ui_joypad->data |= ui_joypad->button_template->buttons[1];
                    else ui_joypad->data &= ~ui_joypad->button_template->buttons[1];
                    break;
                case SDLK_LEFT:
                    if (pressed) ui_joypad->data |= ui_joypad->button_template->buttons[2];
                    else ui_joypad->data &= ~ui_joypad->button_template->buttons[2];
                    break;
                case SDLK_RIGHT:
                    if (pressed) ui_joypad->data |= ui_joypad->button_template->buttons[3];
                    else ui_joypad->data &= ~ui_joypad->button_template->buttons[3];
                    break;
                case SDLK_s:
                    if (pressed) ui_joypad->data |= ui_joypad->button_template->buttons[4];
                    else ui_joypad->data &= ~ui_joypad->button_template->buttons[4];
                    break;
                case SDLK_a:
                    if (pressed) ui_joypad->data |= ui_joypad->button_template->buttons[5];
                    else ui_joypad->data &= ~ui_joypad->button_template->buttons[5];
                    break;
                case SDLK_LEFTBRACKET:
                    if (pressed) ui_joypad->data |= ui_joypad->button_template->buttons[6];
                    else ui_joypad->data &= ~ui_joypad->button_template->buttons[6];
                    break;
                case SDLK_RIGHTBRACKET:
                    if (pressed) ui_joypad->data |= ui_joypad->button_template->buttons[7];
                    else ui_joypad->data &= ~ui_joypad->button_template->buttons[7];
                    break;
                default:
                    break;
                }
            }
            break;
        }
        default:
            break;
        }
    }
}

/* debug console handling */
void deb_printf(const char *fmt, ...)
{
#if 0
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
#endif
}

/* keypad/joypad minimal implementations */
int keypad_register(struct keypad *pad) { (void)pad; return 0; }
int ui_register_joypad(struct joypad *pad)
{
    if (!ui_joypad) { ui_joypad = pad; return 1; }
    return 0;
}
void ui_update_joypad(struct joypad *pad) { (void)pad; }

/* emulation timeslicing */
void (*timeslice)(void *) = NULL;
void *timeslice_data = NULL;

void set_timeslice(void (*proc)(void *), void *data)
{
    timeslice = proc;
    timeslice_data = data;
}

void unset_timeslice(void)
{
    timeslice = NULL;
}

int main(int argc, char *argv[])
{
    extern int nes_psg_quality; /* from nes_psg.h in codebase */
    rom_file romfile;
    int system_type;

    if (argc != 2) {
        printf("usage: %s <romfile>\n", argv[0]);
        return 0x42;
    }

    video_init();

    nes_psg_quality = 2;

    romfile = read_romimage(argv[1]);
    if (romfile) {
        system_type = guess_system(romfile);
        activate_system(system_type, romfile);
    } else {
        printf("error reading romfile.\n");
        return 0x41;
    }

    while ((!(system_flags & F_QUIT)) && timeslice) {
        video_events();
        timeslice(timeslice_data);
    }
    if (dn_shutdown) {
        dn_shutdown();
    }
    return 0;
}

