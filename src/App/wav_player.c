/**
 * @file wav_player.c
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

//---------------------------------------------------------------------------//
//includes
#include "../App/wav_player.h"

#include "fatfs.h" // to deal with files

#include "../Modules/CS43L22/CS43L22.h" //to control the audio codec
#include "../Modules/I2s/I2s.h" //to change I2S clock

//---------------------------------------------------------------------------//
//defines
#define DMA_BUFFER_SIZE  4096

//---------------------------------------------------------------------------//
//typedefs
typedef struct
{
  uint32_t RiffMarker;
  uint32_t FileSize;
  uint32_t Reserved1[4];
  uint32_t SampleRate;
  uint32_t Reserved2[4];
} WavHeader_t;

typedef enum
{
  DMA_STATE_HALF_TRANSFER,
  DMA_STATE_FULL_TRANSFER,
}DmaState_t;

typedef enum
{
  DMA_EVENT_HALF_TRANSFER,
  DMA_EVENT_FULL_TRANSFER,
}DmaEvent_t;

typedef enum
{
  PLAYER_STATE_IDLE,
  PLAYER_STATE_READY,
  PLAYER_STATE_PLAYING,
  PLAYER_STATE_PAUSED,
} PlayerState_t;

typedef enum
{
  PLAYER_EVENT_FILE_IS_CHOSEN,
  PLAYER_EVENT_STOP,
  PLAYER_EVENT_RESUME,
  PLAYER_EVENT_PAUSE,
} PlayerEvent_t;

//---------------------------------------------------------------------------//
//variable definitions
static FIL gWavFile;
static uint32_t gFileLength;
static uint32_t gFileRemainingSize = 0;
static uint32_t gSamplingFreq;
static uint8_t gAudioBuffer[DMA_BUFFER_SIZE];
static UINT gFileReadBytesLen = 0;
static uint8_t gFilesNames[DMA_BUFFER_SIZE];

static WavPlayerConfig_t gConfig;

static DIR gDir;
static FILINFO gFileInfo;

static volatile PlayerState_t gPlayerState = PLAYER_STATE_IDLE;
//---------------------------------------------------------------------------//
//variable declarations
extern I2C_HandleTypeDef hi2c1;

//---------------------------------------------------------------------------//
//Function declarations
static void WavPlayer_DmaUpdate(DmaEvent_t);
static PlayerState_t WavPlayer_PlayerUpdate(PlayerEvent_t);

inline static void WavPlayer_StartAudioCodec(void);
inline static void WavPlayer_StopAudioCodec(void);
//---------------------------------------------------------------------------//
//Function definitions

void 
WavPlayer_Init(WavPlayerConfig_t* Config)
{
  gConfig = *Config;
}

static void 
WavPlayer_Reset(void)
{
  gFileRemainingSize = 0;
  gFileReadBytesLen = 0;
}

inline static void 
WavPlayer_StartAudioCodec(void) 
{
  CS43L22Config_t config = {0};
  config.Muted = gConfig.Muted;
  config.Vol = gConfig.Vol;
  config.i2ch = hi2c1;
  CS43L22_Init(&config);
}

inline static void 
WavPlayer_StopAudioCodec(void)
{
  CS43L22_Stop();
}


bool
WavPlayer_Next(void)
{
  FRESULT fr;
  FILINFO PrevFileInfo;
  DIR PrevFileDir;
  TCHAR CurrFileName[13];

  if(gPlayerState == PLAYER_STATE_IDLE) return false;

  strcpy(CurrFileName, gFileInfo.fname);

  f_closedir(&gDir);
  f_findfirst(&gDir, &gFileInfo, "", "*.wav");
  f_findnext(&gDir, &gFileInfo);

  f_findfirst(&PrevFileDir, &PrevFileInfo, "", "*.wav");

  do {
    if (strcmp(PrevFileInfo.fname, CurrFileName) == 0)
      {
        f_closedir(&PrevFileDir);
        WavPlayer_PlayAudioFile(gFileInfo.fname);
        return true;
      }

    fr = f_findnext(&gDir, &gFileInfo);
    if(!gFileInfo.fname[0])
      {
        f_rewinddir(&gDir);
        fr = f_findnext(&gDir, &gFileInfo);
      }

    fr = f_findnext(&PrevFileDir, &PrevFileInfo);

  } while(fr == FR_OK && PrevFileInfo.fname[0]); // While the previous pointer is not rewinded

  return false;
}

bool
WavPlayer_Previous(void)
{
  FRESULT fr;
  FILINFO NextFileInfo;
  DIR NextFileDir;
  TCHAR	CurrFileName[13];

  if(gPlayerState == PLAYER_STATE_IDLE) return false;

  strcpy(CurrFileName, gFileInfo.fname);

  f_closedir(&gDir);
  f_findfirst(&gDir, &gFileInfo, "", "*.wav");

  f_findfirst(&NextFileDir, &NextFileInfo, "", "*.wav");
  f_findnext(&NextFileDir, &NextFileInfo);

  do {
    if (strcmp(NextFileInfo.fname, CurrFileName) == 0)
      {
        f_closedir(&NextFileDir);
        WavPlayer_PlayAudioFile(gFileInfo.fname);
        return true;
      }

  fr = f_findnext(&NextFileDir, &NextFileInfo);
  if(!NextFileInfo.fname[0])
    {
      f_rewinddir(&NextFileDir);
      fr = f_findnext(&NextFileDir, &NextFileInfo);
    }

  fr = f_findnext(&gDir, &gFileInfo);

  } while(fr == FR_OK && gFileInfo.fname[0]); // While the previous pointer is not rewinded

  return false;
}

/**
 * @brief Read a file given its path
 * 
 * @param FilePath 
 * @return true in case of success
 * @return false in case of failure
 */
