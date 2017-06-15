#define _WIN32_WINNT 0x0501
#include "../SDK/foobar2000.h"
#include "../ATLHelpers/ATLHelpers.h"
#include "resource.h"
#include "SoundTouch/SoundTouch.h"
#include "rubberband/rubberband/RubberBandStretcher.h"
#include "circular_buffer.h"
#include "dsp_guids.h"
using namespace soundtouch;
using namespace RubberBand;

static void RunDSPConfigPopup( const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback );
static void RunDSPConfigPopupRate( const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback );
static void RunDSPConfigPopupTempo( const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback );
#define BUFFER_SIZE 2048
#define BUFFER_SIZE_RB 4096

class dsp_pitch : public dsp_impl_base
{
	SoundTouch * p_soundtouch;
	RubberBandStretcher * rubber;
	float **plugbuf;
	float **m_scratch;

	int m_rate, m_ch, m_ch_mask;
	float pitch_amount;
	circular_buffer<float>sample_buffer;
	pfc::array_t<float>samplebuf;
	unsigned buffered;
	bool st_enabled;
	int pitch_shifter;
private:
	void insert_chunks_rubber()
	{
		while (1)
		{
			t_size samples = rubber->available();
			if (samples <= 0)return;
			samples = rubber->retrieve(m_scratch, samples);
			if (samples > 0)
			{
				float *data = samplebuf.get_ptr();
				for (int c = 0; c < m_ch; ++c) {
					int j = 0;
					while (j < samples) {
						data[j * m_ch + c] = m_scratch[c][j];
						++j;
					}
				}
				audio_chunk * chunk = insert_chunk(samples*m_ch);
				chunk->set_data(data, samples, m_ch, m_rate);
			}
		}
	}


	void insert_chunks_st()
	{
		uint samples;
		soundtouch::SAMPLETYPE * src = samplebuf.get_ptr();
		do
		{
			samples = p_soundtouch->receiveSamples(src, BUFFER_SIZE);
			if (samples > 0)
			{
				audio_chunk * chunk = insert_chunk(samples * m_ch);
				chunk->set_channels(m_ch, m_ch_mask);
				chunk->set_data_32(src, samples, m_ch, m_rate);
			}
		} while (samples != 0);
	}

public:
	dsp_pitch( dsp_preset const & in ) : pitch_amount(0.00), m_rate( 0 ), m_ch( 0 ), m_ch_mask( 0 )
	{
		buffered=0;
		p_soundtouch = 0;
		rubber = 0;
		plugbuf = 0;
		m_scratch = 0;
		st_enabled = false;
		parse_preset( pitch_amount,pitch_shifter,st_enabled, in );
		
	}
	~dsp_pitch(){
		if (p_soundtouch)
		{
			insert_chunks_st();
			delete p_soundtouch;
			p_soundtouch = 0;
		}
		

		if (rubber)
		{
			insert_chunks_rubber();
			delete rubber;
			for (int c = 0; c < m_ch; ++c)
			{
				delete plugbuf[c]; plugbuf[c] = NULL;
				delete m_scratch[c]; m_scratch[c] = NULL;
			}
			delete plugbuf;
			delete m_scratch;
			m_scratch = NULL;
			plugbuf = NULL;
			rubber = 0;
		}
		

	}

	// Every DSP type is identified by a GUID.
	static GUID g_get_guid() {
		// Create these with guidgen.exe.
		// {A7FBA855-56D4-46AC-8116-8B2A8DF2FB34}
		
		return guid_pitch;
	}

	static void g_get_name(pfc::string_base & p_out) {
		p_out = "Pitch Shift";
	}

	virtual void on_endoftrack(abort_callback & p_abort) {
		if (rubber&& pitch_shifter == 1)
		{
			insert_chunks_rubber();
		}
	}

	virtual void on_endofplayback(abort_callback & p_abort) {
        //same as flush, only at end of playback
		if (p_soundtouch && st_enabled)
		{
				insert_chunks_st();
				if (buffered)
				{
					sample_buffer.read(samplebuf.get_ptr(),buffered*m_ch);
					p_soundtouch->putSamples(samplebuf.get_ptr(),buffered);
					buffered = 0;
				}
				p_soundtouch->flush();
				insert_chunks_st();	
		}

		if (rubber&& st_enabled)
		{
			insert_chunks_rubber();
		}
	}

	// The framework feeds input to our DSP using this method.
	// Each chunk contains a number of samples with the same
	// stream characteristics, i.e. same sample rate, channel count
	// and channel configuration.
	virtual bool on_chunk(audio_chunk * chunk, abort_callback & p_abort) {
		t_size sample_count = chunk->get_sample_count();
		audio_sample * src = chunk->get_data();

		if (pitch_amount == 0.0)st_enabled = false;
		if (!st_enabled) return true;

		if ( chunk->get_srate() != m_rate || chunk->get_channels() != m_ch || chunk->get_channel_config() != m_ch_mask )
		{
			m_rate = chunk->get_srate();
			m_ch = chunk->get_channels();
			m_ch_mask = chunk->get_channel_config();
			


			if (pitch_shifter == 1)
			{
				
				RubberBandStretcher::Options options = RubberBandStretcher::DefaultOptions | RubberBandStretcher::OptionProcessRealTime | RubberBandStretcher::OptionPitchHighQuality;
				rubber = new RubberBandStretcher(m_rate, m_ch, options, 1.0, pow(2.0, pitch_amount / 12.0));
				if (!rubber) return 0;
				plugbuf = new float*[m_ch];
				m_scratch = new float*[m_ch];
				sample_buffer.set_size(BUFFER_SIZE*m_ch);
				samplebuf.set_size(BUFFER_SIZE*m_ch + 1024 + 8192);
				rubber->setMaxProcessSize(BUFFER_SIZE);
				for (int c = 0; c < m_ch; ++c) plugbuf[c] = new float[BUFFER_SIZE + 1024 + 8192];
				for (int c = 0; c < m_ch; ++c) m_scratch[c] = new float[BUFFER_SIZE + 1024 + 8192];
				st_enabled = true;
			}

			if (pitch_shifter == 0)
			{

				sample_buffer.set_size(BUFFER_SIZE*m_ch);
				samplebuf.set_size(BUFFER_SIZE*m_ch);
				p_soundtouch = new SoundTouch;
				if (!p_soundtouch) return 0;
				if (p_soundtouch)
				{
					p_soundtouch->setSampleRate(m_rate);
					p_soundtouch->setChannels(m_ch);
					p_soundtouch->setPitchSemiTones(pitch_amount);
					bool usequickseek = true;
					bool useaafilter = true; //seems clearer without it
					p_soundtouch->setSetting(SETTING_USE_QUICKSEEK, true);
					p_soundtouch->setSetting(SETTING_USE_AA_FILTER, useaafilter);
				}
			}
		}
		

	

		if (rubber&& pitch_shifter == 1) {
			while (sample_count > 0)
			{
				int toCauseProcessing = BUFFER_SIZE;
				int todo = min(toCauseProcessing - buffered, sample_count);
				sample_buffer.write(src, todo*m_ch);
				src += todo * m_ch;
				buffered += todo;
				sample_count -= todo;
				if (buffered == toCauseProcessing)
				{
					float*data = samplebuf.get_ptr();
					sample_buffer.read((float*)data, toCauseProcessing*m_ch);
					for (int c = 0; c < m_ch; ++c) {
						int j = 0;
						while (j < toCauseProcessing) {
							plugbuf[c][j] = data[j * m_ch + c];
							++j;
						}
					}
					rubber->process(plugbuf, toCauseProcessing, false);
					insert_chunks_rubber();
					buffered = 0;
				}
			}
		}

		if (p_soundtouch&& pitch_shifter == 0)
		{
			while (sample_count > 0)
			{
				int todo = min(BUFFER_SIZE - buffered, sample_count);
				sample_buffer.write(src, todo*m_ch);
				src += todo * m_ch;
				buffered += todo;
				sample_count -= todo;
				if (buffered == BUFFER_SIZE)
				{
					sample_buffer.read(samplebuf.get_ptr(), buffered*m_ch);
					p_soundtouch->putSamples(samplebuf.get_ptr(), buffered);
					insert_chunks_st();
					buffered = 0;
				}
			}
		}
		
		return false;
	}

