/*
 * ps2x.c - PS2 手柄驱动（纯 C 实现）
 * 基于 GPIO 模拟时序，使用 STM32 HAL 库
 */

#include "ps2.h"
#include <stddef.h>   // 如果需要 NULL

PS2_Controller_t ps2_controller;

// 静态延时函数（依赖定时器）
static void delay_us(PS2_Controller_t *controller, uint16_t us)
{
    __HAL_TIM_SET_COUNTER(controller->communicator.ps2_tim, 0);
    while (__HAL_TIM_GET_COUNTER(controller->communicator.ps2_tim) < us);
}

// 发送一个字节并读取一个字节
static uint8_t ps2_send_byte(PS2_Controller_t *controller, uint8_t cmd)
{
    uint8_t data = 0x00;
    uint16_t buffer;

    for (buffer = 0x01; buffer < 0x0100; buffer <<= 1)
    {
        // 设置 MOSI 电平
        if (buffer & cmd)
            HAL_GPIO_WritePin(controller->communicator.ps2_mosi_port,
                              controller->communicator.ps2_mosi_pin,
                              GPIO_PIN_SET);
        else
            HAL_GPIO_WritePin(controller->communicator.ps2_mosi_port,
                              controller->communicator.ps2_mosi_pin,
                              GPIO_PIN_RESET);

        // 拉高 SCK
        HAL_GPIO_WritePin(controller->communicator.ps2_sck_port,
                          controller->communicator.ps2_sck_pin,
                          GPIO_PIN_SET);
        delay_us(controller, 8);

        // 拉低 SCK
        HAL_GPIO_WritePin(controller->communicator.ps2_sck_port,
                          controller->communicator.ps2_sck_pin,
                          GPIO_PIN_RESET);
        delay_us(controller, 8);

        // 再次拉高 SCK，读取 MISO
        HAL_GPIO_WritePin(controller->communicator.ps2_sck_port,
                          controller->communicator.ps2_sck_pin,
                          GPIO_PIN_SET);
        if (HAL_GPIO_ReadPin(controller->communicator.ps2_miso_port,
                             controller->communicator.ps2_miso_pin))
        {
            data |= buffer;
        }
    }
    delay_us(controller, 8);
    return data;
}

// 发送指定长度的命令（含响应）
static void ps2_cmd(PS2_Controller_t *controller, uint8_t *transmit, uint8_t *receive, uint8_t length)
{
    HAL_GPIO_WritePin(controller->communicator.ps2_ss_port,
                      controller->communicator.ps2_ss_pin,
                      GPIO_PIN_RESET);
    delay_us(controller, 8);

    for (uint8_t i = 0; i < length; i++)
    {
        receive[i] = ps2_send_byte(controller, transmit[i]);
    }

    HAL_GPIO_WritePin(controller->communicator.ps2_ss_port,
                      controller->communicator.ps2_ss_pin,
                      GPIO_PIN_SET);
    delay_us(controller, 8);
}

// 轮询一次（进入配置模式前的握手）
static void ps2_poll(PS2_Controller_t *controller)
{
    uint8_t transmit[5] = {0x01, 0x42};
    uint8_t receive[5] = {0x00};
    ps2_cmd(controller, transmit, receive, 5);
}

// 进入配置模式
static void ps2_enter_config(PS2_Controller_t *controller)
{
    uint8_t transmit[5] = {0x01, 0x43, 0x00, 0x01};
    uint8_t receive[5] = {0x00};
    ps2_cmd(controller, transmit, receive, 5);
}

// 启用模拟模式
static void ps2_analog_mode(PS2_Controller_t *controller)
{
    uint8_t transmit[9] = {0x01, 0x44, 0x00, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00};
    uint8_t receive[9] = {0x00};
    ps2_cmd(controller, transmit, receive, 9);
}

// 退出配置模式
static void ps2_exit_config(PS2_Controller_t *controller)
{
    uint8_t transmit[9] = {0x01, 0x43, 0x00, 0x00, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A};
    uint8_t receive[9] = {0x00};
    ps2_cmd(controller, transmit, receive, 9);
}

