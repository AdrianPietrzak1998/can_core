/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Author: Adrian Pietrzak
 * GitHub: https://github.com/AdrianPietrzak1998
 * Created: May 23, 2025
 */

#ifndef CAN_CORE_H_
#define CAN_CORE_H_

#include <limits.h>
#include <stdint.h>

/**
 * @def CC_TICK_FROM_FUNC
 * @brief Enables system tick retrieval via a function call.
 *
 * If set to 1, the system time base is obtained by calling a function
 * that returns the current tick count.
 * If set to 0, the system time base is read directly from a variable.
 */
#define CC_TICK_FROM_FUNC 0

/**
 * @def CC_RX_BUFFER_SIZE
 * @brief Size of the CAN receive buffer.
 *
 * Defines the number of messages that can be stored in the CAN RX buffer.
 */
#define CC_RX_BUFFER_SIZE 32

/**
 * @def CC_TX_BUFFER_SIZE
 * @brief Size of the CAN transmit buffer.
 *
 * Defines the number of messages that can be stored in the CAN TX buffer.
 */
#define CC_TX_BUFFER_SIZE 32

/**
 * @brief System time base type and maximum timeout definition.
 *
 * By default, the system tick type is `volatile uint32_t`.
 * If you want to use a different type, define `CC_TIME_BASE_TYPE_CUSTOM` as that type
 * and also define the corresponding `_IS_` macro
 * (e.g., `CC_TIME_BASE_TYPE_CUSTOM_IS_UINT16`) to properly set `CC_MAX_TIMEOUT`.
 */
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

/**
 * @brief Enumeration indicating the status of the CAN bus.
 *
 * This enum represents whether the CAN bus is free for transmitting a new message
 * or if it is currently busy.
 *
 * Values:
 * - CC_BUS_BUSY: The CAN bus is currently busy and cannot accept new messages.
 * - CC_BUS_FREE: The CAN bus is free and ready for new message transmission.
 */
typedef enum
{
    CC_BUS_BUSY = 0,
    CC_BUS_FREE
} CC_BusIsFree_t;

/**
 * @brief Enumeration representing the registration status of a received CAN message.
 *
 * This enum indicates whether the received CAN message is registered
 * in the message table or not.
 *
 * Values:
 * - CC_MSG_UNREG: The received message is not registered in the message table.
 * - CC_MSG_REG: The received message is registered in the message table.
 */
typedef enum
{
    CC_MSG_UNREG,
    CC_MSG_REG
} CC_MsgRegStatus_t;

/**
 * @brief Structure representing a CAN message stored in the receive buffer.
 *
 * Fields:
 * - ID: CAN message identifier.
 * - Data: CAN message data payload (up to 8 bytes).
 * - DLC: Data Length Code, number of valid data bytes (0-8).
 * - IDE_flag: Identifier Extension flag (0 = standard, 1 = extended).
 * - Time: Timestamp when the message was received.
 */
typedef struct
{
    uint32_t ID;
    uint8_t Data[8];
    uint8_t DLC : 4;
    uint8_t IDE_flag : 1;
    CC_TIME_t Time;
} CC_RX_message_t;

/**
 * @brief Definition of an entry in the CAN receive message table.
 *
 * Fields:
 * - SlotNo: Slot number in the receive table.
 * - ID: CAN message identifier.
 * - DLC: Data Length Code, expected number of data bytes.
 * - IDE_flag: Identifier Extension flag (0 = standard, 1 = extended).
 * - TimeOut: Timeout duration for this message slot.
 * - Parser: Pointer to message parser callback function.
 * - LastTick: Timestamp of the last received message in this slot.
 */
typedef struct CC_RX_instance_t CC_RX_instance_t;
typedef struct
{
    uint16_t SlotNo;
    uint32_t ID;
    uint8_t DLC : 4;
    uint8_t IDE_flag : 1;
    CC_TIME_t TimeOut;
    void (*Parser)(const CC_RX_instance_t *Instance, CC_RX_message_t *Msg, uint16_t Slot);
    CC_TIME_t LastTick;
} CC_RX_table_t;

