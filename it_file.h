#ifndef IT_FILE_H
#define IT_FILE_H

#include <stdint.h>
#include <stdio.h>

typedef struct __attribute__((packed)) {
    char IMPM[4];      // not include NULL
    char SongName[26]; // includes NULL
    uint16_t PHiligt;
    uint16_t OrdNum;
    uint16_t InsNum;
    uint16_t SmpNum;
    uint16_t PatNum;
    uint16_t CwtV;
    uint16_t Cmwt;
    uint16_t Flags;
    uint16_t Special;
    uint8_t GV;
    uint8_t MV;
    uint8_t IS;
    uint8_t IT;
    uint8_t Sep;
    uint8_t PWD;
    uint16_t MsgLgth;
    uint32_t MsgOfst;
    uint32_t Reserved;
    uint8_t ChnlPan[64];
    uint8_t ChnlVol[64];
    uint8_t *Orders;
    uint32_t *InstOfst;
    uint32_t *SampHeadOfst;
    uint32_t *PatternOfst;
} it_header_t;

typedef struct __attribute__((packed)) {
    char IMPS[4]; // no include NULL
    char DOSFilename[12]; // no include NULL
    uint8_t Reserved;
    uint8_t Gvl; // Global Vol
    uint8_t Flg; // Flags
    uint8_t Vol; // Vol
    char SampleName[26]; // include NULL
    uint32_t Length; // Sample Number
    uint32_t LoopBegin; // Sample Number
    uint32_t LoopEnd; // Sample Number
    uint32_t C5Speed; // Bytes
    uint32_t SusLoopBegin;
    uint32_t SusLoopEnd;
    uint32_t SamplePointer;
    uint8_t ViS;
    uint8_t ViD;
    uint8_t ViR;
    uint8_t ViT;
} it_sample_t;

#define MAX_CHANNELS 64

typedef struct {
    uint8_t note;
    uint8_t instrument;
    uint8_t volume;
    uint8_t command;
    uint8_t command_value;
} pattern_note_t;

typedef struct {
    uint16_t length;
    uint16_t rows;
    uint8_t *packed_data;
} it_packed_pattern_t;

int unpack_pattern(it_packed_pattern_t *packed_pattern, pattern_note_t *unpack_data[MAX_CHANNELS]) {
    uint8_t *data = packed_pattern->packed_data;

    uint8_t last_mask[MAX_CHANNELS] = {0};
    int8_t last_note[MAX_CHANNELS] = {-1};
    int8_t last_instrument[MAX_CHANNELS] = {-1};
    int8_t last_volume[MAX_CHANNELS] = {-1};
    int8_t last_command[MAX_CHANNELS] = {-1};
    int8_t last_command_value[MAX_CHANNELS] = {-1};

    int max_channel_used = -1; // Initialize the maximum channel used.

    for (uint16_t row = 0; row < packed_pattern->rows; ++row) {
        while (1) {
            uint8_t channel_variable = *data++;
            if (channel_variable == 0) {
                // End of row
                break;
            }

            uint8_t channel = (channel_variable - 1) & 63;
            if (channel > max_channel_used) {
                max_channel_used = channel; // Update max channel used if this channel is higher.
            }

            uint8_t mask;

            if (channel_variable & 128) {
                mask = *data++;
                last_mask[channel] = mask;
            } else {
                mask = last_mask[channel];
            }

            pattern_note_t *note = &unpack_data[channel][row];

            if (mask & 1) {
                note->note = *data++;
                last_note[channel] = note->note;
            } else if (mask & 16) {
                note->note = last_note[channel];
            }

            if (mask & 2) {
                note->instrument = *data++;
                last_instrument[channel] = note->instrument;
            } else if (mask & 32) {
                note->instrument = last_instrument[channel];
            }

            if (mask & 4) {
                note->volume = *data++;
                last_volume[channel] = note->volume;
            } else if (mask & 64) {
                note->volume = last_volume[channel];
            }

            if (mask & 8) {
                note->command = *data++;
                note->command_value = *data++;
                last_command[channel] = note->command;
                last_command_value[channel] = note->command_value;
            } else if (mask & 128) {
                note->command = last_command[channel];
                note->command_value = last_command_value[channel];
            }
        }
    }

    return max_channel_used; // Return the maximum channel number used.
}

typedef struct {
    bool stereo;
    bool vol0MixOptz;
    bool useInst;
    bool lineSlid;
    bool oldEfct;
    bool LEGMWEF; // Link Effect G's memory with Effect E/F.
    bool useMidiPitchCtrl;
    bool ReqEmbMidiConf;
} it_head_flags_t;

typedef struct {
    bool enbSongMsg;
    bool embMidiConf;
} it_special_t;

