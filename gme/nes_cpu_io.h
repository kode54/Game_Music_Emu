
#include "Nsf_Emu.h"

#if !NSF_EMU_APU_ONLY
	#include "Nes_Namco_Apu.h"
#endif

#include "blargg_source.h"

int Nsf_Emu::cpu_read( nes_addr_t addr )
{
	int result;
	
	result = cpu::low_mem [addr & 0x7FF];
	if ( !(addr & 0xE000) )
		goto exit;
	
	result = *cpu::get_code( addr );
	if ( addr > sram_addr - 1 )
		goto exit;
	
	if ( addr == Nes_Apu::status_addr )
		return apu.read_status( time() );
	
	result = cpu_read_misc( addr );
	
exit:
	return result;
}

void Nsf_Emu::cpu_write( nes_addr_t addr, int data )
{
	{
		nes_addr_t offset = addr ^ sram_addr;
		if ( offset <= sizeof sram - 1 )
		{
			sram [offset] = data;
			return;
		}
	}
	{
		int temp = addr & 0x7FF;
		if ( !(addr & 0xE000) )
		{
			cpu::low_mem [temp] = data;
			return;
		}
	}
	
	if ( unsigned (addr - Nes_Apu::start_addr) <= Nes_Apu::end_addr - Nes_Apu::start_addr )
	{
		GME_APU_HOOK( this, addr - Nes_Apu::start_addr, data );
		apu.write_register( time(), addr, data );
		return;
	}
	
	cpu_write_misc( addr, data );
}

#define CPU_READ( cpu, addr, time )         STATIC_CAST(Nsf_Emu&,*cpu).cpu_read( addr )
#define CPU_WRITE( cpu, addr, data, time )  STATIC_CAST(Nsf_Emu&,*cpu).cpu_write( addr, data )
