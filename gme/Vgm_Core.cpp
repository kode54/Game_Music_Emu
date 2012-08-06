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
	cmd_ram_block       = 0x68,
	cmd_short_delay     = 0x70,
	cmd_pcm_delay       = 0x80,
	cmd_rf5c68          = 0xB0,
	cmd_rf5c164         = 0xB1,
	cmd_pwm             = 0xB2,
	cmd_segapcm_write   = 0xC0,
	cmd_rf5c68_mem      = 0xC1,
	cmd_rf5c164_mem     = 0xC2,
	cmd_c140            = 0xD4,
	cmd_pcm_seek        = 0xE0,

	rf5c68_ram_block    = 0x01,
	rf5c164_ram_block   = 0x02,
	
	pcm_block_type      = 0x00,
	rom_block_type      = 0x80,
	ram_block_type      = 0xC0,

	rom_segapcm         = 0x80,
	rom_c140            = 0x8D,

	ram_rf5c68          = 0xC0,
	ram_rf5c164         = 0xC1,
	ram_nesapu          = 0xC2,

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

int Vgm_Core::run_ym2151( int time )
{
	return ym2151.run_until( time );
}

int Vgm_Core::run_ym2413( int time )
{
	return ym2413.run_until( time );
}

int Vgm_Core::run_ym2612( int time )
{
	return ym2612.run_until( time );
}

int Vgm_Core::run_c140( int time )
{
	return c140.run_until( time );
}

int Vgm_Core::run_segapcm( int time )
{
	return segapcm.run_until( time );
}

int Vgm_Core::run_rf5c68( int time )
{
	return rf5c68.run_until( time );
}

int Vgm_Core::run_rf5c164( int time )
{
	return rf5c164.run_until( time );
}

int Vgm_Core::run_pwm( int time )
{
	return pwm.run_until( time );
}

Vgm_Core::Vgm_Core() { blip_buf = stereo_buf.center(); }

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

int Vgm_Core::header_t::size() const
{
	unsigned int version = get_le32( this->version );
	return ( version > 0x150 ) ? ( ( version > 0x160 ) ? size_max : size_151 ) : size_min;
}

void Vgm_Core::header_t::cleanup()
{
	unsigned int version = get_le32( this->version );

	if ( version < 0x161 )
	{
		memset( this->gbdmg_rate, 0, size_max - offsetof(header_t, gbdmg_rate) );
	}

	if ( version < 0x160 )
	{
		volume_modifier = 0;
		reserved = 0;
		loop_base = 0;
	}

	if ( version < 0x151 ) memset( this->rf5c68_rate, 0, size_max - size_min );

	if ( version < 0x150 )
	{
		set_le32( data_offset, size_min - offsetof(header_t, data_offset) );
		sn76489_flags = 0;
		set_le32( segapcm_rate, 0 );
		set_le32( segapcm_reg, 0 );
	}

	if ( version < 0x110 )
	{
		set_le16( noise_feedback, 0 );
		noise_width = 0;
		unsigned int rate = get_le32( ym2413_rate );
		set_le32( ym2612_rate, rate );
		set_le32( ym2151_rate, rate );
	}

	if ( version < 0x101 )
	{
		set_le32( frame_rate, 0 );
	}
}

blargg_err_t Vgm_Core::load_mem_( byte const data [], int size )
{
	assert( offsetof (header_t, rf5c68_rate) == header_t::size_min );
	assert( offsetof (header_t, extra_offset[4]) == header_t::size_max );
	
	if ( size <= header_t::size_min )
		return blargg_err_file_type;

	memcpy( &_header, data, header_t::size_min );
	
	header_t const& h = header();
	
	if ( !h.valid_tag() )
		return blargg_err_file_type;

	int version = get_le32( h.version );
	
	check( version < 0x100 );

	if ( version > 0x150 )
	{
		if ( size < header().size() )
			return "Invalid header";

		memcpy( &_header.rf5c68_rate, data + offsetof (header_t, rf5c68_rate), header().size() - header_t::size_min );
	}

	_header.cleanup();

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
	ym2612.enable( false );
	ym2413.enable( false );
	ym2151.enable( false );
	c140.enable( false );
	segapcm.enable( false );
	rf5c68.enable( false );
	rf5c164.enable( false );
	pwm.enable( false );
	
	set_tempo( 1 );
	
	return blargg_ok;
}

