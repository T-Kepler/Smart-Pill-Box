#include "cw2015.h"
#include "i2c.h"

#define CW2015_REG_BATINFO     0x10
#define CW2015_REG_VCELL       0x02
#define CW2015_REG_SOC         0x04
#define CW2015_REG_CONFIG      0x08
#define CW2015_REG_MODE        0x0A

#define SW_SCL_PIN  GPIO_PIN_10
#define SW_SCL_PORT GPIOB
#define SW_SDA_PIN  GPIO_PIN_11
#define SW_SDA_PORT GPIOB

static uint8_t cw_i2c_addr = 0x64;

static uint8_t cw_bat_config_info[64] = {
0X15,0X56,0X54,0X48,0X50,0X56,0X51,0X48,0X43,0X41,0X44,0X4F,0X5D,0X54,0X3E,0X38,
0X33,0X32,0X30,0X33,0X3B,0X4F,0X65,0X6D,0X44,0X38,0X07,0XAE,0X11,0X22,0X40,0X40,
0X54,0X64,0X6D,0X6E,0X29,0X0B,0X79,0X0D,0X06,0X20,0X32,0X65,0X79,0X7C,0X7C,0X32,
0X4D,0X64,0X7B,0X93,0X64,0X83,0X92,0XAF,0X1B,0X00,0X64,0X8F,0X9F,0X11,0XC0,0X11
};

/**
 * @brief  软件 I2C 位延时
 * @details 通过空操作循环实现约数微秒的延时，用于软件 I2C 时序控制
 */
static void SW_I2C_Delay(void)
{
    volatile uint8_t d = 15;
    while (d--) __NOP();
}

/**
 * @brief  配置 SCL 引脚为开漏输出
 */
static void SW_SCL_Out(void)
{
    GPIO_InitTypeDef g = {0};
    g.Pin = SW_SCL_PIN;
    g.Mode = GPIO_MODE_OUTPUT_OD;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(SW_SCL_PORT, &g);
}

/**
 * @brief  配置 SDA 引脚为开漏输出
 */
static void SW_SDA_Out(void)
{
    GPIO_InitTypeDef g = {0};
    g.Pin = SW_SDA_PIN;
    g.Mode = GPIO_MODE_OUTPUT_OD;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(SW_SDA_PORT, &g);
}

/**
 * @brief  配置 SDA 引脚为上拉输入
 */
static void SW_SDA_In(void)
{
    GPIO_InitTypeDef g = {0};
    g.Pin = SW_SDA_PIN;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(SW_SDA_PORT, &g);
}

static void SW_I2C_SDA_H(void) { SW_SDA_In(); }
static void SW_I2C_SDA_L(void) { SW_SDA_Out(); HAL_GPIO_WritePin(SW_SDA_PORT, SW_SDA_PIN, GPIO_PIN_RESET); }
static uint8_t SW_I2C_SDA_Read(void) { return HAL_GPIO_ReadPin(SW_SDA_PORT, SW_SDA_PIN); }
static void SW_I2C_SCL_H(void) { HAL_GPIO_WritePin(SW_SCL_PORT, SW_SCL_PIN, GPIO_PIN_SET); }
static void SW_I2C_SCL_L(void) { HAL_GPIO_WritePin(SW_SCL_PORT, SW_SCL_PIN, GPIO_PIN_RESET); }

/**
 * @brief  软件 I2C 总线恢复
 * @details 当 I2C 总线被从设备拉死时，通过发送 20 个 SCL 时钟脉冲
 *          并产生 STOP 条件来释放总线
 */
static void SW_I2C_BusRecovery(void)
{
    uint8_t i;
    SW_SCL_Out();
    SW_SDA_In();

    for (i = 0; i < 20; i++)
    {
        SW_I2C_SCL_L();
        SW_I2C_Delay();
        SW_I2C_SCL_H();
        SW_I2C_Delay();
    }

    SW_SDA_Out();
    SW_I2C_SDA_L();
    SW_I2C_Delay();
    SW_I2C_SCL_L();
    SW_I2C_Delay();
    SW_I2C_SCL_H();
    SW_I2C_Delay();
    SW_SDA_In();
    SW_I2C_Delay();
    SW_I2C_SCL_L();
    SW_I2C_Delay();
}

