#include <Arduino.h>
#include <driver/i2s_std.h>
#include <esp_spiffs.h>
#include <Adafruit_SSD1306.h>
// #include "debug_memory.h"
#include "SerialTerminal.h"
#include "it_file.h"
#include "extra_func.h"
#include "channel.h"
#include "vol_table.h"
#include "it_config.h"
#include "write_wav.h"

Adafruit_SSD1306 display(128, 64, &SPI, 7, 15, 6, 10000000);

audio_stereo_32_t audioBuffer[BUFF_SIZE];

i2s_chan_handle_t i2s_tx_handle;
i2s_chan_config_t i2s_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
i2s_std_config_t i2s_std_cfg = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SMP_RATE),
    .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
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

int16_t tracker_rows = 0;
int16_t tracker_ords = 0;
uint8_t tracker_pats = 0;

it_unpack_envelope_t *inst_envelope;

uint8_t TicksRow;
uint16_t TempoTickMax;

uint8_t actvChan = 0;

Channel* channels;

uint8_t GlobalVol;

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
            printf("   Channel%02d   |Mask|", i);
    }
    printf("\n");
    for (uint16_t i = chlstart; i < chlend; i++) {
            printf("----------------------", i);
    }
    printf("\n");
    for (uint16_t row_index = rowstart; row_index < rowend; row_index++) {
        printf("%03d: |", row_index);
        for (uint16_t chl_index = chlstart; chl_index < chlend; chl_index++) {
            char note_tmp[4] = "---";
            uint8_t mask = unpack_data[pat][chl_index][row_index].mask;
            if (GET_NOTE(mask)) {
                midi_note_to_string(unpack_data[pat][chl_index][row_index].note, note_tmp);
                printf("%s ", note_tmp);
            } else {
                printf("... ");
            }

            if (GET_INSTRUMENT(mask)) {
                printf("%02d ", unpack_data[pat][chl_index][row_index].instrument);
            } else {
                printf(".. ");
            }

            if (GET_VOLUME(mask)) {
                uint8_t vtmp;
                char stat;
                volCmdToRel(unpack_data[pat][chl_index][row_index].volume, &stat, &vtmp);
                printf("%c%02d ", stat, vtmp);
            } else {
                printf("... ");
            }

            if (GET_COMMAND(mask)) {
                printf("%c%02x ", unpack_data[pat][chl_index][row_index].command + 64,
                                    unpack_data[pat][chl_index][row_index].command_value);
            } else {
                printf("... ");
            }

            printf("|0x%02x|", unpack_data[pat][chl_index][row_index].mask);
        }
        printf("\n");
    }
}

void get_free_heap_cmd(int argc, const char* argv[]) {
    printf("Free heap size: %ld\n", esp_get_free_heap_size());
}

void get_env_cmd(int argc, const char* argv[]) {
    if (argc < 2) {
        printf("%s <instNum>\n", argv[0]);
        return;
    }
    uint8_t instNum = strtol(argv[1], NULL, 0);
    printf("TICK | Y\n");
    for (uint8_t i = 0; i < it_instrument[instNum].volEnv.Num; i++) {
        printf("%04d | %02d\n", it_instrument[instNum].volEnv.envelope[i].tick, it_instrument[instNum].volEnv.envelope[i].y);
    }
}

void get_env_itp_cmd(int argc, const char* argv[]) {
    if (argc < 3) {
        printf("%s <instNum> <tick>\n", argv[0]);
        return;
    }
    uint8_t instNum = strtol(argv[1], NULL, 0);
    uint16_t tick = strtol(argv[2], NULL, 0);
    int8_t y = 0;// get_env_y(&it_instrument[instNum].volEnv, tick);
    printf("TICK%d = ENV%d\n", tick, y);
}

void reboot_cmd(int argc, const char* argv[]) {
    esp_restart();
}

/*
void get_heap_stat(int argc, const char* argv[]) {
    view_heap_status();
}
*/

