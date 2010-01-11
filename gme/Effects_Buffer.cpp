// Game_Music_Emu 0.5.2. http://www.slack.net/~ant/

#include "Effects_Buffer.h"

#include <string.h>

/* Copyright (C) 2006 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
details. You should have received a copy of the GNU Lesser General Public
License along with this module; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */

#include "blargg_source.h"

int const fixed_shift = 12;
#define TO_FIXED( f )   fixed_t ((f) * (1L << fixed_shift) + 0.5)
#define FROM_FIXED( f ) (f >> fixed_shift)

int const max_read = 2560; // determines minimum delay
blargg_long const echo_size = 1024 * 32;
blargg_long const echo_mask = echo_size - 1;
BOOST_STATIC_ASSERT( (echo_size & echo_mask) == 0 ); // must be power of 2

Effects_Buffer::Effects_Buffer() : Multi_Buffer( stereo )
{
	clock_rate_ = 0;
	bass_freq_  = 90;
	chans       = 0;
	chans_size  = 0;
	
	// defaults
	config_.simple.echo      = 0.20f;
	config_.simple.stereo    = 0.20f;
	config_.simple.surround  = true;
	config_.simple.enabled   = false;
	config_.enabled = false;
	apply_simple_config(); // easy way to set other parameters
	
	memset( &s, 0, sizeof s );
	clear();
}

Effects_Buffer::~Effects_Buffer()
{
	delete [] chans;
}

blargg_err_t Effects_Buffer::set_sample_rate( long rate, int msec )
{
	RETURN_ERR( echo.resize( echo_size + 2 ) );
	return Multi_Buffer::set_sample_rate( rate, msec );
}

void Effects_Buffer::clock_rate( long rate )
{
	clock_rate_ = rate;
	for ( int i = chans_size; --i >= 0; )
		chans [i].bb.clock_rate( clock_rate_ );
}

void Effects_Buffer::bass_freq( int freq )
{
	bass_freq_ = freq;
	for ( int i = chans_size; --i >= 0; )
		chans [i].bb.bass_freq( bass_freq_ );
}

blargg_err_t Effects_Buffer::set_channel_count( int count )
{
	RETURN_ERR( Multi_Buffer::set_channel_count( count ) );
	
	delete [] chans;
	chans          = 0;
	chans_size     = 0;
	samples_avail_ = 0;
	samples_read   = 0;
	
	CHECK_ALLOC( chans = BLARGG_NEW chan_t [count + extra_chans] );
	chans_size = count + extra_chans;
	
	for ( int i = chans_size; --i >= 0; )
	{
		chan_t& ch = chans [i];
		ch.cfg.vol [0] = 1.0f;
		ch.cfg.vol [1] = 1.0f;
		ch.cfg.type    = 0;
		ch.cfg.echo    = false;
		RETURN_ERR( ch.bb.set_sample_rate( sample_rate(), length() ) );
	}
	// side channels with echo
	chans [2].cfg.echo = true;
	chans [3].cfg.echo = true;
	
	clock_rate( clock_rate_ );
	bass_freq( bass_freq_ );
	apply_config();
	clear();
	
	return 0;
}

void Effects_Buffer::clear_echo()
{
	if ( echo.size() )
		memset( echo.begin(), 0, echo.size() * sizeof echo [0] );
}

void Effects_Buffer::clear()
{
	samples_avail_ = 0;
	samples_read   = 0;
	echo_pos       = 0;
	s.low_pass [0] = 0;
	s.low_pass [1] = 0;
	
	for ( int i = chans_size; --i >= 0; )
	{
		chans [i].modified = 0;
		chans [i].bb.clear();
	}
	clear_echo();
}

void Effects_Buffer::set_channel_types( int const* types )
{
	Multi_Buffer::set_channel_types( types );
	apply_config();
}

Effects_Buffer::channel_t Effects_Buffer::channel( int i, int )
{
	i += extra_chans;
	require( extra_chans <= i && i < chans_size );
	return chans [i].channel;
}


// Configuration

