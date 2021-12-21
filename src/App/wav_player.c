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

//---------------------------------------------------------------------------//
//variable definitions
static FIL gWavFile;
static uint32_t gFileLength;
static uint32_t gFileRemainingSize = 0;
static uint32_t gSamplingFreq;
static bool gIsFileChosen = false;
static bool gIsFinished;
static uint8_t gAudioBuffer[DMA_BUFFER_SIZE];
static UINT gFileReadBytesLen = 0;
static volatile DmaState_t gDmaState = DMA_STATE_FULL_TRANSFER;
static uint8_t gFilesNames[DMA_BUFFER_SIZE];

static WavPlayerConfig_t gConfig;

static DIR gDir;
static FILINFO gFileInfo;

//---------------------------------------------------------------------------//
//variable declarations
extern I2C_HandleTypeDef hi2c1;

//---------------------------------------------------------------------------//
//Function declarations
static void WavPlayer_PlayFromBeginning(void);
static void WavPlayer_DmaUpdate(DmaEvent_t);
//---------------------------------------------------------------------------//
//Function definitions

void 
WavPlayer_Init(WavPlayerConfig_t* Config)
{
  gConfig = *Config;
  gIsFinished = true;
  gIsFileChosen = false;
}

static void 
WavPlayer_Reset(void)
{
  gFileRemainingSize = 0;
  gFileReadBytesLen = 0;
  gIsFinished = true;
}

static void 
WavPlayer_StartAudioCodec(void) 
{
  CS43L22Config_t config = {0};
  config.Muted = gConfig.Muted;
  config.Vol = gConfig.Vol;
  config.i2ch = hi2c1;
  CS43L22_Init(&config);
}

bool
WavPlayer_Next(void)
{
  FRESULT fr;
  FILINFO PrevFileInfo;
  DIR PrevFileDir;
  TCHAR CurrFileName[13];

  if(!gIsFileChosen) return false;

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

  if(!gIsFileChosen) return false;

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
  WavHeader_t wavHeader;
  FIL TobePlayed;
  if(f_open(&TobePlayed, FilePath, FA_READ) != FR_OK)
    {
      return false;
    }

  if(gIsFileChosen)
    {
      CS43L22_Stop();
      I2s_Stop();
      f_close(&gWavFile);
    }
  memcpy(&gWavFile, &TobePlayed, sizeof(FIL));
  strcpy(gFileInfo.fname, FilePath);

  gIsFileChosen = true;

  f_read(&gWavFile, &wavHeader, sizeof(wavHeader), &gFileReadBytesLen);
  gFileLength = wavHeader.FileSize;
  gSamplingFreq = wavHeader.SampleRate;

  WavPlayer_Reset();
  WavPlayer_PlayFromBeginning();
  
  return true;
}

void
WavPlayer_SetVolume(uint8_t Vol)
{
  gConfig.Vol = Vol;
  CS43L22_SetVolume(Vol);
}

/**
 * @brief WAV player task (main) function
 * 
 */
void
WavPlayer_ChooseTheFirstAudioFile(void)
{
  /* Search for a wav file to play */
  if(!gIsFileChosen)
    {
      FRESULT fr;

      fr = f_findfirst(&gDir, &gFileInfo, "", "*.wav");

      if (fr == FR_OK && gFileInfo.fname[0])
        {
          gIsFileChosen = true;
          WavPlayer_PlayAudioFile(gFileInfo.fname);
          WavPlayer_Pause();
        }
    }
}

/**
 * @brief WAV File Play
 */
static void
WavPlayer_PlayFromBeginning(void)
{
  gIsFinished = false;

  I2s_Init(gSamplingFreq);

  WavPlayer_StartAudioCodec();

  f_lseek(&gWavFile, sizeof(WavHeader_t));
  f_read (&gWavFile, &gAudioBuffer[0], DMA_BUFFER_SIZE, &gFileReadBytesLen);
  gFileRemainingSize = gFileLength - gFileReadBytesLen;

  I2s_Play((uint16_t *)&gAudioBuffer[0], DMA_BUFFER_SIZE);
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
  if(gIsFileChosen)
    {
      CS43L22_Stop();
      I2s_Stop();

      WavPlayer_Reset();
      WavPlayer_PlayFromBeginning();
      WavPlayer_Pause();
    }
}

void
WavPlayer_Pause(void)
{
  if(gIsFileChosen)
    {
      CS43L22_Stop();
      I2s_Pause();
    }
}

void
WavPlayer_Resume(void)
{
  if(gIsFileChosen)
    {
      if(gIsFinished)
      {
        CS43L22_Stop();
        I2s_Stop();

        WavPlayer_Reset();
        WavPlayer_PlayFromBeginning();
      }
    else
      {
        WavPlayer_StartAudioCodec();
        I2s_Resume();
      }
    }
}

bool
WavPlayer_IsFinished(void)
{
  return gIsFinished;
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
  switch(gDmaState)
  {
    case DMA_STATE_HALF_TRANSFER:
      if (event != DMA_EVENT_FULL_TRANSFER) return;
      gFileReadBytesLen = 0;
      f_read (&gWavFile, &gAudioBuffer[DMA_BUFFER_SIZE/2], DMA_BUFFER_SIZE/2, &gFileReadBytesLen);

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
      f_read (&gWavFile, &gAudioBuffer[0], DMA_BUFFER_SIZE/2, &gFileReadBytesLen);
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
