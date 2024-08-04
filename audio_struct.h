#ifndef AUDIO_STRUCT_H
#define AUDIO_STRUCT_H

#include <stdint.h>

typedef struct {
    int16_t l;
    int16_t r;
} audio_stereo_16_t;

typedef struct {
    int8_t l;
    int8_t r;
} audio_stereo_8_t;

typedef struct {
    uint16_t l;
    uint16_t r;
} audio_stereo_u16_t;

typedef struct {
    uint8_t l;
    uint8_t r;
} audio_stereo_u8_t;

typedef int16_t audio_mono_16_t;
typedef int8_t audio_mono_8_t;
typedef uint16_t audio_mono_u16_t;
typedef uint8_t audio_mono_u8_t;

#endif