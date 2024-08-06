#ifndef IT_FILE_H
#define IT_FILE_H

#include <stdint.h>
#include <stdio.h>
#include "extra_func.h"
#include "audio_struct.h"

typedef struct {
    bool stereo : 1;
    bool vol0MixOptz : 1;
    bool useInst : 1;
    bool lineSlid : 1;
    bool oldEfct : 1;
    bool LEGMWEF : 1; // Link Effect G's memory with Effect E/F.
    bool useMidiPitchCtrl : 1;
    bool ReqEmbMidiConf : 1;
    uint8_t Recv;
} it_head_flags_t;

typedef struct {
    bool sampWithHead : 1;
    bool use16Bit : 1; // true = 16Bit Sample, false = 8Bit Sample
    bool stereo : 1;
    bool comprsSamp : 1;
    bool useLoop : 1;
    bool useSusLoop : 1;
    bool pingPongLoop : 1; // true = PingPongLoop, false = Forwards Loop
    bool pingPongSusLoop : 1; // true = PingPongLoop, false = Forwards Loop
} it_sample_flags_t;

typedef struct {
    bool sampIsSigned : 1;
    bool intelLoHiSamp : 1; // Off: Intel lo-hi byte order for 16-bit samples, true: Motorola hi-lo byte order for 16-bit samples
    bool sampIsDPCM : 1;
    bool sampIsPTM : 1;
    bool smpIsTXWave12Bit : 1;
    bool LRAPrompt : 1;
    uint8_t Reserved : 2;
} it_sample_convert_t;

typedef struct {
    bool enbSongMsg : 1;
    uint8_t Recv0 : 2;
    bool embMidiConf : 1;
    uint8_t Recv1 : 4;
    uint8_t Recv2;
} it_special_t;

typedef struct __attribute__((packed)) {
    char IMPM[4];      // not include NULL
    char SongName[26]; // includes NULL
    uint16_t PHiligt; // Pattern row hilight information.
    uint16_t OrdNum; // Number of orders in song
    uint16_t InsNum; // Number of instruments in song
    uint16_t SmpNum; // Number of samples in song
    uint16_t PatNum; // Number of patterns in song
    uint16_t CwtV; // Created with tracker. Impulse Tracker y.xx = 0yxxh
    uint16_t Cmwt; // Compatible with tracker with version greater than value.
    it_head_flags_t Flags; // it_head_flags_t
    it_special_t Special; // it_special_t
    uint8_t GV; // Global volume. (0->128) All volumes are adjusted by this
    uint8_t MV; // Mix volume (0->128) During mixing, this value controls the magnitude of the wave being mixed.
    uint8_t IS; // Initial Speed of song.
    uint8_t IT; // Initial Tempo of song
    uint8_t Sep; // Panning separation between channels (0->128, 128 is max sep.)
    uint8_t PWD; // Pitch wheel depth for MIDI controllers
    uint16_t MsgLgth;
    uint32_t MsgOfst;
    uint32_t Reserved;
    uint8_t ChnlPan[64]; // Volume for each channel. Ranges from 0->64
    uint8_t ChnlVol[64]; // ach byte contains a panning value for a channel.
    uint8_t *Orders;
    uint32_t *InstOfst;
    uint32_t *SampHeadOfst;
    uint32_t *PatternOfst;
} it_header_t;

typedef enum __attribute__((packed)) {
    NNA_CUT,
    NNA_CONTINUE,
    NNA_NOTEOFF,
    NNA_NOTEFADE
} new_note_activ_t;

typedef enum __attribute__((packed)) {
    DCT_OFF,
    DCT_NOTE,
    DCT_SAMPLE,
    DCT_INSTRUMENT
} duplicat_check_type_t;

typedef enum __attribute__((packed)) {
    DCA_CUT,
    DCA_NOTEOFF,
    DCA_NOTEFADE
} duplicat_check_activ_t;

typedef struct {
    bool EnvOn : 1;
    bool LoopOn : 1;
    bool SusLoopOn : 1;
    uint8_t Recv : 4;
    bool usePitchEnvAsFltr : 1;
} it_inst_env_flags_t;

// int8_t y point
typedef struct __attribute__((packed)) {
    int8_t y;
    uint16_t tick;
} it_inst_env_point_t;

// uint8_t y point
typedef struct __attribute__((packed)) {
    uint8_t y;
    uint16_t tick;
} it_inst_env_upoint_t;

