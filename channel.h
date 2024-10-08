#include "it_file.h"
#include "audio_struct.h"
#include "it_config.h"
#include "extra_func.h"

extern it_sample_t *it_samples;
extern it_instrument_t *it_instrument;
extern uint8_t GlobalVol;
extern it_unpack_envelope_t *inst_envelope;

typedef enum __attribute__((packed)) {
    NOTE_NOACTV, // 通道没有任何活动（FV == 0）
    NOTE_ON, // 通道正在播放
    NOTE_OFF, // 通道关闭中
    NOTE_FADE // 通道渐出中
} note_stat_t;

typedef struct {
    note_stat_t note_stat;
    new_note_activ_t nna;
    uint8_t note;
    int8_t note_vol;
    int16_t note_pan;
    uint8_t note_inst;
    uint8_t note_samp;
    uint8_t note_efct0;
    uint8_t note_efct1;
    int8_t volEnvVal;
    uint8_t inst_vol;
    uint8_t volNode;
    uint8_t panNode;
    uint8_t pitNode;
    uint32_t note_freq;
    float frac_index;
    uint32_t int_index;
    int16_t note_fade_comp;
    uint16_t vol_env_tick;
    uint16_t pan_env_tick;
    uint16_t pit_env_tick;
} chl_stat_t;

inline void applyPan(int32_t *left, int32_t *right, int16_t pan) {
    int32_t temp_left = *left;
    int32_t temp_right = *right;

    if (pan < 0) {
        // 将pan的范围从-128~0映射到64~0，0表示完全左声道
        *right = (int32_t)((temp_right * (128 + pan)) >> 7);
    } else if (pan > 0) {
        // 将pan的范围从0~128映射到0~64，0表示完全右声道
        *left = (int32_t)((temp_left * (128 - pan)) >> 7);
    }
    // pan == 0时，不做任何处理，保持原样
}

const uint8_t volCmdPortToneToRelPortToneLUT[10] = {0x00, 0x01, 0x04, 0x08, 0x10, 0x20, 0x40, 0x60, 0x80, 0xff};

class Channel {
public:
    uint8_t num;

    std::vector<chl_stat_t> chl_stat;
    uint8_t ChannelVol;
    int8_t ChannelPan;
    uint8_t FV_SHOW;
    uint8_t chl_inst;
    uint8_t chl_note;

    bool enbVolSild = false;
    uint8_t volSildUpVar = 0;
    uint8_t volSildDownVar = 0;

    bool tonePort = false;

    uint32_t tonePortSetStat = 0;
    uint32_t sourceFreq = 0;
    uint8_t tonePortSource = 0;
    uint8_t tonePortTarget = 0;
    uint8_t tonePortSpeed = 0;

    uint32_t toneSildFreq = 0;

    bool toneUpSild = false;
    uint8_t toneUpSildVar = 0;

    bool toneDownSild = false;
    uint8_t toneDownSildVar = 0;
    // chl_stat.pop_back();

    bool delayCut = false;
    uint8_t noteCutTick = 0;