void Effects_Buffer::apply_simple_config()
{
	config_.delay [0] = 120;
	config_.delay [1] = 122;
	config_.feedback  = config_.simple.echo * 0.7f;
	config_.treble    = 0.6f - 0.3f * config_.simple.echo;
	
	float sep = config_.simple.stereo + 0.80f;
	if ( sep > 1.0f )
		sep = 1.0f;
	config_.side_vol [0] [0] = sep;
	config_.side_vol [1] [1] = sep;
	config_.side_vol [0] [1] = 1.0f - sep;
	config_.side_vol [1] [0] = 1.0f - sep;
	
	for ( int i = chans_size; --i >= extra_chans; )
	{
		chan_t& ch = chans [i];
		ch.cfg.echo = false;
		bool surround = config_.simple.surround;
		float pan = 0.0f;
		
		if ( !(ch.cfg.type & noise_type) )
		{
			int index = (ch.cfg.type & type_index_mask) % 6 - 3;
			if ( index < 0 )
			{
				index += 3;
				surround = false;
				ch.cfg.echo = true;
			}
			if ( index >= 1 )
			{
				pan = config_.simple.stereo;
				if ( index == 1 )
					pan = -pan;
			}
		}
		else if ( ch.cfg.type & 1 )
		{
			surround = false;
		}
		
		ch.cfg.vol [0] = 1.0f - pan;
		ch.cfg.vol [1] = 1.0f + pan;
		if ( surround )
			ch.cfg.vol [1] = -ch.cfg.vol [1];
	}
}

void Effects_Buffer::apply_config()
{
	int i;
	
	// copy channel types to chans []
	if ( channel_types() )
	{
		for ( i = chans_size; --i >= extra_chans; )
			chans [i].cfg.type = channel_types() [i - extra_chans];
	}
	
	if ( config_.simple.enabled )
		apply_simple_config();
	
	s.treble = TO_FIXED( config_.treble );
	
	bool echo_dirty = false;
	
	fixed_t old_feedback = s.feedback;
	s.feedback = TO_FIXED( config_.feedback );
	if ( !old_feedback && s.feedback )
		echo_dirty = true;
	
	// delays
	for ( i = stereo; --i >= 0; )
	{
		long delay = config_.delay [i] * sample_rate() / 1000 * stereo;
		delay = max( delay, long (max_read * stereo) );
		delay = min( delay, long (echo_size - max_read * stereo) );
		if ( s.delay [i] != delay )
		{
			s.delay [i] = delay;
			echo_dirty = true;
		}
	}
	
	// side channels
	for ( i = 2; --i >= 0; )
	{
		for ( int j = 2; --j >= 0; )
			chans [i].cfg.vol [j] = chans [i + 2].cfg.vol [j] = config_.side_vol [i] [j];
	}
	
	// main channels
	for ( i = chans_size; --i >= 0; )
	{
		chan_t& ch = chans [i];
		ch.vol [0] = TO_FIXED( ch.cfg.vol [0] );
		ch.vol [1] = TO_FIXED( ch.cfg.vol [1] );
		ch.channel.center = &ch.bb;
		ch.channel.left   = &chans [ch.cfg.echo*2  ].bb;
		ch.channel.right  = &chans [ch.cfg.echo*2+1].bb;
	}
	
	channels_changed();
	
	bool old_echo = !no_echo && !no_effects;
	optimize_config();
	if ( echo_dirty || (!old_echo && (!no_echo && !no_effects)) )
		clear_echo();
	
}

void Effects_Buffer::optimize_config()
{
	int i;
	
	// merge channels with identical configs
	for ( i = chans_size; --i >= extra_chans; )
	{
		chan_t& ch_i = chans [i];
		for ( int j = chans_size; --j > i; )
		{
			chan_t const& ch_j = chans [j];
			if (    ch_i.vol [0] == ch_j.vol [0] &&
					ch_i.vol [1] == ch_j.vol [1] &&
					(ch_i.cfg.echo == ch_j.cfg.echo || !s.feedback) )
			{
				//dprintf( "Merged %d with %d\n", i - extra_chans, j - extra_chans );
				ch_i.channel = ch_j.channel;
				break;
			}
		}
	}

	// determine whether effects and echo are needed at all
	no_effects = true;
	no_echo    = true;
	for ( i = chans_size; --i >= extra_chans; )
	{
		chan_t& ch = chans [i];
		if ( ch.cfg.echo && s.feedback )
			no_echo = false;
		
		if ( ch.vol [0] != TO_FIXED( 1 ) || ch.vol [1] != TO_FIXED( 1 ) )
			no_effects = false;
	}
	if ( !no_echo )
		no_effects = false;
	
	if (    chans [0].vol [0] != TO_FIXED( 1 ) ||
			chans [0].vol [1] != TO_FIXED( 0 ) ||
			chans [1].vol [0] != TO_FIXED( 0 ) ||
			chans [1].vol [1] != TO_FIXED( 1 ) )
		no_effects = false;
	
	if ( !config_.enabled )
		no_effects = true;
	
	if ( no_effects )
	{
		for ( i = chans_size; --i >= extra_chans; )
		{
			chan_t& ch = chans [i];
			ch.channel.center = &chans [extra_chans].bb;
			ch.channel.left   = &chans [0].bb;
			ch.channel.right  = &chans [1].bb;
		}
	}
}


