/**
 * @file i2s.h
 * @author Mohamed Hassanin Mohamed
 * @brief A helper module for the generation of different I2S PLL
 * clock frequency based on the audio file sampling rate.
 * @version 0.1
 * @date 2021-12-08
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#ifndef I2S_H
#define I2S_H

#include "stm32f4xx_hal.h"
#include "stdbool.h"

//Audio library defines
#define DMA_MAX_SZE                 0xFFFF
#define DMA_MAX(_X_)                (((_X_) <= DMA_MAX_SZE)? (_X_):DMA_MAX_SZE)
#define SAMPLE_SIZE              2   /* 16-bits audio data size */


//---------------------------------------------------------------------------//
//prototypes

void I2s_SetHandle(I2S_HandleTypeDef *pI2Shandle);
void I2s_Init(uint32_t audioFreq);

void I2s_StartNewTransfer(uint16_t* pDataBuf, uint32_t len);
void I2s_Pause(void);
void I2s_Resume(void);
void I2s_SetVolume(uint8_t volume);
void I2s_StopTransfer(void);

void I2s_HalfTransferCallback(void);
void I2s_FullTransferCallback(void);

#endif
//---------------------------------------------------------------------------//
