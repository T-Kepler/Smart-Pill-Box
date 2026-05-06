#include "hx711.h"
#include "hardware_config.h"

#define HX711_SCK_H   HAL_GPIO_WritePin(HX711_SCK_PORT, HX711_SCK_PIN, GPIO_PIN_SET)
#define HX711_SCK_L   HAL_GPIO_WritePin(HX711_SCK_PORT, HX711_SCK_PIN, GPIO_PIN_RESET)
#define HX711_DOUT    HAL_GPIO_ReadPin(HX711_DT_PORT, HX711_DT_PIN)

#define TARE_SAMPLES      5
#define WEIGHT_SAMPLES    3
#define HX711_TIMEOUT     400000UL

static int32_t  g_tare_offset = 148325;
static float    g_coeff = 150000.0f;

int32_t g_last_raw = 0;

uint32_t weight;
uint32_t pi_weight = 148325;
uint32_t hx711_xishu = 150000;

/**
 * @brief  微秒级延时
 * @details 通过空操作循环实现近似微秒级延时，用于 HX711 通信时序控制
 * @param us 延时时间，单位微秒
 */
static void HX711_DelayUs(uint32_t us)
{
    volatile uint32_t n = us * 13;
    while (n--) __NOP();
}

/**
 * @brief  初始化 HX711 称重传感器
 * @details 配置 SCK 引脚为推挽输出、DOUT 引脚为上拉输入，
 *          并将 SCK 初始化为低电平，等待传感器上电稳定
 */
void HX711_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Pin = HX711_SCK_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(HX711_SCK_PORT, &GPIO_InitStruct);

    HX711_SCK_L;

    GPIO_InitStruct.Pin = HX711_DT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(HX711_DT_PORT, &GPIO_InitStruct);

    HAL_Delay(100);
}

/**
 * @brief  检查 HX711 数据是否就绪
 * @details HX711 在数据转换完成后会将 DOUT 拉低
 * @return 1 - 数据就绪，0 - 数据未就绪
 */
uint8_t HX711_IsReady(void)
{
    return (HX711_DOUT == GPIO_PIN_RESET);
}

/**
 * @brief  从 HX711 读取 24 位原始 ADC 值
 * @details 等待 DOUT 拉低表示数据就绪后，通过 SCK 时钟逐位读取 24 位数据，
 *          最后发送第 25 个时钟脉冲选择通道 A 增益 128。带超时保护，
 *          超时后返回 0。对 24 位结果进行符号扩展到 32 位有符号数
 * @return 24 位原始 ADC 值（符号扩展为 int32_t），超时返回 0
 */
int32_t HX711_ReadRaw(void)
{
    uint32_t value = 0;
    uint32_t timeout = HX711_TIMEOUT;
    int i;

    while (HX711_DOUT)
    {
        if (--timeout == 0)
        {
            g_last_raw = 0;
            return 0;
        }
    }

    for (i = 0; i < 24; i++)
    {
        HX711_SCK_H;
        HX711_DelayUs(1);

        value <<= 1;
        if (HX711_DOUT) value |= 1;

        HX711_SCK_L;
        HX711_DelayUs(1);
    }

    HX711_SCK_H;
    HX711_DelayUs(1);
    HX711_SCK_L;
    HX711_DelayUs(1);

    if (value & 0x800000) value |= 0xFF000000;

    g_last_raw = (int32_t)value;
    return g_last_raw;
}

/**
 * @brief  对整数数组进行升序排序（简单选择排序）
 * @param arr 待排序数组
 * @param n   数组元素个数
 */
static void Sort(int32_t arr[], int n)
{
    int i, j;
    for (i = 0; i < n - 1; i++)
    {
        for (j = i + 1; j < n; j++)
        {
            if (arr[i] > arr[j])
            {
                int32_t t = arr[i];
                arr[i] = arr[j];
                arr[j] = t;
            }
        }
    }
}

/**
 * @brief  执行去皮（清零）操作
 * @details 采样指定次数的原始 ADC 值，取中值作为去皮偏移量保存，
 *          后续称重将以此偏移为零点。最多支持 11 次采样，默认 5 次
 * @param times 采样次数，0 则使用默认值（5 次），最大 11 次
 */
void HX711_Tare(uint8_t times)
{
    int32_t samples[11];
    int i;

    if (times > 11) times = 11;
    if (times == 0) times = TARE_SAMPLES;

    for (i = 0; i < times; i++)
    {
        samples[i] = HX711_ReadRaw();
    }

    Sort(samples, times);

    g_tare_offset = samples[times / 2];
    pi_weight = (uint32_t)(g_tare_offset > 0 ? g_tare_offset : 0);
    weight = 0;
}

/**
 * @brief  获取当前重量值
 * @details 采样多次原始 ADC 值取中值，减去皮重偏移后乘以校准系数换算为重量，
 *          结果四舍五入取整存入全局变量 weight。净重为负或零时返回 0
 * @return 当前重量值（单位与校准系数对应），无负载时返回 0
 */
int32_t HX711_GetWeight(void)
{
    int32_t samples[WEIGHT_SAMPLES];
    int32_t net;
    float w;
    int i;

    for (i = 0; i < WEIGHT_SAMPLES; i++)
    {
        samples[i] = HX711_ReadRaw();
    }

    Sort(samples, WEIGHT_SAMPLES);

    net = samples[WEIGHT_SAMPLES / 2] - g_tare_offset;

    if (net <= 0)
    {
        weight = 0;
        return 0;
    }

    w = (float)net * g_coeff / 10000000.0f;
    if (w < 0.0f) w = 0.0f;

    weight = (uint32_t)(w + 0.5f);
    return (int32_t)weight;
}

/**
 * @brief  设置 HX711 重量校准系数
 * @details 校准系数用于将 ADC 原始值偏移转换为实际重量值，
 *          仅接受大于 0.01 的有效系数
 * @param coeff 校准系数，必须大于 0.01
 */
void HX711_SetCoefficient(float coeff)
{
    if (coeff > 0.01f) g_coeff = coeff;
    hx711_xishu = (uint32_t)(coeff + 0.5f);
}
