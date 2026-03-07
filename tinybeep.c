/*
 * tinybeep.c - Minimal square-wave beep library for Linux (ALSA)
 *
 * Requires: sudo apt install libasound2-dev
 * Build:    gcc myapp.c tinybeep.c -lasound -o myapp
 * 
 * for when you need a pc speaker but you live in the modern times
 * 
 * Copyright David Collins 2026 - Public Domain Disclosure:
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * 
 * the above only applies to tinnybeep.c, and not any other software that includes it
 * 
 * no warranty is implied or expressly granted, use at your own risk
 * 
 */

#include "tinybeep.h"
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <alsa/asoundlib.h>

static const char * const alsa_devs[] = {
    "default", "pipewire", "pulse", "plughw:0,0", NULL
};

int tinybeep_clamp_hz(int hz)
{
    if (hz < TINYBEEP_HZ_MIN) return TINYBEEP_HZ_MIN;
    if (hz > TINYBEEP_HZ_MAX) return TINYBEEP_HZ_MAX;
    return hz;
}

int tinybeep_clamp_ms(int ms)
{
    if (ms < TINYBEEP_MS_MIN) return TINYBEEP_MS_MIN;
    if (ms > TINYBEEP_MS_MAX) return TINYBEEP_MS_MAX;
    return ms;
}

static int fill_square(unsigned char *buf, int hz, int rate)
{
    int period = rate / hz;
    if (period < 2) period = 2;
    int half = period / 2, i;
    for (i = 0;    i < half;   i++) buf[i] = 224;
    for (i = half; i < period; i++) buf[i] = 32;
    return period;
}

int tinybeep(int hz, int ms)
{
    hz = tinybeep_clamp_hz(hz);
    ms = tinybeep_clamp_ms(ms);

    snd_pcm_t *pcm = NULL;
    int rc = -1;

    /* Redirect stderr to /dev/null while probing so ALSA warnings stay silent */
    int saved = dup(STDERR_FILENO);
    int null   = open("/dev/null", O_WRONLY);
    if (null >= 0) dup2(null, STDERR_FILENO);

    for (int d = 0; alsa_devs[d] && rc < 0; d++)
        rc = snd_pcm_open(&pcm, alsa_devs[d], SND_PCM_STREAM_PLAYBACK, 0);

    if (null >= 0) { dup2(saved, STDERR_FILENO); close(null); }
    close(saved);

    if (rc < 0) return TINYBEEP_ERR_DEVICE;

    if (snd_pcm_set_params(pcm, SND_PCM_FORMAT_U8,
                           SND_PCM_ACCESS_RW_INTERLEAVED,
                           1, TINYBEEP_SAMPLE_RATE, 1, 20000) < 0) {
        snd_pcm_close(pcm);
        return TINYBEEP_ERR_DEVICE;
    }

    int buflen = TINYBEEP_SAMPLE_RATE / TINYBEEP_HZ_MIN + 2;
    unsigned char *wave = malloc((size_t)buflen);
    if (!wave) { snd_pcm_close(pcm); return TINYBEEP_ERR_DEVICE; }

    int period = fill_square(wave, hz, TINYBEEP_SAMPLE_RATE);
    long total = (long)TINYBEEP_SAMPLE_RATE * ms / 1000, written = 0;

    while (written < total) {
        long chunk = period;
        if (written + chunk > total) chunk = total - written;

        snd_pcm_sframes_t n = snd_pcm_writei(pcm, wave, (snd_pcm_uframes_t)chunk);
        if (n == -EPIPE) { snd_pcm_prepare(pcm); continue; }
        if (n < 0) { free(wave); snd_pcm_close(pcm); return TINYBEEP_ERR_WRITE; }
        written += n;
    }

    free(wave);
    snd_pcm_drain(pcm);
    snd_pcm_close(pcm);
    return TINYBEEP_OK;
}
