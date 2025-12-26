#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".ns_node.data.bss")
#pragma data_seg(".ns_node.data")
#pragma const_seg(".ns_node.text.const")
#pragma code_seg(".ns_node.text")
#endif

#include "jlstream.h"
#include "audio_config.h"
#include "media/audio_base.h"
#include "audio_ns.h"
#include "btstack/avctp_user.h"
#include "effects/effects_adj.h"
#include "frame_length_adaptive.h"
#include "app_config.h"
#include "overlay_code.h"
#include "tws_frame_sync.h"
#include "classic/tws_api.h"
#include "effects/convert_data.h"

#if 1
#define ns_log	printf
#else
#define ns_log(...)
#endif/*log_en*/

#if TCFG_NS_NODE_ENABLE || TCFG_NS_NODE_LITE_ENABLE

extern void aec_code_movable_load(void);
extern void aec_code_movable_unload(void);

#define ESCO_DL_TWS_NS_NB_ENABLE    0

extern const int CONFIG_BTCTLER_TWS_ENABLE;
enum {
    AUDIO_NS_TYPE_ESCO_DL = 1,	//下行降噪类型
    AUDIO_NS_TYPE_GENERAL,		//通用类型
};

#define TWS_NS_STATE_START          0
#define TWS_NS_STATE_WAIT           1
#define TWS_NS_STATE_RESET          2
#define TWS_NS_STATE_RESTART        3
#define TWS_NS_STATE_WAIT_RESTART   4

struct ns_cfg_t {
    u8 bypass;//是否跳过节点处理
    u8 ns_type;//降噪类型选择，通用降噪/通话下行降噪
    u8 call_active_trigger;//接通电话触发标志, 只有通话下行降噪会使用
    u8 mode;               //ans降噪模式(0,1,2:越大越耗资源，效果越好),dns降噪时，用于选择dns算法
    float aggressfactor;   //降噪强度(越大越强:1~2)
    float minsuppress;     //降噪最小压制(越小越强:0~1)
    float noiselevel;      //初始噪声水平(评估初始噪声，加快收敛)
    float eng_gain;        //设置线性值
    float output16; 			//算法内部是否使用16转24保留精度,使用节点后需要24BIT
} __attribute__((packed));

struct ns_node_hdl {
    char name[16];
    u8 bt_addr[6];
    u8 trigger;
    u32 sample_rate;
    void *ns;
    u8 lite;
    struct stream_frame *out_frame;
    struct ns_cfg_t cfg;
    struct fixed_frame_len_handle *fixed_hdl;
    struct node_port_data_wide data_wide;
    u8 scene;
    u8 tws_ns_state;
    u8 frame_interval;
    u16 ns_frame_len;
    u16 ns_frame_offset;
    u32 frame_clkn;
    u8 *ns_frame_buf;
    void *tws_hdl;
};

extern int db2mag(int db, int dbQ, int magDQ);//10^db/20
int ns_param_cfg_read(struct stream_node *node)
{
    struct ns_cfg_t config;
    struct ns_node_hdl *hdl = (struct ns_node_hdl *)node->private_data;
    int ret = 0;
    if (!hdl) {
        return -1 ;
    }
    /*
     *获取配置文件内的参数,及名字
     * */
    ret = jlstream_read_node_data_new(NODE_UUID_NOISE_SUPPRESSOR, node->subid, (void *)&config, hdl->name);
    if (ret != sizeof(config)) {
        ret = jlstream_read_node_data_new(NODE_UUID_NOISE_SUPPRESSOR_LITE, node->subid, (void *)&config, hdl->name);
        if (ret != sizeof(config)) {
            printf("%s: read node data failed, ret = %d\n", __func__, ret);
        }
    }
    /*
     *获取在线调试的临时参数
     * */
    if (config_audio_cfg_online_enable) {
        if (jlstream_read_effects_online_param(hdl_node(hdl)->uuid, hdl->name, &config, sizeof(config))) {
            printf("get ans online param succ\n");
        }
    }

    hdl->cfg = config;

    ns_log("bypass %d\n", hdl->cfg.bypass);
    ns_log("type %d\n", hdl->cfg.ns_type);
    ns_log("call_active_trigger %d\n", hdl->cfg.call_active_trigger);
    ns_log("mode                %d\n", hdl->cfg.mode);
    ns_log("aggressfactor  %d/1000\n", (int)(hdl->cfg.aggressfactor * 1000.f));
    ns_log("minsuppress    %d/1000\n", (int)(hdl->cfg.minsuppress * 1000.f));
    ns_log("noiselevel     %d/1000\n", (int)(hdl->cfg.noiselevel * 1000.f));
    ns_log("eng_gain       %d\n", (int)(hdl->cfg.eng_gain));
    ns_log("output16 %d\n", (int)(hdl->cfg.output16));

    return ret;
}

