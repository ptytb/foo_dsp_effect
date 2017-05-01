#define _WIN32_WINNT 0x0501
#include "../SDK/foobar2000.h"
#include "../ATLHelpers/ATLHelpers.h"
#include "resource.h"
#include "rubberband/rubberband/RubberBandStretcher.h"
#include "circular_buffer.h"
using namespace RubberBand;

static void RunDSPConfigPopup( const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback );

#define BUFFER_SIZE 2048


class dsp_pitch : public dsp_impl_base
{
	unsigned buffered;
	bool st_enabled;
	int m_rate, m_ch, m_ch_mask;
	float pitch_amount;
	circular_buffer<float>sample_buffer;
	pfc::array_t<float>samplebuf;

	RubberBandStretcher * rubber;
	float **plugbuf;
	float **m_scratch;


private:
	void insert_chunks()
	{
		while(1)
		{
			t_size samples = rubber->available();
			if (samples <= 0)break;
			samples = rubber->retrieve(m_scratch,samples);
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
				chunk->set_data(data,samples,m_ch,m_rate);
			}
		}
	}


public:
	dsp_pitch( dsp_preset const & in ) : pitch_amount(0.00), m_rate( 0 ), m_ch( 0 ), m_ch_mask( 0 )
	{
		buffered=0;
		rubber = 0;
		plugbuf = NULL;
		parse_preset( pitch_amount, in );
		st_enabled = true;
	}
	~dsp_pitch(){
		if (rubber)
		{
		   insert_chunks();
		}
		delete rubber;
	    rubber = 0;
	}

	// Every DSP type is identified by a GUID.
	static GUID g_get_guid() {
		// Create these with guidgen.exe.
		// {A7FBA855-56D4-46AC-8116-8B2A8DF2FB34}
		static const GUID guid = 
		{ 0xabc792be, 0x276, 0x47bf, { 0xb2, 0x41, 0x7a, 0xcf, 0xc5, 0x21, 0xcb, 0x50 } };
		return guid;
	}

	static void g_get_name(pfc::string_base & p_out) {
		p_out = "Pitch Shift (Rubber band)";
	}

	virtual void on_endoftrack(abort_callback & p_abort) {
		if (rubber)
		{
			insert_chunks();
		}
	}

	virtual void on_endofplayback(abort_callback & p_abort) {
        if (rubber)
		{
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

		if (chunk->get_srate() != m_rate || chunk->get_channels() != m_ch || chunk->get_channel_config() != m_ch_mask)
		{
			m_rate = chunk->get_srate();
			m_ch = chunk->get_channels();
			m_ch_mask = chunk->get_channel_config();

			RubberBandStretcher::Options options = RubberBandStretcher::DefaultOptions;
			options |= RubberBandStretcher::OptionProcessRealTime|RubberBandStretcher::OptionPitchHighQuality;
			rubber = new RubberBandStretcher(m_rate,m_ch,options,1.0, pow(2.0, pitch_amount / 12.0));
			if (!rubber) return 0;
			sample_buffer.set_size(BUFFER_SIZE*m_ch);
			samplebuf.grow_size(BUFFER_SIZE*m_ch);
			if(plugbuf)delete plugbuf;
			plugbuf = new float*[m_ch];
			m_scratch = new float*[m_ch];

			for (int c = 0; c < m_ch; ++c) plugbuf[c] = new float[BUFFER_SIZE];
			for (int c = 0; c < m_ch; ++c) m_scratch[c] = new float[BUFFER_SIZE];
			st_enabled = true;	
		}
	

		if (!st_enabled) return true;
		
		while (sample_count > 0)
		{    
			int toCauseProcessing = rubber->getSamplesRequired();
			int todo = min(toCauseProcessing - buffered, sample_count);
			sample_buffer.write(src,todo*m_ch);
			src += todo * m_ch;
			buffered += todo;
			sample_count -= todo;
			if (buffered ==toCauseProcessing)
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
				rubber->process(plugbuf, toCauseProcessing,false);
				insert_chunks();
				buffered = 0;
			}
		}
		return false;
	}

	virtual void flush() {
		{
			if (rubber)
			{
				insert_chunks();
			}
		}
		m_rate = 0;
		m_ch = 0;
		buffered = 0;
		m_ch_mask = 0;
	}

	virtual double get_latency() {
		return (rubber && m_rate && st_enabled) ? ((double)(rubber->getLatency()) / (double)m_rate) : 0;
	}


	virtual bool need_track_change_mark() {
		return false;
	}

	static bool g_get_default_preset( dsp_preset & p_out )
	{
		make_preset( 0.0, p_out );
		return true;
	}
	static void g_show_config_popup( const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback )
	{
		::RunDSPConfigPopup( p_data, p_parent, p_callback );
	}
	static bool g_have_config_popup() { return true; }
	static void make_preset( float pitch, dsp_preset & out )
	{
		dsp_preset_builder builder; 
		builder << pitch; 
		builder.finish( g_get_guid(), out );
	}                        
	static void parse_preset(float & pitch, const dsp_preset & in)
	{
		try
		{
			dsp_preset_parser parser(in);
			parser >> pitch; 
		}
		catch(exception_io_data) {pitch = 0.0;}
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
		pitchmax = 4800

	};
	BEGIN_MSG_MAP( CMyDSPPopup )
		MSG_WM_INITDIALOG( OnInitDialog )
		COMMAND_HANDLER_EX( IDOK, BN_CLICKED, OnButton )
		COMMAND_HANDLER_EX( IDCANCEL, BN_CLICKED, OnButton )
		MSG_WM_HSCROLL( OnHScroll )
	END_MSG_MAP()
