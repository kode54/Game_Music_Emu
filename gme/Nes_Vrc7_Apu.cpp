#include "Nes_Vrc7_Apu.h"

#define MAME_YM2413

#ifndef MAME_YM2413
#include "emu2413.h"
#else
#include "ym2413.h"
#endif
#include <string.h>

#include "blargg_source.h"

int const period = 36; // NES CPU clocks per FM clock

Nes_Vrc7_Apu::Nes_Vrc7_Apu()
{
	opll = 0;
}

blargg_err_t Nes_Vrc7_Apu::init()
{
#ifndef MAME_YM2413
	CHECK_ALLOC( opll = VRC7_new( 3579545 ) );
#else
	CHECK_ALLOC( opll = ym2413_init( 3579545, 3579545 / 72, 1 ) );
#endif
	
	output( 0 );
	volume( 1.0 );
	reset();
	return 0;
}

Nes_Vrc7_Apu::~Nes_Vrc7_Apu()
{
	if ( opll )
#ifndef MAME_YM2413
		VRC7_delete( opll );
#else
		ym2413_shutdown( opll );
#endif
}

void Nes_Vrc7_Apu::output( Blip_Buffer* buf )
{
	for ( int i = 0; i < osc_count; i++ )
		oscs [i].output = buf;
	output_changed();
}

void Nes_Vrc7_Apu::output_changed()
{
	mono.output = oscs [0].output;
	for ( int i = 1; i < osc_count; i++ )
	{
		if ( mono.output != oscs [i].output )
		{
			mono.output = 0;
			break;
		}
	}
	
	if ( mono.output )
	{
		for ( int i = 1; i < osc_count; i++ )
		{
			mono.last_amp += oscs [i].last_amp;
			oscs [i].last_amp = 0;
		}
	}
}

void Nes_Vrc7_Apu::reset()
{
	addr      = 0;
	next_time = 0;
	mono.last_amp = 0;
	
	for ( int i = 0; i < osc_count; ++i )
	{
		Vrc7_Osc& osc = oscs [i];
		osc.last_amp = 0;
		for ( int j = 0; j < 3; ++j )
			osc.regs [j] = 0;
	}

#ifndef MAME_YM2413
	VRC7_reset( opll );
#else
	ym2413_reset_chip( opll );
#endif
}

void Nes_Vrc7_Apu::write_reg( int data )
{
	addr = data;
}

void Nes_Vrc7_Apu::write_data( nes_time_t time, int data )
{
	int type = (addr >> 4) - 1;
	int chan = addr & 15;
	if ( (unsigned) type < 3 && chan < osc_count )
		oscs [chan].regs [type] = data;
	
	if ( time > next_time )
		run_until( time );
#ifndef MAME_YM2413
	VRC7_writeReg( opll, addr, data );
#else
	ym2413_write( opll, 0, addr );
	ym2413_write( opll, 1, data );
#endif
}

void Nes_Vrc7_Apu::end_frame( nes_time_t time )
{
	if ( time > next_time )
		run_until( time );
	
	next_time -= time;
	assert( next_time >= 0 );
}

void Nes_Vrc7_Apu::save_snapshot( vrc7_snapshot_t* out ) const
{
	out->latch = addr;
	out->delay = next_time;
	for ( int i = 0; i < osc_count; ++i )
	{
		for ( int j = 0; j < 3; ++j )
			out->regs [i] [j] = oscs [i].regs [j];
	}
#ifndef MAME_YM2413
	memcpy( out->inst, opll->CustInst, 8 );
#else
	memcpy( out->inst, ym2413_get_inst0( opll ), 8 );
#endif
}

void Nes_Vrc7_Apu::load_snapshot( vrc7_snapshot_t const& in )
{
	assert( offsetof (vrc7_snapshot_t,delay) == 28 - 1 );
	
	reset();
	next_time = in.delay;
	write_reg( in.latch );
	int i;
	for ( i = 0; i < osc_count; ++i )
	{
		for ( int j = 0; j < 3; ++j )
			oscs [i].regs [j] = in.regs [i] [j];
	}

	for ( i = 0; i < 8; ++i )
#ifndef MAME_YM2413
		VRC7_writeReg( opll, i, in.inst [i] );
#else
	{
		ym2413_write( opll, 0, i );
		ym2413_write( opll, 1, in.inst [i] );
	}
#endif

	for ( i = 0; i < 3; ++i )
	{
		for ( int j = 0; j < 6; ++j )
#ifndef MAME_YM2413
			VRC7_writeReg( opll, 0x10 + i * 0x10 + j, oscs [j].regs [i] );
#else
		{
			ym2413_write( opll, 0, 0x10 + i * 0x10 + j );
			ym2413_write( opll, 1, oscs [j].regs [i] );
		}
#endif
	}
}

void Nes_Vrc7_Apu::run_until( nes_time_t end_time )
{
	require( end_time > next_time );

	nes_time_t time = next_time;
#ifndef MAME_YM2413
	OPLL* const opll = this->opll; // cache
#else
	void* opll = this->opll;
#endif
	Blip_Buffer* const mono_output = mono.output;
	if ( mono_output )
	{
		mono_output->set_modified();
		// optimal case
		do
		{
#ifndef MAME_YM2413
			VRC7_run( opll );
#else
			ym2413_advance_lfo( opll );
#endif
			int amp = 0;
			for ( int i = 0; i < osc_count; i++ )
#ifndef MAME_YM2413
				amp += VRC7_calcCh( opll, i );
#else
				amp += ym2413_calcch( opll, i );

			ym2413_advance( opll );
#endif
			int delta = amp - mono.last_amp;
			if ( delta )
			{
				mono.last_amp = amp;
				synth.offset_inline( time, delta, mono_output );
			}
			time += period;
		}
		while ( time < end_time );
	}
	else
	{
		for ( int i = osc_count; --i >= 0; )
		{
			Blip_Buffer* output = oscs [i].output;
			if ( output )
				output->set_modified();
		}
		
		mono.last_amp = 0;
		do
		{
#ifndef MAME_YM2413
			VRC7_run( opll );
#else
			ym2413_advance_lfo( opll );
#endif
			for ( int i = 0; i < osc_count; ++i )
			{
				Vrc7_Osc& osc = oscs [i];
				if ( osc.output )
				{
#ifndef MAME_YM2413
					int amp = VRC7_calcCh( opll, i );
#else
					int amp = ym2413_calcch( opll, i );
#endif
					int delta = amp - osc.last_amp;
					if ( delta )
					{
						osc.last_amp = amp;
						synth.offset( time, delta, osc.output );
					}
				}
			}
#ifdef MAME_YM2413
			ym2413_advance( opll );
#endif
			time += period;
		}
		while ( time < end_time );
	}
	next_time = time;
}
