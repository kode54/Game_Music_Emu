// MMC5 sound chip emulator

#ifndef NES_MMC5_APU_H
#define NES_MMC5_APU_H

#include "Nes_Apu.h"

class Nes_Mmc5_Apu : public Nes_Apu
{
public:
	enum { start_addr = 0x5000 };
	enum { end_addr   = 0x5015 };
	enum { osc_count  = 3 };
	void write_register( nes_time_t, nes_addr_t, int data );
	void osc_output( int index, Blip_Buffer* buffer );
};

inline void Nes_Mmc5_Apu::osc_output( int index, Blip_Buffer* buffer )
{
	if ( index > 1 )
		index += 2;
	Nes_Apu::osc_output( index, buffer );
}

#endif