private:
	BOOL OnInitDialog(CWindow, LPARAM)
	{
		slider_drytime = GetDlgItem(IDC_PITCH);
		slider_drytime.SetRange(0, pitchmax);

		{
			float  pitch = 0.0;
			dsp_pitch::parse_preset(pitch, m_initData);
			pitch *= 100.00;
			slider_drytime.SetPos( (double)(pitch+2400));
			RefreshLabel( pitch/100.00);
		}
		return TRUE;
	}

	void OnButton( UINT, int id, CWindow )
	{
		EndDialog( id );
	}

	void OnHScroll( UINT nSBCode, UINT nPos, CScrollBar pScrollBar )
	{
		float pitch;
		pitch = slider_drytime.GetPos()-2400;
		pitch /= 100.00;
		{
			dsp_preset_impl preset;
			dsp_pitch::make_preset(pitch, preset );
			m_callback.on_preset_changed( preset );
		}
		RefreshLabel( pitch);
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
static void RunDSPConfigPopup( const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback )
{
	CMyDSPPopupPitch popup( p_data, p_callback );
	if ( popup.DoModal(p_parent) != IDOK ) p_callback.on_preset_changed( p_data );
}


static void RunDSPConfigPopupTempo(const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback);

class dsp_tempo : public dsp_impl_base
{
	RubberBandStretcher * rubber;
	int m_rate, m_ch, m_ch_mask;
	float pitch_amount;
	circular_buffer<float>sample_buffer;
	pfc::array_t<float>samplebuf;
	float **plugbuf;
	float **m_scratch;
	unsigned buffered;
	bool st_enabled;
private:
	void insert_chunks()
	{
		while (1)
		{
			t_size samples = rubber->available();
			if (samples <= 0)break;
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


public:
	dsp_tempo(dsp_preset const & in) : pitch_amount(0.00), m_rate(0), m_ch(0), m_ch_mask(0)
	{
		buffered = 0;
		rubber = 0;
		plugbuf = NULL;
		parse_preset(pitch_amount, in);
		st_enabled = true;
	}
	~dsp_tempo() {
		if (rubber)
			delete rubber;
		rubber = 0;
	}

	// Every DSP type is identified by a GUID.
	static GUID g_get_guid() {
		// Create these with guidgen.exe.
		// {AA3815D8-AAF8-4BE7-97E5-554452DA5776}
		static const GUID guid =
		{ 0xaa3815d8, 0xaaf8, 0x4be7,{ 0x97, 0xe5, 0x55, 0x44, 0x52, 0xda, 0x57, 0x76 } };


		return guid;
	}

	static void g_get_name(pfc::string_base & p_out) {
		p_out = "Tempo Shift (Rubber band)";
	}

	virtual void on_endoftrack(abort_callback & p_abort) {
		if (rubber)
		{
			insert_chunks();
		}
	}

	virtual void on_endofplayback(abort_callback & p_abort) {
		if (rubber)
		{
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

		if (chunk->get_srate() != m_rate || chunk->get_channels() != m_ch || chunk->get_channel_config() != m_ch_mask)
		{
			m_rate = chunk->get_srate();
			m_ch = chunk->get_channels();
			m_ch_mask = chunk->get_channel_config();

			RubberBandStretcher::Options options = RubberBandStretcher::DefaultOptions;
			options |= RubberBandStretcher::OptionProcessRealTime | RubberBandStretcher::OptionPitchHighQuality;
			rubber = new RubberBandStretcher(m_rate, m_ch, options, 1.0 + 0.01 *-pitch_amount, 1.0);
			if (!rubber) return 0;
			sample_buffer.set_size(BUFFER_SIZE*m_ch);
			samplebuf.grow_size(BUFFER_SIZE*m_ch);
			if (plugbuf)delete plugbuf;
			plugbuf = new float*[m_ch];
			m_scratch = new float*[m_ch];

			for (int c = 0; c < m_ch; ++c) plugbuf[c] = new float[BUFFER_SIZE];
			for (int c = 0; c < m_ch; ++c) m_scratch[c] = new float[BUFFER_SIZE];
			st_enabled = true;
			//	if (pitch_amount== 0)st_enabled = false;
			bool usequickseek = false;
			bool useaafilter = false; //seems clearer without it


		}


		if (!st_enabled) return true;

		while (sample_count > 0)
		{
			int toCauseProcessing = rubber->getSamplesRequired();
			int todo = min(toCauseProcessing - buffered, sample_count);
			sample_buffer.write(src, todo*m_ch);
			src += todo * m_ch;
			buffered += todo;
			sample_count -= todo;
			if (buffered >= toCauseProcessing)
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
				insert_chunks();
				buffered = 0;
			}
		}
		return false;
	}

	virtual void flush() {
		m_rate = 0;
		m_ch = 0;
		buffered = 0;
		m_ch_mask = 0;
	}

	virtual double get_latency() {
		return (rubber && m_rate && st_enabled) ? ((double)(rubber->getLatency()) / (double)m_rate) : 0;
	}


	virtual bool need_track_change_mark() {
		return false;
	}

	static bool g_get_default_preset(dsp_preset & p_out)
	{
		make_preset(0.0, p_out);
		return true;
	}
	static void g_show_config_popup(const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback)
	{
		::RunDSPConfigPopupTempo(p_data, p_parent, p_callback);
	}
	static bool g_have_config_popup() { return true; }
	static void make_preset(float pitch, dsp_preset & out)
	{
		dsp_preset_builder builder;
		builder << pitch;
		builder.finish(g_get_guid(), out);
	}
	static void parse_preset(float & pitch, const dsp_preset & in)
	{
		try
		{
			dsp_preset_parser parser(in);
			parser >> pitch;
		}
		catch (exception_io_data) { pitch = 0.0; }
	}
};

class CMyDSPPopupTempo : public CDialogImpl<CMyDSPPopupTempo>
{
public:
	CMyDSPPopupTempo(const dsp_preset & initData, dsp_preset_edit_callback & callback) : m_initData(initData), m_callback(callback) { }
	enum { IDD = IDD_TEMPO };
	enum
	{
		pitchmin = 0,
		pitchmax = 150

	};
	BEGIN_MSG_MAP(CMyDSPPopup)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_HANDLER_EX(IDOK, BN_CLICKED, OnButton)
		COMMAND_HANDLER_EX(IDCANCEL, BN_CLICKED, OnButton)
		MSG_WM_HSCROLL(OnHScroll)
	END_MSG_MAP()
private:
	BOOL OnInitDialog(CWindow, LPARAM)
	{
		slider_drytime = GetDlgItem(IDC_TEMPO);
		slider_drytime.SetRange(0, pitchmax);

		{
			float  pitch;
			dsp_tempo::parse_preset(pitch, m_initData);
			slider_drytime.SetPos((double)(pitch + 50));
			RefreshLabel(pitch);
		}
		return TRUE;
	}

	void OnButton(UINT, int id, CWindow)
	{
		EndDialog(id);
	}

	void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar pScrollBar)
	{
		float pitch;
		pitch = slider_drytime.GetPos() - 50;
		{
			dsp_preset_impl preset;
			dsp_tempo::make_preset(pitch, preset);
			m_callback.on_preset_changed(preset);
		}
		RefreshLabel(pitch);
	}

	void RefreshLabel(float  pitch)
	{
		pfc::string_formatter msg;
		msg << "Tempo: ";
		msg << (pitch < 0 ? "" : "+");
		msg << pfc::format_int(pitch) << "%";
		::uSetDlgItemText(*this, IDC_TEMPOINFO, msg);
		msg.reset();
	}
	const dsp_preset & m_initData; // modal dialog so we can reference this caller-owned object.
	dsp_preset_edit_callback & m_callback;
	CTrackBarCtrl slider_drytime, slider_wettime, slider_dampness, slider_roomwidth, slider_roomsize;
};
static void RunDSPConfigPopupTempo(const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback)
{
	CMyDSPPopupTempo popup(p_data, p_callback);
	if (popup.DoModal(p_parent) != IDOK) p_callback.on_preset_changed(p_data);
}





static dsp_factory_t<dsp_pitch> g_dsp_pitch_factory;
static dsp_factory_t<dsp_tempo> g_dsp_tempo_factory;