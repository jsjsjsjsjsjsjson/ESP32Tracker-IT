#include <Arduino.h>
#include <driver/i2s_std.h>
#include <esp_spiffs.h>
#include <Adafruit_SSD1306.h>
// #include "debug_memory.h"
#include "SerialTerminal.h"
#include "it_file.h"
#include "extra_func.h"
#include "vol_table.h"

Adafruit_SSD1306 display(128, 64, &SPI, 7, 15, 6);

#define SMP_RATE 22050
#define BUFF_SIZE 4096

audio_stereo_16_t audioBuffer[BUFF_SIZE];

i2s_chan_handle_t i2s_tx_handle;
i2s_chan_config_t i2s_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
i2s_std_config_t i2s_std_cfg = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SMP_RATE),
    .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
    .gpio_cfg = {
        .mclk = I2S_GPIO_UNUSED,
        .bclk = GPIO_NUM_42,
        .ws = GPIO_NUM_40,
        .dout = GPIO_NUM_41,
        .din = I2S_GPIO_UNUSED,
        .invert_flags = {
            .mclk_inv = false,
            .bclk_inv = false,
            .ws_inv = false,
        },
    },
};

it_header_t it_header;
it_instrument_t *it_instrument;
it_sample_t *it_samples;
pattern_note_t ***unpack_data; // unpack_data[PatNum][Channel][Rows].note_data
uint16_t *maxChlTable;
uint16_t *maxRowTable;

uint16_t maxChannel = 0;

void get_track(int argc, const char* argv[]) {
    if (argc < 6) {
        printf("%s <Pat> <ChlStart> <ChlEnd> <RowStart> <RowEnd>\n", argv[0]);
        return;
    }
    uint16_t pat = strtol(argv[1], NULL, 0);
    uint16_t chlstart = strtol(argv[2], NULL, 0);
    uint16_t chlend = strtol(argv[3], NULL, 0);
    uint16_t rowstart = strtol(argv[4], NULL, 0);
    uint16_t rowend = strtol(argv[5], NULL, 0);
    printf("ROWS |");
    for (uint16_t i = chlstart; i < chlend; i++) {
            printf("  Channel%2d         |", i);
    }
    printf("\n");
    for (uint16_t row_index = rowstart; row_index < rowend; row_index++) {
        printf("%3d: |", row_index);
        for (uint16_t chl_index = chlstart; chl_index < chlend; chl_index++) {
            printf("%3d %3d %3d %3d %3d |", unpack_data[pat][chl_index][row_index].note, 
                                                unpack_data[pat][chl_index][row_index].instrument,
                                                    unpack_data[pat][chl_index][row_index].volume,
                                                        unpack_data[pat][chl_index][row_index].command,
                                                            unpack_data[pat][chl_index][row_index].command_value);
        }
        printf("\n");
    }
}

void get_free_heap_cmd(int argc, const char* argv[]) {
    printf("Free heap size: %ld\n", esp_get_free_heap_size());
}

void reboot_cmd(int argc, const char* argv[]) {
    esp_restart();
}

/*
void get_heap_stat(int argc, const char* argv[]) {
    view_heap_status();
}
*/

typedef enum __attribute__((packed)) {
    NOTE_OFF,
    NOTE_ON
} note_stat_t;

note_stat_t note_stat[MAX_CHANNELS];
uint8_t note_vol[MAX_CHANNELS];
uint64_t frac_index[MAX_CHANNELS];
uint32_t int_index[MAX_CHANNELS];