/**
 * @brief CAN receive instance structure.
 *
 * Fields:
 * - Buf: Circular buffer for received CAN messages.
 * - Head: Index of the buffer head (write position).
 * - Tail: Index of the buffer tail (read position).
 * - RxTable: Pointer to the message registration table.
 * - TableSize: Number of entries in the message table.
 * - Parser_unreg_msg: Callback for unregistered messages.
 * - TimeoutCallback: Callback for message timeout events.
 */
struct CC_RX_instance_t
{
    CC_RX_message_t Buf[CC_RX_BUFFER_SIZE];
    uint16_t Head;
    uint16_t Tail;
    CC_RX_table_t *RxTable;
    uint16_t TableSize;
    void (*Parser_unreg_msg)(const CC_RX_instance_t *Instance, CC_RX_message_t *Msg);
    void (*TimeoutCallback)(CC_RX_instance_t *Instance, uint16_t Slot);
};

/**
 * @brief Structure representing a CAN message prepared for transmission.
 *
 * Fields:
 * - ID: CAN message identifier.
 * - Data: CAN message data payload (up to 8 bytes).
 * - DLC: Data Length Code: number of valid data bytes (0-8).
 * - IDE_flag: Identifier Extension flag (0 = standard, 1 = extended).
 */
typedef struct
{
    uint32_t ID;
    uint8_t Data[8];
    uint8_t DLC : 4;
    uint8_t IDE_flag : 1;
} CC_TX_message_t;

/**
 * @brief Definition of an entry in the CAN transmit message table.
 *
 * Fields:
 * - SlotNo: Slot number in the transmit table.
 * - ID: CAN message identifier.
 * - Data: Pointer to the data buffer to be transmitted.
 * - DLC: Data Length Code: number of valid data bytes.
 * - IDE_flag: Identifier Extension flag (0 = standard, 1 = extended).
 * - SendFreq: Frequency at which the message should be sent (in system ticks).
 * - Parser: Pointer to function that prepares data before sending.
 * - LastTick: Timestamp of the last message transmission.
 */
typedef struct CC_TX_instance_t CC_TX_instance_t;
typedef struct CC_TX_table_t CC_TX_table_t;
struct CC_TX_table_t
{
    uint16_t SlotNo;
    uint32_t ID;
    uint8_t *Data;
    uint8_t DLC : 4;
    uint8_t IDE_flag : 1;
    CC_TIME_t SendFreq;
    void (*Parser)(const CC_TX_instance_t *Instance, uint8_t *Data, CC_TX_table_t *TxTable);
    CC_TIME_t LastTick;
};

/**
 * @brief CAN transmit instance structure.
 *
 * Fields:
 * - Buf: Circular buffer for CAN messages to be transmitted.
 * - Head: Index of the buffer head (write position).
 * - Tail: Index of the buffer tail (read position).
 * - TxTable: Pointer to the transmit message registration table.
 * - TableSize: Number of entries in the transmit message table.
 * - SendFunction: Function pointer to the low-level send function.
 * - BusCheck: Function pointer to check if the CAN bus is free.
 */
struct CC_TX_instance_t
{
    CC_TX_message_t Buf[CC_TX_BUFFER_SIZE];
    uint16_t Head;
    uint16_t Tail;
    CC_TX_table_t *TxTable;
    uint16_t TableSize;
    void (*SendFunction)(const CC_TX_instance_t *Instance, const CC_TX_message_t *msg);
    CC_BusIsFree_t (*BusCheck)(const CC_TX_instance_t *Instance);
};

#if CC_TICK_FROM_FUNC
/**
 * @brief Registers the system tick source function.
 *
 * When CC_TICK_FROM_FUNC is set to 1, this function registers a user-provided
 * function that returns the current system tick value.
 *
 * @param Function Pointer to a function that returns the current system tick (CC_TIME_t).
 */
void CC_tick_function_register(CC_TIME_t (*Function)(void));
#else
/**
 * @brief Registers the system tick source variable.
 *
 * When CC_TICK_FROM_FUNC is set to 0, this function registers a pointer
 * to a variable that holds the current system tick value.
 *
 * @param Variable Pointer to a volatile variable of type CC_TIME_t representing the system tick.
 */