    audio_stereo_32_t make_sound() {
        audio_stereo_32_t result_sum = {0, 0};
        if (ChannelVol == 0 || GlobalVol == 0) return result_sum;
        for (uint8_t i = 0; i < chl_stat.size(); i++) {
            audio_stereo_32_t result = {0, 0};
            if ((chl_stat[i].note_stat == NOTE_NOACTV
                || chl_stat[i].note_vol == 0
                || chl_stat[i].inst_vol == 0
                || chl_stat[i].volEnvVal == 0
                || chl_stat[i].note_fade_comp == 0)
                || (it_samples[chl_stat[i].note_samp].sample_data == NULL)) {
                FV_SHOW = 0;
                // printf("CHL%02d: GlobalVol %d, note_stat %d, ChannelVol %d, vol %d, instVol %d, noteFadeComp %d, sampleVol %d\n", chl, GlobalVol, note_stat[chl], ChannelVol[chl], vol, instVol, noteFadeComp, it_samples[smp_num].Gvl);
                continue;
            }
            it_sample_t *sample = &it_samples[chl_stat[i].note_samp];
            /*
            uint64_t increment = (freq << 16) / SMP_RATE;
            frac_index[chl] += increment;
            int_index[chl] += frac_index[chl];
            frac_index[chl] &= 0xFFFF;
            */
            chl_stat[i].frac_index += chl_stat[i].note_freq / (float)SMP_RATE;
            if (chl_stat[i].frac_index >= 1.0f) {
                chl_stat[i].int_index += (int)chl_stat[i].frac_index; // Increment the integer index by the whole part of frac_index
                chl_stat[i].frac_index -= (int)chl_stat[i].frac_index; // Keep only the fractional part
            }

            if (sample->Flg.useLoop || sample->Flg.pingPongLoop) {
                if (chl_stat[i].int_index >= sample->LoopEnd) {
                    chl_stat[i].int_index -= (sample->LoopEnd - sample->LoopBegin);
                    // chl_stat[i].frac_index = 0;
                }
            } else if (chl_stat[i].int_index > sample->Length) {
                chl_stat[i].note_stat = NOTE_NOACTV;
                return result;
            }

            uint32_t idx = chl_stat[i].int_index;

            if (sample->Flg.use16Bit) {
                if (sample->Flg.stereo) {
                    audio_stereo_16_t dataTmp = GET_SAMPLE_DATA(sample, idx, audio_stereo_16_t);
                    result = (audio_stereo_32_t){(int32_t)dataTmp.l, (int32_t)dataTmp.r};
                } else {
                    audio_mono_16_t dataTmp = GET_SAMPLE_DATA(sample, idx, audio_mono_16_t);
                    result = (audio_stereo_32_t){(int32_t)dataTmp, (int32_t)dataTmp};
                }
            } else {
                if (sample->Flg.stereo) {
                    audio_stereo_8_t dataTmp = GET_SAMPLE_DATA(sample, idx, audio_stereo_8_t);
                    result = (audio_stereo_32_t){(int32_t)dataTmp.l << 8, (int32_t)dataTmp.r << 8};
                } else {
                    audio_mono_8_t dataTmp = GET_SAMPLE_DATA(sample, idx, audio_mono_8_t);
                    result = (audio_stereo_32_t){(int32_t)dataTmp << 8, (int32_t)dataTmp << 8};
                }
            }
            uint16_t FV = ((uint64_t)chl_stat[i].note_vol *
                    (uint64_t)sample->Gvl *
                    (uint64_t)chl_stat[i].inst_vol *
                    (uint64_t)ChannelVol *
                    (uint64_t)GlobalVol *
                    (uint64_t)chl_stat[i].volEnvVal *
                    (uint64_t)chl_stat[i].note_fade_comp) >> 38;
            FV_SHOW = FV >> 4;

            // printf("GlobalVol: %d, note_stat: %d, ChannelVol: %d, vol: %d, instVol: %d, noteFadeComp %d, sampleVol: %d, FV: %d\n", GlobalVol, note_stat[chl], ChannelVol[chl], vol, instVol, noteFadeComp, it_samples[smp_num].Gvl, FV);
            applyPan(&result.l, &result.r, chl_stat[i].note_pan);
            result.l *= FV;
            result.r *= FV;

            result_sum.l += result.l;
            result_sum.r += result.r;
        }
        return result_sum;
    }