void play_samp_cmd(int argc, const char* argv[]) {
    if (argc < 3) {
        printf("%s <SmpNum> <note> <s>\n", argv[0]);
        return;
    }
    /*
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
        audioBuffer[buffPoint] = make_sound(it_samples[smp_num].speedTable[note], 64, 128, 64, 1024, 0, smp_num);
        buffPoint++;
        if (buffPoint > BUFF_SIZE) {
            buffPoint = 0;
            i2s_channel_write(i2s_tx_handle, audioBuffer, sizeof(audioBuffer), &writed, portMAX_DELAY);
            vTaskDelay(1);
        }
    }
    memset(audioBuffer, 0, sizeof(audioBuffer));
    i2s_channel_write(i2s_tx_handle, audioBuffer, sizeof(audioBuffer), &writed, portMAX_DELAY);
    */
}

void pause_serial() {
    while(!Serial.available()) {vTaskDelay(16);}
    Serial.read();
}

void displayTask(void *arg) {
    vTaskDelay(512);
    for (;;) {
        display.clearDisplay();
        for (uint8_t i = 0; i < maxChannel; i++) {
            display.drawFastVLine(i*2, 0, channels[i].FV_SHOW, 1);
            display.drawFastVLine((i*2)+1, 0, channels[i].FV_SHOW, 1);
        }
        // for (uint8_t i = 0; i < 64; i++) {
        //     display.drawFastHLine(0, i, now_vol[i], 1);
        // }
        for (uint8_t i = 36; i < 127; i++) {
            display.drawPixel(i, 31 + (audioBuffer[i<<2].l >> 23), 1);
            display.drawPixel(i, 31 + (audioBuffer[i<<2].r >> 23), 1);
        }
        display.display();
        vTaskDelay(1);
    }
}

void skipNextPat(uint8_t row) {
    tracker_rows = row;
    tracker_ords++;
    while (it_header.Orders[tracker_ords] == 254) {
        tracker_ords++;
    }
    if (it_header.Orders[tracker_ords] == 255)
        tracker_ords = 0;

    tracker_pats = it_header.Orders[tracker_ords];
    printf("skip to %d -> %d row %d\n", tracker_ords, tracker_pats, tracker_rows);
}

void jumpToPat(uint8_t ord) {
    tracker_rows = 0;
    tracker_ords = ord;
    tracker_pats = it_header.Orders[tracker_ords];
    printf("skip to %d -> %d\n", tracker_ords, tracker_pats);
}