// Update pre-1.10 header FM rates by scanning commands
void Vgm_Core::update_fm_rates( int* ym2151_rate, int* ym2413_rate, int* ym2612_rate ) const
{
	byte const* p = file_begin() + header().size();
	int data_offset = get_le32( header().data_offset );
	check( data_offset );
	if ( data_offset )
		p += data_offset + offsetof( header_t, data_offset ) - header().size();
	while ( p < file_end() )
	{
		switch ( *p )
		{
		case cmd_end:
			return;
		
		case cmd_psg:
		case cmd_byte_delay:
			p += 2;
			break;
		
		case cmd_delay:
			p += 3;
			break;
		
		case cmd_data_block:
			p += 7 + get_le32( p + 3 );
			break;

		case cmd_ram_block:
			p += 12;
			break;
		
		case cmd_ym2413:
			*ym2151_rate = 0;
			*ym2612_rate = 0;
			return;
		
		case cmd_ym2612_port0:
		case cmd_ym2612_port1:
			*ym2612_rate = *ym2413_rate;
			*ym2413_rate = 0;
			*ym2151_rate = 0;
			return;
		
		case cmd_ym2151:
			*ym2151_rate = *ym2413_rate;
			*ym2413_rate = 0;
			*ym2612_rate = 0;
			return;
		
		default:
			p += command_len( *p );
		}
	}
}

blargg_err_t Vgm_Core::init_chips( double* rate )
{
	int ym2612_rate = get_le32( header().ym2612_rate );
	int ym2413_rate = get_le32( header().ym2413_rate );
	int ym2151_rate = get_le32( header().ym2151_rate );
	int c140_rate = get_le32( header().c140_rate );
	int segapcm_rate = get_le32( header().segapcm_rate );
	int rf5c68_rate = get_le32( header().rf5c68_rate );
	int rf5c164_rate = get_le32( header().rf5c164_rate );
	int pwm_rate = get_le32( header().pwm_rate );
	if ( ym2413_rate && get_le32( header().version ) < 0x110 )
		update_fm_rates( &ym2151_rate, &ym2413_rate, &ym2612_rate );
	
	if ( ym2612_rate > 0 )
	{
		if ( !*rate )
			*rate = ym2612_rate / 144.0;
		RETURN_ERR( ym2612.set_rate( *rate, ym2612_rate ) );
		ym2612.enable();
	}
	else if ( ym2413_rate > 0 )
	{
		if ( !*rate )
			*rate = ym2413_rate / 72.0;
		int result = ym2413.set_rate( *rate, ym2413_rate );
		if ( result == 2 )
			return "YM2413 FM sound not supported";
		CHECK_ALLOC( !result );
		ym2413.enable();
	}
	else if ( ym2151_rate > 0 )
	{
		if ( !*rate )
			*rate = ym2151_rate / 64.0;
		int result = ym2151.set_rate( *rate, ym2151_rate );
		CHECK_ALLOC( !result );
		ym2151.enable();
	}

	if ( c140_rate > 0 )
	{
		if ( !*rate )
			*rate = c140_rate;
		int result = c140.set_rate( header().c140_type, *rate, c140_rate );
		CHECK_ALLOC( !result );
		c140.enable();
	}
	else if ( segapcm_rate > 0 )
	{
		if ( !*rate )
			*rate = segapcm_rate / 128.0;
		int result = segapcm.set_rate( get_le32( header().segapcm_reg ) );
		CHECK_ALLOC( !result );
		segapcm.enable();
	}
	else if ( rf5c68_rate > 0 )
	{
		if ( !*rate )
			*rate = rf5c68_rate / 384.0;
		int result = rf5c68.set_rate();
		CHECK_ALLOC( !result );
		rf5c68.enable();
	}
	else if ( rf5c164_rate > 0 )
	{
		if ( !*rate )
			*rate = rf5c164_rate / 384.0;
		int result = rf5c164.set_rate( rf5c164_rate );
		CHECK_ALLOC( !result );
		rf5c164.enable();
	}
	else if ( pwm_rate > 0 )
	{
		if ( !*rate )
			*rate = 22020;
		int result = pwm.set_rate( pwm_rate );
		CHECK_ALLOC( !result );
		pwm.enable();
	}
	
	fm_rate = *rate;
	
	return blargg_ok;
}