	virtual void flush() {
		if (!st_enabled)return;
		if (p_soundtouch && pitch_shifter == 0){
			p_soundtouch->clear();
		}
		if (rubber && pitch_shifter == 1)
		{
			insert_chunks_rubber();
		}
		m_rate = 0;
		m_ch = 0;
		buffered = 0;
		m_ch_mask = 0;
	}

	virtual double get_latency() {
		if (!st_enabled) return 0;
		if (p_soundtouch && pitch_shifter == 0)
		{
			return (double)(p_soundtouch->numSamples() + buffered) / (double)m_rate;
		}
		if (rubber && pitch_shifter == 1)
		{
			return (double)(rubber->getLatency()) / (double)m_rate;
		}
		return 0;
	}


	virtual bool need_track_change_mark() {
		return false;
	}

	static bool g_get_default_preset( dsp_preset & p_out )
	{
		make_preset( 0.0,0,false, p_out );
		return true;
	}
	static void g_show_config_popup( const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback )
	{
		::RunDSPConfigPopup( p_data, p_parent, p_callback );
	}
	static bool g_have_config_popup() { return true; }
	static void make_preset( float pitch,int pitch_type,bool enabled, dsp_preset & out )
	{
		dsp_preset_builder builder; 
		builder << pitch; 
		builder << pitch_type;
		builder << enabled;
		builder.finish( g_get_guid(), out );
	}                        
	static void parse_preset(float & pitch,int & pitch_type,bool &enabled, const dsp_preset & in)
	{
		try
		{
			dsp_preset_parser parser(in);
			parser >> pitch; 
			parser >> pitch_type;
			parser >> enabled;
		}
		catch (exception_io_data) { pitch = 0.0; pitch_type = 0; enabled = false; }
	}
};

class dsp_tempo : public dsp_impl_base
{
	SoundTouch * p_soundtouch;
	RubberBandStretcher * rubber;
	float **plugbuf;
	float **m_scratch;
	int m_rate, m_ch, m_ch_mask;
	float pitch_amount;
	circular_buffer<float>sample_buffer;
	pfc::array_t<float>samplebuf;
	unsigned buffered;
	bool st_enabled;
	int pitch_shifter;
private:
	void insert_chunks_rubber()
	{	
			while(1)
			{
				t_size samples = rubber->available();
				if (samples <= 0)return;
				samples = rubber->retrieve(m_scratch, samples);
				if (samples > 0)
				{
					float *data = samplebuf.get_ptr();
					for (int c = 0; c < m_ch; ++c) {
						int j = 0;
						while (j < samples) {
							data[j * m_ch + c] = m_scratch[c][j];
							++j;
						}
					}
					audio_chunk * chunk = insert_chunk(samples*m_ch);
					chunk->set_data(data, samples, m_ch, m_rate);
				}
			}
	}


	void insert_chunks_st()
	{
		uint samples;
		soundtouch::SAMPLETYPE * src = samplebuf.get_ptr();
		do
		{
			samples = p_soundtouch->receiveSamples(src, BUFFER_SIZE);
			if (samples > 0)
			{
				audio_chunk * chunk = insert_chunk(samples * m_ch);
				chunk->set_channels(m_ch, m_ch_mask);
				chunk->set_data_32(src, samples, m_ch, m_rate);
			}
		} while (samples != 0);
	}


public:
	dsp_tempo( dsp_preset const & in ) : pitch_amount(0.00), m_rate( 0 ), m_ch( 0 ), m_ch_mask( 0 )
	{
		buffered = 0;
		p_soundtouch = 0;
		rubber = 0;
		plugbuf = 0;
		m_scratch = 0;
		st_enabled =false;
		parse_preset(pitch_amount, pitch_shifter,st_enabled, in);
	}
	~dsp_tempo(){
		if (p_soundtouch)
		{
			insert_chunks_st();
			delete p_soundtouch;
			p_soundtouch = 0;
		}


		if (rubber)
		{
			insert_chunks_rubber();
			delete rubber;
			for (int c = 0; c < m_ch; ++c)
			{
				delete plugbuf[c]; plugbuf[c] = NULL;
				delete m_scratch[c]; m_scratch[c] = NULL;
			}
			delete plugbuf;
			delete m_scratch;
			m_scratch = NULL;
			plugbuf = NULL;
			rubber = 0;
		}
	}

	// Every DSP type is identified by a GUID.
	static GUID g_get_guid() {
		// {44BCACA2-9EDD-493A-BB8F-9474F4B5A76B}
		
		return guid_tempo;
	}

	// We also need a name, so the user can identify the DSP.
	// The name we use here does not describe what the DSP does,
	// so it would be a bad name. We can excuse this, because it
	// doesn't do anything useful anyway.
	static void g_get_name(pfc::string_base & p_out) {
		p_out = "Tempo Shift";
	}

	virtual void on_endoftrack(abort_callback & p_abort) {
		if (rubber)
		{
			insert_chunks_rubber();
		}
	}

