/*************************************************************************************************/
/*!
*  \file      tws_frame_sync.c
*
*  \brief
*
*  Copyright (c) 2011-2025 ZhuHai Jieli Technology Co.,Ltd.
*
*/
/*************************************************************************************************/
#include "tws_frame_sync.h"
#include "system/includes.h"
#include "classic/tws_api.h"

extern const int CONFIG_BTCTLER_TWS_ENABLE;

enum tws_frame_sync_state : u8 {
    TWS_FS_STATE_REQ_ALIGN,
    TWS_FS_STATE_START,         /*本地状态，不用于通讯*/
    TWS_FS_STATE_RESET,
    TWS_FS_STATE_WAIT_START,    /*本地被reset后等待START状态，不用于通讯*/
    TWS_FS_STATE_STOP,
};

struct tws_frame_sync_context {
    u8 state;
    u8 retry;
    u8 wait_start;
    u16 uuid;
    u32 align_time;
    u32 next_align_time;
    u32 start_time;
    u32 timeout;
    u32 sync_time;
    u32 tx_sync_time;
};

struct tws_frame_sync_data {
    u16 uuid;
    u8 state;
    u8 ack;
    u32 sync_time;
};

struct tws_frame_sync_msg {
    struct list_head entry;
    struct tws_frame_sync_data data;
};

/*static LIST_HEAD(g_frame_sync_list); */
static LIST_HEAD(g_msg_list);

#define TWS_AUDIO_FRAME_SYNC \
	 ((int)(('T' + 'W' + 'S') << (3 * 8)) | \
	 (int)(('A' + 'U' + 'D' + 'I' + 'O') << (2 * 8)) | \
     (int)(('F' + 'R' + 'A' + 'M' + 'E') << (1 * 8)) | \
     (int)(('S' + 'Y' + 'N' + 'C') << (0 * 8)))

#define frame_time_after(a, b)     (((a - b) & 0x7ffffff) > 0 && ((a - b) & 0x7ffffff) < 0x3ffffff)
#define frame_time_before(a, b)    frame_time_after(b, a)

static void tws_frame_sync_txrx_callback(void *buf, u16 len, bool rx)
{
    if (!CONFIG_BTCTLER_TWS_ENABLE) {
        return;
    }

    if (rx) {
        struct tws_frame_sync_msg *msg = zalloc(sizeof(struct tws_frame_sync_msg));
        memcpy(&msg->data, buf, sizeof(struct tws_frame_sync_data));
        local_irq_disable();
        list_add_tail(&msg->entry, &g_msg_list);
        local_irq_enable();
    }
}

REGISTER_TWS_FUNC_STUB(tws_audio_frame_sync) = {
    .func_id = TWS_AUDIO_FRAME_SYNC,
    .func    = tws_frame_sync_txrx_callback,
};


