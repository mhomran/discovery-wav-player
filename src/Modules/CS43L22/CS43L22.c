/**
 * @file CS43L22.c
 * @author Mohamed Hassanin
 * @brief A BSP for the audio codec for STM32F407 Discovery board.
 * @version 0.1
 * @date 2021-12-08
 * 
 * @copyright Copyright (c) 2021
 * 
 */

//---------------------------------------------------------------------------//
// includes
#include "../../Modules/CS43L22/CS43L22.h"

#include "../../Modules/CS43L22/CS43L22_MemMap.h"

//defines
#define CS43L22_I2C_ADDR 0x94
#define CS43L22_I2C_TIMEOUT_MS 100

// Min => -102 dB => 25, Max => +12 dB => 24
#define REARRANGE_VOL(volume) (((volume) >= 231 && (volume) <= 255) ? (volume - 231) : (volume + 25))


//variable definitions
static I2C_HandleTypeDef gI2cx;
extern I2S_HandleTypeDef hi2s3;

//functions prototypes
static void CS43L22_PowerDown(void);
static void CS43L22_PowerUp(void);
static void CS43L22_I2CWriteReg(uint8_t Reg, uint8_t Val);
static uint8_t CS43L22_I2CReadReg(uint8_t Reg);

static void CS43L22_EnableLeftHP(void);
static void CS43L22_EnableRightHP(void);
static void CS43L22_DisableLeftHP(void);
static void CS43L22_DisableRightHP(void);

static void CS43L22_DisableLeftSPK(void);
static void CS43L22_DisableRightSPK(void);

static void CS43L22_AutoDetectClock(void);
static void CS43L22_I2SInterface(void);

static void CS43L22_PullUpReset(void);
static void CS43L22_PullDownReset(void);

//functions definitions
void CS43L22_Init(CS43L22Config_t* Config)
{
  //register i2c handle
  gI2cx = Config->i2ch;

  CS43L22_PullUpReset();

  CS43L22_PowerDown(); //power down for configuration

  CS43L22_EnableLeftHP();
  CS43L22_EnableRightHP();
  CS43L22_DisableLeftSPK();
  CS43L22_DisableRightSPK();

  CS43L22_AutoDetectClock();

  CS43L22_I2SInterface();
  if(Config->Muted)
    {
      CS43L22_Mute();
    }
  else
    {
      CS43L22_Unmute();
    }
  CS43L22_SetVolume(Config->Vol);
  CS43L22_PowerUp();
}

void
CS43L22_Start(void)
{
  CS43L22_PowerUp();
}

void
CS43L22_Stop(void)
{
  CS43L22_Mute();
  CS43L22_I2CWriteReg(CS43L22_MISC_CTL_R, 0); // disable softramp
  CS43L22_PowerDown();
  CS43L22_PullDownReset();
}

static void 
CS43L22_PullUpReset(void)
{
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_4, GPIO_PIN_SET);
}

static void
CS43L22_PullDownReset(void)
{
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_4, GPIO_PIN_RESET);
}

void 
CS43L22_SetVolume(uint8_t volume)
{
  CS43L22_I2CWriteReg(CS43L22_MASTER_A_VOL_R, REARRANGE_VOL(volume));
  CS43L22_I2CWriteReg(CS43L22_MASTER_B_VOL_R, REARRANGE_VOL(volume));
}

void 
CS43L22_Mute()
{
  CS43L22_DisableLeftHP();
  CS43L22_DisableRightHP();
}

void 
CS43L22_Unmute()
{
  CS43L22_EnableLeftHP();
  CS43L22_EnableRightHP();
}

static void 
CS43L22_PowerDown(void)
{
	CS43L22_I2CWriteReg(CS43L22_POWER_CTL1_R, CS43L22_POWER_CTL1_POWERED_DOWN_Val);
}

static void 
CS43L22_PowerUp(void)
{
	CS43L22_I2CWriteReg(CS43L22_POWER_CTL1_R, CS43L22_POWER_CTL1_POWERED_UP_Val);
}

static void 
CS43L22_EnableLeftHP(void)
{
  uint8_t Val = CS43L22_I2CReadReg(CS43L22_POWER_CTL2_R);
  Val |= 1 << CS43L22_POWER_CTL2_HPA1_Pos;
  Val &= ~(1 << CS43L22_POWER_CTL2_HPA0_Pos);
	CS43L22_I2CWriteReg(CS43L22_POWER_CTL2_R, Val);
}

