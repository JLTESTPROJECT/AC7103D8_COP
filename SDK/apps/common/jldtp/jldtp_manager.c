#include "jldtp_manager.h"
#include "uart_transport.h"
#include "cpu/periph/gpio.h"

#ifndef JLDTP_NUM
#define JLDTP_NUM  1
#endif

#ifndef JLDTP_MC_CHANNEL_NUM
#define JLDTP_MC_CHANNEL_NUM    4
#endif

static jldtp_handle_t dt_handle[JLDTP_NUM];
static jldtp_mc_handle_t mc_handle[JLDTP_NUM];


int jldtp_manager_init(u8 handle_id)
{
    if (handle_id == JLDTP_WITH_CHIP0) {

        transport_uart_open(2000000, IO_PORTC_01, IO_PORTC_02);

        const jldtp_config_t config = {
            .max_tx_pending = 1,
            .task_name = "app_core",
        };
        dt_handle[0] = jldtp_create_with_config(&config);
        jldtp_register_transport(dt_handle[0], &jldtp_uart_transport);

        mc_handle[0] = jldtp_mc_create(dt_handle[0], JLDTP_MC_CHANNEL_NUM);
    }

    return 0;
}

void jldtp_manager_deinit(void)
{
    for (int i = 0; i < JLDTP_NUM; i++) {
        if (mc_handle[i] != NULL) {
            jldtp_mc_destroy(mc_handle[i]);
            mc_handle[i] = NULL;
        }
    }
}

jldtp_mc_handle_t jldtp_manager_get_mc_handle(u8 handle_id)
{
    if (handle_id >= JLDTP_NUM) {
        return NULL;
    }
    if (mc_handle[handle_id] == NULL) {
        jldtp_manager_init(handle_id);
    }
    return mc_handle[handle_id];
}



#include "system/init.h"
#include "system/timer.h"
static jldtp_channel_handle_t test_channel;
static u8 test_buf[2048];

static void test_channel_data_handler(void *user_data, jldtp_channel_handle_t channel, u8 *data, u16 len)
{
    r_printf("channel_rx_data: %d\n", len);
}

void jldtp_channel_test_send_data()
{
    puts("jldtp_channel_test_init\n");
    jldtp_channel_config_t config = {
        .channel_id = 5,
        .priority = 0,
        .callback_task = "app_core",
    };
    test_channel = jldtp_mc_open_channel(mc_handle[0], &config);

    jldtp_mc_register_data_callback(test_channel, NULL, test_channel_data_handler);

    for (int i = 0; i < sizeof(test_buf); i++) {
        test_buf[i] = i & 0xff;
    }
    jldtp_mc_send_data(test_channel, test_buf, 500, 1000);
    /*jldtp_mc_send_data(test_channel, test_buf, 512, 1000);
    jldtp_mc_send_data(test_channel, test_buf, 500 + 508, 1000);
    jldtp_mc_send_data(test_channel, test_buf, 2048, 1000);*/
}


int jldtp_channel_test_init()
{
    sys_timeout_add(NULL, jldtp_channel_test_send_data, 5000);
    return 0;
}
late_initcall(jldtp_channel_test_init);