void CC_tick_variable_register(CC_TIME_t *Variable);
#endif

/**
 * @brief Initializes the CAN RX instance.
 *
 * Sets up the receive buffer, message registration table, and callback functions.
 *
 * @param Instance Pointer to the RX instance to initialize.
 * @param RxTable Pointer to the array of registered RX messages.
 * @param TableSize Number of entries in the RxTable.
 * @param Parser_unreg_msg Pointer to function called when an unregistered message is received.
 * @param TimeoutCallback Pointer to function called when a registered message times out.
 */
void CC_RX_init(CC_RX_instance_t *Instance, CC_RX_table_t *RxTable, uint16_t TableSize,
                void (*Parser_unreg_msg)(const CC_RX_instance_t *Instance, CC_RX_message_t *Msg),
                void (*TimeoutCallback)(CC_RX_instance_t *Instance, uint16_t Slot));

/**
 * @brief Pushes a raw CAN message into the RX instance buffer.
 *
 * This function should be called from the low-level CAN driver when
 * a raw CAN frame is received. It stores the message into the receive buffer
 * of the specified instance.
 *
 * @param Instance Pointer to the RX instance to receive the message.
 * @param ID The CAN message identifier.
 * @param Data Pointer to the CAN message data bytes (up to 8 bytes).
 * @param DLC Data Length Code - number of valid bytes in Data.
 * @param IDE_flag Identifier Extension flag (0 = standard ID, 1 = extended ID).
 */
void CC_RX_PushMsg(CC_RX_instance_t *Instance, uint32_t ID, uint8_t *Data, uint8_t DLC, uint8_t IDE_flag);

/**
 * @brief Processes received CAN messages for a given RX instance.
 *
 * This function should be called repeatedly in the main loop.
 * It processes messages in the RX buffer, calls appropriate parsers
 * for registered messages, and handles unregistered messages and timeouts.
 *
 * @param Instance Pointer to the RX instance to be processed.
 */
void CC_RX_Poll(CC_RX_instance_t *Instance);

/**
 * @brief Initializes the CAN TX instance.
 *
 * Sets up the transmit buffer, message registration table, and required callbacks.
 *
 * @param Instance Pointer to the TX instance to initialize.
 * @param TxTable Pointer to the array of registered TX messages.
 * @param TableSize Number of entries in the TxTable.
 * @param SendFunction Pointer to function responsible for sending a CAN message.
 * @param BusCheck Pointer to function that checks if the CAN bus is free for transmission.
 */
void CC_TX_init(CC_TX_instance_t *Instance, CC_TX_table_t *TxTable, uint16_t TableSize,
                void (*SendFunction)(const CC_TX_instance_t *Instance, const CC_TX_message_t *msg),
                CC_BusIsFree_t (*BusCheck)(const CC_TX_instance_t *Instance));

/**
 * @brief Asynchronously pushes a CAN message to the transmit buffer.
 *
 * This function can be used to send a CAN frame directly without
 * using the transmission table. If you use a TX table, this function
 * is optional.
 *
 * @param Instance Pointer to the TX instance where the message will be queued.
 * @param ID The CAN message identifier.
 * @param Data Pointer to the data bytes (up to 8 bytes) to be sent.
 * @param DLC Data Length Code - number of valid bytes in Data.
 * @param IDE_flag Identifier Extension flag (0 = standard ID, 1 = extended ID).
 */
void CC_TX_PushMsg(CC_TX_instance_t *Instance, uint32_t ID, uint8_t *Data, uint8_t DLC, uint8_t IDE_flag);

/**
 * @brief Processes the transmit buffer and sends CAN messages when the bus is free.
 *
 * This function should be called regularly in the main loop.
 * It manages sending queued messages based on the TX table and bus availability.
 *
 * @param Instance Pointer to the TX instance to be processed.
 */
void CC_TX_Poll(CC_TX_instance_t *Instance);

#endif /* CAN_CORE_H_ */
