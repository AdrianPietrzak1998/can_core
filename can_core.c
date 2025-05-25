/*
 * can_core.c
 *
 *  Created on: May 23, 2025
 *      Author: Adrian
 */

#include "can_core.h"
#include "assert.h"
#include <stddef.h>

#if CC_TICK_FROM_FUNC

CC_TIME_t (*CC_get_tick)(void) = NULL;

#define CC_GET_TICK ((CC_get_tick != NULL) ? CC_get_tick() : ((CC_TIME_t)0))

void CC_tick_function_register(CC_TIME_t (*Function)(void))
{
	assert(Function != NULL);

    CC_get_tick = Function;
}

#else

CC_TIME_t *CC_tick = NULL;

#define CC_GET_TICK (*(CC_tick))

void CC_tick_variable_register(CC_TIME_t *Variable)
{
	assert(Variable != NULL);

    CC_tick = Variable;
}

#endif

static inline void CopyBuf(uint8_t *restrict src, uint8_t *restrict dst, size_t size)
{
	assert((src != NULL) || (dst != NULL));

	for(size_t i; i < size; i++)
	{
		dst[i] = src[i];
	}
}

void CC_RX_PushMsg(CC_RX_instance_t *Instance, uint32_t ID, uint8_t *Data, uint8_t DLC, uint8_t IDE_flag)
{
	assert(Instance != NULL);


	uint16_t next_head = Instance->Head + 1;
	if (next_head >= CC_RX_BUFFER_SIZE)
	{
		next_head = 0;
	}

	if (next_head == Instance->Tail)
	{
		return;
	}

	Instance->Head = next_head;

	Instance->Buf[Instance->Head].ID = ID;
	Instance->Buf[Instance->Head].DLC = DLC;
	Instance->Buf[Instance->Head].IDE_flag = IDE_flag;

	if( (DLC > 0) && (DLC <= 8) )
	{
		CopyBuf(Data, Instance->Buf[Instance->Head].Data, DLC);
	}

}

static inline CC_MsgRegStatus_t CC_RX_MsgFromTables(CC_RX_instance_t *Instance, CC_RX_message_t *Msg)
{
	assert(Instance != NULL);

	if(NULL == Instance->RxTable)
	{
		return CC_MSG_UNREG;
	}

	for (uint16_t i = 0; i < Instance->TableSize; i++)
	{
		if(Instance->RxTable[i].ID == Msg->ID)
		{
			if( (Instance->RxTable[i].DLC == Msg->DLC) && (Instance->RxTable[i].IDE_flag == Msg->IDE_flag) )
			{
				Instance->RxTable[i].Parser(Instance, Msg, Instance->RxTable[i].SlotNo);
				return CC_MSG_REG;
			}
		}
	}
	return CC_MSG_UNREG;
}

static void CC_Timeout_Check(CC_RX_instance_t *Instance)
{
	if(NULL != Instance->RxTable)
	{
		for (uint16_t i = 0; i < Instance->TableSize; i++)
		{
			if( (0 != Instance->RxTable[i].TimeOut) && (CC_GET_TICK - Instance->RxTable[i].LastTick >= Instance->RxTable[i].TimeOut) )
			{
				Instance->RxTable[i].LastTick = CC_GET_TICK;
				if (NULL != Instance->TimeoutCallback)
				{
					Instance->TimeoutCallback(Instance, Instance->RxTable[i].SlotNo);
				}
			}
		}
	}
}

void CC_RX_Poll(CC_RX_instance_t *Instance)
{
	assert(Instance != NULL);

	CC_Timeout_Check(Instance);

	while(Instance->Head != Instance->Tail)
	{
		Instance->Tail++;
		if(Instance->Tail >= CC_RX_BUFFER_SIZE)
		{
			Instance->Tail = 0;
		}

		if((CC_RX_MsgFromTables(Instance, &Instance->Buf[Instance->Tail]) != CC_MSG_REG) && NULL != Instance->Parser_unreg_msg)
		{
			Instance->Parser_unreg_msg(Instance, &Instance->Buf[Instance->Tail]);
		}

	}
}

void CC_TX_PushMsg(CC_TX_instance_t *Instance, uint32_t ID, uint8_t *Data, uint8_t DLC, uint8_t IDE_flag)
{
	assert(Instance != NULL);

	uint16_t next_head = Instance->Head + 1;
	if (next_head >= CC_RX_BUFFER_SIZE)
	{
		next_head = 0;
	}

	if (next_head == Instance->Tail)
	{
		return;
	}

	Instance->Head = next_head;

	Instance->Buf[Instance->Head].ID = ID;
	Instance->Buf[Instance->Head].DLC = DLC;
	Instance->Buf[Instance->Head].IDE_flag = IDE_flag;

	if( (DLC > 0) && (DLC <= 8) )
	{
		CopyBuf(Data, Instance->Buf[Instance->Head].Data, DLC);
	}
}

static inline void CC_TX_MsgFromTables(CC_TX_instance_t *Instance)
{
	assert(NULL != Instance);
	uint8_t Temp[8];

	for (uint16_t i = 0; i < Instance->TableSize; i++)
	{
		if(CC_GET_TICK - Instance->TxTable[i].LastTick >= Instance->TxTable[i].SendFreq)
		{
			Instance->TxTable[i].LastTick = CC_GET_TICK;
			CopyBuf(Instance->TxTable[i].Data, Temp, Instance->TxTable[i].DLC);
			if(NULL != Instance->TxTable[i].Parser)
			{
				Instance->TxTable[i].Parser(Instance, Temp, &Instance->TxTable[i]);
			}
			CC_TX_PushMsg(Instance, Instance->TxTable[i].ID, Temp, Instance->TxTable[i].DLC, Instance->TxTable[i].IDE_flag);
		}
	}
}

void CC_TX_Poll(CC_TX_instance_t *Instance)
{
	assert(( NULL != Instance) && (NULL != Instance->BusCheck));

	CC_TX_MsgFromTables(Instance);

	while( (Instance->Head != Instance->Tail) && (Instance->BusCheck(Instance) == CC_BUS_FREE))
	{
		Instance->Tail++;
		if(Instance->Tail >= CC_TX_BUFFER_SIZE)
		{
			Instance->Tail = 0;
		}

		assert(NULL != Instance->SendFunction);
		Instance->SendFunction(Instance, &Instance->Buf[Instance->Tail]);

	}
}