bool 
WavPlayer_PlayAudioFile(const char* FilePath)
{
  if(gPlayerState == PLAYER_STATE_IDLE) return false;
  
  WavHeader_t wavHeader;
  FIL TobePlayed;
  if(f_open(&TobePlayed, FilePath, FA_READ) != FR_OK)
    {
      return false;
    }

  WavPlayer_StopAudioCodec();
  I2s_StopTransfer();
  f_close(&gWavFile);
    
  memcpy(&gWavFile, &TobePlayed, sizeof(FIL));
  strcpy(gFileInfo.fname, FilePath);
  WavPlayer_Reset();

  f_read(&gWavFile, &wavHeader, sizeof(wavHeader), &gFileReadBytesLen);
  gFileLength = wavHeader.FileSize;
  gSamplingFreq = wavHeader.SampleRate;

  f_lseek(&gWavFile, sizeof(WavHeader_t));
  f_read (&gWavFile, &gAudioBuffer[0], DMA_BUFFER_SIZE, &gFileReadBytesLen);
  gFileRemainingSize = gFileLength - gFileReadBytesLen;

  I2s_Init(gSamplingFreq);
  I2s_StartNewTransfer((uint16_t *)&gAudioBuffer[0], DMA_BUFFER_SIZE);
  WavPlayer_StartAudioCodec();

  WavPlayer_Resume();
  
  return true;
}

void
WavPlayer_SetVolume(uint8_t Vol)
{
  gConfig.Vol = Vol;
  CS43L22_SetVolume(Vol);
}

/**
 * @brief Open the root directory and choose the first 
 * audio file.
 * 
 */
void
WavPlayer_ChooseTheFirstAudioFile(void)
{
  FRESULT fr;

  if(gPlayerState != PLAYER_STATE_IDLE) return;

  /* Search for a wav file to play */
  fr = f_findfirst(&gDir, &gFileInfo, "", "*.wav");

  if (fr == FR_OK && gFileInfo.fname[0])
    {
      WavPlayer_PlayerUpdate(PLAYER_EVENT_FILE_IS_CHOSEN);
      WavPlayer_PlayAudioFile(gFileInfo.fname);
      WavPlayer_Pause();
    }
}


void
WavPlayer_Mute(void)
{
  gConfig.Muted = true;
  CS43L22_Mute();
}

void
WavPlayer_Unmute(void)
{
  gConfig.Muted = false;
  CS43L22_Unmute();
}

void
WavPlayer_Stop(void)
{
  if (gPlayerState != PLAYER_STATE_PLAYING && 
  gPlayerState != PLAYER_STATE_PAUSED) return;

  CS43L22_Stop();
  I2s_Pause();

  WavPlayer_Reset();

  f_lseek(&gWavFile, sizeof(WavHeader_t));
  f_read (&gWavFile, &gAudioBuffer[0], DMA_BUFFER_SIZE, &gFileReadBytesLen);
  gFileRemainingSize = gFileLength - gFileReadBytesLen;

  WavPlayer_PlayerUpdate(PLAYER_EVENT_STOP);
}

void
WavPlayer_Pause(void)
{
  if (gPlayerState != PLAYER_STATE_PLAYING) return;
 
  CS43L22_Stop();
  I2s_Pause();

  WavPlayer_PlayerUpdate(PLAYER_EVENT_PAUSE);
}