#if 0
static int ns_node_fixed_frame_run(void *priv, u8 *in, u8 *out, int len)
{
    int wlen = 0;
    struct ns_node_hdl *hdl = (struct ns_node_hdl *)priv;
    if (hdl->cfg.bypass) {
        if (hdl->ns) {
            audio_ns_close(hdl->ns);
            hdl->ns = NULL;
        }
        memcpy(out, in, len);
        return len;
    } else {
        if (!hdl->ns) {
            hdl->ns = audio_ns_open(hdl->sample_rate, hdl->cfg.mode, hdl->cfg.noiselevel, hdl->cfg.aggressfactor, hdl->cfg.minsuppress, hdl->lite, hdl->cfg.eng_gain, hdl->cfg.output16);
        }
        if (hdl->ns) {
            wlen = audio_ns_run(hdl->ns, (s16 *)in, (void *)out, len);
        }
    }
    return wlen;
}
#endif

static int tws_esco_ns_node_frame_detect(struct ns_node_hdl *hdl)
{
    if (CONFIG_BTCTLER_TWS_ENABLE) {
        if (hdl->cfg.ns_type == AUDIO_NS_TYPE_ESCO_DL && hdl->scene == STREAM_SCENE_ESCO) {
            if (!hdl->tws_hdl) {
                return 0;
            }
            int ret = tws_audio_frame_sync_detect(hdl->tws_hdl, hdl->frame_clkn);
            if (ret == TWS_FRAME_SYNC_START) {
                if (hdl->tws_ns_state == TWS_NS_STATE_WAIT_RESTART) {
                    hdl->tws_ns_state = TWS_NS_STATE_RESTART;
                }
            }
            hdl->frame_clkn = (hdl->frame_clkn + hdl->frame_interval) & 0x7ffffff;
        }
    }

    return 0;
}

static int tws_esco_ns_node_restart_detect(struct ns_node_hdl *hdl, struct stream_frame *in_frame, struct stream_frame *out_frame)
{
    if (CONFIG_BTCTLER_TWS_ENABLE) {
        if (hdl->cfg.ns_type == AUDIO_NS_TYPE_ESCO_DL && hdl->scene == STREAM_SCENE_ESCO) {

            if (hdl->tws_ns_state == TWS_NS_STATE_RESTART) {
                ASSERT(in_frame->offset == 0, " restart in_frame : %d, %d\n", in_frame->offset, in_frame->len);
                /*与前帧数据拼接处压制系数为1.0f*/
                hdl->ns = audio_ns_open(hdl->sample_rate, hdl->cfg.mode, hdl->cfg.noiselevel, hdl->cfg.aggressfactor, 1.0f/*hdl->cfg.minsuppress*/, hdl->lite, hdl->cfg.eng_gain, hdl->cfg.output16);
                audio_ns_run(hdl->ns, (s16 *)hdl->ns_frame_buf, (void *)hdl->ns_frame_buf, hdl->ns_frame_len); //空跑一次进行NS算法延时数据的刷出
                hdl->ns_frame_offset = 0;
                hdl->tws_ns_state = TWS_NS_STATE_START;
                ns_log("tws ns restart : %d, %d\n", in_frame->offset, in_frame->len);
                return 1;
            } else {
                /*将NS 帧buffer中数据以in frame长度进行排出，并维持NS 帧buffer一帧状态*/
                if (!hdl->cfg.output16) { //算法使用16转24bit，这里仍需要延续算法处理
                    audio_convert_data_16bit_to_32bit_round((s16 *)hdl->ns_frame_buf, (s32 *)out_frame->data, in_frame->len >> 1);
                } else {
                    memcpy(out_frame->data, hdl->ns_frame_buf, in_frame->len);
                }
                hdl->ns_frame_offset = hdl->ns_frame_len - in_frame->len;
                memcpy(hdl->ns_frame_buf, hdl->ns_frame_buf + in_frame->len, hdl->ns_frame_offset);
                memcpy(hdl->ns_frame_buf + hdl->ns_frame_offset, in_frame->data, in_frame->len);
                hdl->ns_frame_offset = hdl->ns_frame_len;
                in_frame->offset += in_frame->len;
                if (!hdl->cfg.output16) {
                    out_frame->len = in_frame->len * 2;
                } else {
                    out_frame->len = in_frame->len;
                }
                /*putchar('N');*/
            }
            return 0;
        }
    }
    return -1;
}