// Mixing

void Effects_Buffer::end_frame( blip_time_t clock_count )
{
	samples_read = 0;
	
	for ( int i = chans_size; --i >= 0; )
	{
		chan_t& ch = chans [i];
		if ( ch.modified )
			ch.modified--;
		if ( ch.bb.clear_modified() )
			ch.modified = 2;
		ch.bb.end_frame( clock_count );
	}
	check( !samples_avail_ );
	samples_avail_ = chans [0].bb.samples_avail() * stereo;
}

long Effects_Buffer::samples_avail() const
{
	return samples_avail_;
}

void Effects_Buffer::mix_mono( blip_sample_t* out_, int pair_count )
{
	int const bass = BLIP_READER_BASS( chans [extra_chans].bb );
	BLIP_READER_BEGIN( center, chans [extra_chans].bb );
	center_reader_buf += samples_read;
	blip_sample_t* BLIP_RESTRICT out = out_;
	
	int count = pair_count;
	do
	{
		fixed_t s = BLIP_READER_READ( center );
		BLIP_READER_NEXT( center, bass );
		if ( (blip_sample_t) s != s )
			s = 0x7FFF - (s >> 24);
		
		out [0] = s;
		out [1] = s;
		out += stereo;
	}
	while ( --count );
	
	BLIP_READER_END( center, chans [extra_chans].bb );
}

void Effects_Buffer::mix_stereo( blip_sample_t* out_, int pair_count )
{
	// Fill echo with left/right channels
	int index = 1;
	do
	{
		fixed_t* BLIP_RESTRICT out = &echo [index];
		int const bass = BLIP_READER_BASS( chans [index].bb );
		BLIP_READER_BEGIN( in, chans [index].bb );
		in_reader_buf += samples_read;
		
		int count = pair_count;
		do
		{
			fixed_t s = BLIP_READER_READ_RAW( in );
			BLIP_READER_NEXT( in, bass );
			*out = s;
			out += stereo;
		}
		while ( --count );
		
		BLIP_READER_END( in, chans [index].bb );
	}
	while ( --index >= 0 );
	
	// Add center, clamp, and output
	int const bass = BLIP_READER_BASS( chans [extra_chans].bb );
	BLIP_READER_BEGIN( center, chans [extra_chans].bb );
	center_reader_buf += samples_read;
	fixed_t const* side = echo.begin();
	blip_sample_t* BLIP_RESTRICT out = out_;
	
	int count = pair_count;
	do
	{
		fixed_t in_0 = (BLIP_READER_READ_RAW( center ) + side [0]) >> (blip_sample_bits - 16);
		fixed_t in_1 = (BLIP_READER_READ_RAW( center ) + side [1]) >> (blip_sample_bits - 16);
		side += stereo;
		
		if ( (blip_sample_t) in_0 != in_0 )
			in_0 = 0x7FFF - (in_0 >> 24);
		out [0] = in_0;
		
		if ( (blip_sample_t) in_1 != in_1 )
			in_1 = 0x7FFF - (in_1 >> 24);
		out [1] = in_1;
		
		BLIP_READER_NEXT( center, bass );
		out += stereo;
	}
	while ( --count );
	
	BLIP_READER_END( center, chans [extra_chans].bb );
}

