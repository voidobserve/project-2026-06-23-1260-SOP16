#include "adc.h"
#include "my_config.h"

// 存放温度状态的变量
volatile u8 temp_status = TEMP_STATUS_NORMAL;
volatile u32 temp_over_heat_time_cnt = 0; // 记录温度过热的时间

volatile u16 adc_val_from_engine;   // 存放 从发动机一侧 采集到的ad值
volatile u16 adc_val_from_knob;     // 存放 从旋钮一侧 采集到的ad值
volatile u16 adc_val_from_temp = 0; // 存放 从热敏电阻一侧 采集到的ad值
volatile u16 adc_val_from_fan;      // 存放 检测风扇一侧 采集到的ad值

volatile bit flag_tim_scan_fan_is_err = 0;      // 标志位，由定时器扫描并累计时间，表示当前风扇是否异常
volatile u8 cur_fan_status = FAN_STATUS_NORMAL; // 当前风扇状态

volatile u8 cur_adc_status = ADC_STATUS_NONE; // 状态机，表示当前adc的状态

volatile u8 adc_engine_val_buff_index = 0;
volatile u16 adc_engine_val_buff[16] = {0xFFFF};

// adc相关的引脚配置
void adc_pin_config(void)
{
    // P30--8脚配置为模拟输入模式
    P3_MD0 |= GPIO_P30_MODE_SEL(0x3);

    // P27--9脚配置为模拟输入模式
    P2_MD1 |= GPIO_P27_MODE_SEL(0x3);

    // P31--7脚配置为模拟输入模式
    P3_PU &= ~(0x01 << 1); // 关闭上拉
    P3_PD &= ~(0x01 << 1); // 关闭下拉
    P3_MD0 |= GPIO_P31_MODE_SEL(0x3);

    // P13 -- 芯片的1脚，配置为模拟输入模式
    P1_PU &= ~(0x01 << 3);             // 关闭上拉
    P1_PD &= ~(0x01 << 3);             // 关闭下拉
    P1_MD0 |= GPIO_P13_MODE_SEL(0x03); // 模拟IO工作模式
}

void adc_config(void)
{
    __EnableIRQ(ADC_IRQn);    // 使能ADC中断
    IE_EA = 1;                // 使能总中断
    ADC_CFG1 |= (0x0F << 3) | // ADC时钟分频为16分频，为系统时钟/16
                (0x01 << 0);  // ADC0 通道中断使能
    delay_ms(1);              // 等待adc稳定
}