static int tws_frame_sync_send_data(struct tws_frame_sync_context *ctx, u8 state, u32 sync_time)
{
    if (!CONFIG_BTCTLER_TWS_ENABLE) {
        return 0;
    }

    if (!(tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
        return -EINVAL;
    }

    struct tws_frame_sync_data data;
    data.uuid = ctx->uuid;
    data.state = state;
    data.ack = 0;
    data.sync_time = sync_time;
    ctx->tx_sync_time = sync_time;
    int err = tws_api_send_data_to_sibling((u8 *)&data, sizeof(data), TWS_AUDIO_FRAME_SYNC);
    ctx->retry = err ? 1 : 0;
    /*r_printf("send data, state : %d, sync_time : %u, err : %d\n", ctx->state, data.sync_time, err);*/

    return err;
}

static int __tws_frame_sync_msg_handler(struct tws_frame_sync_context *ctx, struct tws_frame_sync_data *data, u32 frame_time)
{
    u32 sync_time = frame_time;
    u32 next_sync_time;
    /*r_printf("rx ack : %d, state : %d, local state : %d, sync time : %u - %u - %u\n", data->ack, data->state, ctx->state, data->sync_time, ctx->start_time, frame_time);*/
    if (data->ack) { //ACK类型
        switch (data->state) {
        case TWS_FS_STATE_REQ_ALIGN: /*对方已应答，复位等待对齐*/
            if (ctx->state == TWS_FS_STATE_REQ_ALIGN) {
                ctx->state = TWS_FS_STATE_RESET;
                /*ctx->reset_time = ctx->next_align_time; */
                next_sync_time = ctx->next_align_time + ctx->timeout;
                ctx->start_time = next_sync_time;
                tws_frame_sync_send_data(ctx, ctx->state, next_sync_time);
            }
            break;
        case TWS_FS_STATE_RESET:
            if (ctx->state == TWS_FS_STATE_RESET || ctx->state == TWS_FS_STATE_WAIT_START) {
                /*对方应答确认后代表可以根据对齐时间点进行对齐*/
                /*r_printf("local wait start...\n");*/
                ctx->wait_start = 1;
            }
            break;
        }
        return 0;
    }
    switch (data->state) { /*TWS发送端状态*/
    case TWS_FS_STATE_REQ_ALIGN:
        if (ctx->state == TWS_FS_STATE_START) {
            ctx->state = TWS_FS_STATE_RESET;
            next_sync_time = ctx->next_align_time + ctx->timeout;
            ctx->start_time = next_sync_time; //对齐运行时间点
            ctx->wait_start = 0;
            tws_frame_sync_send_data(ctx, ctx->state, next_sync_time);
            break;
        }

        if (ctx->state == TWS_FS_STATE_RESET) {
            next_sync_time = ctx->next_align_time + ctx->timeout;
            ctx->start_time = next_sync_time; //对齐运行时间点
            tws_frame_sync_send_data(ctx, ctx->state, next_sync_time);
            break;
        }

        if (ctx->state == TWS_FS_STATE_WAIT_START) {
            next_sync_time = frame_time + ctx->timeout;
            ctx->start_time = next_sync_time;
            tws_frame_sync_send_data(ctx, TWS_FS_STATE_RESET, next_sync_time);
        }
        break;
    case TWS_FS_STATE_RESET:
        if (ctx->state == TWS_FS_STATE_RESET || ctx->state == TWS_FS_STATE_WAIT_START) {
            if (time_after(data->sync_time, ctx->start_time)) {
                ctx->start_time = data->sync_time;
            }
            break;
        }
        if (ctx->state == TWS_FS_STATE_START) {
            ctx->state = TWS_FS_STATE_RESET;
            next_sync_time = ctx->next_align_time + ctx->timeout;
            ctx->start_time = next_sync_time; //对齐运行时间点
            ctx->wait_start = 0;
            tws_frame_sync_send_data(ctx, ctx->state, next_sync_time);
            break;
        }
        break;
    case TWS_FS_STATE_STOP:
        if (ctx->state == TWS_FS_STATE_RESET) {
            ctx->wait_start = 1;
        }
        break;
    }

    return 0;
}

int tws_frame_sync_msg_ack(struct tws_frame_sync_context *ctx, struct tws_frame_sync_data *data, u32 frame_time)
{
    if (data->ack) {
        return 0;
    }

    if (!(tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
        return -EINVAL;
    }

    struct tws_frame_sync_data ack_data;
    ack_data.uuid = ctx->uuid;
    ack_data.state = data->state;
    ack_data.ack = 1;
    ack_data.sync_time = frame_time;
    int err = tws_api_send_data_to_sibling((u8 *)&ack_data, sizeof(ack_data), TWS_AUDIO_FRAME_SYNC);
    /*r_printf("ack data, state : %d, sync_time : %u, err : %d\n", ctx->state, ack_data.sync_time, err);*/

    return err;

}

static struct tws_frame_sync_msg *tws_frame_sync_recieve_msg(u16 uuid)
{
    local_irq_disable();
    if (list_empty(&g_msg_list)) {
        local_irq_enable();
        return NULL;
    }

    struct tws_frame_sync_msg *msg, *p;
    if (!(tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
        list_for_each_entry_safe(msg, p, &g_msg_list, entry) {
            __list_del_entry(&msg->entry);
            free(msg);
        }
        local_irq_enable();
        return NULL;
    }

    list_for_each_entry(msg, &g_msg_list, entry) {
        if (msg->data.uuid == uuid) {
            local_irq_enable();
            return msg;
        }
    }
    local_irq_enable();

    return NULL;
}

static int tws_frame_sync_msg_handler(struct tws_frame_sync_context *ctx, u32 frame_time)
{
    struct tws_frame_sync_msg *msg = tws_frame_sync_recieve_msg(ctx->uuid);

    if (!msg) {
        return 0;
    }
    tws_frame_sync_msg_ack(ctx, &msg->data, frame_time);

    __tws_frame_sync_msg_handler(ctx, &msg->data, frame_time);

    local_irq_disable();
    list_del(&msg->entry);
    local_irq_enable();
    free(msg);

    return 0;
}

int tws_audio_frame_sync_start(void *handle, u32 start_time)
{
    struct tws_frame_sync_context *ctx = (struct tws_frame_sync_context *)handle;

    ctx->next_align_time = (start_time + ctx->align_time) & 0x7ffffff;

    u32 sync_time = (start_time + ((ctx->timeout / ctx->align_time) + 1) * ctx->align_time) & 0x7ffffff;

    ctx->state = TWS_FS_STATE_START;

    tws_frame_sync_send_data(ctx, TWS_FS_STATE_REQ_ALIGN, sync_time);

    return 0;
}

void *tws_audio_frame_sync_create(int timeout, int align_time)
{
    if (!CONFIG_BTCTLER_TWS_ENABLE) {
        return NULL;
    }
    struct tws_frame_sync_context *ctx = (struct tws_frame_sync_context *)zalloc(sizeof(struct tws_frame_sync_context));

    if (!ctx) {
        return NULL;
    }

    ctx->timeout = timeout;
    ctx->align_time = align_time;
    /*
     * 本地状态生存期状态:
     * START -> RESET -> WAIT_START -> STOP
     *
     * TWS通讯命令状态：
     * REQ_ALIGN -> RESET -> STOP
     *
     * */
    /*r_printf("tws audio frame sync create : 0x%x\n", (u32)ctx);*/
    return ctx;
}

int tws_audio_frame_sync_stop(void *handle, u32 time)
{
    struct tws_frame_sync_context *ctx = (struct tws_frame_sync_context *)handle;

    ctx->state = TWS_FS_STATE_STOP;
    /*r_printf("tws audio frame sync stop : 0x%x, %u\n", (u32)ctx, time);*/
    tws_frame_sync_send_data(ctx, ctx->state, time);

    return 0;
}

void tws_audio_frame_sync_release(void *handle)
{
    struct tws_frame_sync_context *ctx = (struct tws_frame_sync_context *)handle;

    free(ctx);
}

int tws_audio_frame_sync_detect(void *handle, u32 frame_time)
{
    struct tws_frame_sync_context *ctx = (struct tws_frame_sync_context *)handle;

    /*优先处理消息队列中的消息*/
    tws_frame_sync_msg_handler(ctx, frame_time);

    /*进入本地状态处理*/
    switch (ctx->state) {
    case TWS_FS_STATE_START:
        if (frame_time == ctx->next_align_time) {
            ctx->next_align_time = (ctx->next_align_time + ctx->align_time) & 0x7ffffff;
            /*r_printf("update next align time : %u -> %u\n", frame_time, ctx->next_align_time);*/
        }
        break;
    case TWS_FS_STATE_WAIT_START:
        if (time_after(frame_time, ctx->start_time)) { /*已等待对齐应答或对方响应超时*/
            ctx->start_time = (ctx->start_time + ctx->timeout) & 0x7ffffff;
            /*g_printf("wait start timeout : %u, reset start time : %u\n", frame_time, ctx->start_time); */
            tws_frame_sync_send_data(ctx, TWS_FS_STATE_RESET, ctx->start_time);
            break;
        }
        /*g_printf("wait start : %d, %u - %u\n", ctx->wait_start, frame_time, ctx->start_time);*/
        if (ctx->wait_start && frame_time == ctx->start_time) {
            ctx->next_align_time = (ctx->start_time + ctx->align_time) & 0x7ffffff;
            r_printf("start time : %u, next align time : %u\n", frame_time, ctx->next_align_time);
            ctx->wait_start = 0;
            ctx->state = TWS_FS_STATE_START;
            return TWS_FRAME_SYNC_START;
        }
        break;
    }


    return TWS_FRAME_SYNC_NO_ERR;
}

/*
 * 输入输出已对齐的调用
 */
int tws_frame_sync_reset_enable(void *handle, u32 frame_time)
{
    struct tws_frame_sync_context *ctx = (struct tws_frame_sync_context *)handle;

    /*优先处理消息队列中的消息*/
    tws_frame_sync_msg_handler(ctx, frame_time);
    /*进入本地状态处理*/
    if (ctx->state == TWS_FS_STATE_RESET) {
        ctx->state = TWS_FS_STATE_WAIT_START;
        r_printf("reset time : %u, next align time : %u, start time : %u\n", frame_time, ctx->next_align_time, ctx->start_time);
        return TWS_FRAME_SYNC_RESET;
    }
    return TWS_FRAME_SYNC_NO_ERR;
}


