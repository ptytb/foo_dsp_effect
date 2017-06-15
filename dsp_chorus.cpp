#define _WIN32_WINNT 0x0501
#define _USE_MATH_DEFINES
#include "../SDK/foobar2000.h"
#include "../ATLHelpers/ATLHelpers.h"
#include "resource.h"
#include "dsp_guids.h"

#define CHORUS_MAX_DELAY 4096
#define CHORUS_DELAY_MASK (CHORUS_MAX_DELAY - 1)

float clamp(float val, float minval, float maxval)
{
	// Branchless SSE clamp.
	// return minss( maxss(val,minval), maxval );

	_mm_store_ss(&val, _mm_min_ss(_mm_max_ss(_mm_set_ss(val), _mm_set_ss(minval)), _mm_set_ss(maxval)));
	return val;
}

static inline float undenormalize(volatile float s) {
	if (fabs(s) < 1.0e-15)return 0.;
	return clamp(s,-1.0,1.0);
}



class Chorus
{
private:
	float old[CHORUS_MAX_DELAY];
	unsigned old_ptr;
	float delay_;
	float depth_;
	float delay;
	float depth;
	float rate;
	float mix_dry;
	float mix_wet;
	unsigned lfo_ptr;
	unsigned lfo_period;
	float lfo_freq;
	float drywet;

public:
	Chorus()
	{
		
	}
	~Chorus()
	{
	}

	void init(float delay,float depth,float lfo_freq,float drywet,int rate)
	{
		memset(old, 0, CHORUS_MAX_DELAY*sizeof(float));
		old_ptr = 0;
		delay_ = delay / 1000.0f;
		depth_ = depth /1000.0f;
		this->delay = delay;
		this->depth = depth;
		this->lfo_freq = lfo_freq;
		if (depth_ > delay_)
			depth_ = delay_;

		if (drywet < 0.0f)
			drywet = 0.0f;
		else if (drywet > 1.0f)
			drywet = 1.0f;

		mix_dry = 1.0f - 0.5f * drywet;
		mix_wet = 0.5f * drywet;

		lfo_period = (1.0f / lfo_freq) * rate;
		if (!lfo_period)
			lfo_period = 1;
		this->rate = rate;
		lfo_ptr = 0;
		
	}
	inline int GetDelay()
	{
		return (int)(delay);
	}
	inline int GetDepth()
	{
		return (int)(depth);
	}
	inline int GetSampleRate()
	{
		return rate;
	}
	float Process(float in)
	{
		float in_smp = in;
        float delay2 = this->delay_ + depth_ * sin((2.0 * M_PI * lfo_ptr++) / lfo_period);
		delay2 *= rate;
		if (lfo_ptr >= lfo_period)
			lfo_ptr = 0;
		unsigned delay_int = (unsigned)delay2;
		if (delay_int >= CHORUS_MAX_DELAY - 1)
			delay_int = CHORUS_MAX_DELAY - 2;
		float delay_frac = delay2 - delay_int;
		old[old_ptr] = in_smp;
		float l_a = old[(old_ptr - delay_int - 0) & CHORUS_DELAY_MASK];
		float l_b = old[(old_ptr - delay_int - 1) & CHORUS_DELAY_MASK];
		/* Lerp introduces aliasing of the chorus component,
		* but doing full polyphase here is probably overkill. */
		float chorus_l = l_a * (1.0f - delay_frac) + l_b * delay_frac;
		float smp = mix_dry * in_smp + mix_wet * chorus_l;
	    old_ptr = (old_ptr + 1) & CHORUS_DELAY_MASK;
		return smp;
	}
};

/**********************************
DSP without presets
**********************************/

// This DSP does not alter the signal.
// It merely mangles audio chunks to demonstrate some features
// of the DSP service class. It will buffer one chunk at a time,
// thereby introducing latency in the DSP chain (and some overhead
// for copying data around).
class dsp_chorus : public dsp_impl_base
{
	int m_rate, m_ch, m_ch_mask;
	pfc::array_t<Chorus> m_buffers;
public:
	dsp_chorus():m_rate(0), m_ch(0), m_ch_mask(0) {
		// Mark buffer as empty.
	}

