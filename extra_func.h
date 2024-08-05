#ifndef EXTRA_FUNC_H
#define EXTRA_FUNC_H

#include <stdint.h>

#define GET_BIT(num, n) ((num >> n) & 1)
#define SET_BIT(num, n) (num | (1 << n))
#define CLEAR_BIT(num, n) (num & ~(1 << n))

#define BPM_TO_TICKS(bpm, fs) ((fs * 60.0) / (bpm))

#define TEMPO_TO_TICKS(tempo, smp_rate) ((2500 / tempo) * 0.001) * smp_rate

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

void convert_c5speed(uint32_t C5_Speed, uint32_t midi_frequencies[128]) {
    float semitone_ratio = powf(2.0f, 1.0f / 12.0f);
    float A4_frequency = C5_Speed / powf(semitone_ratio, -9);
    for (int i = 0; i < 128; ++i) {
        midi_frequencies[i] = roundf(A4_frequency * powf(semitone_ratio, i - 69));
    }
}

#endif