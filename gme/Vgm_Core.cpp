// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Vgm_Core.h"

#include "blargg_endian.h"
#include <math.h>

/* Copyright (C) 2003-2008 Shay Green. This module is free software; you
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

int const stereo         =  2;
int const fm_time_bits   = 12;
int const blip_time_bits = 12;

enum {
	cmd_psg_2           = 0x30,
	cmd_gg_stereo_2     = 0x3F,
	cmd_gg_stereo       = 0x4F,
	cmd_psg             = 0x50,
	cmd_ym2413          = 0x51,
	cmd_ym2612_port0    = 0x52,
	cmd_ym2612_port1    = 0x53,
	cmd_ym2151          = 0x54,
	cmd_delay           = 0x61,
	cmd_delay_735       = 0x62,
	cmd_delay_882       = 0x63,
	cmd_byte_delay      = 0x64,
	cmd_end             = 0x66,
	cmd_data_block      = 0x67,
	cmd_data_write      = 0x68,
	cmd_short_delay     = 0x70,
	cmd_pcm_delay       = 0x80,
	cmd_dac_ctrl_setup  = 0x90,
	cmd_dac_ctrl_data   = 0x91,
	cmd_dac_ctrl_freq   = 0x92,
	cmd_dac_ctrl_play   = 0x93,
	cmd_dac_ctrl_stop   = 0x94,
	cmd_dac_ctrl_play_s = 0x95,
	cmd_ym2413_2        = 0xA1,
	cmd_ym2612_2_port0  = 0xA2,
	cmd_ym2612_2_port1  = 0xA3,
	cmd_ym2151_2        = 0xA4,
	cmd_rf5c164_write   = 0xB1,
	cmd_pwm_write       = 0xB2,
	cmd_rf5c164_write_m = 0xC2,
	cmd_pcm_seek        = 0xE0,
	
	pcm_block_type      = 0x00,
	rf5c164_block_type  = 0x02,
	rf5c164_ram_write   = 0xC1,
	pwm_block_type      = 0x03,
	ym2612_dac_port     = 0x2A,
	ym2612_dac_pan_port = 0xB6
};

inline int command_len( int command )
{
	static byte const lens [0x10] = {
	// 0 1 2 3 4 5 6 7 8 9 A B C D E F
	   1,1,1,2,2,3,1,1,1,1,3,3,4,4,5,5
	};
	int len = lens [command >> 4];
	check( len != 1 );
	return len;
}

int Vgm_Core::run_ym2413( int chip, int time )
{
	return ym2413[ chip ].run_until( time );
}

int Vgm_Core::run_ym2612( int chip, int time )
{
	return ym2612[ chip ].run_until( time );
}

Vgm_Core::Vgm_Core() { blip_buf[ 1 ] = blip_buf[ 0 ] = stereo_buf.center(); }

void Vgm_Core::set_tempo( double t )
{
	if ( file_begin() )
	{
		vgm_rate = (int) (44100 * t + 0.5);
		blip_time_factor = (int) ((double)
				(1 << blip_time_bits) / vgm_rate * stereo_buf.center()->clock_rate() + 0.5);
		//dprintf( "blip_time_factor: %ld\n", blip_time_factor );
		//dprintf( "vgm_rate: %ld\n", vgm_rate );
		// TODO: remove? calculates vgm_rate more accurately (above differs at most by one Hz only)
		//blip_time_factor = (int) floor( double (1 << blip_time_bits) * psg_rate_ / 44100 / t + 0.5 );
		//vgm_rate = (int) floor( double (1 << blip_time_bits) * psg_rate_ / blip_time_factor + 0.5 );
		
		fm_time_factor = 2 + (int) (fm_rate * (1 << fm_time_bits) / vgm_rate + 0.5);
	}
}

bool Vgm_Core::header_t::valid_tag() const
{
	return !memcmp( tag, "Vgm ", 4 );
}

blargg_err_t Vgm_Core::load_mem_( byte const data [], int size )
{
	assert( offsetof (header_t,loop_modifier) + sizeof (((header_t*)0)->loop_modifier) == header_t::size );
	
	if ( size <= header_t::size )
		return blargg_err_file_type;
	
	header_t const& h = *(header_t const*) data;
	
	if ( !h.valid_tag() )
		return blargg_err_file_type;
	
	check( get_le32( h.version ) <= 0x150 );
	
	// Get loop
	loop_begin = file_end();
	if ( get_le32( h.loop_offset ) )
		loop_begin = &data [get_le32( h.loop_offset ) + offsetof (header_t,loop_offset)];
	
	// PSG rate
	int psg_rate = get_le32( h.psg_rate );
	if ( !psg_rate )
		psg_rate = 3579545;
	stereo_buf.clock_rate( psg_rate );
	
	// Disable FM
	fm_rate = 0;
	ym2612[ 0 ].enable( false );
	ym2612[ 1 ].enable( false );
	ym2413[ 0 ].enable( false );
	ym2413[ 1 ].enable( false );
	
	set_tempo( 1 );
	
	return blargg_ok;
}

// Update pre-1.10 header FM rates by scanning commands
void Vgm_Core::update_fm_rates( int* ym2413_rate, int* ym2612_rate ) const
{
	byte const* p = file_begin() + 0x40;
	while ( p < file_end() )
	{
		switch ( *p )
		{
		case cmd_end:
			return;
		
		case cmd_psg:
		case cmd_psg_2:
		case cmd_byte_delay:
			p += 2;
			break;
		
		case cmd_delay:
			p += 3;
			break;
		
		case cmd_data_block:
			p += 7 + get_le32( p + 3 );
			break;

		case cmd_data_write:
			p += 12;
			break;
		
		case cmd_ym2413:
		case cmd_ym2413_2:
			*ym2612_rate = 0;
			return;
		
		case cmd_ym2612_port0:
		case cmd_ym2612_port1:
		case cmd_ym2612_2_port0:
		case cmd_ym2612_2_port1:
			*ym2612_rate = *ym2413_rate;
			*ym2413_rate = 0;
			return;
		
		case cmd_ym2151:
		case cmd_ym2151_2:
			*ym2413_rate = 0;
			*ym2612_rate = 0;
			return;
		
		default:
			p += command_len( *p );
		}
	}
}

blargg_err_t Vgm_Core::init_fm( double* rate )
{
	int ym2612_rate = get_le32( header().ym2612_rate );
	int ym2413_rate = get_le32( header().ym2413_rate );
	if ( ym2413_rate && get_le32( header().version ) < 0x110 )
		update_fm_rates( &ym2413_rate, &ym2612_rate );
	
	if ( ym2612_rate )
	{
		bool second_chip = !!(ym2612_rate & 0x40000000);
		ym2612_rate &= ~0x40000000;
		if ( !*rate )
			*rate = ym2612_rate / 144.0;
		RETURN_ERR( ym2612[0].set_rate( *rate, ym2612_rate ) );
		ym2612[0].enable();
		if ( second_chip )
		{
			RETURN_ERR( ym2612[1].set_rate( *rate, ym2612_rate ) );
			ym2612[1].enable();
		}
	}
	else if ( ym2413_rate )
	{
		bool second_chip = !!( ym2413_rate & 0x40000000 );
		ym2413_rate &= ~0x40000000;
		if ( !*rate )
			*rate = ym2413_rate / 72.0;
		int result = ym2413[0].set_rate( *rate, ym2413_rate );
		if ( result == 2 )
			return "YM2413 FM sound not supported";
		CHECK_ALLOC( !result );
		ym2413[0].enable();
		if ( second_chip )
		{
			ym2413[1].set_rate( *rate, ym2413_rate );
			ym2413[1].enable();
		}
	}
	
	fm_rate = *rate;
	
	return blargg_ok;
}

void Vgm_Core::start_track()
{
	psg[ 0 ].reset( get_le16( header().noise_feedback ), header().noise_width );
	psg[ 1 ].reset( get_le16( header().noise_feedback ), header().noise_width );
	
	blip_buf[ 0 ] = stereo_buf.center();
	blip_buf[ 1 ] = blip_buf[ 0 ];

	dac_disabled[ 0 ] = -1;
	dac_disabled[ 1 ] = -1;
	pos          = file_begin() + header_t::size;
	pcm_data     = pos;
	pcm_pos      = pos;
	dac_amp[ 0 ] = -1;
	dac_amp[ 1 ] = -1;
	vgm_time     = 0;
	if ( get_le32( header().version ) >= 0x150 )
	{
		int data_offset = get_le32( header().data_offset );
		check( data_offset );
		if ( data_offset )
			pos += data_offset + offsetof (header_t,data_offset) - 0x40;
	}
	
	if ( uses_fm() )
	{
		if ( ym2413[0].enabled() )
			ym2413[0].reset();
		if ( ym2413[1].enabled() )
			ym2413[1].reset();
		
		if ( ym2612[0].enabled() )
			ym2612[0].reset();
		if ( ym2612[1].enabled() )
			ym2612[1].reset();
		
		stereo_buf.clear();
	}
	
	fm_time_offset = 0;
}

inline Vgm_Core::fm_time_t Vgm_Core::to_fm_time( vgm_time_t t ) const
{
	return (t * fm_time_factor + fm_time_offset) >> fm_time_bits;
}

inline blip_time_t Vgm_Core::to_psg_time( vgm_time_t t ) const
{
	return (t * blip_time_factor) >> blip_time_bits;
}

void Vgm_Core::write_pcm( vgm_time_t vgm_time, int chip, int amp )
{
	if ( blip_buf[ chip ] )
	{
		check( amp >= 0 );
		blip_time_t blip_time = to_psg_time( vgm_time );
		int old = dac_amp[ chip ];
		int delta = amp - old;
		dac_amp[ chip ] = amp;
		blip_buf[ chip ]->set_modified();
		if ( old >= 0 ) // first write is ignored, to avoid click
			pcm.offset_inline( blip_time, delta, blip_buf[ chip ] );
		else
			dac_amp[ chip ] |= dac_disabled[ chip ];
	}
}

blip_time_t Vgm_Core::run( vgm_time_t end_time )
{
	vgm_time_t vgm_time = this->vgm_time; 
	byte const* pos = this->pos;
	if ( pos > file_end() )
		set_warning( "Stream lacked end event" );
	
	while ( vgm_time < end_time && pos < file_end() )
	{
		int current_chip = 0;

		// TODO: be sure there are enough bytes left in stream for particular command
		// so we don't read past end
		switch ( *pos++ )
		{
		case cmd_end:
			pos = loop_begin; // if not looped, loop_begin == file_end()
			break;
		
		case cmd_delay_735:
			vgm_time += 735;
			break;
		
		case cmd_delay_882:
			vgm_time += 882;
			break;
		
		case cmd_gg_stereo_2:
			current_chip = 1;
		case cmd_gg_stereo:
			psg[ current_chip ].write_ggstereo( to_psg_time( vgm_time ), *pos++ );
			break;
		
		case cmd_psg_2:
			current_chip = 1;
		case cmd_psg:
			psg[ current_chip ].write_data( to_psg_time( vgm_time ), *pos++ );
			break;
		
		case cmd_delay:
			vgm_time += pos [1] * 0x100 + pos [0];
			pos += 2;
			break;
		
		case cmd_byte_delay:
			vgm_time += *pos++;
			break;
		
		case cmd_ym2413_2:
			current_chip = 1;
		case cmd_ym2413:
			if ( run_ym2413( current_chip, to_fm_time( vgm_time ) ) )
				ym2413[ current_chip ].write( pos [0], pos [1] );
			pos += 2;
			break;
		
		case cmd_ym2612_2_port0:
			current_chip = 1;
		case cmd_ym2612_port0:
			if ( pos [0] == ym2612_dac_port )
			{
				write_pcm( vgm_time, current_chip, pos [1] );
			}
			else if ( run_ym2612( current_chip, to_fm_time( vgm_time ) ) )
			{
				if ( pos [0] == 0x2B )
				{
					dac_disabled[ current_chip ] = (pos [1] >> 7 & 1) - 1;
					dac_amp[ current_chip ] |= dac_disabled[ current_chip ];
				}
				ym2612[ current_chip ].write0( pos [0], pos [1] );
			}
			pos += 2;
			break;
		
		case cmd_ym2612_2_port1:
			current_chip = 1;
		case cmd_ym2612_port1:
			if ( run_ym2612( current_chip, to_fm_time( vgm_time ) ) )
			{
				if ( pos [0] == ym2612_dac_pan_port )
				{
					Blip_Buffer * blip_buf = NULL;
					switch ( pos [1] >> 6 )
					{
					case 0: blip_buf = NULL; break;
					case 1: blip_buf = stereo_buf.right(); break;
					case 2: blip_buf = stereo_buf.left(); break;
					case 3: blip_buf = stereo_buf.center(); break;
					}
					/*if ( this->blip_buf != blip_buf )
					{
						blip_time_t blip_time = to_psg_time( vgm_time );
						if ( this->blip_buf ) pcm.offset_inline( blip_time, -dac_amp, this->blip_buf );
						if ( blip_buf )       pcm.offset_inline( blip_time,  dac_amp, blip_buf );
					}*/
					this->blip_buf[ current_chip ] = blip_buf;
				}
				ym2612[ current_chip ].write1( pos [0], pos [1] );
			}
			pos += 2;
			break;
			
		case cmd_data_block: {
			check( *pos == cmd_end );
			int type = pos [1];
			int size = get_le32( pos + 2 );
			pos += 6;
			switch ( type & 0xC0 )
			{
			case 0x00:
			case 0x40:
				if ( type == pcm_block_type )
					pcm_data = pos;
				else if ( type == rf5c164_block_type )
					rf5c164_data = pos;
				else if ( type == pwm_block_type )
					pwm_data = pos;
				break;
			case 0xC0:
				{
					int write_offset = get_le16( pos );
					if ( type == rf5c164_ram_write )
					{
						// rf5c164.write_ram( write_offset, pos + 2, size - 2 )
					}
				}
				break;
			}
			pos += size;
			break;
		}

		case cmd_data_write: {
			check( *pos == cmd_end );
			int type = pos [1];
			int data_start = get_le24( pos + 2 );
			int write_offset = get_le24( pos + 5 );
			int data_length = get_le24( pos + 8 );
			pos += 11;
			if ( !data_length ) data_length = 0x1000000;
			byte const* rom_data;
			switch ( type )
			{
			// case 0x01: // rf5c68
			case 0x02:
				rom_data = rf5c164_data + data_start;
				// rf5c68.rom_write( write_offset, data_length, rom_data );
				break;
			}
			break;
		}
		
		case cmd_pcm_seek:
			pcm_pos = pcm_data + pos [3] * 0x1000000 + pos [2] * 0x10000 +
					pos [1] * 0x100 + pos [0];
			pos += 4;
			break;
		
		default:
			int cmd = pos [-1];
			switch ( cmd & 0xF0 )
			{
				case cmd_pcm_delay:
					write_pcm( vgm_time, 0, *pcm_pos++ );
					vgm_time += cmd & 0x0F;
					break;
				
				case cmd_short_delay:
					vgm_time += (cmd & 0x0F) + 1;
					break;
				
				case 0x50:
					pos += 2;
					break;
				
				default:
					pos += command_len( cmd ) - 1;
					set_warning( "Unknown stream event" );
			}
		}
	}
	vgm_time -= end_time;
	this->pos = pos;
	this->vgm_time = vgm_time;
	
	return to_psg_time( end_time );
}