void Effects_Buffer::mix_effects( blip_sample_t* out_, int pair_count )
{
	// add channels with echo, do echo, add channels without echo, then convert to 16-bit and output
	int echo_phase = 1;
	do
	{
		// mix chans
		{
			chan_t* chan = chans;
			int chans_remain = chans_size;
			do
			{
				if ( chan->modified && chan->cfg.echo == echo_phase )
				{
					fixed_t* BLIP_RESTRICT out = &echo [echo_pos];
					int const bass = BLIP_READER_BASS( chan->bb );
					BLIP_READER_BEGIN( in, chan->bb );
					in_reader_buf += samples_read;
					fixed_t const vol_0 = chan->vol [0];
					fixed_t const vol_1 = chan->vol [1];
					
					int count = unsigned (echo_size - echo_pos) / stereo;
					int remain = pair_count;
					if ( count > remain )
						count = remain;
					do
					{
						remain -= count;
						do
						{
							fixed_t s = BLIP_READER_READ( in );
							BLIP_READER_NEXT( in, bass );
							out [0] += s * vol_0;
							out [1] += s * vol_1;
							out += stereo;
						}
						while ( --count );
						out = echo.begin();
						count = remain;
					}
					while ( remain );
					
					BLIP_READER_END( in, chan->bb );
				}
				chan++;
			}
			while ( --chans_remain );
		}
		
		// echo
		if ( echo_phase && !no_echo )
		{
			fixed_t const feedback = s.feedback;
			fixed_t const treble   = s.treble;
			
			int i = 1;
			do
			{
				fixed_t low_pass = s.low_pass [i];
				
				fixed_t* const echo_end        = &echo [echo_size + i];
				fixed_t* BLIP_RESTRICT in_pos  = &echo [echo_pos + i];
				fixed_t* BLIP_RESTRICT out_pos = &echo [(echo_pos + i + s.delay [i]) & echo_mask];
				
				// break into up to three chunks to avoid having to handle wrap-around
				// in middle of core loop
				int remain = pair_count;
				do
				{
					fixed_t* BLIP_RESTRICT pos = in_pos;
					if ( pos < out_pos )
						pos = out_pos;
					int count = blargg_ulong ((byte*) echo_end - (byte*) pos) /
							unsigned (stereo * sizeof *pos);
					if ( count > remain )
						count = remain;
					remain -= count;
					
					do
					{
						low_pass += FROM_FIXED( *in_pos - low_pass ) * treble;
						in_pos += stereo;
						
						*out_pos = FROM_FIXED( low_pass ) * feedback;
						out_pos += stereo;
					}
					while ( --count );
					
					if (  in_pos >= echo_end )  in_pos -= echo_size;
					if ( out_pos >= echo_end ) out_pos -= echo_size;
				}
				while ( remain );
				
				s.low_pass [i] = low_pass;
			}
			while ( --i >= 0 );
		}
	}
	while ( --echo_phase >= 0 );
	
	// convert to 16-bit samples and clamp
	{
		fixed_t const* in = &echo [echo_pos];
		blip_sample_t* BLIP_RESTRICT out = out_;
		int count = unsigned (echo_size - echo_pos) / stereo;
		int remain = pair_count;
		if ( count > remain )
			count = remain;
		do
		{
			remain -= count;
			do
			{
				fixed_t in_0 = FROM_FIXED( in [0] );
				fixed_t in_1 = FROM_FIXED( in [1] );
				in += stereo;
				
				if ( (blip_sample_t) in_0 != in_0 )
					in_0 = 0x7FFF - (in_0 >> 24);
				out [0] = in_0;
				
				if ( (blip_sample_t) in_1 != in_1 )
					in_1 = 0x7FFF - (in_1 >> 24);
				out [1] = in_1;
				out += stereo;
			}
			while ( --count );
			in = echo.begin();
			count = remain;
		}
		while ( remain );
	}
	
	echo_pos = (echo_pos + pair_count * stereo) & echo_mask;
}

long Effects_Buffer::read_samples( blip_sample_t* out, long out_size )
{
	out_size = min( out_size, samples_avail_ );
	
	int pairs_remain = int (out_size >> 1);
	require( pairs_remain * stereo == out_size ); // must read an even number of samples
	if ( pairs_remain )
	{
		do
		{
			// mix at most max_read pairs at a time
			int pair_count = max_read;
			if ( pair_count > pairs_remain )
				pair_count = pairs_remain;
			
			if ( !no_effects )
			{
				if ( no_echo )
				{
					// optimization: clear echo here to keep mix_effects() a leaf function
					echo_pos = 0;
					memset( echo.begin(), 0, pair_count * stereo * sizeof echo [0] );
				}
				mix_effects( out, pair_count );
			}
			else
			{
				chans [extra_chans].modified = 1;
				if ( chans [0].modified | chans [1].modified )
				{
					chans [0].modified = 1;
					chans [1].modified = 1;
					mix_stereo( out, pair_count );
				}
				else
				{
					mix_mono( out, pair_count );
				}
			}
			out += pair_count * stereo;
			samples_read += pair_count;
			pairs_remain -= pair_count;
		}
		while ( pairs_remain );
		
		samples_avail_ -= out_size;
		if ( samples_avail_ <= 0 )
		{
			check( samples_avail_ == 0 );
			
			int active_chans = 0;
			for ( int i = chans_size; --i >= 0; )
			{
				chan_t& ch = chans [i];
				check( samples_read == ch.bb.samples_avail() );
				active_chans += (ch.modified != 0);
				if ( ch.modified )
					ch.bb.remove_samples( samples_read );
				else
					ch.bb.remove_silence( samples_read );
			}
			//dprintf( "%d active chans\n", active_chans );
		}
	}
	return out_size;
}
