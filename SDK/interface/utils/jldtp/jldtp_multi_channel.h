#ifndef JLDTP_MULTI_CHANNEL_H
#define JLDTP_MULTI_CHANNEL_H

#include "jldtp/jldtp.h"
#include "generic/typedef.h"


// 前向声明
typedef struct jldtp_mc_ctrl_t jldtp_mc_ctrl_t;
typedef struct channel_ctrl_t channel_ctrl_t;

// 多通道句柄类型
typedef jldtp_mc_ctrl_t *jldtp_mc_handle_t;

// 通道句柄类型
typedef channel_ctrl_t *jldtp_channel_handle_t;

// 结果码定义
typedef enum {
    JLDTP_MC_SUCCESS = 0,
    JLDTP_MC_ERROR_INVALID_PARAM = -1,
    JLDTP_MC_ERROR_MEMORY = -2,
    JLDTP_MC_ERROR_CHANNEL_NOT_FOUND = -3,
    JLDTP_MC_ERROR_CHANNEL_BUSY = -4,
    JLDTP_MC_ERROR_CHANNEL_FULL = -5,
    JLDTP_MC_ERROR_BUFFER_TOO_SMALL = -6,
    JLDTP_MC_ERROR_BUSY = -7,
    JLDTP_MC_ERROR_TIMEOUT = -8,
    JLDTP_MC_ERROR_INVALID_STATE = -9
} jldtp_mc_result_t;

// 事件类型定义
typedef enum {
    JLDTP_MC_EVENT_CHANNEL_OPENED = 0,
    JLDTP_MC_EVENT_CHANNEL_CLOSED = 1,
    JLDTP_MC_EVENT_CHANNEL_ERROR = 2
} jldtp_mc_event_t;

// 通道配置结构
typedef struct {
    u8 channel_id;          // 通道ID
    u8 priority;           // 优先级
    const char *callback_task;
    bool reliable;         // 是否可靠传输
    u16 timeout_ms;        // 超时时间(ms)
} jldtp_channel_config_t;

// 通道状态结构
typedef struct {
    u8 channel_id;          // 通道ID
    bool is_open;           // 是否打开

    // 统计信息
    u32 tx_packets;         // 发送数据包计数
    u32 rx_packets;         // 接收数据包计数
    u32 tx_bytes;           // 发送字节数
    u32 rx_bytes;           // 接收字节数
    u32 error_count;        // 错误计数

    // 分包统计信息
    u32 fragment_tx_count;  // 分包发送计数
    u32 fragment_rx_count;  // 分包接收计数
    u32 reassembly_timeouts; // 重组超时计数
} jldtp_mc_channel_status_t;

// 数据回调函数类型
typedef void (*jldtp_mc_data_callback_t)(void *user_data,
        jldtp_channel_handle_t channel_handle,
        u8 *data, u16 data_length);

// 事件回调函数类型
typedef void (*jldtp_mc_event_callback_t)(void *user_data,
        u8 channel_id,
        jldtp_mc_event_t event,
        u32 param);

// ==================== 多通道管理函数 ====================

/**
 * @brief 创建多通道控制器
 *
 * @param jldtp_handle 底层JLDTP句柄
 * @param max_channels 最大通道数
 * @return jldtp_mc_handle_t 多通道句柄，失败返回NULL
 */
jldtp_mc_handle_t jldtp_mc_create(jldtp_handle_t jldtp_handle, u8 max_channels);

/**
 * @brief 销毁多通道控制器
 *
 * @param handle 多通道句柄
 */
void jldtp_mc_destroy(jldtp_mc_handle_t handle);

/**
 * @brief 注册全局事件回调函数
 *
 * @param handle 多通道句柄
 * @param user_data 用户数据
 * @param callback 事件回调函数
 */
void jldtp_mc_register_event_callback(jldtp_mc_handle_t handle,
                                      void *user_data,
                                      jldtp_mc_event_callback_t callback);