typedef struct __attribute__((packed)) {
    it_inst_env_flags_t Flg; // it_inst_env_flags_t
    uint8_t Num; // Number of node points
    uint8_t LpB; // Loop beginning
    uint8_t LpE; // Loop end
    uint8_t SLB; // Sustain loop beginning
    uint8_t SLE; // Sustain loop end
    it_inst_env_upoint_t envelope[25];
} it_inst_uenvelope_t;

typedef struct __attribute__((packed)) {
    it_inst_env_flags_t Flg; // it_inst_env_flags_t
    uint8_t Num; // Number of node points
    uint8_t LpB; // Loop beginning
    uint8_t LpE; // Loop end
    uint8_t SLB; // Sustain loop beginning
    uint8_t SLE; // Sustain loop end
    it_inst_env_point_t envelope[25];
} it_inst_envelope_t;

typedef struct __attribute__((packed)) {
    uint8_t note;
    uint8_t sample;
} note_to_samp_table_t;

typedef struct __attribute__((packed)) {
    char IMPI[4];
    char DOSFilename[12];
    uint8_t Reserved0;
    new_note_activ_t NNA;
    duplicat_check_type_t DCT;
    duplicat_check_activ_t DCA;
    uint16_t FadeOut; // Ranges between 0 and 128, but the fadeout "Count" is 1024
    int8_t PPS; // Pitch-Pan separation, range -32 -> +32
    uint8_t PPC; // Pitch-Pan center: C-0 to B-9 represented as 0->119 inclusive
    uint8_t GbV; // Global Volume, 0->128
    uint8_t DfP; // Default Pan, 0->64, &128 => Don't use
    uint8_t RV; // Random volume variation (percentage)
    uint8_t RP; // Random panning variation (panning change - not implemented yet)
    uint16_t TrkVers; // Tracker version
    uint8_t NoS; // Number of samples associated with instrument. >InstFile only<
    uint8_t Reserved1;
    char InstName[26];
    uint8_t IFC; // Initial Filter cutoff
    uint8_t IFR; // Initial Filter resonance
    uint8_t MCh; // MIDI Channel
    uint8_t MPr; // MIDI Program (Instrument)
    uint16_t MIDIBnk; // what is this??
    note_to_samp_table_t noteToSampTable[120];
    it_inst_uenvelope_t volEnv;
    it_inst_envelope_t panEnv;
    it_inst_envelope_t pitEnv;
    uint8_t wastedByte[7];
} it_instrument_t;

typedef enum __attribute__((packed)) {
    WAVE_SINE,
    WAVE_RAMPDOWN,
    WAVE_SQUARE,
    WAVE_RANDOM
} vibrato_waveform_t;

typedef struct __attribute__((packed)) {
    char IMPS[4]; // no include NULL
    char DOSFilename[12]; // no include NULL
    uint8_t Reserved;
    uint8_t Gvl; // Global Vol
    it_sample_flags_t Flg; // Flags
    uint8_t Vol; // Vol
    char SampleName[26]; // include NULL
    it_sample_convert_t Cvt;
    uint8_t DfP; // Default Pan. Bits 0->6 = Pan value, Bit 7 ON to USE (opposite of inst)
    uint32_t Length; // Sample Number
    uint32_t LoopBegin; // Sample Number
    uint32_t LoopEnd; // Sample Number
    uint32_t C5Speed; // Sample Rate
    uint32_t SusLoopBegin;
    uint32_t SusLoopEnd;
    uint32_t SamplePointer;
    uint8_t ViS; // Vibrato Speed, ranges from 0->64
    uint8_t ViD; // Vibrato Depth, ranges from 0->64
    vibrato_waveform_t ViR; // Vibrato waveform type.
    uint8_t ViT; // Vibrato Rate, rate at which vibrato is applied (0->64)
    uint32_t speedTable[128];
    void *sample_data;
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

    printf("  BIT0 STEREO: %d\n", header->Flags.stereo);
    printf("  BIT1 VOL0MIXOPTZ: %d\n", header->Flags.vol0MixOptz);
    printf("  BIT2 USEINST: %d\n", header->Flags.useInst);
    printf("  BIT3 LINESLID: %d\n", header->Flags.lineSlid);
    printf("  BIT4 OLDEFCT: %d\n", header->Flags.oldEfct);
    printf("  BIT5 LEGMWEF: %d\n", header->Flags.LEGMWEF);
    printf("  BIT6 USERMIDIPITCHCTRL: %d\n", header->Flags.useMidiPitchCtrl);
    printf("  BIT7 REQEMBMIDICONF: %d\n", header->Flags.ReqEmbMidiConf);

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
    printf("\n\n");
}