void adc_channel_sel(u8 adc_sel_pin)
{
    ADC_ACON1 &= ~(ADC_VREF_SEL(0x7) |   // 清除选择的参考电压
                   ADC_INREF_SEL(0x01) | // 不选择内部参考电压 内部参考电压不使能
                   ADC_EXREF_SEL(0x01)); // 不选择外部参考电压 外部参考电压不使能

    ADC_ACON0 = ADC_CMP_EN(0x1) |  // 打开ADC中的CMP使能信号
                ADC_BIAS_EN(0x1) | // 打开ADC偏置电流能使信号
                ADC_BIAS_SEL(0x1); // 偏置电流：1x

    switch (adc_sel_pin)
    {
    case ADC_SEL_PIN_ENGINE:
        ADC_ACON1 |= ADC_INREF_SEL(0x01) | // 选择内部参考电压 内部参考电压使能
                     ADC_VREF_SEL(0x5) |   // 选择内部参考电压 4.2V (用户手册说未校准)
                     ADC_TEN_SEL(0x3);     /* 关闭测试信号 */
        ADC_CHS0 = ADC_ANALOG_CHAN(0x17) | // 选则引脚对应的通道（0x17--P27）
                   ADC_EXT_SEL(0x0);       // 选择外部通道
        break;

    case ADC_SEL_PIN_KNOB:
        ADC_ACON1 |= ADC_VREF_SEL(0x6) |   // 选择内部参考电压VCCA （选择VCCA，需要不使能内部参考电压，也不使能外部参考电压）
                     ADC_TEN_SEL(0x3);     // 关闭测试信号
        ADC_CHS0 = ADC_ANALOG_CHAN(0x19) | // 选则引脚对应的通道（0x19--P31）
                   ADC_EXT_SEL(0x0);       // 选择外部通道
        break;

    case ADC_SEL_PIN_TEMP:
        ADC_ACON1 |= ADC_INREF_SEL(0x01) | // 选择内部参考电压 内部参考电压使能
                     ADC_VREF_SEL(0x5) |   // 选择内部参考电压 4.2V
                     ADC_TEN_SEL(0x3);     // 关闭测试信号
        ADC_CHS0 = ADC_ANALOG_CHAN(0x18) | // 选则引脚对应的通道（0x18--P30）
                   ADC_EXT_SEL(0x0);       // 选择外部通道
        break;

    case ADC_SEL_PIN_FAN:
        ADC_ACON1 |= ADC_VREF_SEL(0x6) |   // 选择内部参考电压VCCA （选择VCCA，需要不使能内部参考电压，也不使能外部参考电压）
                     ADC_TEN_SEL(0x3);     // 关闭测试信号
        ADC_CHS0 = ADC_ANALOG_CHAN(0x0B) | // 选则引脚对应的通道（0x0B--P13）
                   ADC_EXT_SEL(0x0);       // 选择外部通道
        break;

    default:
        break;
    }

    __EnableIRQ(ADC_IRQn);          // 使能ADC中断
    IE_EA = 1;                      // 使能总中断
    ADC_CFG0 |= ADC_CHAN0_EN(0x1) | // 使能通道0
                ADC_EN(0x1);        // 使能adc
}

// 温度检测功能
void temperature_scan(void)
{
    if (temp_status == TEMP_STATUS_OVER_90_5_MIN)
    {
        // 温度已经超过90摄氏度且经过了5分钟，不再判断/检测电压，等待用户排查原因，再重启（重新上电）
        return;
    }

    if (adc_val_from_temp == 0)
    {
        // ad值没有被adc中断更新，还是初始值0，直接返回
        return;
    }

    switch (temp_status)
    {
    case TEMP_STATUS_NORMAL:
        if (adc_val_from_temp < ADC_VAL_WHEN_TEMP_OVER_90)
        {
            temp_status = TEMP_STATUS_OVER_90; // 状态标志设置为超过90摄氏度
        }
        break;

    case TEMP_STATUS_OVER_90:
        if (temp_over_heat_time_cnt >= CNT_5_MINUTES_IN_1MS_ISR)
        {
            temp_over_heat_time_cnt = 0;             // 清空时间计数器
            temp_status = TEMP_STATUS_OVER_90_5_MIN; // 状态标志设置为超过90摄氏度且超过5min
        }

        break;
    }
}

// 根据温度（电压值扫描）或9脚的状态来设定占空比
void set_duty(void)
{
    // 如果温度正常，根据9脚的状态来调节PWM占空比
    if (TEMP_STATUS_NORMAL == temp_status)
    {
        if (flag_is_time_to_check_engine)
        {
            flag_is_time_to_check_engine = 0;
            according_pin9_to_adjust_pwm();
        }
    }
    else if (TEMP_STATUS_OVER_90 == temp_status)
    {
        limited_pwm_duty_due_to_temp = PWM_DUTY_50_PERCENT; // 将pwm占空比限制到最大占空比的 50%
    }
    else if (TEMP_STATUS_OVER_90_5_MIN == temp_status)
    {
        limited_pwm_duty_due_to_temp = PWM_DUTY_25_PERCENT; // 将pwm占空比限制到最大占空比的 25%
    }
}

