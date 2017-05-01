#define _WIN32_WINNT 0x0501
#include "../SDK/foobar2000.h"
#include "../ATLHelpers/ATLHelpers.h"
#include "resource.h"
#include "wahwah.h"
#include "dsp_guids.h"
static void RunConfigPopup( const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback );
class dsp_wahwah : public dsp_impl_base
{
	int m_rate, m_ch, m_ch_mask;
	float freq;
	float depth,startphase;
	float freqofs, res;
	bool enabled;
	pfc::array_t<WahWah> m_buffers;
public:
	static GUID g_get_guid()
	{
		// {3E144CFA-C63A-4c12-9503-A5C83C7E5CF8}
		return guid_wahwah;
	}

	dsp_wahwah( dsp_preset const & in ) : m_rate( 0 ), m_ch( 0 ), m_ch_mask( 0 ), freq(1.5),depth(0.70),startphase(0.0),freqofs(0.30),res(2.5)
	{
		enabled = true;
		parse_preset( freq, depth,startphase,freqofs,res,enabled, in );
	}

	static void g_get_name( pfc::string_base & p_out ) { p_out = "WahWah"; }

	bool on_chunk( audio_chunk * chunk, abort_callback & )
	{
		if (!enabled)return true;
		if ( chunk->get_srate() != m_rate || chunk->get_channels() != m_ch || chunk->get_channel_config() != m_ch_mask )
		{
			m_rate = chunk->get_srate();
			m_ch = chunk->get_channels();
			m_ch_mask = chunk->get_channel_config();
			m_buffers.set_count( 0 );
			m_buffers.set_count( m_ch );
			for ( unsigned i = 0; i < m_ch; i++ )
			{
				WahWah & e = m_buffers[ i ];
				e.SetDepth(depth);
				e.SetFreqOffset(freqofs);
				e.SetLFOFreq(freq);
				e.SetLFOStartPhase(startphase);
				e.SetResonance(res);
				e.init(m_rate);
			}
		}

		for ( unsigned i = 0; i < m_ch; i++ )
		{
			WahWah & e = m_buffers[ i ];
			audio_sample * data = chunk->get_data() + i;
			for ( unsigned j = 0, k = chunk->get_sample_count(); j < k; j++ )
			{
				*data = e.Process(*data);
				data += m_ch;
			}
		}

		return true;
	}

	void on_endofplayback( abort_callback & ) { }
	void on_endoftrack( abort_callback & ) { }

	void flush()
	{
		m_buffers.set_count( 0 );
		m_rate = 0;
		m_ch = 0;
		m_ch_mask = 0;
	}

	double get_latency()
	{
		return 0;
	}

	bool need_track_change_mark()
	{
		return false;
	}
	static bool g_get_default_preset( dsp_preset & p_out )
	{
		make_preset( 1.5, 0.70,0.0,0.30,2.5,true, p_out );
		return true;
	}
	static void g_show_config_popup( const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback )
	{
		::RunConfigPopup( p_data, p_parent, p_callback );
	}
	static bool g_have_config_popup() { return true; }


	static void make_preset( float freq,float depth,float startphase,float freqofs,float res,bool enabled, dsp_preset & out )
	{
		dsp_preset_builder builder; 
		builder << freq; 
		builder << depth;
		builder << startphase;
		builder << freqofs;
		builder << res;
		builder << enabled;
		builder.finish( g_get_guid(), out );
	}                        
	static void parse_preset( float & freq,float & depth,float & startphase,float & freqofs,float & res,bool & enabled,const dsp_preset & in )
	{
		try
		{
			dsp_preset_parser parser(in);
			parser >> freq; 
			parser >> depth;
			parser >> startphase;
			parser >> freqofs;
			parser >> res;
			parser >> enabled;
		}
		catch (exception_io_data) { freq = 1.5; depth = 0.70; startphase = 0.0; freqofs = 0.3; res = 2.5; enabled = true; }
	}
};

static const GUID guid_cfg_placement =
{ 0x200fa03d, 0xdc1d, 0x4386,{ 0xb9, 0x34, 0xc4, 0x33, 0x5c, 0x48, 0x2c, 0xda } };

static cfg_window_placement cfg_placement(guid_cfg_placement);

class CMyDSPWahWindow : public CDialogImpl<CMyDSPWahWindow>
{
public:
	CMyDSPWahWindow() {
		freq = 1.5; depth = 0.70; startphase = 0.0; freqofs = 0.3; res = 2.5; wah_enabled = true;

	}
	enum { IDD = IDD_WAHWAH1 };