void read_it_header(FILE *file, it_header_t *header) {
    // sizeof(it_header_t);
    if (!file) {
        perror("Read Error");
        return;
    }
    fread(header, 192, 1, file);
    header->Orders = (uint8_t*)malloc(header->OrdNum * sizeof(uint8_t));
    header->InstOfst = (uint32_t*)malloc(header->InsNum * sizeof(uint32_t));
    header->SampHeadOfst = (uint32_t*)malloc(header->SmpNum * sizeof(uint32_t));
    header->PatternOfst = (uint32_t*)malloc(header->PatNum * sizeof(uint32_t));
    fread(header->Orders, header->OrdNum, 1, file);
    fread(header->InstOfst, header->InsNum, 4, file);
    fread(header->SampHeadOfst, header->SmpNum, 4, file);
    fread(header->PatternOfst, header->PatNum, 4, file);
    printf("IMPM: %.4s\n", header->IMPM);
    printf("SongName: %s\n", header->SongName);
    printf("PHiligt: %u\n", header->PHiligt);
    printf("OrdNum: %u\n", header->OrdNum);
    printf("InsNum: %u\n", header->InsNum);
    printf("SmpNum: %u\n", header->SmpNum);
    printf("PatNum: %u\n", header->PatNum);
    printf("CwtV: %u\n", header->CwtV);
    printf("Cmwt: %u\n", header->Cmwt);
    printf("Flags: %u\n", header->Flags);
    printf("Special: %u\n", header->Special);
    printf("GV: %u\n", header->GV);
    printf("MV: %u\n", header->MV);
    printf("IS: %u\n", header->IS);
    printf("IT: %u\n", header->IT);
    printf("Sep: %u\n", header->Sep);
    printf("PWD: %u\n", header->PWD);
    printf("MsgLgth: %u\n", header->MsgLgth);
    printf("MsgOfst: %d\n", header->MsgOfst);
    printf("Reserved: %d\n", header->Reserved);
    if (header->Reserved == 1414548815) {
        printf("This IT from OpenMPT!\n");
    }
    
    printf("ChnlPan: ");
    for (int i = 0; i < 64; i++) {
        printf("%d ", header->ChnlPan[i]);
    }
    printf("\n");

    printf("ChnlVol: ");
    for (int i = 0; i < 64; i++) {
        printf("%d ", header->ChnlVol[i]);
    }
    printf("\n");

    printf("Orders: ");
    for (int i = 0; i < header->OrdNum; i++) {
        printf("%d ", header->Orders[i]);
    }
    printf("\n");

    printf("Instruments Offset: ");
    for (int i = 0; i < header->InsNum; i++) {
        printf("0x%x ", header->InstOfst[i]);
    }
    printf("\n");

    printf("Samples Headers Offset: ");
    for (int i = 0; i < header->SmpNum; i++) {
        printf("0x%x ", header->SampHeadOfst[i]);
    }
    printf("\n");

    printf("Patterns Offset: ");
    for (int i = 0; i < header->PatNum; i++) {
        printf("0x%x ", header->PatternOfst[i]);
    }
    printf("\n");
}

void read_and_unpack_pattern(FILE *file, it_header_t *header, pattern_note_t **unpack_data, uint16_t PatNum, uint16_t *ChannelsOut, uint16_t *RowsOut) {
    it_packed_pattern_t pattern_data;
    printf("Pat %d in 0x%x\n", PatNum, header->PatternOfst[PatNum]);
    fseek(file, header->PatternOfst[PatNum], SEEK_SET);
    printf("File Jmp To 0x%x\n", ftell(file));
    fread(&pattern_data, 2, 2, file);
    printf("Rows %d, Len %d\n", pattern_data.rows, pattern_data.length);
    fseek(file, 4, SEEK_CUR);
    printf("malloc mem\n");
    pattern_data.packed_data = (uint8_t*)malloc(pattern_data.length);
    printf("read pack data\n");
    fread(pattern_data.packed_data, pattern_data.length, 1, file);
    printf("malloc unpack mem\n");
    for (uint8_t i = 0; i < MAX_CHANNELS; i++) {
        unpack_data[i] = (pattern_note_t*)malloc(pattern_data.rows * sizeof(pattern_note_t));
        memset(unpack_data[i], 0, pattern_data.rows * sizeof(pattern_note_t));
    }
    printf("unpacking...\n");
    *ChannelsOut = unpack_pattern(&pattern_data, unpack_data);
    printf("Pat %d Max Chan: %d\n", PatNum, *ChannelsOut);
    *RowsOut = pattern_data.rows;
    printf("free\n");
    free(pattern_data.packed_data);
    printf("free finish\n");
}

void read_it_sample()

#endif // IT_FILE_H