void playTask(void *arg) {
    size_t writed;
    printf("Initialisation....\n");
    TempoTickMax = TEMPO_TO_TICKS(it_header.IT, SMP_RATE);
    printf("TempoTickMax: %d\n", TempoTickMax);
    TicksRow = it_header.IS;
    printf("TicksRow: %d\n", TicksRow);
    uint16_t bufferIndex = 0;
    tracker_rows = 0;
    tracker_ords = 0;
    tracker_pats = it_header.Orders[tracker_ords];
    uint32_t tempo_tick = 0;
    uint32_t tick = 0;
    uint16_t sampOfst[MAX_CHANNELS]= {0};
    bool reset[MAX_CHANNELS] = {true};
    printf("Readly...\n");
    channels = new Channel[maxChannel];
    xTaskCreatePinnedToCore(displayTask, "DISPLAY", 4096, NULL, 4, NULL, 1);
    for (uint8_t c = 0; c < maxChannel; c++) {
        channels[c].ChannelPan = (it_header.ChnlPan[c] - 31) * 2;
        channels[c].ChannelVol = it_header.ChnlVol[c];
        channels[c].num = c;
    }
    // pause_serial();
    for (;;) {
        if (tempo_tick > TempoTickMax) {
            tempo_tick = 0;
            tick++;
            if (tick >= TicksRow) {
                tick = 0;
                // printf("ROW %03d\n", tracker_rows);
                for (uint16_t chl = 0; chl < maxChannel; chl++) {
                    if (unpack_data[tracker_pats] == NULL) {
                        continue;
                    }
                    uint8_t mask = unpack_data[tracker_pats][chl][tracker_rows].mask;
                    uint8_t noteTmp = unpack_data[tracker_pats][chl][tracker_rows].note;
                    uint8_t commandTmp = unpack_data[tracker_pats][chl][tracker_rows].command;
                    char cmd = 64 + unpack_data[tracker_pats][chl][tracker_rows].command;
                    uint8_t cmdVar = unpack_data[tracker_pats][chl][tracker_rows].command_value;
                    uint8_t instTmp = unpack_data[tracker_pats][chl][tracker_rows].instrument;
                    char volFlg;
                    uint8_t volVar;
                    volCmdToRel(unpack_data[tracker_pats][chl][tracker_rows].volume, &volFlg, &volVar);

                    reset[chl] = true;
                    sampOfst[chl] = 0;
                    channels[chl].setVolSild(false, 0);
                    channels[chl].setPortTone(false, 0);
                    channels[chl].setToneUpSild(false, 0);
                    channels[chl].setToneDownSild(false, 0);
                    if (GET_VOLUME(mask)) {
                        if (volFlg == 'g') {
                            reset[chl] = false;
                            channels[chl].setVolSild(true, volCmdPortToneToRelPortToneLUT[volVar]);
                        }
                    }
                    if (GET_COMMAND(mask)) {
                        if (cmd == 'G') {
                            reset[chl] = false;
                            channels[chl].setPortTone(true, cmdVar);
                        }
                    }
                    if (GET_NOTE(mask)) {
                        if (noteTmp < 120) {
                            if (channels[chl].tonePort) {
                                channels[chl].setPortSource(channels[chl].chl_note);
                                channels[chl].changeNote(noteTmp);
                                channels[chl].setPortTarget(channels[chl].chl_note);
                                /*
                                char st[4];
                                char tt[4];
                                midi_note_to_string(channels[chl].tonePortSource, st);
                                midi_note_to_string(channels[chl].tonePortTarget, tt);
                                printf("INFO: G%02X SOURCE=%s TARGET=%s\n", channels[chl].tonePortSpeed, st, tt);
                                */
                            } else {
                                channels[chl].changeNote(noteTmp);
                            }
                            // printf("NOTE RESET = %d\n", reset);
                            // printf("CHL%02d ROW%03d: NOTE ON NOTE=%2d INST=%2d\n", chl, tracker_rows, channels[chl].chl_note, channels[chl].chl_inst);
                        } else if (noteTmp == 255) {
                            // printf("CHL%02d ROW%03d: NOTE OFF\n", chl, tracker_rows);
                            channels[chl].offNote();
                            
                        } else if (noteTmp == 254) {
                            channels[chl].cutNote();
                        } else {
                            channels[chl].fadeNote();
                        }
                    }
                    if (GET_COMMAND(mask)) {
                        if (cmd == 'A') {
                            // 设定Ticks/Row
                            TicksRow = cmdVar;
                        } else if (cmd == 'B') {
                            jumpToPat(cmdVar);
                        } else if (cmd == 'C') {
                            skipNextPat(cmdVar);
                        } else if (cmd == 'M') {
                            // 设定通道音量
                            channels[chl].setChanVol(cmdVar);
                        } else if (cmd == 'V') {
                            // 设定全局音量
                            GlobalVol = cmdVar;
                        } else if (cmd == 'D') {
                            // 音量滑动
                            channels[chl].setVolSild(true, cmdVar);
                        } else if (cmd == 'S') {
                            switch (hexToDecimalTens(cmdVar)) {
                            case 0x7:
                                channels[chl].chl_stat.clear();
                                break;

                            case 0x8:
                                channels[chl].chl_stat.back().note_pan = hexToDecimalOnes(cmdVar) - 7;
                                break;
                            
                            case 0xC:
                                channels[chl].setCutTick(hexToDecimalOnes(cmdVar));
                                break;
                            }
                        } else if (cmd == 'G') {
                            // ...
                        } else if (cmd == 'E') {
                            reset[chl] = false;
                            channels[chl].setToneDownSild(true, cmdVar);
                            // printf("ROW%d CHL%d TONEUP: %d\n", tracker_rows, chl, channels[chl].toneUpSildVar);
                        } else if (cmd == 'F') {
                            reset[chl] = false;
                            channels[chl].setToneUpSild(true, cmdVar);
                            // printf("ROW%d CHL%d TONEUP: %d\n", tracker_rows, chl, channels[chl].toneUpSildVar);
                        } else if (cmd == 'O') {
                            sampOfst[chl] = cmdVar;
                            if (!channels[chl].chl_stat.empty())
                                channels[chl].chl_stat.back().int_index = cmdVar;
                        } else if (cmd == 'X') {
                            if (!channels[chl].chl_stat.empty())
                                channels[chl].chl_stat.back().note_pan = cmdVar - 127;
                        } else {
                            printf("CHL%d->UNKNOW CMD: %c%02X\n", chl, cmd, cmdVar);
                        }
                    }
                    if (GET_INSTRUMENT(mask)) {
                        channels[chl].startNote(instTmp, reset[chl], sampOfst[chl]);
                    }
                    if (GET_VOLUME(mask)) {
                        channels[chl].setVolVal(volFlg, volVar);
                    }
                }
                //printf("%02d %03d: ", tracker_pats, tracker_rows);
                //for (uint8_t i = 0; i < 10; i++) {
                //    printf("%1d %03d %02d %02d %02d |", note_stat[i], now_note[i], note_inst[i], note_samp[i], note_vol[i]);
                //}
                //printf("\n");
                tracker_rows++;
                if (tracker_rows >= maxRowTable[tracker_pats]) {
                    skipNextPat(0);
                }
            }
            for (uint8_t chl = 0; chl < maxChannel; chl++) {
                // printf("NOW REFRUSH CHL %d\n", chl);
                channels[chl].refrush_note(tick);
                // printf("INST_VOL[%d] = %d NOTE_STAT[%d] = %d\n", chl, channels[chl].inst_vol, chl, channels[chl].note_stat);
            }
        }
        audio_stereo_32_t buftmp = {0, 0};
        for (uint16_t chl = 0; chl < maxChannel; chl++) {
            audio_stereo_32_t tmp = {0, 0};
            tmp = channels[chl].make_sound();
            buftmp.l += tmp.l *2;
            buftmp.r += tmp.r *2;
        }
        audioBuffer[bufferIndex] = buftmp;
        bufferIndex++;
        tempo_tick++;

        if (bufferIndex > BUFF_SIZE) {
            bufferIndex = 0;
            i2s_channel_write(i2s_tx_handle, audioBuffer, sizeof(audioBuffer), &writed, portMAX_DELAY);
            vTaskDelay(1);
        }
    }
    vTaskDelete(NULL);
}