blip_time_t Vgm_Core::run_psg( int msec )
{
	blip_time_t t = run( msec * vgm_rate / 1000 );
	psg[ 0 ].end_frame( t );
	psg[ 1 ].end_frame( t );
	return t;
}

int Vgm_Core::play_frame( blip_time_t blip_time, int sample_count, blip_sample_t out [] )
{
	// to do: timing is working mostly by luck
	int min_pairs = (unsigned) sample_count / 2;
	int vgm_time = (min_pairs << fm_time_bits) / fm_time_factor - 1;
	assert( to_fm_time( vgm_time ) <= min_pairs );
	int pairs;
	while ( (pairs = to_fm_time( vgm_time )) < min_pairs )
		vgm_time++;
	//dprintf( "pairs: %d, min_pairs: %d\n", pairs, min_pairs );
	
	if ( ym2612[ 0 ].enabled() )
	{
		ym2612[ 0 ].begin_frame( out );
		if ( ym2612[ 1 ].enabled() )
			ym2612[ 1 ].begin_frame( out );
		memset( out, 0, pairs * stereo * sizeof *out );
	}
	else if ( ym2413[ 0 ].enabled() )
	{
		ym2413[ 0 ].begin_frame( out );
		if ( ym2413[ 1 ].enabled() )
			ym2413[ 1 ].begin_frame( out );
		memset( out, 0, pairs * stereo * sizeof *out );
	}
	
	run( vgm_time );
	run_ym2612( 0, pairs );
	run_ym2612( 1, pairs );
	run_ym2413( 0, pairs );
	run_ym2413( 1, pairs );
	
	fm_time_offset = (vgm_time * fm_time_factor + fm_time_offset) - (pairs << fm_time_bits);
	
	psg[ 0 ].end_frame( blip_time );
	psg[ 1 ].end_frame( blip_time );
	
	return pairs * stereo;
}
