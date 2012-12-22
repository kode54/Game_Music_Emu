// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Qsound_Emu.h"
#include "qmix.h"

Qsound_Emu::Qsound_Emu() { chip = 0; rom = 0; rom_size = 0; }

Qsound_Emu::~Qsound_Emu()
{
    if ( chip ) free( chip );
    if ( rom ) free( rom );
}

int Qsound_Emu::set_rate( int clock_rate )
{
    if ( chip )
	{
        free( chip );
		chip = 0;
	}
	
    chip = malloc( qmix_get_state_size() );
	if ( !chip )
		return 0;
	
	reset();

    return 1;
}

void Qsound_Emu::reset()
{
    qmix_clear_state( chip );
}

void Qsound_Emu::write( int addr, int data )
{
    qmix_command( chip, addr, data );
}

void Qsound_Emu::write_rom( int size, int start, int length, void * data )
{
    if ( size > rom_size )
    {
        rom_size = size;
        rom = realloc( rom, size );
    }
    if ( start > size ) start = size;
    if ( start + length > size ) length = size - start;
    memcpy( (uint8*)rom + start, data, length );
    qmix_set_sample_rom( chip, rom, rom_size );
}

void Qsound_Emu::run( int pair_count, sample_t* out )
{
    sint16 buf[ 1024 * 2 ];

	while (pair_count > 0)
	{
		int todo = pair_count;
		if (todo > 1024) todo = 1024;
        qmix_render( chip, buf, todo );

        for (int i = 0; i < todo * 2; i++)
		{
            int output = buf [i];
            output += out [0];
            if ( (short)output != output ) output = 0x7FFF ^ ( output >> 31 );
            out [0] = output;
            out++;
		}

		pair_count -= todo;
	}
}