void fan_scan(void)
{
    u16 adc_val = adc_val_from_fan; // adc_val_from_fan 由adc中断触发

    /*
        1脚电压低于4.3V时，14，15脚输出25%占空比，
        1脚电压高于4.5V时，14，15脚输出100%占空比
    */
    if (FAN_STATUS_NORMAL == cur_fan_status)
    {
        if (adc_val <= ADC_VAL_WHEN_FAN_ERR)
        {
            flag_tim_scan_fan_is_err = 1; // 表示风扇异常，让定时器累计时间
        }
        else
        {
            // 风扇正常时，只要有一次ad值不满足异常的条件，便认为它是正常工作
            flag_tim_scan_fan_is_err = 0;
        }

        // 风扇正常工作，pwm正常输出
        limited_pwm_duty_due_to_fan_err = PWM_DUTY_100_PERCENT;
    }
    else // FAN_STATUS_ERROR == cur_fan_status
    {
        // 风扇异常时，检测到的ad值要与【风扇异常时对应的ad值】相隔一个死区，才认为风扇恢复正常
        if (adc_val >= ADC_VAL_WHEN_FAN_NORMAL)
        {
            flag_tim_scan_fan_is_err = 0; // 表示风扇正常
        }

        // 风扇工作异常，限制pwm输出，占空比不超过25%
        limited_pwm_duty_due_to_fan_err = PWM_DUTY_25_PERCENT;
    }
}

void ADC_IRQHandler(void) interrupt ADC_IRQn
{
    // 进入中断设置IP，不可删除
    __IRQnIPnPush(ADC_IRQn);

    // ---------------- 用户函数处理 -------------------

    if (ADC_STA & ADC_CHAN0_DONE(0x01))
    {
        volatile u16 adc_val = (ADC_DATAH0 << 4) | (ADC_DATAL0 >> 4); // 先接收ad值
        ADC_STA |= ADC_CHAN0_DONE(0x01);                              // 清除ADC0转换完成标志位

        if (ADC_STATUS_SEL_ENGINE == cur_adc_status)
        {
            // 更新发动机检测一端的ad值

            static u8 i = 0; // adc采集次数的计数
            static volatile u32 g_tmpbuff = 0;
            static volatile u16 g_adcmax = 0;
            static volatile u16 g_adcmin = 0xFFFF;

            if (i < 20)
            {
                i++;

                if (i >= 2) // 丢弃前两次采样值
                {
                    if (adc_val > g_adcmax)
                        g_adcmax = adc_val; // 最大
                    if (adc_val < g_adcmin)
                        g_adcmin = adc_val; // 最小
                    g_tmpbuff += adc_val;
                }

                if (i < 20)
                    ADC_CFG0 |= 0x01 << 0; // 开启adc0转换
            }

            if (i >= 20)
            {
                adc_val_from_engine = (g_tmpbuff >> 4); // 除以16，取平均值
                cur_adc_status = ADC_STATUS_SEL_ENGINE_DONE;

                // 重新初始化使用到的变量：
                i = 0;
                g_adcmax = 0;
                g_adcmin = 0xFFFF;
                g_tmpbuff = 0;
                // printf("1 engine scan done\n");
            }
        }
        else if (ADC_STATUS_SEL_KNOB == cur_adc_status)
        {
            // 更新旋钮检测一端的ad值
            adc_val_from_knob = adc_val;
            // printf("2 knob scan done\n");
        }
        else if (ADC_STATUS_SEL_GET_TEMP == cur_adc_status)
        {
            // 更新热敏电阻检测一端的ad值
            adc_val_from_temp = adc_val;
            // printf("3 temp scan done\n");
        }
        else if (ADC_STATUS_SEL_FAN_DETECT == cur_adc_status)
        {
            // 更新风扇检测一端的ad值
            adc_val_from_fan = adc_val;
            // printf("4 fan scan done\n");
        }
    }

    // 退出中断设置IP，不可删除
    __IRQnIPnPop(ADC_IRQn);
}