	// Every DSP type is identified by a GUID.
	static GUID g_get_guid() {
		return guid_chorus;
	}

	// We also need a name, so the user can identify the DSP.
	// The name we use here does not describe what the DSP does,
	// so it would be a bad name. We can excuse this, because it
	// doesn't do anything useful anyway.
	static void g_get_name(pfc::string_base & p_out) {
		p_out = "Chorus";
	}

	virtual void on_endoftrack(abort_callback & p_abort) {
		// This method is called when a track ends.
		// We need to do the same thing as flush(), so we just call it.
	}

	virtual void on_endofplayback(abort_callback & p_abort) {
		// This method is called on end of playback instead of flush().
		// We need to do the same thing as flush(), so we just call it.
	}

	// The framework feeds input to our DSP using this method.
	// Each chunk contains a number of samples with the same
	// stream characteristics, i.e. same sample rate, channel count
	// and channel configuration.
	virtual bool on_chunk(audio_chunk * chunk, abort_callback & p_abort) {
		if (chunk->get_srate() != m_rate || chunk->get_channels() != m_ch || chunk->get_channel_config() != m_ch_mask)
		{
			m_rate = chunk->get_srate();
			m_ch = chunk->get_channels();
			m_ch_mask = chunk->get_channel_config();
			m_buffers.set_count(0);
			m_buffers.set_count(m_ch);
			for (unsigned i = 0; i < m_ch; i++)
			{
				//config->get_float(userdata, "delay_ms", &delay, 25.0f);
				//config->get_float(userdata, "depth_ms", &depth, 1.0f);
				//config->get_float(userdata, "lfo_freq", &lfo_freq, 0.5f);
				//config->get_float(userdata, "drywet", &drywet, 0.8f);
				Chorus & e = m_buffers[i];
				e.init(25.0, 9.0, 0.8, 1., m_rate);
			}
		}

		for (unsigned i = 0; i < m_ch; i++)
		{
			Chorus & e = m_buffers[i];
			audio_sample * data = chunk->get_data() + i;
			for (unsigned j = 0, k = chunk->get_sample_count(); j < k; j++)
			{
				*data = e.Process(*data);
				data += m_ch;
			}
		}
		return true;
	}

	virtual void flush() {
		m_buffers.set_count(0);
		m_rate = 0;
		m_ch = 0;
		m_ch_mask = 0;
	}

	virtual double get_latency() {
		// If the buffered chunk is valid, return its length.
		// Otherwise return 0.
		return 0.0;
	}

	virtual bool need_track_change_mark() {
		// Return true if you need to know exactly when a new track starts.
		// Beware that this may break gapless playback, as at least all the
		// DSPs before yours have to be flushed.
		// To picture this, consider the case of a reverb DSP which outputs
		// the sum of the input signal and a delayed copy of the input signal.
		// In the case of a single track:

		// Input signal:   01234567
		// Delayed signal:   01234567

		// For two consecutive tracks with the same stream characteristics:

		// Input signal:   01234567abcdefgh
		// Delayed signal:   01234567abcdefgh

		// If the DSP chain contains a DSP that requires a track change mark,
		// the chain will be flushed between the two tracks:

		// Input signal:   01234567  abcdefgh
		// Delayed signal:   01234567  abcdefgh
		return false;
	}
};

// We need a service factory to make the DSP known to the system.
// DSPs use special service factories that implement a static dsp_entry
// that provides information about the DSP. The static methods in our
// DSP class our used to provide the implementation of this entry class.
// The entry is used to instantiate an instance of our DSP when it is needed.
// We use the "nopreset" version of the DSP factory which blanks out
// preset support.
// Note that there can be multiple instances of a DSP which are used in
// different threads.
static dsp_factory_nopreset_t<dsp_chorus> foo_dsp_tutorial_nopreset;