void read_and_unpack_pattern(FILE *file, uint32_t offset, pattern_note_t **unpack_data, uint16_t *ChannelsOut, uint16_t *RowsOut) {
    it_packed_pattern_t pattern_data;
    printf("Reading Pattern in 0x%x\n", offset);
    fseek(file, offset, SEEK_SET);
    printf("File Jmp To 0x%x\n", ftell(file));
    fread(&pattern_data, 2, 2, file);
    printf("Rows %d, Len %d\n", pattern_data.rows, pattern_data.length);
    if (pattern_data.rows > 200 || pattern_data.rows < 32 || pattern_data.length > 65527) {
        printf("This is not a valid Pattern!\n\n");
        *RowsOut = 0;
        return;
    }
    fseek(file, 4, SEEK_CUR);
    printf("malloc mem\n");
    pattern_data.packed_data = (uint8_t*)malloc(pattern_data.length);
    printf("read pack data\n");
    fread(pattern_data.packed_data, pattern_data.length, 1, file);
    printf("malloc unpack mem\n");
    for (uint8_t i = 0; i < MAX_CHANNELS; i++) {
        unpack_data[i] = (pattern_note_t*)malloc(pattern_data.rows * sizeof(pattern_note_t));
        if (unpack_data[i] == NULL) {
            printf("malloc failed! free_heap_size: %d\n", esp_get_free_heap_size());
        }
        memset(unpack_data[i], 0, pattern_data.rows * sizeof(pattern_note_t));
    }
    printf("unpacking...\n");
    *ChannelsOut = unpack_pattern(&pattern_data, unpack_data);
    printf("Pattern 0x%x Max Chan: %d\n", offset, *ChannelsOut);
    *RowsOut = pattern_data.rows;
    printf("free\n");
    free(pattern_data.packed_data);
    printf("free finish\n\n");
}

void read_it_inst(FILE *file, uint32_t offset, it_instrument_t *inst) {
    printf("Reading Instrument in 0x%x\n", offset);
    fseek(file, offset, SEEK_SET);
    printf("File Jmp To 0x%x\n", ftell(file));
    printf("Reading data...\n");
    fread(inst, sizeof(it_instrument_t), 1, file);
    printf("IMPI: %.4s\n", inst->IMPI);
    printf("DOSFilename: %.12s\n", inst->DOSFilename);
    printf("Instrument Name: %s\n", inst->InstName);
    printf("noteToSampTable: ");
    for (uint8_t i = 0; i < 120; i++) {
        printf("%d ", inst->noteToSampTable[i].sample);
    }
    printf("\n\n");

    /*
    printf("Vol Env:\n");
    for (uint8_t i = 0; i < 25; i++) {
        printf("%4d -> %2d\n", inst->volEnv.envelope[i].tick, inst->volEnv.envelope[i].y);
    }

    printf("Pan Env:\n");
    for (uint8_t i = 0; i < 25; i++) {
        printf("%4d -> %2d\n", inst->panEnv.envelope[i].tick, inst->panEnv.envelope[i].y);
    }

    printf("Pitch Env:\n");
    for (uint8_t i = 0; i < 25; i++) {
        printf("%4d -> %2d\n", inst->pitEnv.envelope[i].tick, inst->pitEnv.envelope[i].y);
    }
    */
}

