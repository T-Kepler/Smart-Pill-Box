#include "actuator.h"
#include "hardware_config.h"

Device_State device_state = {0, 0, 0, DEFAULT_VOLUME, 0};

static Actuator_Thresholds thresholds = {
    TEMP_HIGH_THRESHOLD,
    TEMP_LOW_THRESHOLD,
    LIGHT_ON_THRESHOLD,
    LIGHT_OFF_THRESHOLD
};

static uint8_t fan_latch = 0;
static uint8_t heater_latch = 0;

/**
 * @brief 初始化执行器系统
 * @details 该函数初始化所有执行器相关的硬件状态和设备状态变量，
 *          包括 LED、风扇、加热器的 GPIO 引脚状态，以及设备状态结构体的初始值
 * @param 无
 * @return 无
 * @note 函数将 LED、风扇和加热器的 GPIO 引脚设置为高电平（关闭状态），
 *       并初始化设备状态结构体中的各个成员变量为默认值
 */
void Actuator_Init(void)
{
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(FAN_PORT, FAN_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(HEATER_PORT, HEATER_PIN, GPIO_PIN_SET);
    device_state.led_state = 0;
    device_state.fan_state = 0;
    device_state.heater_state = 0;
    device_state.current_volume = DEFAULT_VOLUME;
    device_state.light_percent = 0;
}

/**
 * @brief 更新执行器状态
 * @details 该函数根据环境参数（温度、湿度、光线强度）自动控制 LED、风扇和加热器的工作状态，
 *          使用滞回控制策略防止设备频繁启停，提高系统稳定性
 * @param temp 当前温度值（单位：摄氏度）
 * @param hum 当前湿度值（单位：百分比，暂未使用）
 * @param light_percent 当前光线强度（单位：百分比）
 * @return 无
 * @note 函数使用滞回控制策略防止设备频繁启停，提高系统稳定性
 */
void Actuator_Update(uint8_t temp, uint8_t hum, uint8_t light_percent)
{
    if (light_percent > 100) light_percent = 100;
    device_state.light_percent = light_percent;

    if (light_percent < thresholds.light_on)
    {
        HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
        device_state.led_state = 1;
    }
    else if (light_percent > thresholds.light_off)
    {
        HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
        device_state.led_state = 0;
    }

    if (temp >= thresholds.temp_high)
    {
        fan_latch = 1;
    }
    else if (temp <= (thresholds.temp_high - 2))
    {
        fan_latch = 0;
    }

    if (fan_latch)
    {
        HAL_GPIO_WritePin(FAN_PORT, FAN_PIN, GPIO_PIN_RESET);
        device_state.fan_state = 1;
    }
    else
    {
        HAL_GPIO_WritePin(FAN_PORT, FAN_PIN, GPIO_PIN_SET);
        device_state.fan_state = 0;
    }

    if (temp <= thresholds.temp_low)
    {
        heater_latch = 1;
    }
    else if (temp >= (thresholds.temp_low + 2))
    {
        heater_latch = 0;
    }

    if (heater_latch)
    {
        HAL_GPIO_WritePin(HEATER_PORT, HEATER_PIN, GPIO_PIN_SET);
        device_state.heater_state = 1;
    }
    else
    {
        HAL_GPIO_WritePin(HEATER_PORT, HEATER_PIN, GPIO_PIN_RESET);
        device_state.heater_state = 0;
    }
}
/**
 * @brief 获取执行器阈值设置
 * @details 该函数将当前的执行器阈值设置复制到指定的结构体中，
 *          包括高温阈值、低温阈值、光线开启阈值和光线关闭阈值
 * @param th 指向 Actuator_Thresholds 结构体的指针，用于存储阈值设置
 * @return 无
 * @note 函数会检查输入指针是否有效，如果为 NULL 则直接返回
 */
void Actuator_GetThresholds(Actuator_Thresholds *th)
{
    if (!th) return;
    th->temp_high = thresholds.temp_high;
    th->temp_low = thresholds.temp_low;
    th->light_on = thresholds.light_on;
    th->light_off = thresholds.light_off;
}
/**
 * @brief 设置执行器阈值
 * @details 该函数根据输入的阈值设置更新系统的执行器阈值，
 *          包括高温阈值、低温阈值、光线开启阈值和光线关闭阈值
 * @param th 指向 Actuator_Thresholds 结构体的指针，包含新的阈值设置
 * @return 无
 * @note 函数会对每个阈值进行有效性检查，确保阈值在合理范围内
 */
void Actuator_SetThresholds(Actuator_Thresholds *th)
{
    if (!th) return;
    if (th->temp_high > 0 && th->temp_high < 50) thresholds.temp_high = th->temp_high;
    if (th->temp_low > 0 && th->temp_low < 40) thresholds.temp_low = th->temp_low;
    if (th->light_on > 0 && th->light_on <= 100) thresholds.light_on = th->light_on;
    if (th->light_off > 0 && th->light_off <= 100) thresholds.light_off = th->light_off;
}