void Vgm_Core::start_track()
{
	psg.reset( get_le16( header().noise_feedback ), header().noise_width );
	
	blip_buf = stereo_buf.center();

	dac_disabled = -1;
	pos          = file_begin() + header().size();
	dac_amp      = -1;
	vgm_time     = 0;
	int data_offset = get_le32( header().data_offset );
	check( data_offset );
	if ( data_offset )
		pos += data_offset + offsetof (header_t,data_offset) - header().size();
	pcm_data[0]  = pos;
	pcm_pos      = pos;
	
	if ( uses_fm() )
	{
		if ( rf5c68.enabled() )
			rf5c68.reset();

		if ( rf5c164.enabled() )
			rf5c164.reset();

		if ( segapcm.enabled() )
			segapcm.reset();

		if ( pwm.enabled() )
			pwm.reset();

		if ( c140.enabled() )
			c140.reset();

		if ( ym2151.enabled() )
			ym2151.reset();

		if ( ym2413.enabled() )
			ym2413.reset();
		
		if ( ym2612.enabled() )
			ym2612.reset();
		
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

void Vgm_Core::write_pcm( vgm_time_t vgm_time, int amp )
{
	if ( blip_buf )
	{
		check( amp >= 0 );
		blip_time_t blip_time = to_psg_time( vgm_time );
		int old = dac_amp;
		int delta = amp - old;
		dac_amp = amp;
		blip_buf->set_modified();
		if ( old >= 0 ) // first write is ignored, to avoid click
			pcm.offset_inline( blip_time, delta, blip_buf );
		else
			dac_amp |= dac_disabled;
	}
}

blip_time_t Vgm_Core::run( vgm_time_t end_time )
{
	vgm_time_t vgm_time = this->vgm_time; 
	vgm_time_t vgm_loop_time = ~0;
	byte const* pos = this->pos;
	if ( pos > file_end() )
		set_warning( "Stream lacked end event" );
	
	while ( vgm_time < end_time && pos < file_end() )
	{
		// TODO: be sure there are enough bytes left in stream for particular command
		// so we don't read past end
		switch ( *pos++ )
		{
		case cmd_end:
			if ( vgm_loop_time == ~0 ) vgm_loop_time = vgm_time;
			else if ( vgm_loop_time == vgm_time ) loop_begin = file_end(); // XXX some files may loop forever on a region without any delay commands
			pos = loop_begin; // if not looped, loop_begin == file_end()
			break;
		
		case cmd_delay_735:
			vgm_time += 735;
			break;
		
		case cmd_delay_882:
			vgm_time += 882;
			break;
		
		case cmd_gg_stereo:
			psg.write_ggstereo( to_psg_time( vgm_time ), *pos++ );
			break;
		
		case cmd_psg:
			psg.write_data( to_psg_time( vgm_time ), *pos++ );
			break;
		
		case cmd_delay:
			vgm_time += pos [1] * 0x100 + pos [0];
			pos += 2;
			break;
		
		case cmd_byte_delay:
			vgm_time += *pos++;
			break;

		case cmd_segapcm_write:
			if ( get_le32( header().segapcm_rate ) > 0 )
				if ( run_segapcm( to_fm_time( vgm_time ) ) )
					segapcm.write( get_le16( pos ), pos [2] );
			pos += 3;
			break;

		case cmd_rf5c68:
			if ( get_le32( header().rf5c68_rate ) > 0 )
				if ( run_rf5c68( to_fm_time( vgm_time ) ) )
					rf5c68.write( pos [0], pos [1] );
			pos += 2;
			break;

		case cmd_rf5c68_mem:
			if ( get_le32( header().rf5c68_rate ) > 0 )
				if ( run_rf5c68( to_fm_time( vgm_time ) ) )
					rf5c68.write_mem( get_le16( pos ), pos [2] );
			pos += 3;
			break;

		case cmd_rf5c164:
			if ( get_le32( header().rf5c164_rate ) > 0 )
				if ( run_rf5c164( to_fm_time( vgm_time ) ) )
					rf5c164.write( pos [0], pos [1] );
			pos += 2;
			break;

		case cmd_rf5c164_mem:
			if ( get_le32( header().rf5c164_rate ) > 0 )
				if ( run_rf5c164( to_fm_time( vgm_time ) ) )
					rf5c164.write_mem( get_le16( pos ), pos [2] );
			pos += 3;
			break;

		case cmd_pwm:
			if ( get_le32( header().pwm_rate ) > 0 )
				if ( run_pwm( to_fm_time( vgm_time ) ) )
					pwm.write( pos [0] >> 4, ( ( pos [0] & 0x0F ) << 8 ) + pos [1] );
			pos += 2;
			break;

		case cmd_c140:
			if ( get_le32( header().c140_rate ) > 0 )
				if ( run_c140( to_fm_time( vgm_time ) ) )
					c140.write( get_be16( pos ), pos [2] );
			pos += 3;
			break;

		case cmd_ym2151:
			if ( run_ym2151( to_fm_time( vgm_time ) ) )
				ym2151.write( pos [0], pos [1] );
			pos += 2;
			break;
		
		case cmd_ym2413:
			if ( run_ym2413( to_fm_time( vgm_time ) ) )
				ym2413.write( pos [0], pos [1] );
			pos += 2;
			break;
		
		case cmd_ym2612_port0:
			if ( pos [0] == ym2612_dac_port )
			{
				write_pcm( vgm_time, pos [1] );
			}
			else if ( run_ym2612( to_fm_time( vgm_time ) ) )
			{
				if ( pos [0] == 0x2B )
				{
					dac_disabled = (pos [1] >> 7 & 1) - 1;
					dac_amp |= dac_disabled;
				}
				ym2612.write0( pos [0], pos [1] );
			}
			pos += 2;
			break;
		
		case cmd_ym2612_port1:
			if ( run_ym2612( to_fm_time( vgm_time ) ) )
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
					this->blip_buf = blip_buf;
				}
				ym2612.write1( pos [0], pos [1] );
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
			case pcm_block_type:
				check( ( type & 0x3F ) <= 2 );
				pcm_data[ type & 0x3F ] = pos;
				break;

			case rom_block_type:
				if ( size >= 8 )
				{
					int rom_size = get_le32( pos );
					int data_start = get_le32( pos + 4 );
					int data_size = size - 8;
					void * rom_data = ( void * ) ( pos + 8 );

					switch ( type )
					{
					case rom_segapcm:
						if ( get_le32( header().segapcm_rate ) )
							segapcm.write_rom( rom_size, data_start, data_size, rom_data );
						break;

					case rom_c140:
						if ( get_le32( header().c140_rate ) )
							c140.write_rom( rom_size, data_start, data_size, rom_data );
						break;
					}
				}
				break;

			case ram_block_type:
				if ( size >= 2 )
				{
					int data_start = get_le16( pos );
					int data_size = size - 2;
					void * ram_data = ( void * ) ( pos + 2 );

					switch ( type )
					{
					case ram_rf5c68:
						if ( get_le32( header().rf5c68_rate ) > 0 )
							rf5c68.write_ram( data_start, data_size, ram_data );
						break;

					case ram_rf5c164:
						if ( get_le32( header().rf5c164_rate ) > 0 )
							rf5c164.write_ram( data_start, data_size, ram_data );
						break;
					}
				}
				break;
			}
			pos += size;
			break;
		}

		case cmd_ram_block: {
			check( *pos == cmd_end );
			int type = pos[ 1 ];
			int data_start = get_le24( pos + 2 );
			int data_addr = get_le24( pos + 5 );
			int data_size = get_le24( pos + 8 );
			if ( !data_size ) data_size += 0x01000000;
			switch ( type )
			{
			case rf5c68_ram_block:
				rf5c68.write_ram( data_addr, data_size, ( void * ) ( pcm_data[ 1 ] + data_start ) );
				break;

			case rf5c164_ram_block:
				rf5c164.write_ram( data_addr, data_size, ( void * ) ( pcm_data[ 2 ] + data_start ) );
				break;
			}
			pos += 11;
			break;
		}
		
		case cmd_pcm_seek:
			pcm_pos = pcm_data[0] + pos [3] * 0x1000000 + pos [2] * 0x10000 +
					pos [1] * 0x100 + pos [0];
			pos += 4;
			break;
		
		default:
			int cmd = pos [-1];
			switch ( cmd & 0xF0 )
			{
				case cmd_pcm_delay:
					write_pcm( vgm_time, *pcm_pos++ );
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
	psg.end_frame( t );
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
	
	if ( ym2612.enabled() || ym2413.enabled() || ym2151.enabled() || c140.enabled() || segapcm.enabled() || rf5c68.enabled() || rf5c164.enabled() || pwm.enabled() )
	{
		memset( out, 0, pairs * stereo * sizeof *out );
	}

	if ( ym2612.enabled() )
	{
		ym2612.begin_frame( out );
	}
	else if ( ym2413.enabled() )
	{
		ym2413.begin_frame( out );
	}
	else if ( ym2151.enabled() )
	{
		ym2151.begin_frame( out );
	}

	if ( c140.enabled() )
	{
		c140.begin_frame( out );
	}
	else if ( segapcm.enabled() )
	{
		segapcm.begin_frame( out );
	}
	else if ( rf5c68.enabled() )
	{
		rf5c68.begin_frame( out );
	}
	else if ( rf5c164.enabled() )
	{
		rf5c164.begin_frame( out );
	}
	else if ( pwm.enabled() )
	{
		pwm.begin_frame( out );
	}
	
	run( vgm_time );
	run_ym2612( pairs );
	run_ym2413( pairs );
	run_ym2151( pairs );
	run_c140( pairs );
	run_segapcm( pairs );
	run_rf5c68( pairs );
	run_rf5c164( pairs );
	run_pwm( pairs );
	
	fm_time_offset = (vgm_time * fm_time_factor + fm_time_offset) - (pairs << fm_time_bits);
	
	psg.end_frame( blip_time );
	
	return pairs * stereo;
}
