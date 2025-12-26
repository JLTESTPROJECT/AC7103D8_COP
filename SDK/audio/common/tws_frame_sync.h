/*************************************************************************************************/
/*!
*  \file      audio/common/tws_frame_sync.h
*
*  \brief
*
*  Copyright (c) 2011-2025 ZhuHai Jieli Technology Co.,Ltd.
*
*/
/*************************************************************************************************/
#ifndef _TWS_FRAME_SYNC_H_
#define _TWS_FRAME_SYNC_H_
#include "typedef.h"

#define TWS_FRAME_SYNC_NO_ERR   0
#define TWS_FRAME_SYNC_START    1
#define TWS_FRAME_SYNC_RESET    2

void *tws_audio_frame_sync_create(int timeout, int align_time);

int tws_audio_frame_sync_detect(void *handle, u32 frame_time);

int tws_frame_sync_reset_enable(void *handle, u32 frame_time);

void tws_audio_frame_sync_release(void *handle);

int tws_audio_frame_sync_start(void *handle, u32 start_time);

int tws_audio_frame_sync_stop(void *handle, u32 time);

#endif