	enum
	{
		FreqMin = 1,
		FreqMax = 200,
		FreqRangeTotal = FreqMax - FreqMin,
		StartPhaseMin = 0,
		StartPhaseMax = 360,
		StartPhaseTotal = 360,
		DepthMin = 0,
		DepthMax = 100,
		DepthRangeTotal = 100,
		ResonanceMin = 0,
		ResonanceMax = 100,
		ResonanceTotal = 100,
		FrequencyOffsetMin = 0,
		FrequencyOffsetMax = 100,
		FrequencyOffsetTotal = 100
	};

	BEGIN_MSG_MAP(CMyDSPWahWindow)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_HANDLER_EX(IDOK, BN_CLICKED, OnButton)
		COMMAND_HANDLER_EX(IDCANCEL, BN_CLICKED, OnButton)
		COMMAND_HANDLER_EX(IDC_WAHENABLED, BN_CLICKED, OnEnabledToggle)
		MSG_WM_HSCROLL(OnScroll)
		MSG_WM_DESTROY(OnDestroy)
	END_MSG_MAP()
private:
	void SetWahEnabled(bool state) { m_buttonWahEnabled.SetCheck(state ? BST_CHECKED : BST_UNCHECKED); }
	bool IsWahEnabled() { return m_buttonWahEnabled == NULL || m_buttonWahEnabled.GetCheck() == BST_CHECKED; }


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
		if (!found) { if (n > 0)n++; cfg.insert_item(data, n); changed = true; }
		if (changed) static_api_ptr_t<dsp_config_manager>()->set_core_settings(cfg);
	}

	void WahDisable() {
		static_api_ptr_t<dsp_config_manager>()->core_disable_dsp(guid_wahwah);
	}


	void WahEnable(float freq, float depth, float startphase, float freqofs, float res,bool wah_enabled) {
		dsp_preset_impl preset;
		dsp_wahwah::make_preset(freq, depth, startphase, freqofs, res, wah_enabled, preset);
		DSPEnable(preset);
	}

	void OnEnabledToggle(UINT uNotifyCode, int nID, CWindow wndCtl) {
		pfc::vartoggle_t<bool> ownUpdate(m_ownWahUpdate, true);
		if (IsWahEnabled()) {
			GetConfig();
			dsp_preset_impl preset;
			dsp_wahwah::make_preset(freq, depth, startphase, freqofs, res, wah_enabled, preset);
			//yes change api;
			DSPEnable(preset);
		}
		else {
			static_api_ptr_t<dsp_config_manager>()->core_disable_dsp(guid_wahwah);
		}

	}

	void OnScroll(UINT scrollID, int pos, CWindow window)
	{
		pfc::vartoggle_t<bool> ownUpdate(m_ownWahUpdate, true);
		GetConfig();
		if (IsWahEnabled())
		{
			if (LOWORD(scrollID) != SB_THUMBTRACK)
			{
				WahEnable(freq, depth, startphase, freqofs, res, wah_enabled);
			}
		}

	}

	void OnChange(UINT, int id, CWindow)
	{
		pfc::vartoggle_t<bool> ownUpdate(m_ownWahUpdate, true);
		GetConfig();
		if (IsWahEnabled())
		{
			OnConfigChanged();
		}
	}

	void DSPConfigChange(dsp_chain_config const & cfg)
	{
		if (!m_ownWahUpdate && m_hWnd != NULL) {
			ApplySettings();
		}
	}

	//set settings if from another control
	void ApplySettings()
	{
		dsp_preset_impl preset;
		if (static_api_ptr_t<dsp_config_manager>()->core_query_dsp(guid_wahwah, preset)) {
			SetWahEnabled(true);
			dsp_wahwah::parse_preset(freq, depth, startphase, freqofs, res, wah_enabled, preset);
			SetWahEnabled(wah_enabled);
			SetConfig();
		}
		else {
			SetWahEnabled(false);
			SetConfig();
		}
	}

	void OnConfigChanged() {
		if (IsWahEnabled()) {
			WahEnable(freq, depth, startphase, freqofs, res, wah_enabled);
		}
		else {
			WahDisable();
		}

	}


	void GetConfig()
	{
		freq = slider_freq.GetPos() / 10.0;
		depth = slider_depth.GetPos() / 100.0;
		startphase = slider_startphase.GetPos() / 100.0;
		freqofs = slider_freqofs.GetPos() / 100.0;
		res = slider_res.GetPos() / 10.0;
		wah_enabled = IsWahEnabled();
		RefreshLabel(freq, depth, startphase, freqofs, res);
	}

	void SetConfig()
	{
		slider_freq.SetPos((double)(10 * freq));
		slider_startphase.SetPos((double)(100 * startphase));
		slider_freqofs.SetPos((double)(100 * freqofs));
		slider_depth.SetPos((double)(100 * depth));
		slider_res.SetPos((double)(10 * res));

		RefreshLabel(freq, depth, startphase, freqofs, res);

	}

	BOOL OnInitDialog(CWindow, LPARAM)
	{

		modeless_dialog_manager::g_add(m_hWnd);
		cfg_placement.on_window_creation(m_hWnd);
		slider_freq = GetDlgItem(IDC_WAHLFOFREQ1);
		slider_freq.SetRange(FreqMin, FreqMax);
		slider_startphase = GetDlgItem(IDC_WAHLFOSTARTPHASE1);
		slider_startphase.SetRange(0, StartPhaseMax);
		slider_freqofs = GetDlgItem(IDC_WAHFREQOFFSET1);
		slider_freqofs.SetRange(0, 100);
		slider_depth = GetDlgItem(IDC_WAHDEPTH1);
		slider_depth.SetRange(0, 100);
		slider_res = GetDlgItem(IDC_WAHRESONANCE1);
		slider_res.SetRange(0, 100);

		m_buttonWahEnabled = GetDlgItem(IDC_WAHENABLED);
		m_ownWahUpdate = false;

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

	void RefreshLabel(float freq, float depth, float startphase, float freqofs, float res)
	{
		pfc::string_formatter msg;
		msg << "LFO Frequency: ";
		msg << pfc::format_float(freq, 0, 1) << " Hz";
		::uSetDlgItemText(*this, IDC_WAHLFOFREQINFO1, msg);
		msg.reset();
		msg << "Depth: ";
		msg << pfc::format_int(depth * 100) << "%";
		::uSetDlgItemText(*this, IDC_WAHDEPTHINFO1, msg);
		msg.reset();
		msg << "LFO Starting Phase: ";
		msg << pfc::format_int(startphase * 100) << " (deg.)";
		::uSetDlgItemText(*this, IDC_WAHLFOPHASEINFO1, msg);
		msg.reset();
		msg << "Wah Frequency Offset: ";
		msg << pfc::format_int(freqofs * 100) << "%";
		::uSetDlgItemText(*this, IDC_WAHFREQOFFSETINFO1, msg);
		msg.reset();
		msg << "Resonance: ";
		msg << pfc::format_float(res, 0, 1) << "";
		::uSetDlgItemText(*this, IDC_WAHRESONANCEINFO1, msg);
		msg.reset();
	}

	bool wah_enabled;
	float freq;
	float depth, startphase;
	float freqofs, res;
	CTrackBarCtrl slider_freq, slider_startphase, slider_freqofs, slider_depth, slider_res;
	CButton m_buttonWahEnabled;
	bool m_ownWahUpdate;
};

