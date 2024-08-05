#ifndef EXTRA_FUNC_H
#define EXTRA_FUNC_H

#include <stdint.h>

#define GET_BIT(num, n) ((num >> n) & 1)
#define SET_BIT(num, n) (num | (1 << n))
#define CLEAR_BIT(num, n) (num & ~(1 << n))

uint16_t findMax(uint16_t arr[], uint16_t size) {
    if (size == 0) return INT16_MIN;
    uint16_t max = arr[0];
    for (uint16_t i = 1; i < size; i++) {
        if (arr[i] > max) {
            max = arr[i];
        }
    }
    return max;
}

void cover_c5speed(uint32_t C5_Speed, float midi_frequencies[128]) {
    float A4_frequency = C5_Speed * powf(2.0f, -9.0f / 12.0f);
    for (uint8_t i = 0; i < 128; i++) {
        midi_frequencies[i] = A4_frequency * powf(2.0f, (i - 69) / 12.0f);
    }
}

#endif