	virtual void on_endofplayback(abort_callback & p_abort) {
		//same as flush, only at end of playback
		if (p_soundtouch && st_enabled)
		{
			insert_chunks_st();
			if (buffered)
			{
				sample_buffer.read(samplebuf.get_ptr(), buffered*m_ch);
				p_soundtouch->putSamples(samplebuf.get_ptr(), buffered);
				buffered = 0;
			}
			p_soundtouch->flush();
			insert_chunks_st();
		}

		if (rubber&& st_enabled)
		{
			insert_chunks_rubber();
		}
	}

	// The framework feeds input to our DSP using this method.
	// Each chunk contains a number of samples with the same
	// stream characteristics, i.e. same sample rate, channel count
	// and channel configuration.




	virtual bool on_chunk(audio_chunk * chunk, abort_callback & p_abort) {
		t_size sample_count = chunk->get_sample_count();
		audio_sample * src = chunk->get_data();

		if (pitch_amount == 0)st_enabled = false;
		if (!st_enabled) return true;

		if (chunk->get_srate() != m_rate || chunk->get_channels() != m_ch || chunk->get_channel_config() != m_ch_mask)
		{
			m_rate = chunk->get_srate();
			m_ch = chunk->get_channels();
			m_ch_mask = chunk->get_channel_config();
		

			if (pitch_shifter == 1)
			{
				RubberBandStretcher::Options options = RubberBandStretcher::DefaultOptions | RubberBandStretcher::OptionProcessRealTime | RubberBandStretcher::OptionPitchHighQuality;
				float ratios = pitch_amount >= 1.0 ? 1.0 - (0.01 * pitch_amount) : 1.0 + 0.01 *-pitch_amount;
				rubber = new RubberBandStretcher(m_rate, m_ch, options, ratios, 1.0);
				m_scratch = new float *[m_ch];
				plugbuf = new float *[m_ch];
				if (!rubber) return 0;
				sample_buffer.set_size(BUFFER_SIZE*m_ch);
				samplebuf.set_size(BUFFER_SIZE*m_ch + 1024 + 8192);
				rubber->setMaxProcessSize(BUFFER_SIZE);
				for (int c = 0; c < m_ch; ++c) plugbuf[c] = new float[BUFFER_SIZE+ 1024 + 8192];
				for (int c = 0; c < m_ch; ++c) m_scratch[c] = new float[BUFFER_SIZE + 1024 + 8192];
				st_enabled = true;
			}

			if (pitch_shifter == 0)
			{
				sample_buffer.set_size(BUFFER_SIZE*m_ch);
				samplebuf.set_size(BUFFER_SIZE*m_ch);
				p_soundtouch = new SoundTouch;
				if (!p_soundtouch) return 0;
				if (p_soundtouch)
				{
					p_soundtouch->setSampleRate(m_rate);
					p_soundtouch->setChannels(m_ch);
					p_soundtouch->setTempoChange(pitch_amount);
					st_enabled = true;
					
					bool usequickseek = true;
					bool useaafilter = true; //seems clearer without it
					p_soundtouch->setSetting(SETTING_USE_QUICKSEEK, true);
					p_soundtouch->setSetting(SETTING_USE_AA_FILTER, useaafilter);
				}
			}
		}


		

		if (rubber&& pitch_shifter == 1) {
			while (sample_count > 0)
			{
				int toCauseProcessing = BUFFER_SIZE;
				int todo = min(toCauseProcessing - buffered, sample_count);
				sample_buffer.write(src, todo*m_ch);
				src += todo * m_ch;
				buffered += todo;
				sample_count -= todo;
				if (buffered == toCauseProcessing)
				{
					float*data = samplebuf.get_ptr();
					sample_buffer.read((float*)data, toCauseProcessing*m_ch);
					for (int c = 0; c < m_ch; ++c) {
						int j = 0;
						while (j < toCauseProcessing) {
							plugbuf[c][j] = data[j * m_ch + c];
							++j;
						}
					}
					rubber->process(plugbuf, toCauseProcessing, false);
					insert_chunks_rubber();
					buffered = 0;
				}
			}
		}

		if (p_soundtouch&& pitch_shifter == 0)
		{
			while (sample_count > 0)
			{
				int todo = min(BUFFER_SIZE - buffered, sample_count);
				sample_buffer.write(src, todo*m_ch);
				src += todo * m_ch;
				buffered += todo;
				sample_count -= todo;
				if (buffered == BUFFER_SIZE)
				{
					sample_buffer.read(samplebuf.get_ptr(), buffered*m_ch);
					p_soundtouch->putSamples(samplebuf.get_ptr(), buffered);
					insert_chunks_st();
					buffered = 0;
				}
			}
		}

		return false;
	}

	virtual void flush() {
		if (!st_enabled) return;
		if (p_soundtouch&& pitch_shifter == 0) {
			p_soundtouch->clear();
		}
		if (rubber&& pitch_shifter == 1)
		{
			insert_chunks_rubber();
		}
		m_rate = 0;
		m_ch = 0;
		buffered = 0;
		m_ch_mask = 0;
	}

	virtual double get_latency() {
		if (!st_enabled) return 0;
		if (p_soundtouch&& pitch_shifter == 0)
		{
			return (m_rate && st_enabled) ? (double)(p_soundtouch->numSamples() + buffered) / (double)m_rate : 0;
		}
		if (rubber&& pitch_shifter == 1)
		{
			return (m_rate && st_enabled) ? (double)(rubber->getLatency()) / (double)m_rate : 0;
		}
		return 0;
	}


	virtual bool need_track_change_mark() {
		return false;
	}

	static bool g_get_default_preset(dsp_preset & p_out)
	{
		make_preset(0.0, 0,false, p_out);
		return true;
	}
	static void g_show_config_popup( const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback )
	{
		::RunDSPConfigPopupTempo( p_data, p_parent, p_callback );
	}
	static bool g_have_config_popup() { return true; }
	static void make_preset(float pitch, int pitch_type,bool enabled, dsp_preset & out)
	{
		dsp_preset_builder builder;
		builder << pitch;
		builder << pitch_type;
		builder << enabled;
		builder.finish(g_get_guid(), out);
	}
	static void parse_preset(float & pitch, int & pitch_type,bool & enabled, const dsp_preset & in)
	{
		try
		{
			dsp_preset_parser parser(in);
			parser >> pitch;
			parser >> pitch_type;
			parser >> enabled;
		}
		catch (exception_io_data) { pitch = 0.0; pitch_type = 0; enabled = false; }
	}
};

