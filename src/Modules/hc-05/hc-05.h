/**
 * @file hc-05.h
 * @author Mohamed Hassanin Mohamed
 * @brief An application layer for the bluetooth input data parsing.
 * @version 0.1
 * @date 2021-12-10
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#ifndef HC05_H
#define HC05_H

/******************************************************************************
* Includes
******************************************************************************/
#include <inttypes.h>
#include "stm32f4xx_hal.h"

/******************************************************************************
* Function Prototypes
******************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

void HC05_Init(UART_HandleTypeDef* UartHandle);
void HC05_Update(void);
void Uart_IDLE_IRQHandler(UART_HandleTypeDef *huart);

#ifdef __cplusplus
}
#endif

#endif
/***************************** END OF FILE ***********************************/
