// Multi-channel effects buffer with echo and individual panning for each channel

// Game_Music_Emu 0.5.2
#ifndef EFFECTS_BUFFER_H
#define EFFECTS_BUFFER_H

#include "Multi_Buffer.h"

class Effects_Buffer : public Multi_Buffer {
public:
	Effects_Buffer();
	
	struct config_t
	{
		bool enabled; // false = disable all effects (acts like Stereo_Buffer)
		
		// Simpler configuration
		struct {
			float echo;     // 0.0 = none, 1.0 = lots
			float stereo;   // 0.0 = channels in center, 1.0 = channels on left/right
			bool surround;  // true = put some channels in back (phase inverted)
			bool enabled;   // false = ignore simple configuration
		} simple;
		
		// More complex configuration
		// Current sound is echoed at adjustable left/right delay,
		// with reduced treble and volume (feedback). 
		float treble;   // 1.0 = full treble, 0.1 = very little, 0.0 = silent
		int delay [2];  // left, right delays (msec)
		float feedback; // 0.0 = no echo, 0.5 = each echo half previous, 1.0 = cacophony
		float side_vol [2] [2]; // left and right volumes for left and right side channels
	};
	config_t& config() { return config_; }
	
	struct chan_config_t
	{
		float vol [2]; // left, right volumes
		int type;
		bool echo;     // false = channel doesn't have any echo
	};
	chan_config_t& chan_config( int i ) { return chans [i + extra_chans].cfg; }
	
	// Apply any changes made to config() and chan_config()
	void apply_config();
	
public:
	~Effects_Buffer();
	blargg_err_t set_sample_rate( long samples_per_sec, int msec = blip_default_length );
	blargg_err_t set_channel_count( int );
	void set_channel_types( int const* );
	void clock_rate( long );
	void bass_freq( int );
	void clear();
	channel_t channel( int, int );
	void end_frame( blip_time_t );
	long read_samples( blip_sample_t*, long );
	long samples_avail() const;
	enum { stereo = 2 };
	typedef blargg_long fixed_t;
private:
	config_t config_;
	long clock_rate_;
	int bass_freq_;
	
	long samples_avail_;
	long samples_read;
	
	struct chan_t
	{
		fixed_t vol [stereo];
		Blip_Buffer bb;
		chan_config_t cfg;
		channel_t channel;
		int modified;
	};
	chan_t* chans;
	int chans_size;
	enum { extra_chans = stereo * stereo };
	
	struct {
		long delay [stereo];
		fixed_t treble;
		fixed_t feedback;
		fixed_t low_pass [stereo];
	} s;
	
	blargg_vector<fixed_t> echo;
	blargg_long echo_pos;
	
	bool no_effects;
	bool no_echo;
	
	void apply_simple_config();
	void optimize_config();
	void clear_echo();
	void mix_effects( blip_sample_t* out, int pair_count );
	void mix_stereo ( blip_sample_t* out, int pair_count );
	void mix_mono   ( blip_sample_t* out, int pair_count );
};

#endif