/**
 * @brief  产生 I2C 起始条件
 * @details SCL 为高电平时，SDA 从高电平拉低，产生 START 信号
 */
static void SW_I2C_Start(void)
{
    SW_SDA_Out();
    SW_I2C_SDA_H();
    SW_I2C_SCL_H();
    SW_I2C_Delay();
    SW_I2C_Delay();
    SW_I2C_SDA_L();
    SW_I2C_Delay();
    SW_I2C_SCL_L();
    SW_I2C_Delay();
}

/**
 * @brief  产生 I2C 停止条件
 * @details SCL 为高电平时，SDA 从低电平拉高，产生 STOP 信号
 */
static void SW_I2C_Stop(void)
{
    SW_SDA_Out();
    SW_I2C_SDA_L();
    SW_I2C_Delay();
    SW_I2C_SCL_H();
    SW_I2C_Delay();
    SW_I2C_SDA_H();
    SW_I2C_Delay();
}

/**
 * @brief  等待从设备 ACK 应答
 * @return 0 - 收到 ACK，1 - 超时未收到 ACK
 */
static uint8_t SW_I2C_WaitAck(void)
{
    uint16_t timeout = 500;

    SW_SDA_In();
    SW_I2C_Delay();
    SW_I2C_SCL_H();
    SW_I2C_Delay();

    while (SW_I2C_SDA_Read() == GPIO_PIN_SET)
    {
        if (--timeout == 0)
        {
            SW_I2C_SCL_L();
            return 1;
        }
    }

    SW_I2C_SCL_L();
    SW_I2C_Delay();
    return 0;
}

/**
 * @brief  发送 ACK 应答信号
 */
static void SW_I2C_SendAck(void)
{
    SW_SDA_Out();
    SW_I2C_SDA_L();
    SW_I2C_Delay();
    SW_I2C_SCL_H();
    SW_I2C_Delay();
    SW_I2C_SCL_L();
    SW_I2C_Delay();
}

/**
 * @brief  发送 NACK 非应答信号
 */
static void SW_I2C_SendNack(void)
{
    SW_SDA_Out();
    SW_I2C_SDA_H();
    SW_I2C_Delay();
    SW_I2C_SCL_H();
    SW_I2C_Delay();
    SW_I2C_SCL_L();
    SW_I2C_Delay();
}

/**
 * @brief  通过 I2C 发送一个字节
 * @param byte 要发送的字节数据，高位先发
 */
static void SW_I2C_SendByte(uint8_t byte)
{
    uint8_t i;
    SW_SDA_Out();
    for (i = 0; i < 8; i++)
    {
        if (byte & 0x80)
            SW_I2C_SDA_H();
        else
            SW_I2C_SDA_L();
        byte <<= 1;
        SW_I2C_Delay();
        SW_I2C_SCL_H();
        SW_I2C_Delay();
        SW_I2C_SCL_L();
        SW_I2C_Delay();
    }
}

/**
 * @brief  通过 I2C 读取一个字节
 * @return 读取到的字节数据，高位先读
 */
static uint8_t SW_I2C_ReadByte(void)
{
    uint8_t i;
    uint8_t byte = 0;
    SW_SDA_In();
    for (i = 0; i < 8; i++)
    {
        byte <<= 1;
        SW_I2C_SCL_H();
        SW_I2C_Delay();
        if (SW_I2C_SDA_Read() == GPIO_PIN_SET)
            byte |= 1;
        SW_I2C_SCL_L();
        SW_I2C_Delay();
    }
    return byte;
}

/**
 * @brief  通过软件 I2C 向指定设备的寄存器写入数据
 * @param dev_addr 设备 7 位地址
 * @param reg      目标寄存器地址
 * @param data     待写入数据指针
 * @param len      待写入数据长度
 * @return 0 - 写入成功，1 - 通信失败（NACK）
 */
