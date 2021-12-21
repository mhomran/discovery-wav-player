/**
 * @file i2s.c
 * @author Mohamed Hassanin Mohamed
 * @brief A helper module for the generation of different I2S PLL
 * clock frequency based on the audio file sampling rate.
 * @version 0.1
 * @date 2021-12-08
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include "../../Modules/I2s/I2s.h"

#include "stm32f4xx_hal.h"


//---------------------------------------------------------------------------//
//variables definitions

//The All possible sampling rates and PLL configs for them
//Reference: section 28.4.4 the second half of Table 127 in RM0090 (TRM)
const uint32_t gI2sFreq[8] = {8000, 11025, 16000, 22050, 32000, 44100, 48000, 96000};
const uint32_t gI2sPLLN[8] = {256, 429, 213, 429, 426, 271, 258, 344};
const uint32_t gI2sPLLR[8] = {5, 4, 4, 4, 4, 6, 3, 1};

static I2S_HandleTypeDef *hAudioI2S;

// functions definitions

/**
 * @brief Configure the I2S PLL to generate the required clock
 * for a give Sampling rate. With CS43L22 Auto clock detection,
 * if the MCLK is 1024 times the sampling rates, it will detect it.
 * 
 * @param audioFreq 
 */
static void I2s_PllClockConfig(uint32_t SamplingRate)
{
  RCC_PeriphCLKInitTypeDef rccclkinit;
  uint8_t freq = 0;

  for(uint8_t i = 0; i < 8; i++)
  {
    if(gI2sFreq[i] == SamplingRate)
    {
      freq = i;
      break;
    }
  }
  /* Enable PLLI2S clock */
  HAL_RCCEx_GetPeriphCLKConfig(&rccclkinit);

  /* PLLI2S_VCO Input = HSE_VALUE/PLL_M = 1 Mhz */
  rccclkinit.PeriphClockSelection = RCC_PERIPHCLK_I2S;
  rccclkinit.PLLI2S.PLLI2SN = gI2sPLLN[freq];
  rccclkinit.PLLI2S.PLLI2SR = gI2sPLLR[freq];
  HAL_RCCEx_PeriphCLKConfig(&rccclkinit);
}

/**
 * @brief update I2S peripheral with selected Sampling Frequency
 */
static bool I2s_FreqUpdate(uint32_t SamplingRate)
{
  hAudioI2S->Instance         = SPI3;
  hAudioI2S->Init.AudioFreq   = SamplingRate;
  hAudioI2S->Init.ClockSource = I2S_CLOCK_PLL;
  hAudioI2S->Init.CPOL        = I2S_CPOL_LOW;
  hAudioI2S->Init.DataFormat  = I2S_DATAFORMAT_16B;
  hAudioI2S->Init.MCLKOutput  = I2S_MCLKOUTPUT_ENABLE;
  hAudioI2S->Init.Mode        = I2S_MODE_MASTER_TX;
  hAudioI2S->Init.Standard    = I2S_STANDARD_PHILIPS;
  return HAL_I2S_Init(hAudioI2S);
}

/**
 * @brief set I2S HAL handle
 */
void 
I2s_SetHandle(I2S_HandleTypeDef *pI2Shandle)
{
  hAudioI2S = pI2Shandle;
}

/**
 * @brief Initialize the I2s peripheral 
 * 
 * @param SamplingRate the I2s clock rate
 */
void 
I2s_Init(uint32_t SamplingRate)
{
  I2s_PllClockConfig(SamplingRate);
  I2s_FreqUpdate(SamplingRate);
}

/**
 * @brief Start I2S DMA transfer
 */
void 
I2s_StartNewTransfer(uint16_t* pDataBuf, uint32_t len)
{
  HAL_I2S_Transmit_DMA(hAudioI2S, pDataBuf, DMA_MAX(len/SAMPLE_SIZE));
}

/**
 * @brief Pause audio out
 */
void I2s_Pause(void)
{
  HAL_I2S_DMAPause(hAudioI2S);
}

/**
 * @brief Resume audio out
 */
void I2s_Resume(void)
{
  HAL_I2S_DMAResume(hAudioI2S);
}

/**
 * @brief Stop audio
 */
void 
I2s_StopTransfer(void)
{
  HAL_I2S_DMAStop(hAudioI2S);
}

/**
 * @brief DMA Callback for an event when it completed the transmission fully
 * 
 * @param hi2s 
 */
void 
HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s)
{
  if(hi2s->Instance == SPI3)
  {
    I2s_FullTransferCallback();
  }
}
/**
 * @brief DMA Callback for an event when it completed the 
 * first half of the transmission 
 * 
 * @param hi2s 
 */
void 
HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
  if(hi2s->Instance == SPI3)
  {
    I2s_HalfTransferCallback();
  }
}

//---------------------------------------------------------------------------//