static int tws_esco_ns_node_reset_detect(struct ns_node_hdl *hdl)
{
    if (CONFIG_BTCTLER_TWS_ENABLE) {
        if (hdl->cfg.ns_type == AUDIO_NS_TYPE_ESCO_DL && hdl->scene == STREAM_SCENE_ESCO) {
            if (hdl->ns_frame_offset == 0 && hdl->tws_hdl) {
                /*in frame与ns算法帧已对齐*/
                if (tws_frame_sync_reset_enable(hdl->tws_hdl, hdl->frame_clkn)) {
                    audio_ns_close(hdl->ns);
                    hdl->ns = NULL;
                    hdl->tws_ns_state = TWS_NS_STATE_WAIT_RESTART;
                    /*NS算法内延时一次ns_frame_len，通过改缓冲进行补偿*/
                    hdl->ns_frame_offset = hdl->ns_frame_len;
                }
            }
        }
    }
    return 0;
}

/*节点输出回调处理，可处理数据或post信号量*/
static void ns_handle_frame(struct stream_iport *iport, struct stream_note *note)
{
    struct ns_node_hdl *hdl = (struct ns_node_hdl *)iport->node->private_data;
    struct stream_node *node = iport->node;
    struct stream_frame *in_frame;
    int wlen;
    int out_frame_len;

    while (1) {
        in_frame = jlstream_pull_frame(iport, note);		//从iport读取数据
        if (!in_frame) {
            break;
        }

        if (CONFIG_BTCTLER_TWS_ENABLE && hdl->cfg.bypass) {
            if (hdl->cfg.ns_type == AUDIO_NS_TYPE_ESCO_DL && hdl->scene == STREAM_SCENE_ESCO) {
                jlstream_push_frame(node->oport, in_frame);
                break;
            }
        }

        if ((hdl->cfg.ns_type == AUDIO_NS_TYPE_ESCO_DL) && hdl->cfg.call_active_trigger) {
            if (!hdl->trigger) {
                if (bt_get_call_status_for_addr(hdl->bt_addr) == BT_CALL_ACTIVE) {
                    hdl->trigger = 1;
                }
            }
        } else {
            hdl->trigger = 1;
        }

        /*设置工具配置的降噪效果*/
        float minsuppress = hdl->cfg.minsuppress;
        if (!hdl->trigger ||
            (CONFIG_BTCTLER_TWS_ENABLE && hdl->tws_ns_state != TWS_NS_STATE_START)) {
            /*没有接通，降降噪效果设置成0*/
            minsuppress = 1.0f;
        }
        /*精简版需要屏蔽掉完整版的config指令*/
        if (hdl->ns && (!hdl->lite)) {
            noise_suppress_config(hdl->ns, NS_CMD_MINSUPPRESS, 0, &minsuppress);
        }

        tws_esco_ns_node_frame_detect(hdl);

        while (in_frame->offset < in_frame->len) {
            int input_len = in_frame->len - in_frame->offset;
            if (input_len > hdl->ns_frame_len - hdl->ns_frame_offset) {
                input_len = hdl->ns_frame_len - hdl->ns_frame_offset;
            }
            memcpy(hdl->ns_frame_buf + hdl->ns_frame_offset, in_frame->data + in_frame->offset, input_len);
            hdl->ns_frame_offset += input_len;
            in_frame->offset += input_len;
            if (hdl->ns_frame_offset == hdl->ns_frame_len) {
                if (!hdl->cfg.output16) { // 使用16转24算法后，输出buffer需要扩大2倍
                    out_frame_len = hdl->ns_frame_len * 2;
                } else {
                    out_frame_len = hdl->ns_frame_len;
                }
                struct stream_frame *frame = jlstream_get_frame(node->oport, out_frame_len);
                if (!frame) {
                    jlstream_return_frame(iport, in_frame);
                    return;
                }

                if (hdl->ns) {
                    audio_ns_run(hdl->ns, (s16 *)hdl->ns_frame_buf, frame->data, hdl->ns_frame_len);
                    frame->len = out_frame_len;
                    hdl->ns_frame_offset = 0;
                } else {
                    int ret = tws_esco_ns_node_restart_detect(hdl, in_frame, frame);
                    if (ret == 1) {
                        if (frame) {
                            jlstream_free_frame(frame);
                        }
                        continue;
                    }
                    if (ret == -1) { //单台可以执行在线调试的功能
                        if (!hdl->ns) {
                            hdl->ns = audio_ns_open(hdl->sample_rate, hdl->cfg.mode, hdl->cfg.noiselevel, hdl->cfg.aggressfactor, hdl->cfg.minsuppress, hdl->lite, hdl->cfg.eng_gain, hdl->cfg.output16);
                        }
                        audio_ns_run(hdl->ns, (s16 *)hdl->ns_frame_buf, frame->data, hdl->ns_frame_len);
                        frame->len = out_frame_len;
                        hdl->ns_frame_offset = 0;
                    }
                }
                /*printf(">%d, in frame : %d\n", frame->len, in_frame->offset);*/
                jlstream_push_frame(node->oport, frame);	//将数据推到oport
            }
        }
        tws_esco_ns_node_reset_detect(hdl);
        jlstream_free_frame(in_frame);	//释放iport资源
    }
}