/**
 * @brief 处理接收到的数据（用于外部数据注入）
 *
 * @param handle 多通道句柄
 * @param data 接收到的数据
 * @param data_len 数据长度
 */
void jldtp_mc_process_rx_data(jldtp_mc_handle_t handle,
                              u8 *data,
                              int data_len);

/**
 * @brief 获取打开的通道列表
 *
 * @param handle 多通道句柄
 * @param channel_list 通道句柄列表输出缓冲区
 * @param list_size 输入：列表大小，输出：实际通道数
 * @return jldtp_mc_result_t 执行结果
 */
jldtp_mc_result_t jldtp_mc_get_open_channels(jldtp_mc_handle_t handle,
        jldtp_channel_handle_t *channel_list,
        u8 *list_size);

// ==================== 通道管理函数 ====================

/**
 * @brief 打开通道
 *
 * @param handle 多通道句柄
 * @param config 通道配置
 * @return jldtp_channel_handle_t 通道句柄，失败返回NULL
 */
jldtp_channel_handle_t jldtp_mc_open_channel(jldtp_mc_handle_t handle,
        const jldtp_channel_config_t *config);

/**
 * @brief 关闭通道
 *
 * @param channel_handle 通道句柄
 * @return jldtp_mc_result_t 执行结果
 */
jldtp_mc_result_t jldtp_mc_close_channel(jldtp_channel_handle_t channel_handle);

/**
 * @brief 注册通道数据回调函数
 *
 * @param channel_handle 通道句柄
 * @param user_data 用户数据
 * @param callback 数据回调函数
 */
void jldtp_mc_register_data_callback(jldtp_channel_handle_t channel_handle,
                                     void *user_data,
                                     jldtp_mc_data_callback_t callback);

/**
 * @brief 发送数据
 *
 * @param channel_handle 通道句柄
 * @param data 要发送的数据
 * @param data_len 数据长度
 * @param timeout_ms 超时时间(ms)
 * @return jldtp_mc_result_t 执行结果
 */
jldtp_mc_result_t jldtp_mc_send_data(jldtp_channel_handle_t channel_handle,
                                     const u8 *data,
                                     u16 data_len,
                                     u16 timeout_ms);

/**
 * @brief 分配发送缓冲区
 *
 * @param channel_handle 通道句柄
 * @param data_length 数据长度
 * @return u8* 数据区指针，失败返回NULL
 */
u8 *jldtp_mc_alloc_tx_buffer(jldtp_channel_handle_t channel_handle,
                             u16 data_length, u8 flag);

/**
 * @brief 提交发送缓冲区
 *
 * @param channel_handle 通道句柄
 * @param buffer 通过jldtp_mc_alloc_tx_buffer获取的缓冲区指针
 * @param data_len 实际数据长度
 * @param timeout_ms 超时时间(ms)
 * @return jldtp_mc_result_t 执行结果
 */
jldtp_mc_result_t jldtp_mc_commit_tx_buffer(jldtp_channel_handle_t channel_handle,
        u8 *buffer,
        u16 data_len);

/**
 * @brief 获取通道状态
 *
 * @param channel_handle 通道句柄
 * @param status 状态输出缓冲区
 * @return jldtp_mc_result_t 执行结果
 */
jldtp_mc_result_t jldtp_mc_get_channel_status(jldtp_channel_handle_t channel_handle,
        jldtp_mc_channel_status_t *status);

/**
 * @brief 重置通道统计信息
 *
 * @param channel_handle 通道句柄
 * @return jldtp_mc_result_t 执行结果
 */
jldtp_mc_result_t jldtp_mc_reset_channel_stats(jldtp_channel_handle_t channel_handle);

/**
 * @brief 设置通道配置
 *
 * @param channel_handle 通道句柄
 * @param config 新的通道配置
 * @return jldtp_mc_result_t 执行结果
 */
jldtp_mc_result_t jldtp_mc_set_channel_config(jldtp_channel_handle_t channel_handle,
        const jldtp_channel_config_t *config);


#endif // JLDTP_MULTI_CHANNEL_H
