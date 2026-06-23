#ifndef __MY_CONFIG_H
#define __MY_CONFIG_H

#include "include.h" // 芯片官方提供的头文件

// MY_DEBUG:
#define USE_MY_DEBUG 0 // 是否使用打印调试

// MY_DEBUG:
/*
    测试引脚：
    使用 P00 作为 串口打印输出 （实际用到的芯片的第4脚）
    rf检测引脚 用 P01 代替 P03
    pwm_channel_1 输出引脚 用 P05 代替 P15
    用 p06 代替 xx
*/
#define USE_MY_TEST_PIN 0 // 是否使用测试用的引脚（开发板没有相关的引脚，用其他空闲的引脚来代替）

// MY_DEBUG:
#define USE_MY_TEST_433_REMOTE 0 // 是否使用测试用的433遥控器按键，用于修改特定值，观察变化

#if USE_MY_DEBUG
#include <stdio.h>
#endif // #if USE_MY_DEBUG

#define ARRAY_SIZE(n) (sizeof(n) / sizeof(n[0]))

// tmr1配置成每10ms产生一次中断，计数值加一，
// 这里定义时间对应的计数值
// #define TMR1_CNT_30_MINUTES 180000UL // 30min（这个是可以在30min后调节PWM占空比的）
#define TMR1_CNT_5_MINUTES 30000UL // 5min

// 定义1ms定时中断计数5分钟所对应的数值
#define CNT_5_MINUTES_IN_1MS_ISR ((u32)5 * 60 * 1000)

// // 测试用的计数值
// #define TMR1_CNT_30_MINUTES 1000 // 10s
// #define TMR1_CNT_30_MINUTES 18000UL // 3min

// 在热敏电阻端检测的电压值与温度对应的关系，电压值单位：mV
#define VOLTAGE_TEMP_75 (3050) // 这一个值在客户那边测试出来是74摄氏度,对应的电压是3.1V

// 温度状态定义
enum
{
    // TEMP_NORMAL, // 正常温度
    // TEMP_75,     // 超过75摄氏度（±5摄氏度）
    // // TEMP_75_30MIN, // 超过75摄氏度（±5摄氏度）30min
    // TEMP_75_5_MIN, // 超过75摄氏度（±5摄氏度）5min

    TEMP_STATUS_NORMAL,  // 默认状态，温度正常 
    TEMP_STATUS_OVER_90,       // 超过 90 摄氏度（±5摄氏度） 
    TEMP_STATUS_OVER_90_5_MIN, // 超过 90 摄氏度（±5摄氏度）5min 
};
 
#define ADC_VAL_WHEN_UNSTABLE (2243)          // 9脚检测到电压不稳定、发动机功率不足时，对应的的阈值，大于该值就认为不稳定

#define TEMP_PULL_UP_RESISTOR_VALUE ((u32)10000) // 温度检测脚外部的上拉电阻阻值，单位：欧姆
/*
    温度超过 90 摄氏度时，
    温度检测脚外部的下拉热敏电阻阻值（这里取中心值），单位：欧姆
*/
#define TEMP_PULL_DOWN_RESISTOR_VALUE ((u32)8824)

/*
    温度超过 90 摄氏度时，对应的ad值阈值（前提条件：adc检测使用的参考电压为VCC）

    补充：当前热敏电阻，温度越高，阻值越小，检测脚检测到的ad值也越小
        判断时，只要检测到小于该阈值，就认为温度过高

    计算方式：
    ad值 == 下拉电阻阻值 / (上拉电阻 + 下拉电阻) * 上拉电压 * 4096 / 上拉电压
        ==  下拉电阻阻值 / (上拉电阻 + 下拉电阻) * 4096
        ==  下拉电阻阻值 * 4096 / (上拉电阻 + 下拉电阻)
*/
#define ADC_VAL_WHEN_TEMP_OVER_90                     \
    (u16)((u32)TEMP_PULL_DOWN_RESISTOR_VALUE * 4096 / \
          (TEMP_PULL_UP_RESISTOR_VALUE + TEMP_PULL_DOWN_RESISTOR_VALUE))

#if 0

/*
    风扇异常时，对应的ad值

    客户要求： 检测到大于3.9V，认为风扇异常，所有pwm占空比降低至25%，小于3.9V，认为风扇正常工作

    检测时使用 4.2V 内部参考电压， 3.9V 对应的ad值是 4803
*/
#define ADC_VAL_WHEN_FAN_ERR (3803)
#define ADC_VAL_WHEN_FAN_NORMAL (3705) // 风扇正常时，对应的ad值 （3.8V--对应ad值3705）

#endif

/*
    1脚电压低于4.3V时，14，15脚输出25%占空比，
    1脚电压高于4.5V时，14，15脚输出100%占空比

    检测时使用VCCA作为参考电压，客户测试VCC对GND的电压为4.87V
    那么，使用VCCA作为参考电压，adc 12位精度
    4.30 V 对应的ad值是  4300 / 4870 * 4096  == 3616.59
    4.50 V 对应的ad值是  4500 / 4870 * 4096  == 3784.80
*/
#define ADC_REF_VOLTAGE_FAN ((u16)4870)                                       // 检测风扇一侧的ad值时，使用的参考电压，单位：mV
#define ADC_VAL_WHEN_FAN_ERR (u16)((u32)4300 * 4096 / ADC_REF_VOLTAGE_FAN)    // 风扇异常时，对应的ad值
#define ADC_VAL_WHEN_FAN_NORMAL (u16)((u32)4500 * 4096 / ADC_REF_VOLTAGE_FAN) // 风扇正常时，对应的ad值

// 累计检测风扇工作正常多少时间，才认为是真的工作正常，并更新对应的状态，单位：ms （例如，累计检测风扇正常工作5s，才将风扇的状态更新为工作正常）
#define FAN_SCAN_TIMES_WHEN_NORMAL (8000)
// 累计检测风扇工作异常多少时间，才认为是真的工作异常，并更新对应的状态，单位：ms （例如，累计检测风扇工作异常5s，才将风扇的状态更新为工作异常）
#define FAN_SCAN_TIMES_WHEN_ERR (50)

// 定义风扇状态
enum FAN_STATUS
{
    FAN_STATUS_NORMAL = 0,
    FAN_STATUS_ERROR,
};

#include "flash.h"
#include "pwm.h"
#include "adc.h"
#include "time0.h" // 定时器0
// #include "timer1.h"
#include "timer2.h"
#include "timer3.h"
#include "knob_dimming.h" // 旋钮调光头文件
#include "rf_recv.h"
#include "key_driver.h"
#include "fan_ctl.h"

#include "engine.h" // 发动机

#endif
