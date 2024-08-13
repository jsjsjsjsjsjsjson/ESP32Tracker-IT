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
    uint8_t note;
    uint8_t note_vol;
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
    uint16_t vol_env_point;
    uint16_t pan_env_point;
    uint16_t pit_env_point;
} chl_stat_t;

class Channel {
public:
    std::vector<chl_stat_t> chl_stat;
    uint8_t ChannelVol;
    uint8_t ChannelPan;
    uint8_t FV_SHOW;
    uint8_t chl_inst;
    uint8_t chl_note;
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
                    chl_stat[i].int_index = sample->LoopBegin;
                    chl_stat[i].frac_index = 0;
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
        instNum --;
    }

    void setInst(uint8_t instNum, bool reset) {
        if (!instNum) return;

    }

    void offNote() {

    }

    void fadeNote() {

    }

    void cutNote() {

    }

    void clearBeginNote() {

    }

    void setVolVal(uint8_t volVal, bool reset) {

    }

    void setChanVol(uint8_t vol) {
        ChannelVol = vol;
    }

    void refrush_note() {

    }
};
