/**
 * @file CS43L22.h
 * @author Mohamed Hassanin
 * @brief A BSP for the audio codec for STM32F407 Discovery board.
 * @version 0.1
 * @date 2021-12-08
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#ifndef CS43L22_H
#define CS43L22_H
//---------------------------------------------------------------------------//
// includes
#include <inttypes.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"

//---------------------------------------------------------------------------//
typedef struct {
  bool Muted;
  uint8_t Vol;
  I2C_HandleTypeDef i2ch;
} CS43L22Config_t;

//---------------------------------------------------------------------------//
// functions prototypes
void CS43L22_Init(CS43L22Config_t* Config);
void CS43L22_SetVolume(uint8_t volume);
void CS43L22_Mute();
void CS43L22_Unmute();
void CS43L22_Start(void);
void CS43L22_Stop(void);

#endif
//---------------------------------------------------------------------------//
