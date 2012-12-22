// Capcom QSound sound chip emulator interface

// Game_Music_Emu $vers
#ifndef QSOUND_EMU_H
#define QSOUND_EMU_H

class Qsound_Emu  {
	void* chip;
    void* rom;
    int rom_size;
public:
    Qsound_Emu();
    ~Qsound_Emu();
	
	// Sets output sample rate and chip clock rates, in Hz. Returns non-zero
	// if error.
	int set_rate( int clock_rate );
	
	// Resets to power-up state
	void reset();
	
	// Writes data to addr
	void write( int addr, int data );

	// Scales ROM size, then writes length bytes from data at start offset
	void write_rom( int size, int start, int length, void * data );
	
	// Runs and writes pair_count*2 samples to output
	typedef short sample_t;
	enum { out_chan_count = 2 }; // stereo
	void run( int pair_count, sample_t* out );
};

#endif