static uint8_t SW_I2C_WriteReg(uint8_t dev_addr, uint8_t reg, uint8_t *data, uint16_t len)
{
    uint16_t i;
    SW_I2C_Start();
    SW_I2C_SendByte(dev_addr << 1);
    if (SW_I2C_WaitAck()) { SW_I2C_Stop(); return 1; }
    SW_I2C_SendByte(reg);
    if (SW_I2C_WaitAck()) { SW_I2C_Stop(); return 1; }
    for (i = 0; i < len; i++)
    {
        SW_I2C_SendByte(data[i]);
        if (SW_I2C_WaitAck()) { SW_I2C_Stop(); return 1; }
    }
    SW_I2C_Stop();
    return 0;
}

/**
 * @brief  通过软件 I2C 从指定设备的寄存器读取数据
 * @param dev_addr 设备 7 位地址
 * @param reg      目标寄存器地址
 * @param buf      读取数据存放缓冲区
 * @param len      读取数据长度
 * @return 0 - 读取成功，1 - 通信失败（NACK）
 */
static uint8_t SW_I2C_ReadReg(uint8_t dev_addr, uint8_t reg, uint8_t *buf, uint16_t len)
{
    uint16_t i;
    SW_I2C_Start();
    SW_I2C_SendByte(dev_addr << 1);
    if (SW_I2C_WaitAck()) { SW_I2C_Stop(); return 1; }
    SW_I2C_SendByte(reg);
    if (SW_I2C_WaitAck()) { SW_I2C_Stop(); return 1; }

    SW_I2C_Start();
    SW_I2C_SendByte((dev_addr << 1) | 1);
    if (SW_I2C_WaitAck()) { SW_I2C_Stop(); return 1; }

    for (i = 0; i < len; i++)
    {
        buf[i] = SW_I2C_ReadByte();
        if (i < len - 1)
            SW_I2C_SendAck();
        else
            SW_I2C_SendNack();
    }
    SW_I2C_Stop();
    return 0;
}

/**
 * @brief  探测指定地址的 CW2015 是否存在
 * @param addr 待探测的 I2C 设备地址
 * @return 1 - 设备存在，0 - 设备无响应
 */
static uint8_t CW2015_ProbeAddr(uint8_t addr)
{
    uint8_t buf[1];
    return (SW_I2C_ReadReg(addr, CW2015_REG_MODE, buf, 1) == 0);
}

/**
 * @brief  扫描 CW2015 的 I2C 设备地址
 * @details 先尝试 CW2015 常用地址列表（0x62-0x66），若均无响应则进行
 *          全地址范围（0x08-0x77）扫描，找到第一个响应的设备地址
 * @return 找到的设备地址，0 表示未找到
 */
static uint8_t CW2015_ScanAddr(void)
{
    uint8_t addr;
    uint8_t addrs[] = {0x62, 0x63, 0x64, 0x65, 0x66};

    for (uint8_t idx = 0; idx < sizeof(addrs); idx++)
    {
        addr = addrs[idx];
        if (CW2015_ProbeAddr(addr))
        {
            return addr;
        }
        HAL_Delay(5);
    }

    SW_I2C_Start();
    SW_I2C_SendByte(0x00);
    HAL_Delay(1);
    SW_I2C_Stop();

    for (uint8_t a = 0x08; a < 0x78; a++)
    {
        SW_I2C_Start();
        SW_I2C_SendByte(a << 1);
        if (SW_I2C_WaitAck() == 0)
        {
            SW_I2C_Stop();
            return a;
        }
        SW_I2C_Stop();
        HAL_Delay(1);
    }

    return 0;
}

/**
 * @brief  根据电池电压线性估算电量百分比
 * @param voltage_mv 电池电压，单位毫伏
 * @return 估算的电量百分比（0-100），4200mV 对应 100%，3000mV 及以下对应 0%
 */
