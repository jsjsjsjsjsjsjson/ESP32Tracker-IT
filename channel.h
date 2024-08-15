#include "it_file.h"
#include "audio_struct.h"
#include "it_config.h"
#include "extra_func.h"

extern it_sample_t *it_samples;
extern it_instrument_t *it_instrument;
extern uint8_t GlobalVol;

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
    uint8_t note_pan;
    uint8_t note_inst;
    uint8_t note_samp;
    uint8_t note_efct0;
    uint8_t note_efct1;
    int8_t volEnvVal;
    uint8_t inst_vol;
    uint8_t volNode;
    uint8_t panNode;
    uint8_t pitNode;
    float note_freq;
    float frac_index;
    uint32_t int_index;
    int16_t note_fade_comp;
    uint16_t vol_env_tick;
    uint16_t pan_env_tick;
    uint16_t pit_env_tick;
} chl_stat_t;

void applyPan(int32_t *left, int32_t *right, uint16_t pan) {
    int32_t temp_left = *left;
    int32_t temp_right = *right;
    
    if (pan < 32) {
        *right = (int16_t)((temp_right * pan) >> 5);
    } else if (pan > 32) {
        *left = (int16_t)((temp_left * (64 - pan)) >> 5);
    }
}

class Channel {
public:
    std::vector<chl_stat_t> chl_stat;
    uint8_t ChannelVol;
    uint8_t ChannelPan;
    uint8_t FV_SHOW;
    uint8_t chl_inst;
    uint8_t chl_note;

    bool enbVolSild;
    uint8_t volSildUpVar;
    uint8_t volSildDownVar;
    // chl_stat.pop_back();

    audio_stereo_32_t make_sound() {
        audio_stereo_32_t result_sum = {0, 0};
        for (uint8_t i = 0; i < chl_stat.size(); i++) {
            audio_stereo_32_t result = {0, 0};
            if (/*(chl_stat[i].note_stat == NOTE_NOACTV
                || chl_stat[i].note_vol == 0
                || chl_stat[i].inst_vol == 0
                || chl_stat[i].volEnvVal == 0
                || chl_stat[i].note_fade_comp == 0)
                || */(it_samples[chl_stat[i].note_samp].sample_data == NULL)) {
                // printf("CHL%02d: GlobalVol %d, note_stat %d, ChannelVol %d, vol %d, instVol %d, noteFadeComp %d, sampleVol %d\n", chl, GlobalVol, note_stat[chl], ChannelVol[chl], vol, instVol, noteFadeComp, it_samples[smp_num].Gvl);
                return result;
            }
            it_sample_t *sample = &it_samples[chl_stat[i].note_samp];
            /*
            uint64_t increment = (freq << 16) / SMP_RATE;
            frac_index[chl] += increment;
            int_index[chl] += frac_index[chl];
            frac_index[chl] &= 0xFFFF;
            */
            chl_stat[i].frac_index += chl_stat[i].note_freq / SMP_RATE;
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
            applyPan(&result.l, &result.r, ((uint16_t)ChannelPan + (uint16_t)chl_stat[i].note_pan) >> 1);
            result.l *= FV;
            result.r *= FV;

            result_sum.l += result.l;
            result_sum.r += result.r;
        }
        return result_sum;
    }

    void startNote(uint8_t note_in, uint8_t instNum, bool reset) {
        if (!instNum) return;
        chl_stat_t tmp;
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
        tmp.note_stat = NOTE_ON;
        tmp.note = note_in;
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

        tmp.frac_index = 0, tmp.int_index = 0;
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
        chl_note = note_in;
    }

    void setInst(uint8_t instNum, bool reset) {
        if (!instNum) return;
        startNote(chl_note, instNum, 1);
    }

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
            volSildUpVar = hexToDecimalTens(var);
            volSildDownVar = hexToDecimalOnes(var);
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