void read_it_sample(FILE *file, uint32_t offset, it_sample_t *sample) {
    printf("Reading Sample Header in 0x%x\n", offset);
    fseek(file, offset, SEEK_SET);
    printf("File Jmp To 0x%x\n", ftell(file));
    printf("Reading header...\n");
    fread(sample, 80, 1, file);
    printf("IMPS: %.4s\n", sample->IMPS);
    printf("DOSFilename: %.12s\n", sample->DOSFilename);
    printf("Reserved: 0x%x\n", sample->Reserved);
    printf("Gvl: 0x%x\n", sample->Gvl);
    printf("Flg: 0x%x\n", sample->Flg);

    printf("  smpWithHead: %d\n", sample->Flg.sampWithHead);
    printf("  use16Bit: %d\n", sample->Flg.use16Bit);
    printf("  stereo: %d\n", sample->Flg.stereo);
    printf("  comprsSamp: %d\n", sample->Flg.comprsSamp);
    printf("  useLoop: %d\n", sample->Flg.useLoop);
    printf("  useSusLoop: %d\n", sample->Flg.useSusLoop);
    printf("  pingPongLoop: %d\n", sample->Flg.pingPongLoop);
    printf("  pingPongSusLoop: %d\n", sample->Flg.pingPongSusLoop);

    printf("Vol: 0x%x\n", sample->Vol);
    printf("SampleName: %.26s\n", sample->SampleName);
    printf("Cvt: 0x%x\n", sample->Cvt);
    printf("DfP: 0x%x\n", sample->DfP);
    printf("Length: %d\n", sample->Length);
    printf("LoopBegin: %d\n", sample->LoopBegin);
    printf("LoopEnd: %d\n", sample->LoopEnd);
    printf("C5Speed: %d\n", sample->C5Speed);
    printf("SusLoopBegin: %d\n", sample->SusLoopBegin);
    printf("SusLoopEnd: %d\n", sample->SusLoopEnd);
    printf("SamplePointer: 0x%x\n", sample->SamplePointer);
    printf("ViS: 0x%x\n", sample->ViS);
    printf("ViD: 0x%x\n", sample->ViD);
    printf("ViR: 0x%x\n", sample->ViR);
    printf("ViT: 0x%x\n", sample->ViT);
    if (sample->Length == 0) {
        printf("This Sample is NULL, Skip!\n");
        return;
    }
    // printf("sample_data: 0x%p\n", sample->sample_data);
    printf("Sample Data in 0x%x\n", sample->SamplePointer);
    fseek(file, sample->SamplePointer, SEEK_SET);
    uint32_t sampRelSizeByte;
    printf("Generate frequency tables...\n");
    printf("C5->A4 = %f\n", sample->C5Speed * powf(2.0f, -9.0f / 12.0f));
    if (sample->Flg.stereo) {
        if (sample->Flg.use16Bit) {
            sampRelSizeByte = sample->Length * sizeof(audio_stereo_16_t);
            convert_c5speed(sample->C5Speed, sample->speedTable);
        } else {
            sampRelSizeByte = sample->Length * sizeof(audio_stereo_8_t);
            convert_c5speed(sample->C5Speed, sample->speedTable);
        }
    } else {
        if (sample->Flg.use16Bit) {
            sampRelSizeByte = sample->Length * sizeof(audio_mono_16_t);
            convert_c5speed(sample->C5Speed, sample->speedTable);
        } else {
            sampRelSizeByte = sample->Length * sizeof(audio_mono_8_t);
            convert_c5speed(sample->C5Speed, sample->speedTable);
        }
    }
    printf("Sample Rel Byte is %d\n", sampRelSizeByte);
    printf("Generate Success\n");
    printf("malloc mem\n");
    sample->sample_data = malloc(sampRelSizeByte);
    if (sample->sample_data == NULL) {
        printf("malloc failed!!\n");
        printf("Because: %s\n", strerror(errno));
        printf("free heap size: %d\n", esp_get_free_heap_size());
        for (;;) {
            vTaskDelay(32);
        }
    }
    printf("File Jmp To 0x%x\n", ftell(file));
    printf("reading data...\n");
    if (sample->Flg.stereo) {
        printf("This is a Srereo Sample, Converting...\n");
        void *tmp = malloc(sampRelSizeByte);
        fread(tmp, sampRelSizeByte, 1, file);
        uint32_t dataSize = sampRelSizeByte / 2;
        printf("%d = %d / 2\n", dataSize, sampRelSizeByte);
        if (sample->Flg.use16Bit) {
            uint32_t sampSize = dataSize / 2;
            audio_stereo_16_t *tmp16 = (audio_stereo_16_t*)sample->sample_data;
            int16_t *vtmp16 = (int16_t*)tmp;
            for (uint32_t i = 0; i < sampSize; i++) {
                tmp16[i].l = vtmp16[i];
                tmp16[i].r = vtmp16[i+sampSize];
            }
        } else {
            audio_stereo_8_t *tmp8 = (audio_stereo_8_t*)sample->sample_data;
            int8_t *vtmp8 = (int8_t*)tmp;
            for (uint32_t i = 0; i < dataSize; i++) {
                tmp8[i].l = vtmp8[i];
                tmp8[i].r = vtmp8[i+dataSize];
            }
        }
    } else {
        fread(sample->sample_data, sampRelSizeByte, 1, file);
    }
    printf("read finish!\n\n");
}

#endif // IT_FILE_H