    void startNote(uint8_t instNum, bool reset, uint16_t sampOfst) {
        if (!instNum) return;
        if (reset || chl_stat.empty()) {
            new_note_activ_t last_nna;
            if (!chl_stat.empty()) {
                if (chl_stat.back().note_inst) {
                    last_nna = it_instrument[chl_stat.back().note_inst].NNA;
                } else {
                    last_nna = NNA_CUT;
                }
            } else {
                last_nna = NNA_CUT;
            }
            chl_stat_t tmp;
            tmp.note_stat = NOTE_ON;
            tmp.note = chl_note;
            tmp.note_fade_comp = 1024;
            tmp.note_inst = instNum;
            tmp.note_samp = it_instrument[instNum].noteToSampTable[tmp.note].sample;
            tmp.note_freq = it_samples[tmp.note_samp].speedTable[tmp.note];
            tmp.note_vol = it_samples[tmp.note_samp].Vol;
            tmp.inst_vol = it_instrument[instNum].GbV;
            tmp.note_pan = 32;
            if (it_samples[tmp.note_samp].DfP & 128)
                tmp.note_pan = it_samples[tmp.note_samp].DfP - 128;
            if (!it_instrument[instNum].DfP & 128)
                tmp.note_pan = it_instrument[instNum].DfP - 128;

            tmp.frac_index = 0, tmp.int_index = sampOfst;
            tmp.volNode = 0, tmp.vol_env_tick = 0;
            if (chl_stat.empty()) {
                chl_stat.push_back(tmp);
            } else {
                if (last_nna == NNA_CUT) {
                    chl_stat.back() = tmp;
                } else {
                    if (last_nna == NNA_CONTINUE) {
                        chl_stat.push_back(tmp);
                    } else if (last_nna == NNA_NOTEFADE) {
                        chl_stat.back().note_stat = NOTE_FADE;
                        chl_stat.push_back(tmp);
                    } else if (last_nna == NNA_NOTEOFF) {
                        chl_stat.back().note_stat = NOTE_OFF;
                        chl_stat.push_back(tmp);
                    }
                }
            }
        } else {
            chl_stat_t tmp;
            if (!chl_stat.empty()) {
                chl_stat_t* tmp = &chl_stat.back();
                tmp->note_stat = NOTE_ON;
                tmp->note = chl_note;
                tmp->note_freq = it_samples[tmp->note_samp].speedTable[tmp->note];
                tmp->note_fade_comp = 1024;
                tmp->note_inst = instNum;
                tmp->note_samp = it_instrument[instNum].noteToSampTable[tmp->note].sample;
                tmp->note_vol = it_samples[tmp->note_samp].Vol;
                tmp->inst_vol = it_instrument[instNum].GbV;
                tmp->note_pan = 32;
                if (it_samples[tmp->note_samp].DfP & 128)
                    tmp->note_pan = it_samples[tmp->note_samp].DfP - 128;
                if (!it_instrument[instNum].DfP & 128)
                    tmp->note_pan = it_instrument[instNum].DfP - 128;
            }
        }
    }

    void changeNote(uint8_t note) {
        chl_note = note;
        if (!chl_stat.empty()) {
            chl_stat.back().note = note;
            chl_stat.back().note_freq = it_samples[chl_stat.back().note_samp].speedTable[note];
        }
    }

    /*
    void setInst(uint8_t instNum, bool reset) {
        if (!instNum) return;
        startNote(chl_note, instNum, reset);
    }
    */

    void offNote() {
        if (it_instrument[chl_inst].NNA == NNA_CUT) {
            if (!chl_stat.empty()) chl_stat.pop_back();
        } else {
            chl_stat.back().note_stat = NOTE_OFF;
        }
    }

    void fadeNote() {
        chl_stat.back().note_stat = NOTE_FADE;
    }

    void setVolSild(bool stat, uint8_t var) {
        enbVolSild = stat;
        if (stat && var) {
            uint8_t x = (var >> 4) & 0xF;
            uint8_t y = var & 0xF;
            if (x == 0x0 && y != 0x0) {
                volSildDownVar = y;
            } else if (x != 0x0 && y == 0x0) {
                volSildUpVar = x;
            } else if (x == 0xF && y != 0xF) {
                enbVolSild = false;
                volSildDown(y);
            } else if (x != 0xF && y == 0xF) {
                enbVolSild = false;
                volSildUp(x);
            } else {
                enbVolSild = false;
            }
        }
    }

    void volSildDown(uint8_t val) {
        if (chl_stat.empty()) {
            printf("WARNING: SET A EMPTY CHL\n");
        } else {
            chl_stat.back().note_vol -= val;
            if (chl_stat.back().note_vol < 0) {
                chl_stat.back().note_vol = 0;
            }
        }
    }

    void volSildUp(uint8_t val) {
        if (chl_stat.empty()) {
            printf("WARNING: SET A EMPTY CHL\n");
        } else {
            chl_stat.back().note_vol += val;
            if (chl_stat.back().note_vol > 64) {
                chl_stat.back().note_vol = 64;
            }
        }
    }

    void cutNote() {
        if (chl_stat.empty()) {
            printf("WARNING: CUT A EMPTY CHL\n");
        } else {
            chl_stat.pop_back();
        }
    }

    void clearBeginNote(uint8_t index) {
        if (chl_stat.empty()) {
            printf("WARNING: CLEAR A EMPTY CHL\n");
        } else {
            chl_stat.erase(chl_stat.begin() + index);
        }
    }

