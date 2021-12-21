/**
 * @file hc-05.c
 * @author Mohamed Hassanin Mohamed
 * @brief An application layer for the bluetooth input data parsing.
 * @version 0.1
 * @date 2021-12-10
 * 
 * @copyright Copyright (c) 2021
 * 
 */
/******************************************************************************
* Includes
******************************************************************************/
#include "../../Modules/hc-05/hc-05.h"

#include <stdbool.h>
#include <string.h>

#include "../../App/wav_player.h"

/******************************************************************************
* Definitions
******************************************************************************/
#define DATA_MAX_SIZE 50

/******************************************************************************
* Module Variable Definitions
******************************************************************************/
static UART_HandleTypeDef* gUartHandle;
static bool gIsReceived = false;
static uint8_t gDataLen;
static uint8_t gData[DATA_MAX_SIZE];

/******************************************************************************
* Functions prototypes
******************************************************************************/
static HAL_StatusTypeDef HC05_Print(const char* Data);
static bool atoi (const char* Data, uint8_t* Num);
/******************************************************************************
* Functions definitions
******************************************************************************/

/**
 * @brief Initialize the Uart driver
 * 
 */
void 
HC05_Init(UART_HandleTypeDef* UartHandle)
{
  //register the handle
  gUartHandle = UartHandle;
  // Start DMA receive
  if(HAL_OK != HAL_UART_Receive_DMA(gUartHandle, gData, DATA_MAX_SIZE))
    {
    }
}

static void
UpCase(char* Data)
{
  //up-case conversion for fatfs
  for (uint8_t i = 0; Data[i]!='\0'; i++)
    {
      if(Data[i] >= 'a' && Data[i] <= 'z')
	{
	  Data[i] = Data[i] - 32;
	}
   }
}

void
HC05_Update(void)
{
  if(!gIsReceived) return;
  gIsReceived = false;

  uint8_t Vol;

  char FirstChar = (char)gData[0];
  switch(FirstChar)
  {
    case '>':
      if(!WavPlayer_Next())
	  {
	    HC05_Print("[ERROR] couldn't find an audio file.\n");
	    return;
	  }
      break;
    case '<':
      if(!WavPlayer_Previous())
      {
	HC05_Print("[ERROR] couldn't find an audio file.\n");
	return;
      }
      break;
    case 'l':
      HC05_Print(WavPlayer_ListAudioFiles());
      return;
    case 'c':
      if(strlen((char*)gData) > 2)
	{
	  UpCase((char*)&gData[2]);
	}
      else
	{
	  HC05_Print("[ERROR] No file name.\n");
	  return;
	}
      if(!WavPlayer_PlayAudioFile((const char*)&gData[2]))
	{
	  HC05_Print("[ERROR] couldn't open the file.\n");
	  return;
	}
      break;
    case 'p':
      WavPlayer_Pause();
      break;
    case 'r':
      WavPlayer_Resume();
      break;
    case 's':
      WavPlayer_Stop();
      break;
    case 'm':
      WavPlayer_Mute();
      break;
    case 'u':
      WavPlayer_Unmute();
      break;
    case 'v':
      if(!atoi((const char*)&gData[2], &Vol))
        {
          HC05_Print("[ERROR] invalid volume value, volume range is [0-255].\n");
          return;
        }
      WavPlayer_SetVolume(Vol);
      break;
    default:
      HC05_Print("[ERROR] undefined command.\n");
      return;
      break;
  }

  HC05_Print("[SUCCESS]\n");
}

static bool 
atoi (const char* Data, uint8_t* Num)
{
  uint8_t len = strlen(Data);
  uint16_t res;
  if(len > 3 || len == 0) 
    {
      return false;
    }
  for (uint8_t i = 0; i < len; i++)
    {
      if(!('0' <= Data[i] && Data[i] <= '9')) return false;
    }

  if(len == 1) 
    {
      *Num = (Data[0]-0x30); 
    }
  else if(len == 2) 
    {
      *Num = (Data[0]-0x30) * 10 + (Data[1]-0x30); 
    }
  else if(len == 3) 
    {
      res = (Data[0]-0x30) * 100 + (Data[1]-0x30) * 10 + (Data[2]-0x30); 
      if (res > 255) return false;
      *Num = res;
    }
  return true;
}

static HAL_StatusTypeDef
HC05_Print(const char* Data)
{ 
  return HAL_UART_Transmit_DMA(gUartHandle, (uint8_t*)Data, strlen(Data));
}

/******************************************************************************
* Callbacks
******************************************************************************/

/**
 * @brief Handler for the idle line of a specific UART peripheral
 * 
 * @param huart UART peripheral handle
 */
void 
Uart_IDLE_IRQHandler(UART_HandleTypeDef *huart)
{
  gIsReceived = true;
  gDataLen = DATA_MAX_SIZE - huart->hdmarx->Instance->NDTR;
  //null terminate
  gData[gDataLen] = '\0';

  HC05_Update();

  // Start another DMA receive transfer
  if(HAL_OK != HAL_UART_Receive_DMA(gUartHandle, gData, DATA_MAX_SIZE))
    {
    }
}
/***************************** END OF FILE ***********************************/