// 这个make_sound实现非常非常慢，希望好心人能优化一下
inline audio_stereo_16_t make_sound(uint32_t freq, uint8_t vol, uint8_t chl, uint16_t smp_num) {
    audio_stereo_16_t result = {0, 0};
    if (note_stat[chl] == NOTE_OFF || vol == 0 || it_samples[smp_num].sample_data == NULL) 
        return result;
    it_sample_t *sample = &it_samples[smp_num];
    uint64_t increment = (freq << 16) / SMP_RATE;
    frac_index[chl] += increment;
    int_index[chl] += frac_index[chl] >> 16;
    frac_index[chl] &= 0xFFFF;

    if (sample->Flg.useLoop || sample->Flg.pingPongLoop) {
        if (int_index[chl] >= sample->LoopEnd) {
            int_index[chl] = sample->LoopBegin;
        }
    } else if (int_index[chl] >= sample->Length) {
        note_stat[chl] = NOTE_OFF;
        int_index[chl] = frac_index[chl] = note_vol[chl] = 0;
        return result;
    }

    uint32_t idx = int_index[chl];

    if (sample->Flg.use16Bit) {
        if (sample->Flg.stereo) {
            audio_stereo_16_t *dataTmp = (audio_stereo_16_t *)sample->sample_data;
            result = dataTmp[idx];
        } else {
            audio_mono_16_t *dataTmp = (audio_mono_16_t *)sample->sample_data;
            result = (audio_stereo_16_t){dataTmp[idx], dataTmp[idx]};
        }
    } else {
        if (sample->Flg.stereo) {
            audio_stereo_8_t *dataTmp = (audio_stereo_8_t *)sample->sample_data;
            result = (audio_stereo_16_t){(int16_t)(dataTmp[idx].l << 8), (int16_t)(dataTmp[idx].r << 8)};
        } else {
            audio_mono_8_t *dataTmp = (audio_mono_8_t *)sample->sample_data;
            result = (audio_stereo_16_t){(int16_t)(dataTmp[idx] << 8), (int16_t)(dataTmp[idx] << 8)};
        }
    }

    float vol_factor = vol_table[vol];
    result.l *= vol_factor;
    result.r *= vol_factor;
    return result;
}

void play_samp_cmd(int argc, const char* argv[]) {
    if (argc < 3) {
        printf("%s <SmpNum> <note> <s>\n", argv[0]);
        return;
    }
    size_t writed;
    uint16_t buffPoint = 0;
    uint16_t smp_num = strtol(argv[1], NULL, 0);
    uint16_t note = strtol(argv[2], NULL, 0);
    uint32_t time = strtol(argv[3], NULL, 0) * SMP_RATE;
    printf("Playing SMP #%d %s %dHz %dBit %s %s %s, %dTick\n", smp_num, it_samples[smp_num].SampleName, it_samples[smp_num].speedTable[note],
                                                            it_samples[smp_num].Flg.use16Bit ? 16 : 8, it_samples[smp_num].Cvt.sampIsSigned ? "Signed" : "UnSigned",
                                                            it_samples[smp_num].Flg.stereo ? "Stereo" : "Mono",
                                                            it_samples[smp_num].Cvt.sampIsDPCM ? "DPCM" : "PCM", time);
    note_stat[0] = NOTE_ON;
    int_index[0] = 0;
    frac_index[0] = 0;
    // note_vol[0] = 0;
    for (uint32_t t = 0; t < time; t++) {
        audioBuffer[buffPoint] = make_sound(it_samples[smp_num].speedTable[note], 16, 0, smp_num);
        buffPoint++;
        if (buffPoint > BUFF_SIZE) {
            buffPoint = 0;
            i2s_channel_write(i2s_tx_handle, audioBuffer, sizeof(audioBuffer), &writed, portMAX_DELAY);
            vTaskDelay(1);
        }
    }
    memset(audioBuffer, 0, sizeof(audioBuffer));
    i2s_channel_write(i2s_tx_handle, audioBuffer, sizeof(audioBuffer), &writed, portMAX_DELAY);
}

void pause_serial() {
    while(!Serial.available()) {vTaskDelay(16);}
    Serial.read();
}

uint8_t now_note[MAX_CHANNELS];
float now_freq[MAX_CHANNELS];
uint8_t now_vol[MAX_CHANNELS];
uint8_t now_samp[MAX_CHANNELS];
uint8_t now_inst[MAX_CHANNELS];
uint8_t now_efct0[MAX_CHANNELS];
uint8_t now_efct1[MAX_CHANNELS];