static CWindow g_pitchdlg;
void WahMainMenuWindow()
{
	if (!core_api::assert_main_thread()) return;

	if (!g_pitchdlg.IsWindow())
	{
		CMyDSPWahWindow  * dlg = new  CMyDSPWahWindow();
		g_pitchdlg = dlg->Create(core_api::get_main_window());

	}
	if (g_pitchdlg.IsWindow())
	{
		g_pitchdlg.ShowWindow(SW_SHOW);
		::SetForegroundWindow(g_pitchdlg);
	}
}

class CMyDSPPopupWah : public CDialogImpl<CMyDSPPopupWah>
{
public:
	CMyDSPPopupWah( const dsp_preset & initData, dsp_preset_edit_callback & callback ) : m_initData( initData ), m_callback( callback ) { }
	enum { IDD = IDD_WAHWAH };

	enum
	{
		FreqMin = 1,
		FreqMax = 200,
		FreqRangeTotal = FreqMax - FreqMin,
		StartPhaseMin = 0,
		StartPhaseMax = 360,
		StartPhaseTotal =  360,
		DepthMin = 0,
		DepthMax = 100,
		DepthRangeTotal = 100,
		ResonanceMin = 0,
		ResonanceMax = 100,
		ResonanceTotal = 100,
		FrequencyOffsetMin = 0,
		FrequencyOffsetMax = 100,
		FrequencyOffsetTotal = 100
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
		if (static_api_ptr_t<dsp_config_manager>()->core_query_dsp(guid_wahwah, preset2)) {
			bool enabled;
			dsp_wahwah::parse_preset(freq, depth, startphase, freqofs, res, enabled, m_initData);

			slider_freq.SetPos((double)(10 * freq));
			slider_startphase.SetPos((double)(100 * startphase));
			slider_freqofs.SetPos((double)(100 * freqofs));
			slider_depth.SetPos((double)(100 * depth));
			slider_res.SetPos((double)(10 * res));

			RefreshLabel(freq, depth, startphase, freqofs, res);
		}
	}

