#ifndef OPL_APU_H
#define OPL_APU_H

#include "blargg_common.h"
#include "Blip_Buffer.h"

class Opl_Apu {
public:
	Opl_Apu();
	~Opl_Apu();
	
	enum type_t { type_opll = 0x10, type_msxmusic = 0x11, type_smsfmunit = 0x12,
			type_vrc7 = 0x13, type_opl = 0x20, type_msxaudio = 0x21, type_opl2 = 0x22 };
	blargg_err_t init( long clock, long rate, blip_time_t period, type_t );
	
	void reset();
	void volume( double v ) { synth.volume( 1.0 / 3 * v ); }
	void treble_eq( blip_eq_t const& eq ) { synth.treble_eq( eq ); }
	enum { osc_count = 1 };
	void osc_output( int index, Blip_Buffer* );
	void output( Blip_Buffer* buf ) { osc_output( 0, buf ); }
	void end_frame( blip_time_t );
	
	void write_reg( int data ) { addr = data; }
	void write_data( blip_time_t, int data );

	int read( blip_time_t, int port );
	
private:
	// noncopyable
	Opl_Apu( const Opl_Apu& );
	Opl_Apu& operator = ( const Opl_Apu& );

	Blip_Buffer* output_;
	type_t type_;
	void* opl;
	blip_time_t next_time;
	int last_amp;
	int addr;
	
	long clock_;
	long rate_;
	blip_time_t period_;
	
	Blip_Synth<blip_med_quality,2048*9*2> synth;
	
	void run_until( blip_time_t );
};

inline void Opl_Apu::osc_output( int i, Blip_Buffer* buf )
{
	assert( (unsigned) i < osc_count );
	output_ = buf;
}

#endif
