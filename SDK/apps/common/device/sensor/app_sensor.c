#include "app_config.h"
#include "timer.h"
#include "gSensor/gSensor_manage.h"
#include "hr_sensor/hrSensor_manage.h"
#include "gSensor/p33_algo_motion.h"

#if TCFG_GSENSOR_ENABLE || TCFG_HRSENSOR_ENABLE

extern int gsensor_data_get(short *buf);
#define SENSOR_KICK  (Q_MSG + 1)
u16 cb_timer_id = 0;

static void refresh_sensor_data_check_cb(void *parm)
{
    os_taskq_post_type("app_sensor", SENSOR_KICK, 0, NULL);
}

void sensor_del_data_check_cb()
{
    if (cb_timer_id != 0) {
        sys_s_hi_timer_del(cb_timer_id);
        cb_timer_id = 0;
    }
}

#if TCFG_GSENSOR_ENABLE

// 计步
u32 total_steps = 0;
void gsensor_motion_process(short *p)
{
    short point = p[0]; //frame number
    printf("point = %d\n", point);
    if ((point <= 0) || (point > 64)) {
        return;
    }

    u32 timestamp = 0; // timestamp_mytime_2_utc_sec(&time);   // 计步算法不需要时间戳，这个时间戳主要是给睡眠算法的

    algo_out out = algo_motion_run(timestamp, (accel_data *)&p[1], point, 0, 0); // 算法输出实时的步数
    printf("out.update.step_counter = %d\n", out.update.step_counter);
    // printf("out.steps:%d", out.steps);
    if (out.update.step_counter) {
        total_steps += out.steps;//通过total_steps 去累加，获取total_steps即可
        printf("steps:%d\n", out.steps);
    }
    printf("total_steps:%d\n", total_steps);
}
#endif

void app_sensor_task(void *p)
{
    short accel_data[1 + 64 * sizeof(axis_info_t)]; //lenght + 64 frame acceleromter data
    memset(accel_data, 0, sizeof(accel_data));

#if TCFG_GSENSOR_ENABLE
    // 计步算法初始化
    algo_type open_algo;
    accel_config accel;
    user_info user = {.ages = 28, .gender = 1, .height = 170, .weight = 60, .step_factor = 80};
    unsigned char thres[4] = {25, 7, 5, 2};
    open_algo.all = 0;
    open_algo.step_counter = 1;
    accel.sps = 25;
    accel.lsb_g = 1024;
    algo_motion_init(open_algo, accel, user);
#endif

    cb_timer_id = sys_s_hi_timer_add(NULL, refresh_sensor_data_check_cb, 320); //320ms 这个timer是读gsensor数据，并输入给心率，hx3918是320ms读取ppg数据


    os_time_dly(100);
#if TCFG_HRSENSOR_ENABLE
    /* hr_sensor_measure_spo2_start(0, 0); */
    hr_sensor_measure_hr_start(0, 0);
#endif

    int msg[8];
    int ret;
    while (1) {
        ret = os_taskq_pend("taskq", msg, ARRAY_SIZE(msg));
        if (ret != OS_TASKQ) {
            continue;
        }
        switch (msg[0]) {
        case SENSOR_KICK:
#if TCFG_GSENSOR_ENABLE
            accel_data[0] = gsensor_data_get((short *)accel_data + 1);				// 读加速度数据
#endif
#if TCFG_HRSENSOR_ENABLE
            hr_sensor_io_ctl(INPUT_ACCELEROMETER_DATA, accel_data);  //加速度数据输入给心率
#endif
#if TCFG_GSENSOR_ENABLE
            // 计步
            gsensor_motion_process(accel_data);
#endif
            //如果有其它算法需要使用加速度数据可在此添加
            break;
        default:
            break;
        }

    }
}


void app_sensor_init(void)
{
    task_create(app_sensor_task, NULL, "app_sensor");
}


#endif
