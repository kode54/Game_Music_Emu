/*
  File: fm.h -- header file for software emulation for FM sound generator

*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define BUILD_YM2612  1

/* select bit size of output : 8 or 16 */
#define FM_SAMPLE_BITS 16

/* select timer system internal or external */
#define FM_INTERNAL_TIMER 1

/* --- speedup optimize --- */
/* busy flag enulation , The definition of FM_GET_TIME_NOW() is necessary. */
//#define FM_BUSY_FLAG_SUPPORT 1

/* compiler dependence */
#ifndef __OSDCOMM_H__
#define __OSDCOMM_H__
typedef unsigned char	UINT8;   /* unsigned  8bit */
typedef unsigned short	UINT16;  /* unsigned 16bit */
typedef unsigned int	UINT32;  /* unsigned 32bit */
typedef signed char		INT8;    /* signed  8bit   */
typedef signed short	INT16;   /* signed 16bit   */
typedef signed int		INT32;   /* signed 32bit   */

typedef INT32           stream_sample_t;
#endif



typedef stream_sample_t FMSAMPLE;
/*
#if (FM_SAMPLE_BITS==16)
typedef INT16 FMSAMPLE;
#endif
#if (FM_SAMPLE_BITS==8)
typedef unsigned char  FMSAMPLE;
#endif
*/

/* FM_TIMERHANDLER : Stop or Start timer         */
/* int n          = chip number                  */
/* int c          = Channel 0=TimerA,1=TimerB    */
/* int count      = timer count (0=stop)         */
/* doube stepTime = step time of one count (sec.)*/

/* FM_IRQHHANDLER : IRQ level changing sense     */
/* int n       = chip number                     */
/* int irq     = IRQ level 0=OFF,1=ON            */

#if (BUILD_YM2612||BUILD_YM3438)
//void * ym2612_init(void *param, const device_config *device, int baseclock, int rate,
//               FM_TIMERHANDLER TimerHandler,FM_IRQHANDLER IRQHandler);
void * ym2612_init(int baseclock, int rate);
void ym2612_shutdown(void *chip);
void ym2612_reset_chip(void *chip);
void ym2612_update_one(void *chip, FMSAMPLE **buffer, int length);

int ym2612_write(void *chip, int a,unsigned char v);
unsigned char ym2612_read(void *chip,int a);

void ym2612_set_mutemask(void *chip, UINT32 MuteMask);
void ym2612_setoptions(void *chip, UINT8 Flags);
#endif /* (BUILD_YM2612||BUILD_YM3438) */

#ifdef __cplusplus
};
#endif
