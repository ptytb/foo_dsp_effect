#define _WIN32_WINNT 0x0501
#include "../SDK/foobar2000.h"
#include "../ATLHelpers/ATLHelpers.h"
#include "resource.h"
#include "iirfilters.h"
#include "dsp_guids.h"

static void RunConfigPopup( const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback );

class dsp_iir : public dsp_impl_base
{
	int m_rate, m_ch, m_ch_mask;
	int p_freq; //40.0, 13000.0 (Frequency: Hz)
	int p_gain; //gain
	int p_type; //filter type
	bool iir_enabled;
	pfc::array_t<IIRFilter> m_buffers;
public:
	static GUID g_get_guid()
	{
		// {FEA092A6-EA54-4f62-B180-4C88B9EB2B67}
		
		return guid_iir;
	}

	dsp_iir( dsp_preset const & in ) :m_rate( 0 ), m_ch( 0 ), m_ch_mask( 0 ), p_freq(400), p_gain(10), p_type(0)
	{
		iir_enabled = true;
		parse_preset( p_freq,p_gain,p_type,iir_enabled, in );
	}

	static void g_get_name( pfc::string_base & p_out ) { p_out = "IIR Filter"; }

	bool on_chunk( audio_chunk * chunk, abort_callback & )
	{
		if (!iir_enabled)return true;
		if ( chunk->get_srate() != m_rate || chunk->get_channels() != m_ch || chunk->get_channel_config() != m_ch_mask )
		{
			m_rate = chunk->get_srate();
			m_ch = chunk->get_channels();
			m_ch_mask = chunk->get_channel_config();
			m_buffers.set_count( 0 );
			m_buffers.set_count( m_ch );
			for ( unsigned i = 0; i < m_ch; i++ )
			{
				IIRFilter & e = m_buffers[ i ];
				e.setFrequency(p_freq);
				e.setQuality(0.707);
				e.setGain(p_gain);
				e.init(m_rate,p_type);
			}
		}

		for ( unsigned i = 0; i < m_ch; i++ )
		{
			IIRFilter & e = m_buffers[ i ];
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
		make_preset(400, 10, 1,true, p_out );
		return true;
	}
	static void g_show_config_popup( const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback )
	{
		::RunConfigPopup( p_data, p_parent, p_callback );
	}
	static bool g_have_config_popup() { return true; }

	static void make_preset( int p_freq,int p_gain,int p_type,bool enabled, dsp_preset & out )
	{
		dsp_preset_builder builder; 
		builder << p_freq; 
		builder << p_gain; //gain
		builder << p_type; //filter type
		builder << enabled;
		builder.finish( g_get_guid(), out );
	}                        
	static void parse_preset(int & p_freq,int & p_gain,int & p_type,bool & enabled, const dsp_preset & in)
	{
		try
		{
			dsp_preset_parser parser(in);
			parser >> p_freq; 
			parser >> p_gain; //gain
			parser >> p_type; //filter type
			parser >> enabled;
		}
		catch (exception_io_data) { p_freq = 400; p_gain = 10; p_type = 1; enabled = true; }
	}
};

class CMyDSPPopupIIR : public CDialogImpl<CMyDSPPopupIIR>
{
public:
	CMyDSPPopupIIR( const dsp_preset & initData, dsp_preset_edit_callback & callback ) : m_initData( initData ), m_callback( callback ) { }
	enum { IDD = IDD_IIR1 };

	enum
	{
		FreqMin = 0,
		FreqMax = 40000,
		FreqRangeTotal = FreqMax,
		GainMin = -100,
		GainMax = 100,
		GainRangeTotal= GainMax - GainMin
	};

	BEGIN_MSG_MAP( CMyDSPPopup )
		MSG_WM_INITDIALOG( OnInitDialog )
		COMMAND_HANDLER_EX( IDOK, BN_CLICKED, OnButton )
		COMMAND_HANDLER_EX( IDCANCEL, BN_CLICKED, OnButton )
		COMMAND_HANDLER_EX(IDC_IIRTYPE, CBN_SELCHANGE, OnChange)
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
		if (static_api_ptr_t<dsp_config_manager>()->core_query_dsp(guid_iir, preset2)) {
			bool enabled;
			dsp_iir::parse_preset(p_freq, p_gain, p_type,enabled, preset2);
			slider_freq.SetPos(p_freq);
			slider_gain.SetPos(p_gain);
			CWindow w = GetDlgItem(IDC_IIRTYPE1);
			::SendMessage(w, CB_SETCURSEL, p_type, 0);
			if (p_type == 10)
			{
				slider_freq.EnableWindow(FALSE);
				slider_gain.EnableWindow(FALSE);
			}
			else
			{
				slider_freq.EnableWindow(TRUE);
				slider_gain.EnableWindow(TRUE);
			}
			RefreshLabel(p_freq, p_gain, p_type);
		}
	}

