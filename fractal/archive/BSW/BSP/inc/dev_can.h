#ifndef _DEV_CAN_H_
#define _DEV_CAN_H_

/**
 ******************************************************************************
 * @Copyright (C), 1997-2015, Hangzhou Gold Electronic Equipment Co., Ltd.
 * @file name: dev_can.h
 * @author: 叶建强
 * @Descriptiuon:
 *     CAN驱动
 * @Others: None
 * @History: 1. Created by YeJianqiang.
 * @version: V1.0.0
 * @date:    2024.1.18
 ******************************************************************************
 */
#include <stdint.h>
#include "DeviceManage.h"
//-----------------------------------------------------------------------------
//              配置
//-----------------------------------------------------------------------------
#define CAN_BUSOFF_LIMIT    (10)//busoff门限,超过该门限重启CAN

//-----------------------------------------------------------------------------
//              预定义
//-----------------------------------------------------------------------------
//CAN帧定义
typedef enum
{
	CAN_STD_DATA_FRAME = 0,//can标准数据帧
	CAN_STD_REMOTE_FRAME,//can标准远程帧,
	CAN_EXT_DATA_FRAME,//can扩展数据帧
	CAN_EXT_REMOTE_FRAME,//can扩展远程帧,
} Can_Frame_Type_Def;

/* CAN message */
typedef struct
{
	uint32_t id;  /* CAN frame ID value */
	Can_Frame_Type_Def frametype; /* CAN frame type and ID format */
	uint8_t len; /* CAN frame data length */
	uint8_t data[8]; /* 8 bytes data */
} Can_Frame_Def;

#define MSCAN_MessageTypeDef    Can_Frame_Def
//-----------------------------------------------------------------------------
//              变量申明
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
//              接口
//-----------------------------------------------------------------------------
int init_can(void);//初始化dec_can

int Can_Rx_Hook(int can, Can_Frame_Def* p_can_frame);

//功能: 向CAN0 写入数据
//返回值:1--成功,0--失败(溢出)
//int Send_Can_Tx_Frame(int can, Can_Frame_Def* p_message);

int Send_Can_Tx_Frame(Can_Chl_Def can, Can_Frame_Def* p_message);
//修改CAN0 波特率
void Modify_Can0_Baud(Can_Baud_Def can_baud);
void Modify_Can1_Baud(Can_Baud_Def can_baud);

//修改CAN2 波特率
void Modify_Can2_Baud(Can_Baud_Def can_baud);

//获取CAN0 故障状态
uint32_t Get_Can0_Err_code(void);

//获取CAN2 故障状态
uint32_t Get_Can2_Err_code(void);


#endif //_DEV_CAN_H_

//EOF
