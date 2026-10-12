#include "myadc.h"
#include "adc.h"

uint16_t voltage_read_raw(void)
{
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
    {
        return HAL_ADC_GetValue(&hadc1);
    }
    return 0;
}

float voltage_read_battery(void)
{
    /* ==================== 单点校准系数 CAL_SCALE 计算方法 ====================
     * 1. 用万用表量电池真实电压 V_real(如 12.45V)
     * 2. 在电池为 V_real 时, 读一次 ADC 原始值 raw(如 3471)
     * 3. CAL_SCALE = V_real / raw = 12.45 / 3471 ≈ 0.003587
     *
     * 原理: 电压 = raw × CAL_SCALE, 该系数一次性打包了
     *       - 分压电阻的容差
     *       - ADC 参考电压的真实值(负载下的实际值, 而非空载测得)
     *       - ADC 本身的增益误差
     * 前提: 链路线性(电阻分压 + 12位ADC基本线性), 一个点标定后全量程适用
     *
     * 重新标定: 换电池/换板子后, 量新电池电压V_new, 读新raw_new,
     *           更新为 CAL_SCALE = V_new / raw_new
     * ====================================================================== */
    const float CAL_SCALE = 0.003587f;
    return (float)voltage_read_raw() * CAL_SCALE;
}