    void setVolVal(char flg, uint8_t var) {
        if (chl_stat.empty()) {
            printf("WARNING: SET A EMPTY CHL\n");
        } else {
            if (flg == 'v') {
                chl_stat.back().note_vol = var;
            } else if (flg == 'p') {
                chl_stat.back().note_pan = var - 32;
            } else if (flg == 'c') {
                enbVolSild = true;
                volSildUpVar = var;
            } else if (flg == 'd') {
                enbVolSild = true;
                volSildDownVar = var;
            } else if (flg == 'a') {
                volSildUp(var);
            } else if (flg == 'b') {
                volSildDown(var);
            } else if (flg == 'g') {
                // ...
            } else {
                printf("CHL%d->UNKNOW VOLCMD: %c%02d\n", num, flg, var);
            }
        }
    }

    void setChanVol(uint8_t vol) {
        ChannelVol = vol;
    }

    void setPortTone(bool stat, uint8_t speed) {
        tonePort = stat;
        if (speed)
            tonePortSpeed = speed;
    }

    void setPortSource(uint8_t note) {
        tonePortSource = note;
        if (!chl_stat.empty())
            tonePortSetStat = chl_stat.back().note_freq;
    }

    void setToneUpSild(bool stat, uint8_t var) {
        toneUpSild = stat;
        if (var) {
            toneUpSildVar = var;
        }
        /*
        if (stat) {
            if (toneUpSildVar > 239) {
                if (!chl_stat.empty()) {
                    printf("FINE VOL UP: %d -> ", chl_stat.back().note_freq);
                    chl_stat.back().note_freq *= powf(2, (toneUpSildVar - 240) / 192.0f);
                    printf("%d\n", chl_stat.back().note_freq);
                }
                toneUpSild = false;
            }
        }
        */
    }

    void setToneDownSild(bool stat, uint8_t var) {
        toneDownSild = stat;
        if (var) {
            toneDownSildVar = var;
        }
        /*
        if (stat) {
            if (toneDownSildVar > 239) {
                if (!chl_stat.empty()) {
                    printf("FINE VOL DOWN: %d -> ", chl_stat.back().note_freq);
                    chl_stat.back().note_freq *= powf(2, -(toneUpSildVar - 240) / 192.0f);
                    printf("%d\n", chl_stat.back().note_freq);
                }
                toneDownSild = false;
            }
        }
        */
    }

    void setPortTarget(uint8_t note) {
        tonePortTarget = note;
        if (tonePortTarget == tonePortSource && tonePortSetStat) {
            // printf("SET TARGET = SOURCE\n");
            sourceFreq = tonePortSetStat;
        }
        tonePortSetStat = 0;
    }

    void setCutTick(uint8_t tick) {
        delayCut = true;
        noteCutTick = tick;
    }

