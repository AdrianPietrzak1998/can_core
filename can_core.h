/*
 * can_core.h
 *
 *  Created on: May 23, 2025
 *      Author: Adrian
 */

#ifndef CAN_CORE_H_
#define CAN_CORE_H_

#include <limits.h>
#include <stdint.h>

#define CC_TICK_FROM_FUNC 0

#define CC_RX_BUFFER_SIZE 32
#define CC_TX_BUFFER_SIZE 32

#ifndef CC_TIME_BASE_TYPE_CUSTOM

#define CC_MAX_TIMEOUT UINT32_MAX
typedef volatile uint32_t CC_TIME_t;

#else

typedef CC_TIME_BASE_TYPE_CUSTOM CC_TIME_t;

#if defined(CC_TIME_BASE_TYPE_CUSTOM_IS_UINT8)
#define CC_MAX_TIMEOUT UINT8_MAX
#elif defined(CC_TIME_BASE_TYPE_CUSTOM_IS_UINT16)
#define CC_MAX_TIMEOUT UINT16_MAX
#elif defined(CC_TIME_BASE_TYPE_CUSTOM_IS_UINT32)
#define CC_MAX_TIMEOUT UINT32_MAX
#elif defined(CC_TIME_BASE_TYPE_CUSTOM_IS_UINT64)
#define CC_MAX_TIMEOUT UINT64_MAX
#elif defined(CC_TIME_BASE_TYPE_CUSTOM_IS_INT8)
#define CC_MAX_TIMEOUT INT8_MAX
#elif defined(CC_TIME_BASE_TYPE_CUSTOM_IS_INT16)
#define CC_MAX_TIMEOUT INT16_MAX
#elif defined(CC_TIME_BASE_TYPE_CUSTOM_IS_INT32)
#define CC_MAX_TIMEOUT INT32_MAX
#elif defined(CC_TIME_BASE_TYPE_CUSTOM_IS_INT64)
#define CC_MAX_TIMEOUT INT64_MAX
#else
#error "CC_MAX_TIMEOUT: Unknown CC_TIME_BASE_TYPE_CUSTOM or missing _IS_* define"
#endif

#endif

typedef enum{
	CC_BUS_BUSY = 0,
	CC_BUS_FREE
}CC_BusIsFree_t;

typedef enum{
	CC_MSG_UNREG,
	CC_MSG_REG
}CC_MsgRegStatus_t;

typedef struct {
	uint32_t ID;
	uint8_t Data[8];
	uint8_t DLC : 4;
	uint8_t IDE_flag :1;
	CC_TIME_t Time;
}CC_RX_message_t;

typedef struct CC_RX_instance_t CC_RX_instance_t;
typedef struct {
	uint16_t SlotNo;
	uint32_t ID;
	uint8_t DLC : 4;
	uint8_t IDE_flag :1;
	CC_TIME_t TimeOut;
	void (*Parser)(const CC_RX_instance_t *Instance, CC_RX_message_t *Msg, uint16_t Slot);
	CC_TIME_t LastTick;
}CC_RX_table_t;

struct CC_RX_instance_t
{
	CC_RX_message_t Buf[CC_RX_BUFFER_SIZE];
	uint16_t Head, Tail;
	CC_RX_table_t *RxTable;
	uint16_t TableSize;
	void (*Parser_unreg_msg)(const CC_RX_instance_t *Instance, CC_RX_message_t *Msg);
	void (*TimeoutCallback)(CC_RX_instance_t *Instance, uint16_t Slot);
};




typedef struct {
	uint32_t ID;
	uint8_t Data[8];
	uint8_t DLC : 4;
	uint8_t IDE_flag :1;
}CC_TX_message_t;

typedef struct CC_TX_instance_t CC_TX_instance_t;
typedef struct CC_TX_table_t CC_TX_table_t;
struct CC_TX_table_t
{
	uint16_t SlotNo;
	uint32_t ID;
	uint8_t *Data;
	uint8_t DLC : 4;
	uint8_t IDE_flag :1;
	CC_TIME_t SendFreq;
	void (*Parser)(const CC_TX_instance_t *Instance, uint8_t *Data, CC_TX_table_t *TxTable);
	CC_TIME_t LastTick;
};

struct CC_TX_instance_t
{
	CC_TX_message_t Buf[CC_TX_BUFFER_SIZE];
	uint16_t Head, Tail;
	CC_TX_table_t *TxTable;
	uint16_t TableSize;
	void (*SendFunction)(const CC_TX_instance_t *Instance, const CC_TX_message_t *msg);
	CC_BusIsFree_t (*BusCheck)(const CC_TX_instance_t *Instance);
};

// Tick source registration
#if CC_TICK_FROM_FUNC
void CC_tick_function_register(CC_TIME_t (*Function)(void));
#else
void CC_tick_variable_register(CC_TIME_t *Variable);
#endif

void CC_RX_PushMsg(CC_RX_instance_t *Instance, uint32_t ID, uint8_t *Data, uint8_t DLC, uint8_t IDE_flag);
void CC_RX_Poll(CC_RX_instance_t *Instance);

void CC_TX_PushMsg(CC_TX_instance_t *Instance, uint32_t ID, uint8_t *Data, uint8_t DLC, uint8_t IDE_flag);
void CC_TX_Poll(CC_TX_instance_t *Instance);

#endif /* CAN_CORE_H_ */
