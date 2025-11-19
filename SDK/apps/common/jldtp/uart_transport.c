#include "uart_transport.h"
#include "cpu/periph/uart.h"


/* 默认配置常量 */
#define UART_MTU_SIZE      256
#define RX_BUFFER_SIZE     (UART_MTU_SIZE * 2)
#define IRQ_PRIORITY       3
#define RX_TIMEOUT_BYTES   3


struct uart_trans_handle {
    int uart_id;
    u8 *rx_dma_buffer;
    u8 *tx_dma_buffer;
    u16 tx_dma_buffer_size;
    jldtp_handle_t jldtp_handle;
};

static struct uart_trans_handle *g_handle = NULL;


static void uart_transport_cleanup(struct uart_trans_handle *handle);

static void uart_irq_callback(int uart_num, enum uart_event event)
{
    if (!g_handle) {
        return;
    }

    if (event & UART_EVENT_TX_DONE) {
        puts("uart_event_tx_done\n");
        jldtp_notify_tx_complete(g_handle->jldtp_handle, 1);
    }

    if (event & UART_EVENT_RX_DATA) {
        puts("uart_event_rx_data\n");
        jldtp_notify_rx_ready(g_handle->jldtp_handle);
    }
    if (event & UART_EVENT_RX_TIMEOUT) {
        puts("uart_event_rx_timeout\n");
        jldtp_notify_rx_ready(g_handle->jldtp_handle);
    }
}

static void *__transport_uart_open(jldtp_handle_t jldtp_handle)
{
    struct uart_trans_handle *handle = g_handle;
    if (handle) {
        handle->jldtp_handle = jldtp_handle;
    }
    return handle;
}

int transport_uart_open(u32 baud_rate, u32 tx_pin, u32 rx_pin)
{
    struct uart_trans_handle *handle = g_handle;
    if (handle) {
        return 0;
    }

    handle = zalloc(sizeof(*handle));
    if (!handle) {
        return -ENOMEM;
    }

    struct uart_config uart_cfg = {
        .baud_rate      = baud_rate,
        .tx_pin         = tx_pin,
        .rx_pin         = rx_pin,
        .parity         = UART_PARITY_DISABLE,
        .tx_wait_mutex  = 0,  // 支持中断，不互斥
    };

    handle->uart_id = uart_init(-1, &uart_cfg);
    if (handle->uart_id < 0) {
        printf("UART init failed: %d\n", handle->uart_id);
        goto error_cleanup;
    }

    /* 初始化DMA */
    handle->rx_dma_buffer = dma_malloc(RX_BUFFER_SIZE);
    if (!handle->rx_dma_buffer) {
        printf("Failed to allocate RX DMA buffer, size: %d\n", RX_BUFFER_SIZE);
        goto error_cleanup;
    }

    /* 计算RX超时阈值（单位：us） */
    u32 rx_timeout_thresh = RX_TIMEOUT_BYTES * 10000000 / baud_rate;

    /* 配置DMA参数 */
    struct uart_dma_config dma_config = {
        .rx_timeout_thresh  = rx_timeout_thresh,
        .event_mask         = UART_EVENT_TX_DONE | UART_EVENT_RX_DATA |
        UART_EVENT_RX_FIFO_OVF | UART_EVENT_RX_TIMEOUT,
        .irq_priority       = IRQ_PRIORITY,
        .irq_callback       = uart_irq_callback,
        .rx_cbuffer         = handle->rx_dma_buffer,
        .rx_cbuffer_size    = RX_BUFFER_SIZE,
        .frame_size         = UART_MTU_SIZE,
    };

    int ret = uart_dma_init(handle->uart_id, &dma_config);
    if (ret < 0) {
        printf("UART(%d) DMA init error: %d\n", handle->uart_id, ret);
        goto error_cleanup;
    }
    g_handle = handle;

    printf("UART transport initialized: baud=%d, tx_pin=%d, rx_pin=%d\n", baud_rate, tx_pin, rx_pin);

    return 0;

error_cleanup:
    uart_transport_cleanup(handle);
    return -EFAULT;
}

static int transport_uart_read(void *handle, u8 *buffer, int len)
{
    if (!g_handle) {
        return -1;
    }
    return uart_recv_bytes(g_handle->uart_id, buffer, len);
}

static int transport_uart_write(void *handle, u8 *buffer, int len)
{
    if (!g_handle || !buffer || len <= 0) {
        return 0;
    }
    if (len > UART_MTU_SIZE) {
        printf("len %d > mute_size %d\n", len, UART_MTU_SIZE);
    }

    /* 检查是否需要重新分配TX缓冲区 */
    if (g_handle->tx_dma_buffer && g_handle->tx_dma_buffer_size < len) {
        dma_free(g_handle->tx_dma_buffer);
        g_handle->tx_dma_buffer = NULL;
        g_handle->tx_dma_buffer_size = 0;
    }

    /* 分配TX缓冲区（如果需要） */
    if (!g_handle->tx_dma_buffer) {
        g_handle->tx_dma_buffer_size = (len + 3) / 4 * 4;
        g_handle->tx_dma_buffer = dma_malloc(g_handle->tx_dma_buffer_size);
        if (!g_handle->tx_dma_buffer) {
            printf("Failed to allocate TX DMA buffer, size: %d\n",
                   g_handle->tx_dma_buffer_size);
            return 0;
        }
    }

    /* 拷贝数据并发送 */
    memcpy(g_handle->tx_dma_buffer, buffer, len);
    return uart_send_bytes(g_handle->uart_id, g_handle->tx_dma_buffer, len);
}

static void uart_transport_cleanup(struct uart_trans_handle *handle)
{
    if (handle->uart_id >= 0) {
        uart_deinit(handle->uart_id);
    }

    if (handle->rx_dma_buffer) {
        dma_free(handle->rx_dma_buffer);
    }

    if (handle->tx_dma_buffer) {
        dma_free(handle->tx_dma_buffer);
    }
    free(handle);
    g_handle = NULL;
}

static void transport_uart_close(void *handle)
{
    if (!g_handle) {
        return;
    }
    uart_transport_cleanup(g_handle);
}

const struct jldtp_transport jldtp_uart_transport = {
    .mtu_size = UART_MTU_SIZE,
    .open = __transport_uart_open,
    .read = transport_uart_read,
    .write = transport_uart_write,
    .close = transport_uart_close,
};




