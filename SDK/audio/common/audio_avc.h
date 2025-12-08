#ifndef _AUDIO_AVC_H_
#define _AUDIO_AVC_H_

#define THR_LVL_MAX_NUM             16
#define VOL_LVL_MAX_NUM             16

//AVC使能回声消除
#define AVC_USE_AEC                 1              //与通话回声消除互斥
#define AVC_AEC_CLOCK               (96 * 1000000) //启动回声消除后所设置的最小系统时钟

//AVC检测算法每帧运算长度
#define ALGO_RUN_FRAME_LEN          1024      //byte

//AVC算法类型选择
#define ICSD_AVC_ALGO_RUN           BIT(0)    //ICSD 算法，需要开启ANC宏，与环境自适应(标准版)互斥
#define ENERGY_DETECT_RUN           BIT(1)    //能量检测
#define AVC_ALGO_RUN_TYPE           ICSD_AVC_ALGO_RUN

//ICSD AVC算法版本选择：0 NORAML 1 LITE
#define ICSD_AVC_ALGO_TYPE          0

//ADC采样率
#define AVC_ADC_SAMPLE_RATE         16000

struct avc_tool_common_param {
    int is_bypass;
    int thr_lvl_num;
    int vol_lvl_num;
    int thr_debounce;
    int default_lvl;
    int lvl_up_hold_time;
    int lvl_down_hold_time;
    int vol_offset_fade_time;
    int high_lvl_sync;
    int aec_en;
    int nlp_en;
    float aec_dt_aggress;   //原音回音追踪等级, default: 1.0f(1 ~ 5)
    float aec_refengthr;    //进入回音消除参考值, default: -70.0f(-90 ~ -60 dB)
    float es_aggress_factor;//回音前级动态压制,越小越强,default: -3.0f(-1 ~ -5)
    float es_min_suppress;  //回音后级静态压制,越大越强,default: 4.f(0 ~ 10)
};

struct avc_tool_table_param {
    int thr_len;
    int *thr_table;
    int vol_len;
    s8 *vol_table; //len(u8) + 音量档位1 + len(u8) + 音量档位2 ......
};

struct avc_param_tool_set {
    struct avc_tool_common_param common_param;
    struct avc_tool_table_param table_param;
};

int audio_avc_aec_data_fill(s16 *data, u16 len);
int audio_avc_aec_open();
int audio_avc_aec_close();

#endif
