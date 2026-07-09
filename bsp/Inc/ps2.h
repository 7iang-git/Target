#ifndef PS2X_H_
#define PS2X_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "gpio.h"
#include "tim.h"

    typedef struct
    {
        GPIO_TypeDef *ps2_miso_port;
        uint16_t ps2_miso_pin;
        GPIO_TypeDef *ps2_mosi_port;
        uint16_t ps2_mosi_pin;
        GPIO_TypeDef *ps2_sck_port;
        uint16_t ps2_sck_pin;
        GPIO_TypeDef *ps2_ss_port;
        uint16_t ps2_ss_pin;
        TIM_HandleTypeDef *ps2_tim;
    } PS2_Communicator_Controller_t;

    typedef struct
    {
        uint8_t SELECT;
        uint8_t L3;
        uint8_t R3;
        uint8_t START;
        uint8_t UP;
        uint8_t RIGHT;
        uint8_t DOWN;
        uint8_t LEFT;
        uint8_t L2;
        uint8_t R2;
        uint8_t L1;
        uint8_t R1;
        uint8_t TRIANGLE;
        uint8_t CIRCLE;
        uint8_t CROSS;
        uint8_t SQUARE;
        uint8_t RX;
        uint8_t RY;
        uint8_t LX;
        uint8_t LY;
    } PS2_Status_Controller_t;

    typedef struct
    {
        PS2_Communicator_Controller_t communicator;
        PS2_Status_Controller_t status;
    } PS2_Controller_t;

    void ps2_init(void);
    void ps2_setup(PS2_Controller_t *controller);
    void ps2_control(PS2_Controller_t *controller);
	extern PS2_Controller_t ps2_controller;
#ifdef __cplusplus
}
#endif
#endif /* INC_PS2_PS2_H_ */
