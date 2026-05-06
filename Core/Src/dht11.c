#include "dht11.h"

/**
 * @brief 设置 DHT11 引脚为输出模式
 * @details 该函数将 DHT11 传感器的 GPIO 引脚配置为开漏输出模式，
 *          用于发送起始信号和读取数据
 * @param 无
 * @return 无
 * @note DHT11 使用单总线通信协议，需要在输出模式和输入模式之间切换
 */
static void DHT11_SetOutput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}
 
/**
 * @brief 读取 DHT11 数据位
 * @details 该函数从 DHT11 传感器读取一个数据位，用于读取温湿度数据，
 *          通过检测低电平和高电平的持续时间来判断数据位的值（0 或 1）
 * @param 无
 * @return 1 - 数据位为 1，0 - 数据位为 0
 * @note DHT11 数据位的定义：低电平持续约 50μs 后高电平持续约 26-28μs 表示 1，
 *       低电平持续约 50μs 后高电平持续约 70μs 表示 0
 */
static uint8_t DHT11_ReadBit(void)
{
    uint8_t retry = 0;  
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_RESET && retry < 100)
    {
        retry++;
        HAL_Delay_us(1);
    }
    HAL_Delay_us(30);
    if (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET)
    {
        retry = 0;
        while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET && retry < 100)
        {
            retry++;
            HAL_Delay_us(1);
        }
        return 1;
    }
    return 0;
}
 
/**
 * @brief 读取 DHT11 数据字节
 * @details 该函数从 DHT11 传感器读取一个完整的数据字节（8位），
 *          通过连续调用 DHT11_ReadBit 函数逐位读取数据
 * @param 无
 * @return 读取到的 8 位数据字节
 * @note 数据位按照从高位到低位的顺序读取，先读取的数据位作为字节的最高位
 */
static uint8_t DHT11_ReadByte(void)
{
    uint8_t i, byte = 0;  
    for (i = 0; i < 8; i++)
    {
        byte <<= 1;
        byte |= DHT11_ReadBit();
    }
    return byte;
}

/**
 * @brief 初始化 DHT11 温湿度传感器
 * @details 该函数初始化 DHT11 传感器的 GPIO 引脚为输出模式，
 *          并将引脚设置为高电平，为后续的数据读取做准备
 * @param 无
 * @return 无
 * @note DHT11 使用单总线通信协议，需要正确的时序控制
 */
void DHT11_Init(void)
{
    DHT11_SetOutput();
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
}

/**
 * @brief 读取 DHT11 温湿度数据
 * @details 该函数通过单总线协议读取 DHT11 传感器的温度和湿度数据，
 *          并进行校验和验证，确保数据的正确性
 * @param dat 指向 DHT11_Data 结构体的指针，用于存储读取的温湿度数据
 * @return 1 - 读取成功，0 - 读取失败
 * @note DHT11 的通信时序要求严格，需要精确的延时控制
 */
uint8_t DHT11_ReadData(DHT11_Data *dat)
{
    uint8_t buf[5];  
    uint8_t i;       
    uint16_t retry;    

    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET);
    HAL_Delay(20);
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
    HAL_Delay_us(30);

    retry = 0;
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET && retry < 100)
    {
        retry++;
        HAL_Delay_us(1);
    }
    if (retry >= 100) return 0; 
    
    retry = 0;
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_RESET && retry < 100)
    {
        retry++;
        HAL_Delay_us(1);
    }
    if (retry >= 100) return 0; 

    retry = 0;
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET && retry < 100)
    {
        retry++;
        HAL_Delay_us(1);
    }
    if (retry >= 100) return 0;  

    for (i = 0; i < 5; i++)
    {
        buf[i] = DHT11_ReadByte();
    }

    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);

    if (buf[0] + buf[1] + buf[2] + buf[3] == buf[4])
    {
        dat->hum = buf[0];   // 湿度整数部分
        dat->temp = buf[2];  // 温度整数部分
        return 1; 
    }
    return 0; 
}
