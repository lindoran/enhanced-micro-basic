/*
 * tinybeep.h - Minimal square-wave beep library for Linux (ALSA)
 *
 * Requires: sudo apt install libasound2-dev
 * Build:    gcc myapp.c tinybeep.c -lasound -o myapp
 */

#ifndef TINYBEEP_H
#define TINYBEEP_H

#ifdef __cplusplus
extern "C" {
#endif

#define TINYBEEP_HZ_MIN       20      /* lower bound of human hearing  */
#define TINYBEEP_HZ_MAX       20000   /* upper bound of human hearing  */
#define TINYBEEP_MS_MIN       1
#define TINYBEEP_MS_MAX       60000   /* 60 s ceiling                  */
#define TINYBEEP_SAMPLE_RATE  44100

#define TINYBEEP_OK           0
#define TINYBEEP_ERR_DEVICE  -2
#define TINYBEEP_ERR_WRITE   -3

int tinybeep_clamp_hz(int hz);  /* clamp hz to [HZ_MIN, HZ_MAX] */
int tinybeep_clamp_ms(int ms);  /* clamp ms to [MS_MIN, MS_MAX] */
int tinybeep(int hz, int ms);   /* beep at hz for ms milliseconds */

#ifdef __cplusplus
}
#endif

#endif /* TINYBEEP_H */