static uint8_t CW2015_SocFromVoltage(uint16_t voltage_mv)
{
    if (voltage_mv >= 4200) return 100;
    if (voltage_mv <= 3000) return 0;
    return (uint8_t)((uint32_t)(voltage_mv - 3000) * 100 / 1200);
}

/**
 * @brief  反初始化硬件 I2C2 外设
 * @details 关闭 I2C2 时钟并释放 SCL/SDA 引脚，为软件 I2C 腾出 GPIO 资源
 */
static void CW2015_DeInitHWI2C(void)
{
    HAL_I2C_DeInit(&hi2c2);
    __HAL_RCC_I2C2_CLK_DISABLE();
    HAL_GPIO_DeInit(SW_SCL_PORT, SW_SCL_PIN);
    HAL_GPIO_DeInit(SW_SDA_PORT, SW_SDA_PIN);
}

/**
 * @brief  初始化 CW2015 电量计芯片
 * @details 完整的初始化流程：反初始化硬件 I2C → 总线恢复 → 扫描设备地址 →
 *          复位芯片 → 写入电池配置信息 → 设置配置寄存器 → 退出休眠模式。
 *          若扫描不到设备地址则直接返回
 */
void CW2015_Init(void)
{
    uint8_t buf[2];

    __HAL_RCC_GPIOB_CLK_ENABLE();

    CW2015_DeInitHWI2C();

    SW_I2C_BusRecovery();
    HAL_Delay(10);

    cw_i2c_addr = CW2015_ScanAddr();
    if (cw_i2c_addr == 0)
    {
        return;
    }

    buf[0] = 0x30;
    SW_I2C_WriteReg(cw_i2c_addr, CW2015_REG_MODE, buf, 1);
    HAL_Delay(20);

    buf[0] = 0x00;
    SW_I2C_WriteReg(cw_i2c_addr, CW2015_REG_MODE, buf, 1);
    HAL_Delay(100);

    SW_I2C_WriteReg(cw_i2c_addr, CW2015_REG_BATINFO, cw_bat_config_info, 64);
    HAL_Delay(200);

    buf[0] = 0xC0;
    SW_I2C_WriteReg(cw_i2c_addr, CW2015_REG_CONFIG, buf, 1);
    HAL_Delay(50);

    buf[0] = 0x04;
    SW_I2C_WriteReg(cw_i2c_addr, CW2015_REG_MODE, buf, 1);
    HAL_Delay(100);

    buf[0] = 0x00;
    SW_I2C_WriteReg(cw_i2c_addr, CW2015_REG_MODE, buf, 1);
    HAL_Delay(200);
}

/**
 * @brief  读取 CW2015 电池电压和电量数据
 * @details 分别读取 VCELL 寄存器获取电池电压、SOC 寄存器获取电量百分比，
 *          各最多重试 3 次。若 SOC 读取失败但电压读取成功，则使用电压线性估算电量
 * @param data 输出参数，存储电压（mV）、电量百分比（SOC）和通信状态
 */
void CW2015_ReadData(CW2015_Data *data)
{
    uint8_t buf[2];
    uint16_t raw;
    uint8_t retry;

    data->comm_ok = 0;

    retry = 3;
    while (retry--)
    {
        if (SW_I2C_ReadReg(cw_i2c_addr, CW2015_REG_VCELL, buf, 2) == 0)
        {
            raw = ((uint16_t)buf[0] << 8) | buf[1];
            data->voltage_mv = (uint16_t)((uint32_t)raw * 305 / 1000);
            data->comm_ok = 1;
            break;
        }
        HAL_Delay(5);
    }

    retry = 3;
    while (retry--)
    {
        if (SW_I2C_ReadReg(cw_i2c_addr, CW2015_REG_SOC, buf, 2) == 0)
        {
            data->soc = buf[0];
            if (data->soc > 100) data->soc = 100;
            break;
        }
        HAL_Delay(5);
    }

    if (!data->comm_ok && data->voltage_mv > 0)
    {
        data->soc = CW2015_SocFromVoltage(data->voltage_mv);
    }
}