static void 
CS43L22_EnableRightHP(void)
{
  uint8_t Val = CS43L22_I2CReadReg(CS43L22_POWER_CTL2_R);
  Val |= 1 << CS43L22_POWER_CTL2_HPB1_Pos;
  Val &= ~(1 << CS43L22_POWER_CTL2_HPB0_Pos);
	CS43L22_I2CWriteReg(CS43L22_POWER_CTL2_R, Val);
}

static void 
CS43L22_DisableLeftHP(void)
{
  uint8_t Val = CS43L22_I2CReadReg(CS43L22_POWER_CTL2_R);
  Val |= 1 << CS43L22_POWER_CTL2_HPA1_Pos;
  Val |= 1 << CS43L22_POWER_CTL2_HPA0_Pos;
	CS43L22_I2CWriteReg(CS43L22_POWER_CTL2_R, Val);
}

static void 
CS43L22_DisableRightHP(void)
{
  uint8_t Val = CS43L22_I2CReadReg(CS43L22_POWER_CTL2_R);
  Val |= 1 << CS43L22_POWER_CTL2_HPB1_Pos;
  Val |= 1 << CS43L22_POWER_CTL2_HPB0_Pos;
	CS43L22_I2CWriteReg(CS43L22_POWER_CTL2_R, Val);
}

static void 
CS43L22_DisableLeftSPK(void)
{
  uint8_t Val = CS43L22_I2CReadReg(CS43L22_POWER_CTL2_R);
  Val |= 1 << CS43L22_POWER_CTL2_SPKA1_Pos;
  Val |= 1 << CS43L22_POWER_CTL2_SPKA0_Pos;
	CS43L22_I2CWriteReg(CS43L22_POWER_CTL2_R, Val);
}

static void 
CS43L22_DisableRightSPK(void)
{
  uint8_t Val = CS43L22_I2CReadReg(CS43L22_POWER_CTL2_R);
  Val |= 1 << CS43L22_POWER_CTL2_SPKB1_Pos;
  Val |= 1 << CS43L22_POWER_CTL2_SPKB0_Pos;
	CS43L22_I2CWriteReg(CS43L22_POWER_CTL2_R, Val);
}

static void 
CS43L22_AutoDetectClock()
{
  uint8_t Val = CS43L22_I2CReadReg(CS43L22_CLOCKING_CTL_R);
  Val |= 1 << CS43L22_CLOCKING_CTL_AUTO_Pos;
	CS43L22_I2CWriteReg(CS43L22_CLOCKING_CTL_R, Val);
}

static void 
CS43L22_I2SInterface()
{
  uint8_t Val = 0;
  Val &= ~(1 << CS43L22_INTERFACE_CTL1_MS_Pos); // slave
  Val &= ~(1 << CS43L22_INTERFACE_CTL1_INV_SCLK_Pos); // Not inverted clock
  Val &= ~(1 << CS43L22_INTERFACE_CTL1_DSP_Pos); //No DSP mode
  Val &= ~(1 << CS43L22_INTERFACE_CTL1_DACDIF1_Pos); // I2S, up to 24-bit data
  Val |= (1 << CS43L22_INTERFACE_CTL1_DACDIF0_Pos); // I2S, up to 24-bit data
  Val |= (1 << CS43L22_INTERFACE_CTL1_AWL1_Pos); // 16-bit audio sample word length
  Val |= (1 << CS43L22_INTERFACE_CTL1_AWL0_Pos); // 16-bit audio sample word length
	CS43L22_I2CWriteReg(CS43L22_INTERFACE_CTL1_R, Val);
}

static void 
CS43L22_I2CWriteReg(uint8_t Reg, uint8_t Val)
{
  uint8_t data[2];
	data[0] = Reg;
	data[1] = Val;
	HAL_I2C_Master_Transmit(&gI2cx, CS43L22_I2C_ADDR, data, 2, CS43L22_I2C_TIMEOUT_MS);
}

static uint8_t 
CS43L22_I2CReadReg(uint8_t Reg)
{
  uint8_t data;
	HAL_I2C_Master_Transmit(&gI2cx, CS43L22_I2C_ADDR, &Reg, 1, CS43L22_I2C_TIMEOUT_MS);
	HAL_I2C_Master_Receive(&gI2cx, CS43L22_I2C_ADDR, &data, 1, CS43L22_I2C_TIMEOUT_MS);
  return data;
}

//---------------------------------------------------------------------------//