    void refrush_note(uint32_t Gtick) {
        if (chl_stat.empty()) return;
        // printf("ACTV CHL: EMPTY=%d SIZE=%zu\n", chl_stat.empty(), chl_stat.size());
        for (uint8_t i = 0; i < chl_stat.size(); i++) {
            if (chl_stat[i].note_stat == NOTE_NOACTV) {
                clearBeginNote(i);
                continue;
            }
            chl_stat_t *tmp = &chl_stat[i];
            it_inst_envelope_t vol_env = it_instrument[tmp->note_inst].volEnv;
            if (vol_env.Flg.EnvOn) {
                tmp->volEnvVal = inst_envelope[tmp->note_inst].envelope[tmp->vol_env_tick];
                tmp->vol_env_tick++;
                if (vol_env.Flg.LoopOn) {
                    if (tmp->note_stat == NOTE_ON && vol_env.Flg.SusLoopOn) {
                        if (tmp->vol_env_tick >= inst_envelope[tmp->note_inst].SLE) {
                            tmp->vol_env_tick = inst_envelope[tmp->note_inst].SLB;
                        }
                    } else {
                        if (tmp->vol_env_tick >= inst_envelope[tmp->note_inst].LpE) {
                            tmp->vol_env_tick = inst_envelope[tmp->note_inst].LpB;
                        }
                    }
                } else {
                    if (tmp->note_stat == NOTE_ON && vol_env.Flg.SusLoopOn) {
                        if (tmp->vol_env_tick >= inst_envelope[tmp->note_inst].SLE) {
                            tmp->vol_env_tick = inst_envelope[tmp->note_inst].SLB;
                        }
                    } else {
                        if (tmp->vol_env_tick >= inst_envelope[tmp->note_inst].Num) {
                            tmp->vol_env_tick--;
                            tmp->note_stat = NOTE_OFF;
                        }
                    }
                }
            } else {
                tmp->volEnvVal = 64;
            }
            if (tmp->note_stat == NOTE_OFF || tmp->note_stat == NOTE_FADE) {
                if (it_instrument[tmp->note_inst].FadeOut) {
                    // printf("NOTE FADE COMP %d - %d = ", tmp->note_fade_comp, it_instrument[tmp->note_inst].FadeOut);
                    tmp->note_fade_comp -= it_instrument[tmp->note_inst].FadeOut;
                    // printf("%d\n", tmp->note_fade_comp);
                    if (tmp->note_fade_comp < 0) {
                        tmp->note_fade_comp = 0;
                        tmp->note_stat = NOTE_NOACTV;
                        clearBeginNote(i);
                        // printf("CHL CLEAR NOTE %d\n", i);
                    }
                }/* else {
                    tmp->note_fade_comp = 0;
                    tmp->note_stat = NOTE_NOACTV;
                    clearBeginNote(i);
                }*/
            }
        }

        // refrush Effect
        if (delayCut) {
            if (noteCutTick) {
                noteCutTick--;
            } else {
                delayCut = false;
                cutNote();
            }
        }

        if (Gtick) {
            if (enbVolSild) {
                if (volSildDownVar) {
                    volSildDown(volSildDownVar);
                } else if (volSildUpVar) {
                    volSildUp(volSildUpVar);
                }
            }

            if (tonePort) {
                if (!chl_stat.empty()) {
                    uint32_t freq = chl_stat.back().note_freq;
                    if (tonePortSource != tonePortTarget)
                        sourceFreq = it_samples[chl_stat.back().note_samp].speedTable[tonePortSource];
                    uint32_t targetFreq = it_samples[chl_stat.back().note_samp].speedTable[tonePortTarget];
                    if (tonePortSource > tonePortTarget) {
                        // printf("TONEPORT-: SOURCE %d * %f = ", freq, powf(2, -tonePortSpeed / 192.0f));
                        if (freq > targetFreq) {
                            freq *= powf(2, -tonePortSpeed / 192.0f);
                        } else {
                            freq = targetFreq;
                        }
                        // printf("%d\n", freq);
                    } else if (tonePortSource < tonePortTarget) {
                        // printf("TONEPORT+: SOURCE %d * %f = ", freq, powf(2, tonePortSpeed / 192.0f));
                        if (freq < targetFreq) {
                            freq *= powf(2, tonePortSpeed / 192.0f);
                        } else {
                            freq = targetFreq;
                        }
                        // printf("%d\n", freq);
                    } else {
                        // printf("TONEPORT: TARGET = SOURCE!\n");
                        if (sourceFreq > targetFreq) {
                            // printf("TONEPORT-: SOURCE %d * %f = ", freq, powf(2, -tonePortSpeed / 192.0f));
                            if (freq > targetFreq) {
                                freq *= powf(2, -tonePortSpeed / 192.0f);
                            } else {
                                freq = targetFreq;
                            }
                            // printf("%d\n", freq);
                        } else if (sourceFreq < targetFreq) {
                            // printf("TONEPORT+: SOURCE %d * %f = ", freq, powf(2, tonePortSpeed / 192.0f));
                            if (freq < targetFreq) {
                                freq *= powf(2, tonePortSpeed / 192.0f);
                            } else {
                                freq = targetFreq;
                            }
                            // printf("%d\n", freq);
                        }
                    }
                    chl_stat.back().note_freq = freq;
                }
            }

            if (toneUpSild) {
                if (!chl_stat.empty())
                    chl_stat.back().note_freq *= powf(2, toneUpSildVar / 192.0f);
            }

            if (toneDownSild) {
                if (!chl_stat.empty())
                    chl_stat.back().note_freq *= powf(2, -toneDownSildVar / 192.0f);
            }
        }
    }
};