/*节点预处理-在ioctl之前*/
static int ns_adapter_bind(struct stream_node *node, u16 uuid)
{
    ns_param_cfg_read(node);

    return 0;
}

/*打开改节点输入接口*/
static void ns_ioc_open_iport(struct stream_iport *iport)
{
    iport->handle_frame = ns_handle_frame;				//注册输出回调
}

/*节点参数协商*/
static int ans_ioc_negotiate(struct stream_iport *iport)
{
    struct ns_node_hdl *hdl = (struct ns_node_hdl *)iport->node->private_data;
    struct stream_fmt *in_fmt = &iport->prev->fmt;
    struct stream_oport *oport = iport->node->oport;
    int ret = NEGO_STA_ACCPTED;
    int nb_sr, wb_sr, nego_sr;

#if (TCFG_AUDIO_CVP_BAND_WIDTH_CFG == CVP_WB_EN)
    nb_sr = 16000;
    wb_sr = 16000;
    nego_sr  = 16000;
#elif (TCFG_AUDIO_CVP_BAND_WIDTH_CFG == CVP_NB_EN)
    nb_sr = 8000;
    wb_sr = 8000;
    nego_sr  = 8000;
#else
    nb_sr = 8000;
    wb_sr = 16000;
    nego_sr  = 16000;
#endif
    if (hdl->cfg.ns_type == AUDIO_NS_TYPE_ESCO_DL && hdl->scene == STREAM_SCENE_ESCO) {
        nb_sr = 8000;
    }
    //要求输入为8K或者16K
    if (in_fmt->sample_rate != nb_sr && in_fmt->sample_rate != wb_sr) {
        in_fmt->sample_rate = nego_sr;
        oport->fmt.sample_rate = in_fmt->sample_rate;
        ret = NEGO_STA_CONTINUE | NEGO_STA_SAMPLE_RATE_LOCK;
    }

    //要求输入16bit位宽的数据
    if (in_fmt->bit_wide != DATA_BIT_WIDE_16BIT) {
        in_fmt->bit_wide = DATA_BIT_WIDE_16BIT;
        in_fmt->Qval = AUDIO_QVAL_16BIT;
        oport->fmt.bit_wide = in_fmt->bit_wide;
        oport->fmt.Qval = in_fmt->Qval;
        ret = NEGO_STA_CONTINUE;
    }

    hdl->sample_rate = in_fmt->sample_rate;
    return ret;
}