void playTask(void *arg) {
    size_t writed;
    printf("Initialisation....\n");
    uint16_t TempoTickMax = TEMPO_TO_TICKS(it_header.IT, SMP_RATE);
    printf("TempoTickMax: %d\n", TempoTickMax);
    uint8_t TicksRow = it_header.IS;
    printf("TicksRow: %d\n", TicksRow);
    uint16_t bufferIndex = 0;
    int16_t tracker_rows = 0;
    int16_t tracker_ords = 0;
    uint8_t tracker_pats = it_header.Orders[tracker_ords];
    uint32_t tempo_tick = 0;
    uint32_t tick = 0;
    memset(now_note, 0, sizeof(now_note));
    memset(now_vol, 0, sizeof(now_vol));
    memset(now_freq, 0, sizeof(now_freq));
    memset(now_vol, 0, sizeof(now_vol));
    memset(now_samp, 0, sizeof(now_samp));
    memset(now_inst, 0, sizeof(now_inst));
    printf("Readly...\n");
    // pause_serial();
    for (;;) {
        audio_stereo_32_t tmp = {0, 0};
        for (uint16_t chl = 0; chl < maxChannel; chl++) {
            audio_stereo_16_t tmp16;
            tmp16 = make_sound(now_freq[chl], now_vol[chl], chl, now_samp[chl]);
            tmp.l += tmp16.l;
            tmp.r += tmp16.r;
        }
        audioBuffer[bufferIndex].l = tmp.l / maxChannel;
        audioBuffer[bufferIndex].r = tmp.r / maxChannel;
        bufferIndex++;
        tempo_tick++;

        if (bufferIndex > BUFF_SIZE) {
            bufferIndex = 0;
            i2s_channel_write(i2s_tx_handle, audioBuffer, sizeof(audioBuffer), &writed, portMAX_DELAY);
            vTaskDelay(1);
        }

        if (tempo_tick >= TempoTickMax) {
            tempo_tick = 0;
            tick++;
            if (tick >= TicksRow) {
                tick = 0;
                for (uint16_t chl = 0; chl < maxChannel; chl++) {
                    // 这里的逻辑本身是轨道读取，之前有个简单的实现但是我觉得可能会造成石山所以准备重构...
                }
                // printf("%2d %3d: ", tracker_pats, tracker_rows);
                // for (uint8_t i = 0; i < 12; i++) {
                //     printf("%3d %2d %2d |", now_note[i], now_inst[i], now_samp[i]);
                // }
                // printf("\n");
                tracker_rows++;
                if (tracker_rows >= maxRowTable[tracker_pats]) {
                    tracker_rows = 0;
                    tracker_ords++;
                    if (tracker_ords >= it_header.OrdNum)
                        tracker_ords = 0;
                    tracker_pats = it_header.Orders[tracker_ords];
                    printf("skip to %d -> %d\n", tracker_ords, tracker_pats);
                }
            }
        }
    }
    vTaskDelete(NULL);
}

void displayTask(void *arg) {
    for (;;) {
        display.clearDisplay();
        // for (uint8_t i = 0; i < 64; i++) {
        //     display.drawFastHLine(0, i, now_vol[i], 1);
        // }
        for (uint8_t i = 0; i < 127; i++) {
            display.drawPixel(i, 31 + (audioBuffer[i<<2].l >> 9), 1);
            display.drawPixel(i, 31 + (audioBuffer[i<<2].r >> 9), 1);
        }
        display.display();
        vTaskDelay(1);
    }
}

void start_play_cmd(int argc, const char* argv[]) {
    printf("Starting PlayTask...\n");
    xTaskCreate(playTask, "PLAYTASK", 10240, NULL, 6, NULL);
}

void get_c5_speed_cmd(int argc, const char* argv[]) {
    if (argc < 2) {printf("get_c5_speed <smp_num>\n"); return;}
    uint16_t num = strtol(argv[1], NULL, 0);
    printf("SMP #%d %s C5Speed: %dHz\n", num, it_samples[num].SampleName, it_samples[num].C5Speed);
}

void get_speed_table_cmd(int argc, const char* argv[]) {
    if (argc < 2) {printf("get_speed_table <smp_num>\n"); return;}
    uint16_t num = strtol(argv[1], NULL, 0);
    printf("SMP #%d %s C5Speed: %dHz\n", num, it_samples[num].SampleName, it_samples[num].C5Speed);
    printf("NOTE | FREQ\n");
    for (uint8_t i = 0; i < 128; i++) {
        printf("%4d | %f\n", i, it_samples[num].speedTable[i]);
        pause_serial();
    }
}