class dsp_rate : public dsp_impl_base
{
	SoundTouch * p_soundtouch;
	int m_rate, m_ch, m_ch_mask;
	float pitch_amount;
	circular_buffer<soundtouch::SAMPLETYPE>sample_buffer;
	pfc::array_t<soundtouch::SAMPLETYPE>samplebuf;
	unsigned buffered;
	bool st_enabled;
private:
	void insert_chunks()
	{
		uint samples = p_soundtouch->numSamples();
		if (!samples) return;
		samplebuf.grow_size(BUFFER_SIZE * m_ch);
		soundtouch::SAMPLETYPE * src = samplebuf.get_ptr();
		do
		{
			samples = p_soundtouch->receiveSamples(src, BUFFER_SIZE);
			if (samples > 0)
			{
				audio_chunk * chunk = insert_chunk(samples * m_ch);
				//	chunk->set_channels(m_ch,m_ch_mask);
				chunk->set_data_32(src, samples, m_ch, m_rate);
			}
		}
		while (samples != 0);
	}

public:
	dsp_rate( dsp_preset const & in ) : pitch_amount(0.00), m_rate( 0 ), m_ch( 0 ), m_ch_mask( 0 )
	{
		p_soundtouch=0;
		buffered=0;
		st_enabled = false;
		parse_preset( pitch_amount,st_enabled, in );
		
	}
	~dsp_rate(){
		if (p_soundtouch) delete p_soundtouch;
		p_soundtouch = 0;
	}

	// Every DSP type is identified by a GUID.
	static GUID g_get_guid() {
		// {8C12D81E-BB88-4056-B4C0-EAFA4E9F3B95}
		
		return guid_pbrate;
	}

	// We also need a name, so the user can identify the DSP.
	// The name we use here does not describe what the DSP does,
	// so it would be a bad name. We can excuse this, because it
	// doesn't do anything useful anyway.
	static void g_get_name(pfc::string_base & p_out) {
		p_out = "Playback Rate Shift";
	}

	virtual void on_endoftrack(abort_callback & p_abort) {
		// This method is called when a track ends.
		// We need to do the same thing as flush(), so we just call it.

	}

	virtual void on_endofplayback(abort_callback & p_abort) {
		// This method is called on end of playback instead of flush().
		// We need to do the same thing as flush(), so we just call it.
		if (p_soundtouch && st_enabled)
		{
			insert_chunks();
			if (buffered)
			{
				sample_buffer.read(samplebuf.get_ptr(),buffered*m_ch);
				p_soundtouch->putSamples(samplebuf.get_ptr(),buffered);
				buffered = 0;
			}
			p_soundtouch->flush();
			insert_chunks();
		}
	}

	// The framework feeds input to our DSP using this method.
	// Each chunk contains a number of samples with the same
	// stream characteristics, i.e. same sample rate, channel count
	// and channel configuration.
	virtual bool on_chunk(audio_chunk * chunk, abort_callback & p_abort) {
		t_size sample_count = chunk->get_sample_count();
		audio_sample * src = chunk->get_data();

		if (pitch_amount == 0)st_enabled = false;
		if (!st_enabled) return true;

		if ( chunk->get_srate() != m_rate || chunk->get_channels() != m_ch || chunk->get_channel_config() != m_ch_mask )
		{
			if (p_soundtouch)
			{
				insert_chunks();
				delete p_soundtouch;
				p_soundtouch = 0;
			}

			m_rate = chunk->get_srate();
			m_ch = chunk->get_channels();
			m_ch_mask = chunk->get_channel_config();
			p_soundtouch = new SoundTouch;
			sample_buffer.set_size(BUFFER_SIZE*m_ch);
			if (!p_soundtouch) return 0;
			p_soundtouch->setSampleRate(m_rate);
			p_soundtouch->setChannels(m_ch);
			p_soundtouch->setRateChange(pitch_amount);
			st_enabled = true;
			
		}
		samplebuf.grow_size(BUFFER_SIZE * m_ch);
		if (!st_enabled) return true;
		while (sample_count)
		{    
			int todo = min(BUFFER_SIZE - buffered, sample_count);
			sample_buffer.write(src,todo*m_ch);
			src += todo * m_ch;
			buffered += todo;
			sample_count -= todo;
			if (buffered == BUFFER_SIZE)
			{
				sample_buffer.read(samplebuf.get_ptr(),buffered*m_ch);
				p_soundtouch->putSamples(samplebuf.get_ptr(), buffered);
				buffered = 0;
				insert_chunks();
			}
		}
		return false;
	}

	virtual void flush() {
		if (!st_enabled)return;
		if (p_soundtouch){
			p_soundtouch->clear();
		}
		m_rate = 0;
		m_ch = 0;
		m_ch_mask = 0;
	}

	virtual double get_latency() {
		return (p_soundtouch && m_rate && st_enabled) ? ((double)(p_soundtouch->numSamples() + buffered) / (double)m_rate) : 0;
	}

	virtual bool need_track_change_mark() {
		return false;
	}

	static bool g_get_default_preset( dsp_preset & p_out )
	{
		make_preset( 0.0,false, p_out );
		return true;
	}
	static void g_show_config_popup( const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback )
	{
		::RunDSPConfigPopupRate( p_data, p_parent, p_callback );
	}
	static bool g_have_config_popup() { return true; }
	static void make_preset( float pitch,bool enabled, dsp_preset & out )
	{
		dsp_preset_builder builder; 
		builder << pitch; 
		builder << enabled;
		builder.finish( g_get_guid(), out );
	}                        
	static void parse_preset(float & pitch,bool & enabled, const dsp_preset & in)
	{
		try
		{
			dsp_preset_parser parser(in);
			parser >> pitch; 
			parser >> enabled;
		}
		catch (exception_io_data) { pitch = 0.0; enabled = false; }
	}
};


class CMyDSPPopupPitch : public CDialogImpl<CMyDSPPopupPitch>
{
public:
	CMyDSPPopupPitch( const dsp_preset & initData, dsp_preset_edit_callback & callback ) : m_initData( initData ), m_callback( callback ) { }
	enum { IDD = IDD_PITCH };
	enum
	{
		pitchmin = 0,
		pitchmax = 2400
		
	};
	BEGIN_MSG_MAP( CMyDSPPopup )
		MSG_WM_INITDIALOG( OnInitDialog )
		COMMAND_HANDLER_EX( IDOK, BN_CLICKED, OnButton )
		COMMAND_HANDLER_EX( IDCANCEL, BN_CLICKED, OnButton )
		COMMAND_HANDLER_EX(IDC_PITCHTYPE, CBN_SELCHANGE, OnChange)
		MSG_WM_HSCROLL( OnScroll )
	END_MSG_MAP()
private:
	void DSPConfigChange(dsp_chain_config const & cfg)
	{
		if (m_hWnd != NULL) {
			ApplySettings();
		}
	}

