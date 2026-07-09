#ifndef BSP_CAN_H
#define BSP_CAN_H
#include "can.h"
#include "DJ_Motor.h"
#include "DM_Control.h"
#include "DM_Motor.h"

//过滤器配置
// FIFO 0 接收ID
#define CAN1_FIFO0_ID0 0x201
#define CAN1_FIFO0_ID1 0x202
#define CAN1_FIFO0_ID2 0x203
#define CAN1_FIFO0_ID3 0x204

// FIFO 1 接收ID
#define CAN1_FIFO1_ID0 0x205
#define CAN1_FIFO1_ID1 0x206
#define CAN1_FIFO1_ID2 0x207
#define CAN1_FIFO1_ID3 0x01  //给DM的

// FIFO 0 接收ID
#define CAN2_FIFO0_ID0 0x10//0x201
#define CAN2_FIFO0_ID1 0x202
#define CAN2_FIFO0_ID2 0x203
#define CAN2_FIFO0_ID3 0x204


// FIFO 1 接收ID
#define CAN2_FIFO1_ID0 0x205
#define CAN2_FIFO1_ID1 0x206
#define CAN2_FIFO1_ID2 0x207
#define CAN2_FIFO1_ID3 0x208


void can_filter_init(void);

#endif
