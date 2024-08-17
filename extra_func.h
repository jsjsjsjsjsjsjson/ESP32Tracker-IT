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

#define hexToDecimalTens(num) (((num) >> 4) & 0x0F)
#define hexToDecimalOnes(num) ((num) & 0x0F)
#define hexToRow(num) (hexToDecimalTens(num) * 10 + hexToDecimalOnes(num))

#define PORTUP16(f, t) (f + ((f * t) >> 8))
#define PORTUP64(f, t) (f + ((f * t) >> 10))

#define PORTDOWN16(f, t) (f - ((f * t) >> 8))
#define PORTDOWN64(f, t) (f - ((f * t) >> 10))

inline uint32_t tone_freq_sild(uint32_t original_frequency, int32_t slide_units, uint8_t extra_fine) {
    float freq = (float)original_frequency;
    float semitone_per_unit = extra_fine ? 1.0f / 64.0f : 1.0f / 16.0f;
    float semitone_change = slide_units * semitone_per_unit;
    float new_freq = freq * powf(2.0f, semitone_change / 12.0f);
    return (uint32_t)new_freq;
}

inline uint8_t COSINE_INTERP(uint16_t x1, uint16_t x2, uint8_t y1, uint8_t y2, uint16_t x) {
    if (x < x1) x = x1;
    if (x > x2) x = x2;
    float mu = (float)(x - x1) / (x2 - x1);
    float mu2 = (1 - cosf(mu * M_PI)) / 2;
    return (uint8_t)(y1 * (1 - mu2) + y2 * mu2);
}

inline uint8_t LINEAR_INTERP(uint16_t x1, uint16_t x2, uint8_t y1, uint8_t y2, uint16_t x) {
    if (x1 == x2) return y1;
    int16_t dx = x2 - x1;
    int16_t dy = y2 - y1;
    int16_t y = y1 + (dy * (x - x1)) / dx;
    if (y < 0) return 0;
    if (y > 255) return 255;
    return static_cast<uint8_t>(y);
}

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
    for (uint8_t i = 0; i < 128; ++i) {
        midi_frequencies[i] = roundf(A4_frequency * powf(semitone_ratio, i - 69));
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

void volCmdToRel(uint8_t val, char *flg, uint8_t *rel_val) {
    *flg = 'v';
    if (val > 64 && val < 75) {
        *flg = 'a';
        val -= 65;
    } else if (val > 74 && val < 85) {
        *flg = 'b';
        val -= 75;
    } else if (val > 84 && val < 95) {
        *flg = 'c';
        val -= 85;
    } else if (val > 94 && val < 105) {
        *flg = 'd';
        val -= 95;
    } else if (val > 104 && val < 115) {
        *flg = 'e';
        val -= 105;
    } else if (val > 114 && val < 125) {
        *flg = 'f';
        val -= 115;
    } else if (val > 127 && val < 193) {
        *flg = 'p';
        val -= 128;
    } else if (val > 192 && val < 203) {
        *flg = 'g';
        val -= 193;
    } else if (val > 202 && val < 213) {
        *flg = 'h';
        val -= 203;
    }
    *rel_val = val;
}

#endif