void mainTask(void *arg) {
    SerialTerminal terminal;
    SPI.begin(17, -1, 16);
    display.begin(SSD1306_SWITCHCAPVCC);
    display.display();
    terminal.begin(115200, "ESP32Tracker DEBUG");
    terminal.addCommand("reboot", reboot_cmd);
    terminal.addCommand("get_track", get_track);
    terminal.addCommand("get_free_heap", get_free_heap_cmd);
    terminal.addCommand("start_play", start_play_cmd);
    terminal.addCommand("get_c5_speed", get_c5_speed_cmd);
    terminal.addCommand("get_speed_table", get_speed_table_cmd);
    terminal.addCommand("play_smp", play_samp_cmd);
    // terminal.addCommand("get_heap_stat", get_heap_stat);
    // Open File
    FILE *file = fopen("/spiffs/fod_splice_slice.it", "rb");

    // Read Header
    read_it_header(file, &it_header);
    pause_serial();

    // Read Instrument
    it_instrument = (it_instrument_t*)malloc(it_header.InsNum * sizeof(it_instrument_t));
    for (uint16_t inst = 0; inst < it_header.InsNum; inst++) {
        printf("Reading Instrument #%d...\n", inst);
        read_it_inst(file, it_header.InstOfst[inst], &it_instrument[inst]);
    }

    // Read Samples
    it_samples = (it_sample_t*)malloc(it_header.SmpNum * sizeof(it_sample_t));
    for (uint16_t smp = 0; smp < it_header.SmpNum; smp++) {
        printf("Reading Sample #%d...\n", smp);
        read_it_sample(file, it_header.SampHeadOfst[smp], &it_samples[smp]);
    }

    // Read Pattern
    unpack_data = (pattern_note_t***)malloc(it_header.PatNum * sizeof(pattern_note_t**));
    memset(unpack_data, 0, it_header.PatNum * sizeof(unpack_data));
    for (uint16_t pat = 0; pat < it_header.PatNum; pat++) {
        unpack_data[pat] = (pattern_note_t**)malloc(MAX_CHANNELS * sizeof(pattern_note_t*));
    }
    maxChlTable = (uint16_t*)malloc(it_header.PatNum * sizeof(uint16_t));
    maxRowTable = (uint16_t*)malloc(it_header.PatNum * sizeof(uint16_t));
    for (uint16_t i = 0; i < it_header.PatNum; i++) {
        printf("Read and Unpack Pattern #%d...\n", i);
        read_and_unpack_pattern(file, it_header.PatternOfst[i], unpack_data[i], &maxChlTable[i], &maxRowTable[i]);
    }
    maxChannel = findMax(maxChlTable, it_header.PatNum) + 1;
    printf("This is a %d Channel IT\n", maxChannel);
    printf("Freeing up memory on redundant channels...\n");
    for (uint16_t pat = 0; pat < it_header.PatNum; pat++) {
        if (maxRowTable[pat]) {
            printf("Free Pat %d's Chl%d ~ Chl%d\n", pat, maxChannel, MAX_CHANNELS);
            for (uint16_t chl = maxChannel; chl < MAX_CHANNELS; chl++) {
                free(unpack_data[pat][chl]);
            }
        } else {
            printf("Pat #%d is valid, Skip!\n", pat);
            free(unpack_data[pat]);
        }
    }
    printf("%d\n", sizeof(it_instrument_t));
    fclose(file);
    xTaskCreate(displayTask, "DISPLAY", 4096, NULL, 4, NULL);

    for (;;) {
        terminal.update();
        vTaskDelay(2);
    }
    //fclose(file);
    vTaskDelete(NULL);
}

void setup() {
    esp_vfs_spiffs_conf_t spiffs_conf = {
        .base_path = "/spiffs",
        .partition_label = "spiffs",
        .max_files = 2,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&spiffs_conf);
    if (ret == ESP_OK) 
        printf("SPIFFS mounted!\n");

    printf("I2S NEW CHAN %d\n", i2s_new_channel(&i2s_chan_cfg, &i2s_tx_handle, NULL));
    printf("I2S INIT CHAN %d\n", i2s_channel_init_std_mode(i2s_tx_handle, &i2s_std_cfg));
    printf("I2S ENABLE %d\n", i2s_channel_enable(i2s_tx_handle));
    xTaskCreate(mainTask, "MAINTASK", 40960, NULL, 4, NULL);
}

void loop() {
    printf("DELETE LOOP\n");
    vTaskDelete(NULL);
}