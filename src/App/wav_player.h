/**
 * @file wav_player.h
 * @author Mohamed Hassanin
 * @brief This is an application layer for managing the state of the
 * WAV player (muted, last file, etc.) and to respond to any command for the
 * player.
 * @version 0.1
 * @date 2021-12-09
 *
 * @copyright Copyright (c) 2021
 *
 */

#ifndef WAV_PLAYER_H_
#define WAV_PLAYER_H_

#include <stdbool.h>
#include <stdint.h>
#include "stm32f4xx_hal.h"

//---------------------------------------------------------------------------//
//typedefs
typedef struct {
  bool Muted;
  uint8_t Vol;
  I2C_HandleTypeDef i2ch;
} WavPlayerConfig_t;

//---------------------------------------------------------------------------//
//functions prototypes

// init functions
void WavPlayer_Init(WavPlayerConfig_t* Config);
void WavPlayer_ChooseTheFirstAudioFile(void);

// player control
bool WavPlayer_PlayAudioFile(const char* filePath);
bool WavPlayer_Next(void);
bool WavPlayer_Previous(void);

void WavPlayer_Stop(void);
void WavPlayer_Pause(void);
void WavPlayer_Resume(void);

// Sound control
void WavPlayer_SetVolume(uint8_t Vol);
void WavPlayer_Mute(void);
void WavPlayer_Unmute(void);

// Audio files control
const char* WavPlayer_ListAudioFiles(void);




#endif
//---------------------------------------------------------------------------//
