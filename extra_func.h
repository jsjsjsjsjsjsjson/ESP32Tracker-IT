#ifndef EXTRA_FUNC_H
#define EXTRA_FUNC_H

#include <stdint.h>

#define GET_BIT(num, n) ((num >> n) & 1)
#define SET_BIT(num, n) (num | (1 << n))
#define CLEAR_BIT(num, n) (num & ~(1 << n))

#define BPM_TO_TICKS(bpm, fs) ((fs * 60.0) / (bpm))

#define TEMPO_TO_TICKS(tempo, smp_rate) ((2500 / tempo) * 0.001) * smp_rate

#define GET_SAMPLE_DATA(sample, idx, type) ((type *)sample->sample_data)[idx]

#define GET_NOTE(mask)    ((mask) & 1 || (mask) & 16)
#define GET_INSTRUMENT(mask) ((mask) & 2 || (mask) & 32)
#define GET_VOLUME(mask)  ((mask) & 4 || (mask) & 64)
#define GET_COMMAND(mask) ((mask) & 8 || (mask) & 128)

#define LINEAR_INTERP(x1, x2, y1, y2, x) \
    ((y1) + ((y2) - (y1)) * ((x) - (x1)) / ((x2) - (x1)))


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

void convert_c5speed(uint32_t C5_Speed, float midi_frequencies[128]) {
    float semitone_ratio = powf(2.0f, 1.0f / 12.0f);
    float A4_frequency = C5_Speed / powf(semitone_ratio, -9);
    for (uint8_t i = 0; i < 128; ++i) {
        midi_frequencies[i] = A4_frequency * powf(semitone_ratio, i - 69);
    }
}

void midi_note_to_string(uint8_t midi_note, char result[4]) {
    if (midi_note > 119) {
        if (midi_note == 254)
            strcpy(result, "^^^");
        else if (midi_note == 255)
            strcpy(result, "===");
        else
            strcpy(result, "~~~");

        return;
    }
    const char* note_names[] = {"C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"};
    const char* note_name = note_names[midi_note % 12];
    int8_t octave = midi_note / 12;
    result[0] = note_name[0];
    result[1] = note_name[1];
    result[2] = '0' + octave;
    result[3] = '\0';
}

#endif