void start_play_cmd(int argc, const char* argv[]) {
    printf("Starting PlayTask...\n");
    xTaskCreate(playTask, "PLAYTASK", 8192, NULL, 10, NULL);
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

void set_ticksrow_cmd(int argc, const char* argv[]) {
    if (argc < 2) {printf("set_ticksrow <num>\n"); return;}
    TicksRow = strtol(argv[1], NULL, 0);
    printf("TicksRow set to %d\n", TicksRow);
}

void debug_note_stat_cmd(int argc, const char* argv[]) {
    for (;;) {
        for (uint8_t i = 0; i < maxChannel; i++) {
            // printf("%d", note_stat[i]);
        }
        printf("\n");
        vTaskDelay(2);
        if (Serial.available()) {
            Serial.read();
            break;
        }
    }
}

void debug_note_map_cmd(int argc, const char* argv[]) {
    for (;;) {
        for (uint8_t i = 0; i < MAX_CHANNELS; i++) {
            // printf("%d", note_mapper[i]);
        }
        printf("\n");
        vTaskDelay(2);
        if (Serial.available()) {
            Serial.read();
            break;
        }
    }
}

void debug_actv_cmd(int argc, const char* argv[]) {
    if (argc < 2) {printf("%s <ChannelNum>\n", argv[0]);return;}
    uint8_t chl = strtol(argv[1], NULL, 0);
    if (chl == 255) {
        for (;;) {
            uint16_t sum = 0;
            for (uint8_t i = 0; i < maxChannel; i++) {
                sum += channels[i].chl_stat.size();
            }
            printf("ACTV NOTE: %d\n", sum);
            if (Serial.available()) {
                Serial.read();
                break;
            }
            vTaskDelay(2);
        }
    } else {
        for (;;) {
            printf("%d\n", channels[chl].chl_stat.size());
            if (Serial.available()) {
                Serial.read();
                break;
            }
            vTaskDelay(2);
        }
    }
}

void debug_pitsild_cmd(int argc, const char* argv[]) {
    if (argc < 2) {printf("%s <ChannelNum>\n", argv[0]);return;}
    uint8_t chl = strtol(argv[1], NULL, 0);
    if (chl == 255) {
        for (;;) {
            for (uint8_t i = 0; i < maxChannel; i++) {
                printf("%d", channels[i].toneUpSild);
            }
            printf("\n");
            if (Serial.available()) {
                Serial.read();
                break;
            }
            vTaskDelay(2);
        }
    } else {
        for (;;) {
            printf("%d\n", channels[chl].toneUpSild);
            if (Serial.available()) {
                Serial.read();
                break;
            }
            vTaskDelay(2);
        }
    }
}

void mainTask(void *arg) {
    SerialTerminal terminal;
    SPI.begin(17, -1, 16);
    display.begin(SSD1306_SWITCHCAPVCC);
    display.display();
    display.setTextColor(1);
    display.setTextSize(0);
    display.setTextWrap(true);
    terminal.begin(115200, "ESP32Tracker DEBUG");
    terminal.addCommand("reboot", reboot_cmd);
    terminal.addCommand("get_track", get_track);
    terminal.addCommand("play_smp", play_samp_cmd);
    terminal.addCommand("get_env", get_env_cmd);
    terminal.addCommand("get_env_itp", get_env_itp_cmd);
    terminal.addCommand("get_free_heap", get_free_heap_cmd);
    terminal.addCommand("start_play", start_play_cmd);
    terminal.addCommand("get_c5_speed", get_c5_speed_cmd);
    terminal.addCommand("get_speed_table", get_speed_table_cmd);
    terminal.addCommand("play_smp", play_samp_cmd);
    terminal.addCommand("set_ticksrow", set_ticksrow_cmd);
    terminal.addCommand("debug_note_stat", debug_note_stat_cmd);
    terminal.addCommand("debug_note_map", debug_note_map_cmd);
    terminal.addCommand("debug_actv", debug_actv_cmd);
    terminal.addCommand("debug_pitsild", debug_pitsild_cmd);
    // terminal.addCommand("play_chl", play_chl_cmd);
    // terminal.addCommand("get_heap_stat", get_heap_stat);
    // Open File
    // FILE *file = fopen("/spiffs/laamaa_-_bluesy.it", "rb");
    // FILE *file = fopen("/spiffs/laamaa_-_wb22-wk21.it", "rb");
    // FILE *file = fopen("/spiffs/atlantishighway.it", "rb");
    FILE *file = fopen("/spiffs/fod_absolutezerob.it", "rb");

    // Read Header
    display.clearDisplay();
    display.setCursor(0, 0);
    display.printf("Read Header...");
    display.display();
    read_it_header(file, &it_header);
    GlobalVol = it_header.GV;

    pause_serial();
    // Read Instrument
    it_instrument = (it_instrument_t*)malloc((it_header.InsNum + 1) * sizeof(it_instrument_t));
    inst_envelope = (it_unpack_envelope_t*)malloc((it_header.InsNum + 1) * sizeof(it_unpack_envelope_t));
    for (uint16_t inst = 0; inst < it_header.InsNum; inst++) {
        display.clearDisplay();
        display.setCursor(0, 0);
        display.printf("Instrument #%d...\n", inst+1);
        display.display();
        printf("Reading Instrument #%d...\n", inst+1);
        read_it_inst(file, it_header.InstOfst[inst], &it_instrument[inst+1]);
        printf("Unpack Envelope...\n");
        unpack_inst_env(&it_instrument[inst+1].volEnv, &inst_envelope[inst+1]);
        if (inst_envelope[inst+1].envelope == NULL) {
            printf("NULL ENV, SKIP!\n");
        } else {
            if (inst_envelope[inst+1].Num == 0) {
                printf("DISABLE ENV!\n");
            }
            printf("Envelope Data:\n");
            for (uint16_t p = 0; p < inst_envelope[inst+1].Num; p++) {
                printf("%d ", inst_envelope[inst+1].envelope[p]);
            }
            printf("\n");
        }
        // pause_serial();
    }

    // Read Samples
    it_samples = (it_sample_t*)malloc((it_header.SmpNum + 1) * sizeof(it_sample_t));
    for (uint16_t smp = 0; smp < it_header.SmpNum; smp++) {
        display.clearDisplay();
        display.setCursor(0, 0);
        display.printf("Reading Sample #%d...", smp+1);
        display.display();
        printf("Reading Sample #%d...\n", smp+1);
        read_it_sample(file, it_header.SampHeadOfst[smp], &it_samples[smp+1]);
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
        display.clearDisplay();
        display.setCursor(0, 0);
        display.printf("Unpack Pattern #%d...", i);
        display.display();
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
    xTaskCreate(mainTask, "MAINTASK", 20480, NULL, 4, NULL);
}

void loop() {
    printf("DELETE LOOP\n");
    vTaskDelete(NULL);
}