	void ApplySettings()
	{
		dsp_preset_impl preset2;
		if (static_api_ptr_t<dsp_config_manager>()->core_query_dsp(guid_pitch, preset2)) {
			float  pitch;
			bool enabled;
			int pitch_type;
			dsp_pitch::parse_preset(pitch, pitch_type,enabled, preset2);
			CWindow w = GetDlgItem(IDC_PITCHTYPE);
			::SendMessage(w, CB_SETCURSEL, pitch_type, 0);
			float pitch_val = pitch *= 100.00;
			slider_drytime.SetPos((double)(pitch_val + 1200));
			RefreshLabel(pitch_val / 100.00);
		}
	}

	BOOL OnInitDialog(CWindow, LPARAM)
	{
		slider_drytime = GetDlgItem(IDC_PITCH);
		slider_drytime.SetRange(0, pitchmax);

		CWindow w = GetDlgItem(IDC_PITCHTYPE);
		uSendMessageText(w, CB_ADDSTRING, 0, "SoundTouch");
		uSendMessageText(w, CB_ADDSTRING, 0, "Rubber Band");
		int pitch_type;
		{
			float  pitch;
			bool enabled = true;
			dsp_pitch::parse_preset(pitch, pitch_type,enabled, m_initData);
			::SendMessage(w, CB_SETCURSEL, pitch_type, 0);
			pitch *= 100.00;
			slider_drytime.SetPos( (double)(pitch+1200));
			RefreshLabel( pitch/100.00);
		}
		return TRUE;
	}

	void OnButton( UINT, int id, CWindow )
	{
		EndDialog( id );
	}

	void OnChange(UINT scrollID, int id, CWindow window)
	{
			float pitch;
			pitch = slider_drytime.GetPos() - 1200;
			pitch /= 100.00;

			bool enabled = false;
			int p_type; //filter type
			p_type = SendDlgItemMessage(IDC_PITCHTYPE, CB_GETCURSEL);
			{
				dsp_preset_impl preset;
				enabled = true;
				dsp_pitch::make_preset(pitch, p_type,enabled, preset);
				m_callback.on_preset_changed(preset);
			}
			RefreshLabel(pitch);
	}

	void OnScroll(UINT scrollID, int id, CWindow window)
	{
		float pitch;
		pitch = slider_drytime.GetPos() - 1200;
		pitch /= 100.00;

		bool enabled = false;
		int p_type; //filter type
		p_type = SendDlgItemMessage(IDC_PITCHTYPE, CB_GETCURSEL);
		if ((LOWORD(scrollID) != SB_THUMBTRACK) && window.m_hWnd == slider_drytime.m_hWnd)
		{
			dsp_preset_impl preset;
			enabled = true;
			dsp_pitch::make_preset(pitch, p_type, enabled, preset);
			m_callback.on_preset_changed(preset);
		}
		RefreshLabel(pitch);
	}
	

	void RefreshLabel(float  pitch )
	{
		pfc::string_formatter msg; 
		msg << "Pitch: ";
		msg << (pitch < 0 ? "" : "+");
		msg << pfc::format_float(pitch,0,2) << " semitones";
		::uSetDlgItemText( *this, IDC_PITCHINFO, msg );
		msg.reset();
	}
	const dsp_preset & m_initData; // modal dialog so we can reference this caller-owned object.
	dsp_preset_edit_callback & m_callback;
	CTrackBarCtrl slider_drytime,slider_wettime,slider_dampness,slider_roomwidth,slider_roomsize;
};

static void RunDSPConfigPopup(const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback)
{
	CMyDSPPopupPitch popup(p_data, p_callback);

	if (popup.DoModal(p_parent) != IDOK) p_callback.on_preset_changed(p_data);
}


static const GUID guid_cfg_placement = { 0xb348277, 0x38ee, 0x479d, { 0xbd, 0x7b, 0xb9, 0x37, 0x41, 0x11, 0x86, 0x97 } }; 
static cfg_window_placement cfg_placement(guid_cfg_placement);



class CMyDSPPopupPitchTempoRate : public CDialogImpl<CMyDSPPopupPitchTempoRate>
{
public:
	CMyDSPPopupPitchTempoRate() { pitch = 0.0; p_type = 0; pitch_enabled = false;
	tempo = 0.0; t_type = 0; tempo_enabled = false; rate = 0; rate_enabled = false;
	}
	enum { IDD = IDD_PITCHTEMPO };
	enum
	{
		pitchmin = 0,
		pitchmax = 2400,
		tempomax = 150

	};
	BEGIN_MSG_MAP(CMyDSPPopupPitchTempoRate)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_HANDLER_EX(IDOK, BN_CLICKED, OnButton)
		COMMAND_HANDLER_EX(IDCANCEL, BN_CLICKED, OnButton)
		COMMAND_HANDLER_EX(IDC_PITCHTYPE2, CBN_SELCHANGE, OnChange)
		COMMAND_HANDLER_EX(IDC_TEMPOTYPE2, CBN_SELCHANGE, OnChange)
		COMMAND_HANDLER_EX(IDC_PITCHENABLED, BN_CLICKED, OnEnabledToggle)
		COMMAND_HANDLER_EX(IDC_TEMPOENABLED, BN_CLICKED, OnEnabledToggle2)
		COMMAND_HANDLER_EX(IDC_RATENABLED,BN_CLICKED,OnEnabledToggle3)
		MSG_WM_HSCROLL(OnScroll)
		MSG_WM_DESTROY(OnDestroy)
	END_MSG_MAP()
private:
	void SetPitchEnabled(bool state) { m_buttonPitchEnabled.SetCheck(state ? BST_CHECKED : BST_UNCHECKED); }
	bool IsPitchEnabled() { return m_buttonPitchEnabled == NULL || m_buttonPitchEnabled.GetCheck() == BST_CHECKED; }
	void SetTempoEnabled(bool state) { m_buttonTempoEnabled.SetCheck(state ? BST_CHECKED : BST_UNCHECKED); }
	bool IsTempoEnabled() { return m_buttonTempoEnabled == NULL || m_buttonTempoEnabled.GetCheck() == BST_CHECKED; }
	void SetRateEnabled(bool state) { m_buttonRateEnabled.SetCheck(state ? BST_CHECKED : BST_UNCHECKED); }
	bool IsRateEnabled() { return m_buttonRateEnabled == NULL || m_buttonRateEnabled.GetCheck() == BST_CHECKED; }