	BOOL OnInitDialog(CWindow, LPARAM)
	{
		

		slider_freq = GetDlgItem(IDC_IIRFREQ);
		slider_freq.SetRangeMin(0);
		slider_freq.SetRangeMax(FreqMax);
		slider_gain = GetDlgItem(IDC_IIRGAIN);
		slider_gain.SetRange(GainMin,GainMax);
		{
			
			bool enabled;
			dsp_iir::parse_preset(p_freq, p_gain, p_type, enabled, m_initData);
			if (p_type == 10)
			{
				slider_freq.EnableWindow(FALSE);
				slider_gain.EnableWindow(FALSE);
			}
			else
			{
				slider_freq.EnableWindow(TRUE);
				slider_gain.EnableWindow(TRUE);
			}
			
				
			
			slider_freq.SetPos(p_freq );
			slider_gain.SetPos(p_gain);
			CWindow w = GetDlgItem(IDC_IIRTYPE);
			uSendMessageText(w, CB_ADDSTRING, 0, "Resonant Lowpass");
			uSendMessageText(w, CB_ADDSTRING, 0, "Resonant Highpass");
			uSendMessageText(w, CB_ADDSTRING, 0, "Bandpass (CSG)");
			uSendMessageText(w, CB_ADDSTRING, 0, "Bandpass (ZPG)");
			uSendMessageText(w, CB_ADDSTRING, 0, "Allpass");
			uSendMessageText(w, CB_ADDSTRING, 0, "Notch");
			uSendMessageText(w, CB_ADDSTRING, 0, "RIAA Tape/Vinyl De-emphasis");
			uSendMessageText(w, CB_ADDSTRING, 0, "Parametric EQ (single band)");
			uSendMessageText(w, CB_ADDSTRING, 0, "Bass Boost");
			uSendMessageText(w, CB_ADDSTRING, 0, "Low shelf");
			uSendMessageText(w, CB_ADDSTRING, 0, "CD De-emphasis");
			uSendMessageText(w, CB_ADDSTRING, 0, "High shelf");
			::SendMessage(w, CB_SETCURSEL, p_type, 0);
			RefreshLabel(  p_freq,p_gain,p_type);

		}
		return TRUE;
	}

	void OnButton( UINT, int id, CWindow )
	{
		EndDialog( id );
	}

	void OnChange( UINT scrollid, int id, CWindow window)
	{
		CWindow w;
		p_freq = slider_freq.GetPos();
		p_gain = slider_gain.GetPos();
		p_type = SendDlgItemMessage( IDC_IIRTYPE, CB_GETCURSEL );
		{
			dsp_preset_impl preset;
			dsp_iir::make_preset( p_freq,p_gain,p_type,true, preset );
			m_callback.on_preset_changed( preset );
		}
		if (p_type == 10){
		slider_freq.EnableWindow(FALSE);
		slider_gain.EnableWindow(FALSE);
		}
		else
		{
			slider_freq.EnableWindow(TRUE);
			slider_gain.EnableWindow(TRUE);
		}
		RefreshLabel(  p_freq,p_gain, p_type);
		
	}
	void OnScroll(UINT scrollid, int id, CWindow window)
	{
		CWindow w;
		p_freq = slider_freq.GetPos();
		p_gain = slider_gain.GetPos();
		p_type = SendDlgItemMessage(IDC_IIRTYPE, CB_GETCURSEL);
		if (LOWORD(scrollid) != SB_THUMBTRACK)
		{
			dsp_preset_impl preset;
			dsp_iir::make_preset(p_freq, p_gain, p_type, true, preset);
			m_callback.on_preset_changed(preset);
		}
		if (p_type == 10) {
			slider_freq.EnableWindow(FALSE);
			slider_gain.EnableWindow(FALSE);
		}
		else
		{
			slider_freq.EnableWindow(TRUE);
			slider_gain.EnableWindow(TRUE);
		}
		RefreshLabel(p_freq, p_gain, p_type);

	}


