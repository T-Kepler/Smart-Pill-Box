#include "light_sensor.h"
#include "adc.h"
#include "hardware_config.h"

/**
 * @brief 获取光线传感器百分比
 * @details 该函数通过 ADC 读取光线传感器的模拟值，并将其转换为百分比形式，
 *          用于自动控制 LED 的开关
 * @param 无
 * @return 光线强度百分比（0-100），值越大表示光线越强
 * @note ADC 值越大表示光线越暗，百分比越大表示光线越亮
 */
uint8_t LightSensor_GetPercent(void)
{
    uint16_t adc_val;   
    uint8_t percent;    
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
    {
        adc_val = HAL_ADC_GetValue(&hadc1);
    }
    else
    {
        adc_val = 0;
    }
    HAL_ADC_Stop(&hadc1);

    percent =(uint8_t)((uint32_t)adc_val * 100 / 4095);
    if (percent > 100) percent = 100;

    return percent;
}