	void DSPEnable(const dsp_preset & data) {
		//altered from enable_dsp to append to DSP array
		dsp_chain_config_impl cfg;
		static_api_ptr_t<dsp_config_manager>()->get_core_settings(cfg);
		bool found = false;
		bool changed = false;
		t_size n, m = cfg.get_count();
		for (n = 0; n < m; n++) {
			if (cfg.get_item(n).get_owner() == data.get_owner()) {
				found = true;
				if (cfg.get_item(n) != data) {
					cfg.replace_item(data, n);
					changed = true;
				}
				break;
			}
		}
		//append to DSP queue
		if (!found) { if (n>0)n++; cfg.insert_item(data, n); changed = true; }
		if (changed) static_api_ptr_t<dsp_config_manager>()->set_core_settings(cfg);
	}

	void PitchDisable() {
		static_api_ptr_t<dsp_config_manager>()->core_disable_dsp(guid_pitch);
	}


	void PitchEnable(float pitch, int pitch_type, bool enabled) {
		dsp_preset_impl preset;
		dsp_pitch::make_preset(pitch, pitch_type, enabled, preset);
		DSPEnable(preset);
	}

	void TempoDisable() {
		static_api_ptr_t<dsp_config_manager>()->core_disable_dsp(guid_tempo);
	}


	void TempoEnable(float pitch, int pitch_type, bool enabled) {
		dsp_preset_impl preset;
		dsp_tempo::make_preset(pitch, pitch_type, enabled, preset);
		DSPEnable(preset);
	}

	void RateDisable() {
		static_api_ptr_t<dsp_config_manager>()->core_disable_dsp(guid_pbrate);
	}


	void RateEnable(float pitch, bool enabled) {
		dsp_preset_impl preset;
		dsp_rate::make_preset(pitch, enabled, preset);
		DSPEnable(preset);
	}

	void OnEnabledToggle(UINT uNotifyCode, int nID, CWindow wndCtl) {
		pfc::vartoggle_t<bool> ownUpdate(m_ownPitchUpdate, true);
		if (IsPitchEnabled()) {
			GetConfig();
			dsp_preset_impl preset;
			dsp_pitch::make_preset(pitch, p_type, pitch_enabled, preset);
			//yes change api;
			DSPEnable(preset);
		}
		else {
			static_api_ptr_t<dsp_config_manager>()->core_disable_dsp(guid_pitch);
		}
		
	}

	void OnEnabledToggle2(UINT uNotifyCode, int nID, CWindow wndCtl) {
		pfc::vartoggle_t<bool> ownUpdate2(m_ownTempoUpdate, true);
		if (IsTempoEnabled()) {
			GetConfig();
			dsp_preset_impl preset;
			dsp_tempo::make_preset(tempo, t_type, tempo_enabled, preset);
			//yes change api;
			DSPEnable(preset);
		}
		else {
			static_api_ptr_t<dsp_config_manager>()->core_disable_dsp(guid_tempo);
		}

	}

	void OnEnabledToggle3(UINT uNotifyCode, int nID, CWindow wndCtl) {
		pfc::vartoggle_t<bool> ownUpdate2(m_ownRateUpdate, true);
		if (IsRateEnabled()) {
			GetConfig();
			dsp_preset_impl preset;
			dsp_rate::make_preset(rate, rate_enabled, preset);
			//yes change api;
			DSPEnable(preset);
		}
		else {
			static_api_ptr_t<dsp_config_manager>()->core_disable_dsp(guid_pbrate);
		}

	}

	void OnScroll(UINT scrollID, int pos, CWindow window)
	{
			pfc::vartoggle_t<bool> ownUpdate(m_ownPitchUpdate, true);
			pfc::vartoggle_t<bool> ownUpdate2(m_ownTempoUpdate, true);
			pfc::vartoggle_t<bool> ownUpdate3(m_ownRateUpdate, true);
			GetConfig();
			if (IsPitchEnabled())
			{
					
					if ((LOWORD(scrollID) != SB_THUMBTRACK) && window.m_hWnd == slider_pitch.m_hWnd)
					{
						PitchEnable(pitch, p_type, pitch_enabled);
					}
			}

			if (IsTempoEnabled())
			{
			
				if ((LOWORD(scrollID) != SB_THUMBTRACK) && window.m_hWnd == slider_tempo.m_hWnd)
				{
					TempoEnable(tempo, t_type, tempo_enabled);
				}
			}

			if (IsRateEnabled())
			{
				if ((LOWORD(scrollID) != SB_THUMBTRACK) && window.m_hWnd == slider_rate.m_hWnd)
				{
					RateEnable(rate,rate_enabled);
				}
			}
	}

	void OnChange(UINT, int id, CWindow)
	{
		pfc::vartoggle_t<bool> ownUpdate(m_ownPitchUpdate, true);
		pfc::vartoggle_t<bool> ownUpdate2(m_ownTempoUpdate, true);
		pfc::vartoggle_t<bool> ownUpdate3(m_ownRateUpdate, true);
		GetConfig();
		if (IsPitchEnabled() || IsRateEnabled() || IsTempoEnabled())
		{
			
			OnConfigChanged();
		}
	}

	void DSPConfigChange(dsp_chain_config const & cfg)
	{
		if(!m_ownPitchUpdate && !m_ownTempoUpdate && !m_ownRateUpdate && m_hWnd != NULL)  {
			ApplySettings();
		}
	}

	//set settings if from another control
	void ApplySettings()
	{
		dsp_preset_impl preset;
		if (static_api_ptr_t<dsp_config_manager>()->core_query_dsp(guid_pitch, preset)) {
			SetPitchEnabled(true);
			dsp_pitch::parse_preset(pitch, p_type,pitch_enabled, preset);
			SetPitchEnabled(pitch_enabled);
			SetConfig();
		}
		else {
			SetPitchEnabled(false);
			SetConfig();
		}

		if (static_api_ptr_t<dsp_config_manager>()->core_query_dsp(guid_tempo, preset)) {
			SetTempoEnabled(true);
			dsp_tempo::parse_preset(tempo, t_type, tempo_enabled, preset);
			SetTempoEnabled(tempo_enabled);
			SetConfig();
		}
		else {
			SetTempoEnabled(false);
			SetConfig();
		}


		if (static_api_ptr_t<dsp_config_manager>()->core_query_dsp(guid_pbrate, preset)) {
			SetTempoEnabled(true);
			dsp_rate::parse_preset(rate,rate_enabled, preset);
			SetRateEnabled(rate_enabled);
			SetConfig();
		}
		else {
			SetRateEnabled(false);
			SetConfig();
		}
		
	
		
	}