void
WavPlayer_Resume(void)
{
  if (gPlayerState != PLAYER_STATE_PAUSED &&
  gPlayerState != PLAYER_STATE_READY) return;

  WavPlayer_StartAudioCodec();
  I2s_Resume();
    
  WavPlayer_PlayerUpdate(PLAYER_EVENT_RESUME);
}

const char*
WavPlayer_ListAudioFiles(void)
{
  /* Search a directory for objects and display it */
  FRESULT fr;     /* Return value */
  DIR dj;         /* Directory object */
  FILINFO fno;    /* File information */
  uint32_t DmaBufferIdx = 0;

  fr = f_findfirst(&dj, &fno, "", "*.wav");

  while (fr == FR_OK && fno.fname[0] && DmaBufferIdx < DMA_BUFFER_SIZE) {
      memcpy(&gFilesNames[DmaBufferIdx], fno.fname, strlen(fno.fname));
      DmaBufferIdx += strlen(fno.fname);
      gFilesNames[DmaBufferIdx] = '\n';
      DmaBufferIdx++;

      fr = f_findnext(&dj, &fno);
  }
  gFilesNames[DmaBufferIdx] = '\0';

  f_closedir(&dj);

  return (const char*) gFilesNames;
}

static void 
WavPlayer_DmaUpdate(DmaEvent_t event)
{
  static volatile DmaState_t gDmaState = DMA_STATE_FULL_TRANSFER;
  FRESULT fr;
  switch(gDmaState)
  {
    case DMA_STATE_HALF_TRANSFER:
      if (event != DMA_EVENT_FULL_TRANSFER) return;
      gFileReadBytesLen = 0;
      fr = f_read (&gWavFile, &gAudioBuffer[DMA_BUFFER_SIZE/2], DMA_BUFFER_SIZE/2, &gFileReadBytesLen);

      if(gFileRemainingSize > (DMA_BUFFER_SIZE / 2))
        {
          gFileRemainingSize -= gFileReadBytesLen;
        }
      else
        {
          gFileRemainingSize = 0;
          WavPlayer_Next();
        }
      gDmaState = DMA_STATE_FULL_TRANSFER;
      break;

    case DMA_STATE_FULL_TRANSFER:
      if (event != DMA_EVENT_HALF_TRANSFER) return;
      gFileReadBytesLen = 0;
      fr = f_read (&gWavFile, &gAudioBuffer[0], DMA_BUFFER_SIZE/2, &gFileReadBytesLen);
      if(gFileRemainingSize > (DMA_BUFFER_SIZE / 2))
        {
          gFileRemainingSize -= gFileReadBytesLen;
        }
      else
        {
          gFileRemainingSize = 0;
          WavPlayer_Next();
        }
      gDmaState = DMA_STATE_HALF_TRANSFER;
      break;
      default:
      //DO NOTHING
      break;
  }
}

static PlayerState_t 
WavPlayer_PlayerUpdate(PlayerEvent_t event)
{
  switch(gPlayerState)
  {
    case PLAYER_STATE_IDLE:
      if(event == PLAYER_EVENT_FILE_IS_CHOSEN) gPlayerState = PLAYER_STATE_READY;
    break;

    case PLAYER_STATE_READY:
      if(event == PLAYER_EVENT_RESUME) gPlayerState = PLAYER_STATE_PLAYING;
    break;

    case PLAYER_STATE_PLAYING:
      if(event == PLAYER_EVENT_STOP) gPlayerState = PLAYER_STATE_READY;
      else if(event == PLAYER_EVENT_PAUSE) gPlayerState = PLAYER_STATE_PAUSED;
    break;

    case PLAYER_STATE_PAUSED:
      if(event == PLAYER_EVENT_RESUME) gPlayerState = PLAYER_STATE_PLAYING;
      else if(event == PLAYER_EVENT_STOP) gPlayerState = PLAYER_STATE_READY;
    break;

    default:
      // DO NOTHING
    break;
  }

  return gPlayerState;
}
/**
 * @brief Half DMA transfer callback. It should load the first half of the buffer.
 */
void
I2s_HalfTransferCallback(void)
{
  WavPlayer_DmaUpdate(DMA_EVENT_HALF_TRANSFER);
}

/**
 * @brief Full DMA transfer callback. It should load the second half of the buffer.
 */
void
I2s_FullTransferCallback(void)
{
  WavPlayer_DmaUpdate(DMA_EVENT_FULL_TRANSFER);
}

//---------------------------------------------------------------------------//
