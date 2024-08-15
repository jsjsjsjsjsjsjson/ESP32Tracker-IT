#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Helper function to read bits from compressed data
static uint32_t read_bits(const uint8_t *data, uint32_t *bit_offset, int num_bits) {
    uint32_t result = 0;
    for (int i = 0; i < num_bits; ++i) {
        result <<= 1;
        result |= (data[*bit_offset / 8] >> (7 - (*bit_offset % 8))) & 1;
        (*bit_offset)++;
    }
    return result;
}

// Helper function to sign-extend a value
static int32_t sign_extend(uint32_t value, int bit_length) {
    int32_t mask = 1 << (bit_length - 1);
    return (value ^ mask) - mask;
}

void decode_sample(void *compressed_data, uint16_t compressed_length, void *output_data, int double_delta) {
    uint8_t *input = (uint8_t *)compressed_data;
    int16_t *output = (int16_t *)output_data;

    uint32_t bit_offset = 0;
    int bit_width = 9;  // Start with sample bitrate + 1 (9 for 8-bit)

    int output_index = 0;

    while (bit_offset / 8 < compressed_length) {
        uint32_t value = read_bits(input, &bit_offset, bit_width);

        if (bit_width <= 6) {
            // Type A decoding
            if ((value & (1 << (bit_width - 1))) && !(value & ((1 << (bit_width - 1)) - 1))) {
                int new_bits = read_bits(input, &bit_offset, bit_width == 9 ? 4 : 3);
                bit_width = new_bits + 1;
                if (bit_width == bit_width + 1) {
                    bit_width++;
                }
            } else {
                output[output_index++] = sign_extend(value, bit_width);
            }
        } else if (bit_width < 9) {
            // Type B decoding
            int32_t lower_bound = (1 << (bit_width - 1)) - (bit_width == 9 ? 4 : 8);
            int32_t upper_bound = (1 << (bit_width - 1)) + (bit_width == 9 ? 3 : 7);
            if (value >= lower_bound && value <= upper_bound) {
                value -= lower_bound;
                bit_width = value + 1;
                if (bit_width == bit_width + 1) {
                    bit_width++;
                }
            } else {
                output[output_index++] = sign_extend(value, bit_width);
            }
        } else {
            // Type C decoding
            if (value >= (1 << (bit_width - 1))) {
                bit_width = (value & 0xFF) + 1;
            } else {
                output[output_index++] = sign_extend(value, bit_width - 1);
            }
        }
    }

    // Perform delta decoding
    for (int i = 1; i < output_index; ++i) {
        output[i] += output[i - 1];
    }

    if (double_delta) {
        for (int i = 2; i < output_index; ++i) {
            output[i] += output[i - 2];
        }
    }
}