	void OnConfigChanged() {
		if (IsPitchEnabled()) {
			PitchEnable(pitch,p_type,pitch_enabled);
		}
		else {
		 PitchDisable();
		}

		if (IsTempoEnabled()) {
			TempoEnable(tempo, t_type, tempo_enabled);
		}
		else {
			TempoDisable();
		}

		if (IsRateEnabled()) {
			RateEnable(rate, rate_enabled);
		}
		else {
			RateDisable();
		}
	}


	void GetConfig()
	{
		float pitch_sl = slider_pitch.GetPos() - 1200;
		pitch = pitch_sl / 100.00;
		p_type = SendDlgItemMessage(IDC_PITCHTYPE2, CB_GETCURSEL);
		pitch_enabled = IsPitchEnabled();
		

		tempo= slider_tempo.GetPos() - 95;
		t_type = SendDlgItemMessage(IDC_TEMPOTYPE2, CB_GETCURSEL);
		tempo_enabled = IsTempoEnabled();

		rate = slider_rate.GetPos() - 50;
		rate_enabled = IsRateEnabled();

		RefreshLabel(pitch,tempo,rate);
	
		
	}

	void SetConfig()
	{
		CWindow w = GetDlgItem(IDC_PITCHTYPE2);
		
		::SendMessage(w, CB_SETCURSEL, p_type, 0);
		float pitch2 = pitch * 100.00;
		slider_pitch.SetPos((double)(pitch2 + 1200));
		
		
		w = GetDlgItem(IDC_TEMPOTYPE2);
		::SendMessage(w, CB_SETCURSEL, t_type, 0);
		slider_tempo.SetPos((double)(tempo + 95));

		slider_rate.SetPos((double)(rate + 50));

		RefreshLabel(pitch2/100,tempo,rate);
		
	}

	BOOL OnInitDialog(CWindow, LPARAM)
	{
		
		modeless_dialog_manager::g_add(m_hWnd);
		cfg_placement.on_window_creation(m_hWnd);
	//	RECT rect;
	//	GetWindowRect(&rect);
	//	SetWindowPos(HWND_TOPMOST,&rect,0);


		slider_pitch = GetDlgItem(IDC_PITCH2);
		m_buttonPitchEnabled = GetDlgItem(IDC_PITCHENABLED);
		slider_pitch.SetRange(0, pitchmax);
		CWindow w = GetDlgItem(IDC_PITCHTYPE2);
		uSendMessageText(w, CB_ADDSTRING, 0, "SoundTouch");
		uSendMessageText(w, CB_ADDSTRING, 0, "Rubber Band");
		m_ownPitchUpdate = false;

		slider_tempo = GetDlgItem(IDC_TEMPO2);
		slider_tempo.SetRange(0, 190);
		m_buttonTempoEnabled = GetDlgItem(IDC_TEMPOENABLED);
		w = GetDlgItem(IDC_TEMPOTYPE2);
		uSendMessageText(w, CB_ADDSTRING, 0, "SoundTouch");
		uSendMessageText(w, CB_ADDSTRING, 0, "Rubber Band");
		m_ownTempoUpdate = false;

		m_buttonRateEnabled = GetDlgItem(IDC_RATENABLED);
		slider_rate = GetDlgItem(IDC_RATE2);
		slider_rate.SetRange(0, tempomax);
		m_ownRateUpdate = false;

		ApplySettings();
		return TRUE;
	}

	
	void OnDestroy()
	{
		modeless_dialog_manager::g_remove(m_hWnd);
		cfg_placement.on_window_destruction(m_hWnd);
	}

	void OnButton(UINT, int id, CWindow)
	{
		DestroyWindow();
	}

	void RefreshLabel(float  pitch,float tempo,float rate)
	{
		pfc::string_formatter msg;
		msg << "Pitch: ";
		msg << (pitch < 0 ? "" : "+");
		msg << pfc::format_float(pitch, 0, 2) << " semitones";
		::uSetDlgItemText(*this, IDC_PITCHINFO2, msg);
		msg.reset();

		msg << "Tempo: ";
		msg << (tempo < 0 ? "" : "+");
		msg << pfc::format_int(tempo) << "%";
		::uSetDlgItemText(*this, IDC_TEMPOINFO2, msg);

		msg.reset();
		msg << "Playback Rate: ";
		msg << (rate < 0 ? "" : "+");
		msg << pfc::format_int(rate) << "%";
		::uSetDlgItemText(*this, IDC_RATEINFO2, msg);
	}
	float  pitch; 
	int p_type;
	int t_type;
	float tempo;
	float rate;
	bool pitch_enabled,tempo_enabled,rate_enabled;
	CTrackBarCtrl slider_pitch,slider_tempo,slider_rate;
	CButton m_buttonPitchEnabled,m_buttonTempoEnabled,m_buttonRateEnabled;
	bool m_ownPitchUpdate, m_ownTempoUpdate,m_ownRateUpdate;
};



static CWindow g_pitchdlg;
void PitchMainMenuWindow()
{
	if (!core_api::assert_main_thread()) return;

	if (!g_pitchdlg.IsWindow())
	{
		CMyDSPPopupPitchTempoRate * dlg = new CMyDSPPopupPitchTempoRate();
		g_pitchdlg = dlg->Create(core_api::get_main_window());
		
	}
	if (g_pitchdlg.IsWindow())
	{
		g_pitchdlg.ShowWindow(SW_SHOW);
		::SetForegroundWindow(g_pitchdlg);
	}
}

class CMyDSPPopupRate : public CDialogImpl<CMyDSPPopupRate>
{
public:
	CMyDSPPopupRate( const dsp_preset & initData, dsp_preset_edit_callback & callback ) : m_initData( initData ), m_callback( callback ) { }
	enum { IDD = IDD_RATE };
	enum
	{
		pitchmin = 0,
		pitchmax = 150

	};
	BEGIN_MSG_MAP( CMyDSPPopup )
		MSG_WM_INITDIALOG( OnInitDialog )
		COMMAND_HANDLER_EX( IDOK, BN_CLICKED, OnButton )
		COMMAND_HANDLER_EX( IDCANCEL, BN_CLICKED, OnButton )
		MSG_WM_HSCROLL( OnHScroll )
	END_MSG_MAP()
private:
	void DSPConfigChange(dsp_chain_config const & cfg)
	{
		if (m_hWnd != NULL) {
			ApplySettings();
		}
	}

	void ApplySettings()
	{
		dsp_preset_impl preset2;
		if (static_api_ptr_t<dsp_config_manager>()->core_query_dsp(guid_pbrate, preset2)) {
			float  rate;
			bool enabled;
			int pitch_type;
			dsp_rate::parse_preset(rate,enabled, preset2);
			slider_drytime.SetPos((double)(rate+50));
			RefreshLabel(rate);
		}
	}