/*节点start函数*/
static void ns_ioc_start(struct ns_node_hdl *hdl)
{
    /* struct stream_fmt *fmt = &hdl_node(hdl)->oport->fmt; */
    u8 ns_close = hdl->cfg.bypass;
    printf("ans node start");
    overlay_load_code(OVERLAY_AEC);
    aec_code_movable_load();

    if (hdl_node(hdl)->uuid == NODE_UUID_NOISE_SUPPRESSOR_LITE) {
        hdl->lite = 1;	// lite = 0 完整降噪    lite = 1 精简版降噪
    } else {
        hdl->lite = 0;
    }

    /*打开算法*/
    if (hdl->scene == STREAM_SCENE_ESCO && hdl->cfg.ns_type == AUDIO_NS_TYPE_ESCO_DL) {
        if (CONFIG_BTCTLER_TWS_ENABLE) {
#if ESCO_DL_TWS_NS_NB_ENABLE == 0
            ns_close = 1;
            hdl->cfg.bypass = 1;
#endif
            if (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED) {
                hdl->tws_ns_state = TWS_NS_STATE_WAIT;
            }
        }
    }
    if (!ns_close) {
        hdl->ns = audio_ns_open(hdl->sample_rate, hdl->cfg.mode, hdl->cfg.noiselevel, hdl->cfg.aggressfactor, hdl->cfg.minsuppress, hdl->lite, hdl->cfg.eng_gain, hdl->cfg.output16);
    }

    hdl->trigger = 0;
    hdl->ns_frame_len = hdl->sample_rate == 16000 ? ANS_FRAME_SIZE : ANS_NB_FRAME_SIZE;
    hdl->ns_frame_buf = malloc(hdl->ns_frame_len);
    hdl->ns_frame_offset = 0;
    /*hdl->fixed_hdl = audio_fixed_frame_len_init(ANS_FRAME_SIZE, ns_node_fixed_frame_run, hdl);*/
}

/*节点stop函数*/
static void ns_ioc_stop(struct ns_node_hdl *hdl)
{
    if (hdl->ns) {
        audio_ns_close(hdl->ns);
        hdl->ns = NULL;
    }
    aec_code_movable_unload();
    /*
    if (hdl->fixed_hdl) {
        audio_fixed_frame_len_exit(hdl->fixed_hdl);
        hdl->fixed_hdl = NULL;
    }
    if (hdl->out_frame) {
        jlstream_free_frame(hdl->out_frame);
        hdl->out_frame = NULL;
    }
    */
    if (hdl->ns_frame_buf) {
        free(hdl->ns_frame_buf);
        hdl->ns_frame_buf = NULL;
    }
    if (CONFIG_BTCTLER_TWS_ENABLE && hdl->tws_hdl) {
        tws_audio_frame_sync_stop(hdl->tws_hdl, hdl->frame_clkn);
        hdl->tws_hdl = NULL;
    }
    hdl->trigger = 0;
    printf("ns_ioc_stop\n");
}

static int ans_ioc_update_parm(struct ns_node_hdl *hdl, int parm)
{
    if (hdl == NULL) {
        return false;
    }
    memcpy(&hdl->cfg, (u8 *)parm, sizeof(hdl->cfg));
    /*精简版需要屏蔽掉完整版的config指令*/
    if (hdl->ns && (!hdl->lite)) {
        /*设置工具配置的降噪效果*/
        float aggressfactor = hdl->cfg.aggressfactor;
        noise_suppress_config(hdl->ns, NS_CMD_AGGRESSFACTOR, 0, &aggressfactor);

        float minsuppress = hdl->cfg.minsuppress;
        noise_suppress_config(hdl->ns, NS_CMD_MINSUPPRESS, 0, &minsuppress);
    }
    return true;
}

static u8 is_node_after_syncts(struct stream_node *node)
{
    struct stream_iport *iport = node->iport;
    if (!iport) {
        return 0;
    }

    if (iport->node->adapter->uuid == NODE_UUID_BT_AUDIO_SYNC) {
        return 1;
    }

    while (iport) {
        if (is_node_after_syncts(iport->prev->node)) {
            return 1;
        }
        iport = iport->sibling;
    }
    return 0;
}

