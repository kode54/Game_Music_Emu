/*********************************************************/
/*    ricoh RF5C68(or clone) PCM controller              */
/*********************************************************/

#pragma once

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
typedef INT32           offs_t;
#endif

/******************************************/
/*WRITE8_DEVICE_HANDLER( rf5c68_w );

READ8_DEVICE_HANDLER( rf5c68_mem_r );
WRITE8_DEVICE_HANDLER( rf5c68_mem_w );

DEVICE_GET_INFO( rf5c68 );
#define SOUND_RF5C68 DEVICE_GET_INFO_NAME( rf5c68 )*/

#ifdef __cplusplus
extern "C" {
#endif

void rf5c68_update(void *chip, stream_sample_t **outputs, int samples);

void * device_start_rf5c68();
void device_stop_rf5c68(void *chip);
void device_reset_rf5c68(void *chip);

void rf5c68_w(void *chip, offs_t offset, UINT8 data);

UINT8 rf5c68_mem_r(void *chip, offs_t offset);
void rf5c68_mem_w(void *chip, offs_t offset, UINT8 data);
void rf5c68_write_ram(void *chip, offs_t DataStart, offs_t DataLength, const UINT8* RAMData);

void rf5c68_set_mute_mask(void *chip, UINT32 MuteMask);

#ifdef __cplusplus
}
#endif