	BOOL OnInitDialog(CWindow, LPARAM)
	{
		slider_drytime = GetDlgItem(IDC_RATE);
		slider_drytime.SetRange(0, pitchmax);

		{
			float  pitch;
			bool enabled;
			dsp_rate::parse_preset(pitch,enabled, m_initData);
			slider_drytime.SetPos( (double)(pitch+50));
			RefreshLabel( pitch);
		}
		
		return TRUE;
	}

	void OnButton( UINT, int id, CWindow )
	{
		EndDialog( id );
	}

	void OnHScroll( UINT nSBCode, UINT nPos, CWindow window )
	{
		float pitch;
		pitch = slider_drytime.GetPos()-50;
		{
			if ((LOWORD(nSBCode) != SB_THUMBTRACK) && window.m_hWnd == slider_drytime.m_hWnd)
			{
				dsp_preset_impl preset;
				dsp_rate::make_preset(pitch, true, preset);
				m_callback.on_preset_changed(preset);
			}

		}
		RefreshLabel( pitch);
	}

	void RefreshLabel(float  pitch )
	{
		pfc::string_formatter msg; 
		msg << "Playback Rate: ";
		msg << (pitch < 0 ? "" : "+");
		msg << pfc::format_int( pitch) << "%";
		::uSetDlgItemText( *this, IDC_RATEINFO, msg );
	
	}
	const dsp_preset & m_initData; // modal dialog so we can reference this caller-owned object.
	dsp_preset_edit_callback & m_callback;
	CTrackBarCtrl slider_drytime,slider_wettime,slider_dampness,slider_roomwidth,slider_roomsize;
};
static void RunDSPConfigPopupRate( const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback )
{
	CMyDSPPopupRate popup( p_data, p_callback );
	if ( popup.DoModal(p_parent) != IDOK ) p_callback.on_preset_changed( p_data );
}

class CMyDSPPopupTempo : public CDialogImpl<CMyDSPPopupTempo>
{
public:
	CMyDSPPopupTempo( const dsp_preset & initData, dsp_preset_edit_callback & callback ) : m_initData( initData ), m_callback( callback ) { }
	enum { IDD = IDD_TEMPO };
	enum
	{
		pitchmin = 0,
		pitchmax = 180

	};
	BEGIN_MSG_MAP( CMyDSPPopup )
		MSG_WM_INITDIALOG( OnInitDialog )
		COMMAND_HANDLER_EX( IDOK, BN_CLICKED, OnButton )
		COMMAND_HANDLER_EX( IDCANCEL, BN_CLICKED, OnButton )
		COMMAND_HANDLER_EX(IDC_TEMPOTYPE, CBN_SELCHANGE, OnChange)
		
		MSG_WM_HSCROLL(OnScroll)
	END_MSG_MAP()
private:
	void DSPConfigChange(dsp_chain_config const & cfg)
	{
		if (m_hWnd != NULL) {
			ApplySettings();
		}
	}

	void ApplySettings()
	{
		dsp_preset_impl preset2;
		if (static_api_ptr_t<dsp_config_manager>()->core_query_dsp(guid_tempo, preset2)) {
			float  pitch;
			bool enabled;
			int pitch_type;
			dsp_pitch::parse_preset(pitch, pitch_type, enabled, preset2);
			CWindow w = GetDlgItem(IDC_TEMPOTYPE);
			::SendMessage(w, CB_SETCURSEL, pitch_type, 0);
			slider_drytime.SetPos((double)(pitch + 90));
			RefreshLabel(pitch);
		}
	}


	BOOL OnInitDialog(CWindow, LPARAM)
	{
		slider_drytime = GetDlgItem(IDC_TEMPO);
		slider_drytime.SetRange(0, pitchmax);

		CWindow w = GetDlgItem(IDC_TEMPOTYPE);
		uSendMessageText(w, CB_ADDSTRING, 0, "SoundTouch");
		uSendMessageText(w, CB_ADDSTRING, 0, "Rubber Band");
		int pitch_type;
		{
			float  pitch;
			bool enabled;
			dsp_tempo::parse_preset(pitch,pitch_type,enabled, m_initData);
			::SendMessage(w, CB_SETCURSEL, pitch_type, 0);
			slider_drytime.SetPos( (double)(pitch+75));
			RefreshLabel( pitch);
		}
		return TRUE;
	}


	void OnButton( UINT, int id, CWindow )
	{
		EndDialog( id );
	}

	void OnChange(UINT, int id, CWindow)
	{
		float pitch;
		pitch = slider_drytime.GetPos() - 90;
		int p_type; //filter type
		p_type = SendDlgItemMessage(IDC_TEMPOTYPE, CB_GETCURSEL);
		{
			dsp_preset_impl preset;
			dsp_tempo::make_preset(pitch, p_type,true, preset);
			m_callback.on_preset_changed(preset);
		}
		RefreshLabel(pitch);
	}

	void OnScroll(UINT scrollID, int id, CWindow window)
	{
		float pitch;
		pitch = slider_drytime.GetPos() - 90;
		int p_type; //filter type
		p_type = SendDlgItemMessage(IDC_TEMPOTYPE, CB_GETCURSEL);
		if ((LOWORD(scrollID) != SB_THUMBTRACK) && window.m_hWnd == slider_drytime.m_hWnd)
		{
			dsp_preset_impl preset;
			dsp_tempo::make_preset(pitch, p_type,true, preset);
			m_callback.on_preset_changed(preset);
		}
		RefreshLabel(pitch);
	}


	void RefreshLabel(float  pitch )
	{
		pfc::string_formatter msg; 
		msg << "Tempo: ";
		msg << (pitch < 0 ? "" : "+");
		msg << pfc::format_int( pitch) << "%";
		::uSetDlgItemText( *this, IDC_TEMPOINFO, msg );
		msg.reset();
	}
	const dsp_preset & m_initData; // modal dialog so we can reference this caller-owned object.
	dsp_preset_edit_callback & m_callback;
	CTrackBarCtrl slider_drytime;
};
static void RunDSPConfigPopupTempo( const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback )
{
	CMyDSPPopupTempo popup( p_data, p_callback );
	if ( popup.DoModal(p_parent) != IDOK ) p_callback.on_preset_changed( p_data );
}






static dsp_factory_t<dsp_tempo> g_dsp_tempo_factory;
static dsp_factory_t<dsp_pitch> g_dsp_pitch_factory;
static dsp_factory_t<dsp_rate> g_dsp_rate_factory;