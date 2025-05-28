/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Author: Adrian Pietrzak
 * GitHub: https://github.com/AdrianPietrzak1998
 * Created: May 23, 2025
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

/**
 * @brief Initializes the CAN RX instance with message table and callbacks.
 *
 * Sets the RX message table, its size, and function pointers for
 * parsing unregistered messages and handling timeouts.
 *
 * @param Instance Pointer to the RX instance to initialize.
 * @param RxTable Pointer to the RX message registration table.
 * @param TableSize Number of entries in the RxTable.
 * @param Parser_unreg_msg Callback for parsing unregistered messages.
 * @param TimeoutCallback Callback for timeout handling.
 */
void CC_RX_init(CC_RX_instance_t *Instance, CC_RX_table_t *RxTable, uint16_t TableSize,
                void (*Parser_unreg_msg)(const CC_RX_instance_t *Instance, CC_RX_message_t *Msg),
                void (*TimeoutCallback)(CC_RX_instance_t *Instance, uint16_t Slot))
{
    Instance->RxTable = RxTable;
    Instance->TableSize = TableSize;
    Instance->Parser_unreg_msg = Parser_unreg_msg;
    Instance->TimeoutCallback = TimeoutCallback;
}

/**
 * @brief Initializes the CAN TX instance with message table and function callbacks.
 *
 * Sets the TX message table, its size, and function pointers for
 * sending messages and checking bus availability.
 *
 * @param Instance Pointer to the TX instance to initialize.
 * @param TxTable Pointer to the TX message registration table.
 * @param TableSize Number of entries in the TxTable.
 * @param SendFunction Callback function to send a CAN message.
 * @param BusCheck Callback function to check if the CAN bus is free.
 */
void CC_TX_init(CC_TX_instance_t *Instance, CC_TX_table_t *TxTable, uint16_t TableSize,
                void (*SendFunction)(const CC_TX_instance_t *Instance, const CC_TX_message_t *msg),
                CC_BusIsFree_t (*BusCheck)(const CC_TX_instance_t *Instance))
{
    Instance->TxTable = TxTable;
    Instance->TableSize = TableSize;
    Instance->SendFunction = SendFunction;
    Instance->BusCheck = BusCheck;
}

/**
 * @brief Copies a buffer of bytes from source to destination.
 *
 * This function copies `size` bytes from the memory area pointed to by `src`
 * to the memory area pointed to by `dst`. The `restrict` qualifier indicates
 * that the source and destination buffers do not overlap.
 *
 * @param[in] src Pointer to the source buffer.
 * @param[out] dst Pointer to the destination buffer.
 * @param[in] size Number of bytes to copy.
 */
static inline void CopyBuf(uint8_t *restrict src, uint8_t *restrict dst, size_t size)
{
    assert((src != NULL) && (dst != NULL));

    for (size_t i; i < size; i++)
    {
        dst[i] = src[i];
    }
}

/**
 * @brief Pushes a raw CAN message into the RX instance buffer.
 *
 * This function should be called from lower-level CAN driver code when
 * a new raw CAN frame is received. It copies the received frame data into
 * the RX buffer of the provided instance.
 *
 * @param[in,out] Instance Pointer to the RX instance where the message will be stored.
 * @param[in] ID CAN message identifier.
 * @param[in] Data Pointer to the data bytes of the CAN message.
 * @param[in] DLC Data length code (number of valid bytes in Data, max 8).
 * @param[in] IDE_flag Identifier extension flag (0 for standard 11-bit ID, 1 for extended 29-bit ID).
 */
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

    if ((DLC > 0) && (DLC <= 8))
    {
        CopyBuf(Data, Instance->Buf[Instance->Head].Data, DLC);
    }
}

/**
 * @brief Helper function to search for a received message in the RX message table.
 *
 * This function checks if the given received message matches any entry in the RX
 * table of the specified instance. It returns the registration status indicating
 * whether the message is registered or unregistered.
 *
 * @param[in] Instance Pointer to the RX instance containing the RX table.
 * @param[in] Msg Pointer to the received CAN message to search for.
 * @return CC_MsgRegStatus_t Returns CC_MSG_REG if the message is registered in the table,
 *         otherwise CC_MSG_UNREG.
 */
static inline CC_MsgRegStatus_t CC_RX_MsgFromTables(CC_RX_instance_t *Instance, CC_RX_message_t *Msg)
{
    assert(Instance != NULL);

    if (NULL == Instance->RxTable)
    {
        return CC_MSG_UNREG;
    }

    for (uint16_t i = 0; i < Instance->TableSize; i++)
    {
        if (Instance->RxTable[i].ID == Msg->ID)
        {
            if ((Instance->RxTable[i].DLC == Msg->DLC) && (Instance->RxTable[i].IDE_flag == Msg->IDE_flag))
            {
                Instance->RxTable[i].Parser(Instance, Msg, Instance->RxTable[i].SlotNo);
                Instance->RxTable[i].LastTick = Msg->Time;
                return CC_MSG_REG;
            }
        }
    }
    return CC_MSG_UNREG;
}