	void RefreshLabel( int p_freq,int p_gain, int p_type)
	{
		pfc::string_formatter msg; 

		if (p_type == 10)
		{
			msg << "Frequency: disabled";
			::uSetDlgItemText( *this, IDC_IIRFREQINFO, msg );
			msg.reset();
			msg << "Gain: disabled";
			::uSetDlgItemText( *this, IDC_IIRGAININFO, msg );
			return;

		}
		msg << "Frequency: ";
		msg << pfc::format_int(  p_freq ) << " Hz";
		::uSetDlgItemText( *this, IDC_IIRFREQINFO, msg );
		msg.reset();
		msg << "Gain: ";
		msg << pfc::format_int(  p_gain) << " db";
		::uSetDlgItemText( *this, IDC_IIRGAININFO, msg );
	}
	int p_freq;
	int p_gain;
	int p_type;
	const dsp_preset & m_initData; // modal dialog so we can reference this caller-owned object.
	dsp_preset_edit_callback & m_callback;
	CTrackBarCtrl slider_freq, slider_gain;
};

static void RunConfigPopup( const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback )
{
	CMyDSPPopupIIR popup( p_data, p_callback );
	if ( popup.DoModal(p_parent) != IDOK ) p_callback.on_preset_changed( p_data );
}


static dsp_factory_t<dsp_iir> g_dsp_iir_factory;

static const GUID guid_cfg_placement =
{ 0x7863cf13, 0x8565, 0x4323,{ 0xaa, 0xa8, 0x73, 0x51, 0x16, 0xb6, 0x4f, 0xdc } };

static cfg_window_placement cfg_placement(guid_cfg_placement);






class CMyDSPIIRWindow : public CDialogImpl<CMyDSPIIRWindow>
{
public:
	CMyDSPIIRWindow() {
		p_freq = 400; p_gain = 10; p_type = 1;
		IIR_enabled = true;

	}
	enum { IDD = IDD_IIR };
	enum
	{
		FreqMin = 0,
		FreqMax = 40000,
		FreqRangeTotal = FreqMax,
		GainMin = -100,
		GainMax = 100,
		GainRangeTotal = GainMax - GainMin
	};
	BEGIN_MSG_MAP(CMyDSPIIRWindow)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_HANDLER_EX(IDOK, BN_CLICKED, OnButton)
		COMMAND_HANDLER_EX(IDCANCEL, BN_CLICKED, OnButton)
		COMMAND_HANDLER_EX(IDC_IIRENABLED, BN_CLICKED, OnEnabledToggle)
		MSG_WM_HSCROLL(OnScroll)
		MSG_WM_DESTROY(OnDestroy)
	END_MSG_MAP()
private:
	void SetIIREnabled(bool state) { m_buttonIIREnabled.SetCheck(state ? BST_CHECKED : BST_UNCHECKED); }
	bool IsIIREnabled() { return m_buttonIIREnabled == NULL || m_buttonIIREnabled.GetCheck() == BST_CHECKED; }


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

	void IIRDisable() {
		static_api_ptr_t<dsp_config_manager>()->core_disable_dsp(guid_iir);
	}


	void IIREnable(int p_freq, int p_gain,int p_type, bool IIR_enabled) {
		dsp_preset_impl preset;
		dsp_iir::make_preset(p_freq, p_gain,p_type, IIR_enabled, preset);
		DSPEnable(preset);
	}

	void OnEnabledToggle(UINT uNotifyCode, int nID, CWindow wndCtl) {
		pfc::vartoggle_t<bool> ownUpdate(m_ownIIRUpdate, true);
		if (IsIIREnabled()) {
			GetConfig();
			dsp_preset_impl preset;
			dsp_iir::make_preset(p_freq, p_gain,p_type, IIR_enabled, preset);
			//yes change api;
			DSPEnable(preset);
		}
		else {
			static_api_ptr_t<dsp_config_manager>()->core_disable_dsp(guid_iir);
		}

	}

	void OnScroll(UINT scrollID, int pos, CWindow window)
	{
		pfc::vartoggle_t<bool> ownUpdate(m_ownIIRUpdate, true);
		GetConfig();
		if (IsIIREnabled())
		{
			if (LOWORD(scrollID) != SB_THUMBTRACK)
			{
				IIREnable(p_freq, p_gain,p_type, IIR_enabled);
			}
		}

	}

	void OnChange(UINT, int id, CWindow)
	{
		pfc::vartoggle_t<bool> ownUpdate(m_ownIIRUpdate, true);
		GetConfig();
		if (IsIIREnabled())
		{

			OnConfigChanged();
		}
	}

	void DSPConfigChange(dsp_chain_config const & cfg)
	{
		if (!m_ownIIRUpdate && m_hWnd != NULL) {
			ApplySettings();
		}
	}

