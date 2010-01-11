#include "Opl_Apu.h"

#include "blargg_source.h"

Opl_Apu::Opl_Apu() { }

blargg_err_t Opl_Apu::init( long clock, long rate, blip_time_t period, type_t type )
{
	output( 0 );
	volume( 1.0 );
	reset();
	return 0;
}

Opl_Apu::~Opl_Apu() { }

void Opl_Apu::reset() { }

void Opl_Apu::write_data( blip_time_t time, int data ) { }

void Opl_Apu::end_frame( blip_time_t time ) { }
