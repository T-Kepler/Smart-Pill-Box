#include "servo.h"
#include "tim.h"
#include "hardware_config.h"

typedef enum {
    SERVO_STATE_CLOSED = 0,
    SERVO_STATE_OPEN = 1
} Servo_State;

static Servo_State servo_state = SERVO_STATE_CLOSED;
static uint8_t servo_initialized = 0;

/**
 * @brief 初始化舵机系统
 * @details 该函数初始化舵机的 PWM 输出，将舵机设置为关闭位置，
 *          为后续的药盒开合控制做准备
 * @param 无
 * @return 无
 * @note 舵机使用 PWM 信号控制，关闭位置为 500μs，打开位置为 2000μs
 */
void Servo_Init(void)
{
    HAL_TIM_PWM_Start(&htim3, SERVO_TIM_CHANNEL);
    __HAL_TIM_SET_COMPARE(&htim3, SERVO_TIM_CHANNEL, SERVO_CLOSE_US);
    servo_state = SERVO_STATE_CLOSED;
    servo_initialized = 1;
}

/**
 * @brief 打开药盒
 * @details 该函数控制舵机转动到打开位置，实现药盒的开启功能
 * @param 无
 * @return 无
 * @note 如果舵机未初始化或已经处于打开状态，则不执行任何操作
 */
void Servo_Open(void)
{
    if (!servo_initialized) return;

    if (servo_state == SERVO_STATE_OPEN)
    {
        return;
    }

    __HAL_TIM_SET_COMPARE(&htim3, SERVO_TIM_CHANNEL, SERVO_OPEN_US);
    servo_state = SERVO_STATE_OPEN;
}

/**
 * @brief 关闭药盒
 * @details 该函数控制舵机转动到关闭位置，实现药盒的关闭功能
 * @param 无
 * @return 无
 * @note 如果舵机未初始化或已经处于关闭状态，则不执行任何操作
 */
void Servo_Close(void)
{
    if (!servo_initialized) return;

    if (servo_state == SERVO_STATE_CLOSED)
    {
        return;
    }

    __HAL_TIM_SET_COMPARE(&htim3, SERVO_TIM_CHANNEL, SERVO_CLOSE_US);
    servo_state = SERVO_STATE_CLOSED;
}

/**
 * @brief 切换药盒状态
 * @details 该函数根据当前舵机状态，执行相反的操作，
 *          实现药盒开合状态的切换
 * @param 无
 * @return 无
 * @note 如果当前是打开状态则关闭，如果是关闭状态则打开
 */
void Servo_Toggle(void)
{
    if (servo_state == SERVO_STATE_OPEN)
    {
        Servo_Close();
    }
    else
    {
        Servo_Open();
    }
}


/**
 * @brief 获取舵机状态
 * @details 该函数返回当前舵机的状态，用于判断药盒是打开还是关闭
 * @param 无
 * @return 舵机状态：0-关闭，1-打开
 */
uint8_t Servo_GetState(void)
{
    return (uint8_t)servo_state;
}

/**
 * @brief 检查药盒是否打开
 * @details 该函数检查当前药盒的状态，判断是否处于打开状态
 * @param 无
 * @return 1-药盒已打开，0-药盒已关闭
 */
uint8_t Servo_IsOpen(void)
{
    return servo_state == SERVO_STATE_OPEN ? 1 : 0;
}