	//set settings if from another control
	void ApplySettings()
	{
		dsp_preset_impl preset;
		if (static_api_ptr_t<dsp_config_manager>()->core_query_dsp(guid_iir, preset)) {
			SetIIREnabled(true);
			dsp_iir::parse_preset(p_freq, p_gain,p_type, IIR_enabled, preset);
			SetIIREnabled(IIR_enabled);
			SetConfig();
		}
		else {
			SetIIREnabled(false);
			SetConfig();
		}
	}

	void OnConfigChanged() {
		if (IsIIREnabled()) {
			IIREnable(p_freq, p_gain,p_type, IIR_enabled);
		}
		else {
			IIRDisable();
		}

	}


	void GetConfig()
	{
		p_freq = slider_freq.GetPos();
		p_gain = slider_gain.GetPos();
		p_type = SendDlgItemMessage(IDC_IIRTYPE1, CB_GETCURSEL);
		IIR_enabled = IsIIREnabled();
		RefreshLabel(p_freq, p_gain,p_type);


	}

	void SetConfig()
	{
		slider_freq.SetPos(p_freq);
		slider_gain.SetPos(p_gain);
		CWindow w = GetDlgItem(IDC_IIRTYPE1);
		::SendMessage(w, CB_SETCURSEL, p_type, 0);
		RefreshLabel(p_freq,p_gain,p_type);

	}

	BOOL OnInitDialog(CWindow, LPARAM)
	{

		modeless_dialog_manager::g_add(m_hWnd);
		cfg_placement.on_window_creation(m_hWnd);

		slider_freq = GetDlgItem(IDC_IIRFREQ1);
		slider_freq.SetRangeMin(0);
		slider_freq.SetRangeMax(FreqMax);
		slider_gain = GetDlgItem(IDC_IIRGAIN1);
		slider_gain.SetRange(GainMin, GainMax);
		CWindow w = GetDlgItem(IDC_IIRTYPE1);
		uSendMessageText(w, CB_ADDSTRING, 0, "Resonant Lowpass");
		uSendMessageText(w, CB_ADDSTRING, 0, "Resonant Highpass");
		uSendMessageText(w, CB_ADDSTRING, 0, "Bandpass (CSG)");
		uSendMessageText(w, CB_ADDSTRING, 0, "Bandpass (ZPG)");
		uSendMessageText(w, CB_ADDSTRING, 0, "Allpass");
		uSendMessageText(w, CB_ADDSTRING, 0, "Notch");
		uSendMessageText(w, CB_ADDSTRING, 0, "RIAA Tape/Vinyl De-emphasis");
		uSendMessageText(w, CB_ADDSTRING, 0, "Parametric EQ (single band)");
		uSendMessageText(w, CB_ADDSTRING, 0, "Bass Boost");
		uSendMessageText(w, CB_ADDSTRING, 0, "Low shelf");
		uSendMessageText(w, CB_ADDSTRING, 0, "CD De-emphasis");
		uSendMessageText(w, CB_ADDSTRING, 0, "High shelf");


		m_buttonIIREnabled = GetDlgItem(IDC_IIRENABLED);
		m_ownIIRUpdate = false;

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

	void RefreshLabel(int p_freq, int p_gain, int p_type)
	{
		pfc::string_formatter msg;

		if (p_type == 10)
		{
			msg << "Frequency: disabled";
			::uSetDlgItemText(*this, IDC_IIRFREQINFO1, msg);
			msg.reset();
			msg << "Gain: disabled";
			::uSetDlgItemText(*this, IDC_IIRGAININFO1, msg);
			return;

		}
		msg << "Frequency: ";
		msg << pfc::format_int(p_freq) << " Hz";
		::uSetDlgItemText(*this, IDC_IIRFREQINFO1, msg);
		msg.reset();
		msg << "Gain: ";
		msg << pfc::format_int(p_gain) << " db";
		::uSetDlgItemText(*this, IDC_IIRGAININFO1, msg);
	}

	bool IIR_enabled;
	int p_freq; //40.0, 13000.0 (Frequency: Hz)
	int p_gain; //gain
	int p_type; //filter type
	CTrackBarCtrl slider_freq, slider_gain;
	CButton m_buttonIIREnabled;
	bool m_ownIIRUpdate;
};

static CWindow g_pitchdlg;
void IIRMainMenuWindow()
{
	if (!core_api::assert_main_thread()) return;

	if (!g_pitchdlg.IsWindow())
	{
		CMyDSPIIRWindow  * dlg = new  CMyDSPIIRWindow();
		g_pitchdlg = dlg->Create(core_api::get_main_window());

	}
	if (g_pitchdlg.IsWindow())
	{
		g_pitchdlg.ShowWindow(SW_SHOW);
		::SetForegroundWindow(g_pitchdlg);
	}
}