/**
 * @brief Helper function to check for message timeouts in the RX instance.
 *
 * This function iterates through the RX message table of the given instance,
 * checking if any registered message has exceeded its configured timeout period.
 * If a timeout is detected, the user-defined TimeoutCallback function is called
 * for the respective message slot.
 *
 * @param[in] Instance Pointer to the RX instance to check for timeouts.
 */
static void CC_Timeout_Check(CC_RX_instance_t *Instance)
{
    if (NULL != Instance->RxTable)
    {
        for (uint16_t i = 0; i < Instance->TableSize; i++)
        {
            if ((0 != Instance->RxTable[i].TimeOut) &&
                (CC_GET_TICK - Instance->RxTable[i].LastTick >= Instance->RxTable[i].TimeOut))
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

/**
 * @brief Processes received CAN messages and handles timeouts.
 *
 * This function should be called regularly in the main loop.
 * It processes all pending messages in the RX buffer, attempts to parse them using
 * the registered parsers or the unregistered message parser if applicable,
 * and checks for message timeouts by invoking timeout callbacks.
 *
 * @param[in,out] Instance Pointer to the RX instance to process.
 */
void CC_RX_Poll(CC_RX_instance_t *Instance)
{
    assert(Instance != NULL);

    CC_Timeout_Check(Instance);

    while (Instance->Head != Instance->Tail)
    {
        Instance->Tail++;
        if (Instance->Tail >= CC_RX_BUFFER_SIZE)
        {
            Instance->Tail = 0;
        }

        if ((CC_RX_MsgFromTables(Instance, &Instance->Buf[Instance->Tail]) != CC_MSG_REG) &&
            NULL != Instance->Parser_unreg_msg)
        {
            Instance->Parser_unreg_msg(Instance, &Instance->Buf[Instance->Tail]);
        }
    }
}

/**
 * @brief Asynchronously sends a CAN message by pushing it into the TX buffer.
 *
 * This function places a new message into the transmit buffer of the specified
 * TX instance. It can be used to send messages outside of the configured TX table.
 *
 * @param[in,out] Instance Pointer to the TX instance where the message will be queued.
 * @param[in] ID        CAN message identifier.
 * @param[in] Data      Pointer to the data bytes to be sent (up to 8 bytes).
 * @param[in] DLC       Data Length Code (number of valid bytes in Data).
 * @param[in] IDE_flag  Identifier Extension flag (0 for standard, 1 for extended).
 */
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

    if ((DLC > 0) && (DLC <= 8))
    {
        CopyBuf(Data, Instance->Buf[Instance->Head].Data, DLC);
    }
}

/**
 * @brief Helper function that processes TX messages from the transmit table.
 *
 * This internal function iterates through the TX message table in the given instance,
 * updating and preparing messages for transmission according to their configured parameters.
 *
 * @param[in,out] Instance Pointer to the TX instance whose TX table will be processed.
 */
static inline void CC_TX_MsgFromTables(CC_TX_instance_t *Instance)
{
    assert(NULL != Instance);
    uint8_t Temp[8];

    for (uint16_t i = 0; i < Instance->TableSize; i++)
    {
        if (CC_GET_TICK - Instance->TxTable[i].LastTick >= Instance->TxTable[i].SendFreq)
        {
            Instance->TxTable[i].LastTick = CC_GET_TICK;
            CopyBuf(Instance->TxTable[i].Data, Temp, Instance->TxTable[i].DLC);
            if (NULL != Instance->TxTable[i].Parser)
            {
                Instance->TxTable[i].Parser(Instance, Temp, &Instance->TxTable[i]);
            }
            CC_TX_PushMsg(Instance, Instance->TxTable[i].ID, Temp, Instance->TxTable[i].DLC,
                          Instance->TxTable[i].IDE_flag);
        }
    }
}

/**
 * @brief Poll function to handle CAN transmission for a single TX instance.
 *
 * This function should be called regularly in the main loop. It manages the transmission
 * of CAN messages by checking the bus state, preparing messages from the TX table, and
 * invoking the configured send function when the bus is free.
 *
 * @param[in,out] Instance Pointer to the TX instance to be processed.
 */
void CC_TX_Poll(CC_TX_instance_t *Instance)
{
    assert((NULL != Instance) && (NULL != Instance->BusCheck));

    CC_TX_MsgFromTables(Instance);

    while ((Instance->Head != Instance->Tail) && (Instance->BusCheck(Instance) == CC_BUS_FREE))
    {
        Instance->Tail++;
        if (Instance->Tail >= CC_TX_BUFFER_SIZE)
        {
            Instance->Tail = 0;
        }

        assert(NULL != Instance->SendFunction);
        Instance->SendFunction(Instance, &Instance->Buf[Instance->Tail]);
    }
}