    void setVolVal(uint8_t volVal, bool reset) {
        if (chl_stat.empty()) {
            printf("WARNING: SET A EMPTY CHL\n");
        } else {
            char flg;
            uint8_t val;
            volCmdToRel(volVal, &flg, &val);
            if (flg == 'v') {
                chl_stat.back().note_vol = val;
            } else if (flg == 'p') {
                chl_stat.back().note_pan = val;
            }
        }
    }

    void setChanVol(uint8_t vol) {
        ChannelVol = vol;
    }

    void refrush_note() {
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
                if (vol_env.Flg.LoopOn) {
                    tmp->vol_env_tick++;
                    if (vol_env.Flg.SusLoopOn && tmp->note_stat == NOTE_ON) {
                        if (vol_env.SLB == vol_env.SLE) {
                            tmp->volNode = vol_env.SLB;
                            tmp->vol_env_tick = vol_env.envelope[vol_env.SLB].tick;
                        }
                        if (tmp->vol_env_tick >= vol_env.envelope[tmp->volNode+1].tick) {
                            tmp->vol_env_tick = 0;
                            tmp->volNode++;
                            if (tmp->volNode >= vol_env.SLE) {
                                tmp->volNode = vol_env.SLB;
                                tmp->vol_env_tick = vol_env.envelope[vol_env.SLB].tick;
                            }
                        }
                    } else {
                        if (vol_env.LpB == vol_env.LpE) {
                            tmp->volNode = vol_env.LpB;
                            tmp->vol_env_tick = vol_env.envelope[vol_env.LpB].tick;
                        }
                        if (tmp->vol_env_tick >= vol_env.envelope[tmp->volNode+1].tick) {
                            // tmp->vol_env_tick = 0;
                            tmp->volNode++;
                            if (tmp->volNode >= vol_env.LpE) {
                                tmp->volNode = vol_env.LpB;
                                tmp->vol_env_tick = vol_env.envelope[vol_env.LpB].tick;
                            }
                        }
                    }
                    // tmp->volEnvVal = 64;
                    tmp->volEnvVal = abs(LINEAR_INTERP(vol_env.envelope[tmp->volNode].tick, vol_env.envelope[tmp->volNode+1].tick, 
                                                vol_env.envelope[tmp->volNode].y, vol_env.envelope[tmp->volNode+1].y, tmp->vol_env_tick));
                } else {
                    tmp->vol_env_tick++;
                    if (vol_env.Flg.SusLoopOn && tmp->note_stat == NOTE_ON) {
                        if (vol_env.SLB == vol_env.SLE) {
                            tmp->volNode = vol_env.SLB;
                            tmp->vol_env_tick = vol_env.envelope[vol_env.SLB].tick;
                        }
                        if (tmp->vol_env_tick >= vol_env.envelope[tmp->volNode+1].tick) {
                            tmp->vol_env_tick = 0;
                            tmp->volNode++;
                            if (tmp->volNode >= vol_env.SLE) {
                                tmp->volNode = vol_env.SLB;
                                tmp->vol_env_tick = vol_env.envelope[vol_env.SLB].tick;
                            }
                        }
                    } else {
                        if (tmp->vol_env_tick >= vol_env.envelope[tmp->volNode+1].tick) {
                            tmp->volNode++;
                            if (tmp->volNode >= vol_env.Num - 2) {
                                tmp->volNode--;
                                tmp->vol_env_tick--;
                                tmp->note_stat = NOTE_OFF;
                            } else {
                                tmp->vol_env_tick = 0;
                            }
                        }
                    }
                    tmp->volEnvVal = abs(LINEAR_INTERP(vol_env.envelope[tmp->volNode].tick, vol_env.envelope[tmp->volNode+1].tick, 
                                                vol_env.envelope[tmp->volNode].y, vol_env.envelope[tmp->volNode+1].y, tmp->vol_env_tick));
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
                } else {
                    tmp->note_fade_comp = 0;
                    tmp->note_stat = NOTE_NOACTV;
                    clearBeginNote(i);
                }
            }
        }

        // refrush Effect

        if (volSildDownVar) {
            volSildDown(volSildDownVar);
        } else if (volSildUpVar) {
            volSildUp(volSildUpVar);
        }
    }
};