	BOOL OnInitDialog(CWindow, LPARAM)
	{
		slider_freq = GetDlgItem(IDC_WAHLFOFREQ);
		slider_freq.SetRange(FreqMin,FreqMax);
		slider_startphase = GetDlgItem(IDC_WAHLFOSTARTPHASE);
		slider_startphase.SetRange(0,StartPhaseMax);
		slider_freqofs = GetDlgItem(IDC_WAHFREQOFFSET);
		slider_freqofs.SetRange(0,100);
		slider_depth = GetDlgItem(IDC_WAHDEPTH);
		slider_depth.SetRange(0,100);
		slider_res = GetDlgItem(IDC_WAHRESONANCE);
		slider_res.SetRange(0,100);

		{
			bool enabled;
			dsp_wahwah::parse_preset(freq,depth,startphase,freqofs,res,enabled,m_initData);

			slider_freq.SetPos((double)(10*freq));
			slider_startphase.SetPos((double)(100*startphase));
			slider_freqofs.SetPos((double)(100*freqofs));
			slider_depth.SetPos((double)(100*depth));
			slider_res.SetPos((double)(10*res));

			RefreshLabel(freq,depth,startphase,freqofs,res);


		}
		return TRUE;
	}

	void OnButton( UINT, int id, CWindow )
	{
		EndDialog( id );
	}

	void OnHScroll( UINT nSBCode, UINT nPos, CScrollBar pScrollBar )
	{
		freq = slider_freq.GetPos()/10.0;
		depth = slider_depth.GetPos()/100.0;
		startphase = slider_startphase.GetPos()/100.0;
		freqofs = slider_freqofs.GetPos()/100.0;
		res = slider_res.GetPos()/10.0;
		if (LOWORD(nSBCode) != SB_THUMBTRACK)
		{
			dsp_preset_impl preset;
			dsp_wahwah::make_preset(freq,depth,startphase,freqofs,res,true,preset);
			m_callback.on_preset_changed( preset );
		}
		RefreshLabel(freq,depth,startphase,freqofs,res);

	}

	void RefreshLabel(float freq,float depth, float startphase, float freqofs, float res)
	{
		pfc::string_formatter msg; 
		msg << "LFO Frequency: ";
		msg << pfc::format_float( freq,0,1 ) << " Hz";
		::uSetDlgItemText( *this, IDC_WAHLFOFREQINFO, msg );
		msg.reset();
		msg << "Depth: ";
		msg << pfc::format_int( depth*100 ) << "%";
		::uSetDlgItemText( *this, IDC_WAHDEPTHINFO, msg );
		msg.reset();
		msg << "LFO Starting Phase: ";
		msg << pfc::format_int( startphase*100 ) << " (deg.)";
		::uSetDlgItemText( *this, IDC_WAHLFOPHASEINFO, msg );
		msg.reset();
		msg << "Wah Frequency Offset: ";
		msg << pfc::format_int( freqofs*100 ) << "%";
		::uSetDlgItemText( *this, IDC_WAHFREQOFFSETINFO, msg );
		msg.reset();
		msg << "Resonance: ";
		msg << pfc::format_float( res,0,1 ) << "";
		::uSetDlgItemText( *this, IDC_WAHRESONANCEINFO, msg );
		msg.reset();
	}

	const dsp_preset & m_initData; // modal dialog so we can reference this caller-owned object.
	dsp_preset_edit_callback & m_callback;
	float freq;
	float depth, startphase;
	float freqofs, res;
	CTrackBarCtrl slider_freq, slider_startphase,slider_freqofs,slider_depth,slider_res;
};

static void RunConfigPopup( const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback )
{
	CMyDSPPopupWah popup( p_data, p_callback );
	if ( popup.DoModal(p_parent) != IDOK ) p_callback.on_preset_changed( p_data );
}

static dsp_factory_t<dsp_wahwah> g_dsp_wahwah_factory;