// 初始化控制器结构体（单例模式）
void ps2_init(void)
{
    static uint8_t initialized = 0;

    if (!initialized)
    {
        // 配置通信引脚（根据实际硬件修改）
        ps2_controller.communicator.ps2_miso_port = GPIOF;
        ps2_controller.communicator.ps2_miso_pin = GPIO_PIN_0;
        ps2_controller.communicator.ps2_mosi_port = GPIOF;
        ps2_controller.communicator.ps2_mosi_pin = GPIO_PIN_1;
        ps2_controller.communicator.ps2_sck_port  = GPIOB;
        ps2_controller.communicator.ps2_sck_pin  = GPIO_PIN_12;
        ps2_controller.communicator.ps2_ss_port  = GPIOB;
        ps2_controller.communicator.ps2_ss_pin   = GPIO_PIN_13;
        ps2_controller.communicator.ps2_tim      = &htim9;  // 用于延时的定时器句柄

        // 清空状态
        ps2_controller.status.SELECT   = 0;
        ps2_controller.status.L3       = 0;
        ps2_controller.status.R3       = 0;
        ps2_controller.status.START    = 0;
        ps2_controller.status.UP       = 0;
        ps2_controller.status.RIGHT    = 0;
        ps2_controller.status.DOWN     = 0;
        ps2_controller.status.LEFT     = 0;
        ps2_controller.status.L2       = 0;
        ps2_controller.status.R2       = 0;
        ps2_controller.status.L1       = 0;
        ps2_controller.status.R1       = 0;
        ps2_controller.status.TRIANGLE = 0;
        ps2_controller.status.CIRCLE   = 0;
        ps2_controller.status.CROSS    = 0;
        ps2_controller.status.SQUARE   = 0;
        ps2_controller.status.RX       = 0;
        ps2_controller.status.RY       = 0;
        ps2_controller.status.LX       = 0;
        ps2_controller.status.LY       = 0;

        initialized = 1;
    }
}

// 手柄初始化配置（启动定时器并进入模拟模式）
void ps2_setup(PS2_Controller_t *controller)
{
    HAL_TIM_Base_Start(controller->communicator.ps2_tim);
    ps2_poll(controller);
    ps2_enter_config(controller);
    ps2_analog_mode(controller);
    ps2_poll(controller);
    ps2_exit_config(controller);
}

// 读取手柄当前状态（需周期性调用）
void ps2_control(PS2_Controller_t *controller)
{
    uint8_t transmit[9] = {0x01, 0x42};
    uint8_t receive[9] = {0x00};
    uint16_t button_status;

    ps2_cmd(controller, transmit, receive, 9);

    // 检查返回模式：0x41=数字模式，0x73=模拟模式，其它无效
    if (receive[1] != 0x41 && receive[1] != 0x73)
        return;

    // 解析按键（低字节在前，高字节在后）
    button_status = receive[3] | ((uint16_t)receive[4] << 8);
    controller->status.SELECT   = !((button_status >> 0) & 0x01);
    controller->status.L3       = !((button_status >> 1) & 0x01);
    controller->status.R3       = !((button_status >> 2) & 0x01);
    controller->status.START    = !((button_status >> 3) & 0x01);
    controller->status.UP       = !((button_status >> 4) & 0x01);
    controller->status.RIGHT    = !((button_status >> 5) & 0x01);
    controller->status.DOWN     = !((button_status >> 6) & 0x01);
    controller->status.LEFT     = !((button_status >> 7) & 0x01);
    controller->status.L2       = !((button_status >> 8) & 0x01);
    controller->status.R2       = !((button_status >> 9) & 0x01);
    controller->status.L1       = !((button_status >> 10) & 0x01);
    controller->status.R1       = !((button_status >> 11) & 0x01);
    controller->status.TRIANGLE = !((button_status >> 12) & 0x01);
    controller->status.CIRCLE   = !((button_status >> 13) & 0x01);
    controller->status.CROSS    = !((button_status >> 14) & 0x01);
    controller->status.SQUARE   = !((button_status >> 15) & 0x01);

    // 如果是模拟模式，解析摇杆值
    if (receive[1] != 0x73)
        return;

    controller->status.RX = receive[5];
    controller->status.RY = receive[6];
    controller->status.LX = receive[7];
    controller->status.LY = receive[8];
}