#define TWS_ALIGN_TIME      320//slots
static int ns_set_priv_data(struct ns_node_hdl *hdl, u32 *private_data)
{
    if (hdl->scene != STREAM_SCENE_ESCO) {
        return 0;
    }

    if (CONFIG_BTCTLER_TWS_ENABLE && hdl->cfg.ns_type == AUDIO_NS_TYPE_ESCO_DL) {
        if (is_node_after_syncts(hdl_node(hdl))) {
            /*流程图连接检查：TWS通话下行NS节点必须位于音频同步节点之前*/
            ns_log("WRAN: NS node is located after the sync node，must be move the ns node to before the sync node!!!\n");
            return 0;
        }
        hdl->frame_clkn = private_data[0];
        hdl->frame_interval = private_data[1];
        u16 frame_pcms = 120;
        u16 align_pcms = 0;
        u16 ans_frame_pcms = ANS_FRAME_POINTS;
        u16 multiple = 1;
        if (hdl->sample_rate == 8000) {
            frame_pcms = hdl->frame_interval == 12 ? 60 : 30;
            ans_frame_pcms = ANS_NB_FRAME_POINTS;
        }
        do {
            align_pcms = frame_pcms * multiple;
            if ((align_pcms % ans_frame_pcms) == 0) {
                break;
            }
        } while (multiple++);
        u32 ans_align_time = multiple * hdl->frame_interval;
        u32 timeout = (TWS_ALIGN_TIME / ans_align_time + 1) * ans_align_time;
        ns_log("NS node TWS ESCO DL frame sync : %d, %d, %u, %d\n", timeout, ans_align_time, hdl->frame_clkn, hdl->frame_interval);
        if (!hdl->tws_hdl) {
            hdl->tws_hdl = tws_audio_frame_sync_create(timeout, ans_align_time);
        }
        tws_audio_frame_sync_start(hdl->tws_hdl, hdl->frame_clkn);
    }
    return 0;
}
/*节点ioctl函数*/
static int ns_adapter_ioctl(struct stream_iport *iport, int cmd, int arg)
{
    int ret = 0;
    struct ns_node_hdl *hdl = (struct ns_node_hdl *)iport->node->private_data;
    switch (cmd) {
    case NODE_IOC_SET_BTADDR:
        u8 *bt_addr = (u8 *)arg;
        memcpy(hdl->bt_addr, bt_addr, 6);
        break;
    case NODE_IOC_OPEN_IPORT:
        ns_ioc_open_iport(iport);
        break;
    case NODE_IOC_NEGOTIATE:
        *(int *)arg |= ans_ioc_negotiate(iport);
        break;
    case NODE_IOC_START:
        ns_ioc_start(hdl);
        break;
    case NODE_IOC_SUSPEND:
    case NODE_IOC_STOP:
        ns_ioc_stop(hdl);
        break;
    case NODE_IOC_NAME_MATCH:
        if (!strcmp((const char *)arg, hdl->name)) {
            ret = 1;
        }
        break;

    case NODE_IOC_SET_PARAM:
        ret = ans_ioc_update_parm(hdl, arg);
        break;
    case NODE_IOC_SET_SCENE:
        hdl->scene = arg;
        break;
    case NODE_IOC_SET_PRIV_FMT:
        ns_set_priv_data(hdl, (u32 *)arg);
        break;
    }

    return ret;
}

/*节点用完释放函数*/
static void ns_adapter_release(struct stream_node *node)
{
    struct ns_node_hdl *hdl = (struct ns_node_hdl *)node->private_data;

    if (CONFIG_BTCTLER_TWS_ENABLE && hdl->tws_hdl) {
        tws_audio_frame_sync_release(hdl->tws_hdl);
        hdl->tws_hdl = NULL;
    }
}

/*节点adapter 注意需要在sdk_used_list声明，否则会被优化*/
REGISTER_STREAM_NODE_ADAPTER(ns_node_adapter) = {
    .name       = "ns",
    .uuid       = NODE_UUID_NOISE_SUPPRESSOR,
    .bind       = ns_adapter_bind,
    .ioctl      = ns_adapter_ioctl,
    .release    = ns_adapter_release,
    .hdl_size   = sizeof(struct ns_node_hdl),
    .ability_bit_wide = 1,
};

REGISTER_STREAM_NODE_ADAPTER(ns_node_lite_adapter) = {
    .name       = "ns_lite",
    .uuid       = NODE_UUID_NOISE_SUPPRESSOR_LITE,
    .bind       = ns_adapter_bind,
    .ioctl      = ns_adapter_ioctl,
    .release    = ns_adapter_release,
    .hdl_size   = sizeof(struct ns_node_hdl),
    .ability_bit_wide = 1,
};



//注册工具在线调试
REGISTER_ONLINE_ADJUST_TARGET(noise_suppressor) = {
    .uuid = NODE_UUID_NOISE_SUPPRESSOR,
};


//注册工具在线调试
REGISTER_ONLINE_ADJUST_TARGET(noise_suppressor_lite) = {
    .uuid = NODE_UUID_NOISE_SUPPRESSOR_LITE,
};



#endif/* TCFG_NS_NODE_ENABLE*/

