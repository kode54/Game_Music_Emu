#include "Nes_Mmc5_Apu.h"

#include "blargg_source.h"

void Nes_Mmc5_Apu::write_register( nes_time_t time, nes_addr_t addr, int data )
{
	// just pass relevant register writes to the standard APU
	switch ( addr )
	{
	case 0x5015: // enables
		data &= 0x03;
	
	case 0x5000: // Square 1
	case 0x5002:
	case 0x5003:
	
	case 0x5004: // Square 2
	case 0x5006:
	case 0x5007:
	
	case 0x5011: // DAC
		Nes_Apu::write_register( time, addr ^ 0x1000, data );
		break;
	
	case 0x5010: // written to for some reason
		break;
	
	default:
		debug_printf( "Unmapped MMC5 write: $%04X <- $%02X\n", (unsigned) addr, data );
	}
}
