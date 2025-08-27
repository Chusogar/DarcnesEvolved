/*
 * snd_sdl2.c
 *
 * SDL2 audio backend implementing snd.h
 */

#include <stdio.h>
#include <string.h>
#include <SDL.h>
#include "ui.h"
#include "snd.h"

static SDL_AudioDeviceID audio_device = 0;
static SDL_AudioSpec obtained;

/* ring buffer for 8-bit mono samples */
#define RB_SIZE 4096
static unsigned char ring_buffer[RB_SIZE];
static volatile int rb_head = 0; /* write */
static volatile int rb_tail = 0; /* read */

static void audio_callback(void *userdata, Uint8 *stream, int len)
{
    (void)userdata;
    for (int i = 0; i < len; ++i) {
        if (rb_tail != rb_head) {
            stream[i] = ring_buffer[rb_tail];
            rb_tail = (rb_tail + 1) & (RB_SIZE - 1);
        } else {
            stream[i] = 0x80; /* silence for unsigned 8-bit */
        }
    }
}

void snd_init(void) { /* no-op */ }

int snd_open(int samples_per_sync, int sample_rate)
{
    (void)samples_per_sync;
    SDL_AudioSpec desired;
    SDL_zero(desired);
    desired.freq = sample_rate > 0 ? sample_rate : 48000;
    desired.format = AUDIO_U8;
    desired.channels = 1;
    desired.samples = 1024;
    desired.callback = audio_callback;
    audio_device = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0);
    if (!audio_device) {
        deb_printf("SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        return 0;
    }
    rb_head = rb_tail = 0;
    SDL_PauseAudioDevice(audio_device, 0);
    return 1;
}

void snd_close(void)
{
    if (audio_device) {
        SDL_CloseAudioDevice(audio_device);
        audio_device = 0;
    }
}

void snd_output_4_waves(int samples, u8 *wave1, u8 *wave2, u8 *wave3, u8 *wave4)
{
    if (!audio_device) return;
    for (int i = 0; i < samples; ++i) {
        unsigned int mix;
        if (wave4) {
            mix = (wave1[i] + wave2[i] + wave3[i] + wave4[i]) >> 2;
        } else {
            mix = (wave1[i] + wave2[i] + wave3[i]) / 3;
        }
        int next_head = (rb_head + 1) & (RB_SIZE - 1);
        if (next_head != rb_tail) {
            ring_buffer[rb_head] = (unsigned char)mix;
            rb_head = next_head;
        }
    }
}

