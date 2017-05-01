#include <math.h>
#define _WIN32_WINNT 0x0501
#include "../SDK/foobar2000.h"
#include "../ATLHelpers/ATLHelpers.h"
#include "resource.h"
#include "freeverb.h"
#include "dsp_guids.h"
static void RunDSPConfigPopup( const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback );
class dsp_reverb : public dsp_impl_base
{
	int m_rate, m_ch, m_ch_mask;
	float drytime;
	float wettime;
    float dampness;
	float roomwidth;
	float roomsize;
	bool enabled;
	pfc::array_t<revmodel> m_buffers;
	public:

	dsp_reverb( dsp_preset const & in ) : drytime(0.43),wettime (0.57),dampness (0.45),roomwidth(0.56),roomsize (0.56), m_rate( 0 ), m_ch( 0 ), m_ch_mask( 0 )
	{
		enabled = true;
		parse_preset( drytime, wettime,dampness,roomwidth,roomsize,enabled, in );
	}
	static GUID g_get_guid()
	{
		// {97C60D5F-3572-4d35-9260-FD0CF5DBA480}
		
		return guid_freeverb;
	}
	static void g_get_name( pfc::string_base & p_out ) { p_out = "Reverb"; }
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
				revmodel & e = m_buffers[ i ];
				e.setwet(wettime);
				e.setdry(drytime);
				e.setdamp(dampness);
				e.setroomsize(roomsize);
				e.setwidth(roomwidth);
			}
		}
		for ( unsigned i = 0; i < m_ch; i++ )
		{
			revmodel & e = m_buffers[ i ];
			audio_sample * data = chunk->get_data() + i;
			for ( unsigned j = 0, k = chunk->get_sample_count(); j < k; j++ )
			{
				*data = e.processsample( *data );
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
		make_preset( 0.43, 0.57,0.45,0.56,0.56,true, p_out );
		return true;
	}
	static void g_show_config_popup( const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback )
	{
		::RunDSPConfigPopup( p_data, p_parent, p_callback );
	}
	static bool g_have_config_popup() { return true; }
	static void make_preset( float drytime,float wettime,float dampness,float roomwidth,float roomsize,bool enabled, dsp_preset & out )
	{
		dsp_preset_builder builder; 
		builder << drytime; 
		builder << wettime;
		builder << dampness;
		builder << roomwidth;
		builder << roomsize;
		builder << enabled;
		builder.finish( g_get_guid(), out );
	}                        
	static void parse_preset(float & drytime, float & wettime,float & dampness,float & roomwidth,float & roomsize,bool & enabled, const dsp_preset & in)
	{
		try
		{
			dsp_preset_parser parser(in);
			parser >> drytime; 
			parser >> wettime;
			parser >> dampness;
			parser >> roomwidth;
			parser >> roomsize;
			parser >> enabled;
		}
		catch (exception_io_data) { drytime = 0.43; wettime = 0.57; dampness = 0.45; roomwidth = 0.56; roomsize = 0.56; enabled = true; }
	}
};

static const GUID guid_cfg_placement =
{ 0x240b5f4c, 0x7fd0, 0x41c1,{ 0xa0, 0x24, 0x58, 0x43, 0x59, 0x6a, 0x90, 0xb8 } };



static cfg_window_placement cfg_placement(guid_cfg_placement);

class CMyDSPReverbWindow : public CDialogImpl<CMyDSPReverbWindow>
{
public:
	CMyDSPReverbWindow() {
		drytime = 0.43; wettime = 0.57; dampness = 0.45; 
		roomwidth = 0.56; roomsize = 0.56; reverb_enabled = true;

	}
	enum { IDD = IDD_REVERB1 };
	enum
	{
		drytimemin = 0,
		drytimemax = 100,
		drytimetotal = 100,
		wettimemin = 0,
		wettimemax = 100,
		wettimetotal = 100,
		dampnessmin = 0,
		dampnessmax = 100,
		dampnesstotal = 100,
		roomwidthmin = 0,
		roomwidthmax = 100,
		roomwidthtotal = 100,
		roomsizemin = 0,
		roomsizemax = 100,
		roomsizetotal = 100
	};

	BEGIN_MSG_MAP(CMyDSPReverbWindow)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_HANDLER_EX(IDOK, BN_CLICKED, OnButton)
		COMMAND_HANDLER_EX(IDCANCEL, BN_CLICKED, OnButton)
		COMMAND_HANDLER_EX(IDC_FREEVERBENABLE, BN_CLICKED, OnEnabledToggle)
		MSG_WM_HSCROLL(OnScroll)
		MSG_WM_DESTROY(OnDestroy)
	END_MSG_MAP()
private:
	void SetReverbEnabled(bool state) { m_buttonReverbEnabled.SetCheck(state ? BST_CHECKED : BST_UNCHECKED); }
	bool IsReverbEnabled() { return m_buttonReverbEnabled == NULL || m_buttonReverbEnabled.GetCheck() == BST_CHECKED; }


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

	void ReverbDisable() {
		static_api_ptr_t<dsp_config_manager>()->core_disable_dsp(guid_freeverb);
	}


	void ReverbEnable(float  drytime, float wettime, float dampness, float roomwidth, float roomsize,bool reverb_enabled) {
		dsp_preset_impl preset;
		dsp_reverb::make_preset(drytime, wettime, dampness, roomwidth, roomsize, reverb_enabled, preset);
		DSPEnable(preset);
	}

	void OnEnabledToggle(UINT uNotifyCode, int nID, CWindow wndCtl) {
		pfc::vartoggle_t<bool> ownUpdate(m_ownReverbUpdate, true);
		if (IsReverbEnabled()) {
			GetConfig();
			dsp_preset_impl preset;
			dsp_reverb::make_preset(drytime, wettime, dampness, roomwidth, roomsize, reverb_enabled, preset);
			//yes change api;
			DSPEnable(preset);
		}
		else {
			static_api_ptr_t<dsp_config_manager>()->core_disable_dsp(guid_freeverb);
		}

	}

	void OnScroll(UINT scrollID, int pos, CWindow window)
	{
		pfc::vartoggle_t<bool> ownUpdate(m_ownReverbUpdate, true);
		GetConfig();
		if (IsReverbEnabled())
		{
			if (LOWORD(scrollID) != SB_THUMBTRACK)
			{
				ReverbEnable(drytime, wettime, dampness, roomwidth, roomsize, reverb_enabled);
			}
		}

	}

	void OnChange(UINT, int id, CWindow)
	{
		pfc::vartoggle_t<bool> ownUpdate(m_ownReverbUpdate, true);
		GetConfig();
		if (IsReverbEnabled())
		{

			OnConfigChanged();
		}
	}

	void DSPConfigChange(dsp_chain_config const & cfg)
	{
		if (!m_ownReverbUpdate && m_hWnd != NULL) {
			ApplySettings();
		}
	}

	//set settings if from another control
	void ApplySettings()
	{
		dsp_preset_impl preset;
		if (static_api_ptr_t<dsp_config_manager>()->core_query_dsp(guid_freeverb, preset)) {
			SetReverbEnabled(true);
			dsp_reverb::parse_preset(drytime, wettime, dampness, roomwidth, roomsize, reverb_enabled, preset);
			SetReverbEnabled(reverb_enabled);
			SetConfig();
		}
		else {
			SetReverbEnabled(false);
			SetConfig();
		}
	}

	void OnConfigChanged() {
		if (IsReverbEnabled()) {
			ReverbEnable(drytime, wettime, dampness, roomwidth, roomsize, reverb_enabled);
		}
		else {
			ReverbDisable();
		}

	}


	void GetConfig()
	{
		drytime = slider_drytime.GetPos() / 100.0;
		wettime = slider_wettime.GetPos() / 100.0;
		dampness = slider_dampness.GetPos() / 100.0;
		roomwidth = slider_roomwidth.GetPos() / 100.0;
		roomsize = slider_roomsize.GetPos() / 100.0;
		reverb_enabled = IsReverbEnabled();
		RefreshLabel(drytime, wettime, dampness, roomwidth, roomsize);
	}

	void SetConfig()
	{
		slider_drytime.SetPos((double)(100 * drytime));
		slider_wettime.SetPos((double)(100 * wettime));
		slider_dampness.SetPos((double)(100 * dampness));
		slider_roomwidth.SetPos((double)(100 * roomwidth));
		slider_roomsize.SetPos((double)(100 * roomsize));
		RefreshLabel(drytime, wettime, dampness, roomwidth, roomsize);

	}

	BOOL OnInitDialog(CWindow, LPARAM)
	{

		modeless_dialog_manager::g_add(m_hWnd);
		cfg_placement.on_window_creation(m_hWnd);
		slider_drytime = GetDlgItem(IDC_DRYTIME1);
		slider_drytime.SetRange(0, drytimetotal);
		slider_wettime = GetDlgItem(IDC_WETTIME1);
		slider_wettime.SetRange(0, wettimetotal);
		slider_dampness = GetDlgItem(IDC_DAMPING1);
		slider_dampness.SetRange(0, dampnesstotal);
		slider_roomwidth = GetDlgItem(IDC_ROOMWIDTH1);
		slider_roomwidth.SetRange(0, roomwidthtotal);
		slider_roomsize = GetDlgItem(IDC_ROOMSIZE1);
		slider_roomsize.SetRange(0, roomsizetotal);

		m_buttonReverbEnabled = GetDlgItem(IDC_FREEVERBENABLE);
		m_ownReverbUpdate = false;

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

	void RefreshLabel(float  drytime, float wettime, float dampness, float roomwidth, float roomsize)
	{
		pfc::string_formatter msg;
		msg << "Dry Time: ";
		msg << pfc::format_int(100 * drytime) << "%";
		::uSetDlgItemText(*this, IDC_DRYTIMEINFO1, msg);
		msg.reset();
		msg << "Wet Time: ";
		msg << pfc::format_int(100 * wettime) << "%";
		::uSetDlgItemText(*this, IDC_WETTIMEINFO1, msg);
		msg.reset();
		msg << "Damping: ";
		msg << pfc::format_int(100 * dampness) << "%";
		::uSetDlgItemText(*this, IDC_DAMPINGINFO1, msg);
		msg.reset();
		msg << "Room Width: ";
		msg << pfc::format_int(100 * roomwidth) << "%";
		::uSetDlgItemText(*this, IDC_ROOMWIDTHINFO1, msg);
		msg.reset();
		msg << "Room Size: ";
		msg << pfc::format_int(100 * roomsize) << "%";
		::uSetDlgItemText(*this, IDC_ROOMSIZEINFO1, msg);
	}

	bool reverb_enabled;
	float  drytime, wettime, dampness, roomwidth, roomsize;
	CTrackBarCtrl slider_drytime, slider_wettime, slider_dampness, slider_roomwidth, slider_roomsize;
	CButton m_buttonReverbEnabled;
	bool m_ownReverbUpdate;
};

static CWindow g_pitchdlg;
void ReverbMainMenuWindow()
{
	if (!core_api::assert_main_thread()) return;

	if (!g_pitchdlg.IsWindow())
	{
		CMyDSPReverbWindow  * dlg = new  CMyDSPReverbWindow();
		g_pitchdlg = dlg->Create(core_api::get_main_window());

	}
	if (g_pitchdlg.IsWindow())
	{
		g_pitchdlg.ShowWindow(SW_SHOW);
		::SetForegroundWindow(g_pitchdlg);
	}
}

class CMyDSPPopupReverb : public CDialogImpl<CMyDSPPopupReverb>
{
public:
	CMyDSPPopupReverb( const dsp_preset & initData, dsp_preset_edit_callback & callback ) : m_initData( initData ), m_callback( callback ) { }
	enum { IDD = IDD_REVERB };
	enum
	{
		drytimemin = 0,
		drytimemax = 100,
		drytimetotal = 100,
		wettimemin = 0,
		wettimemax = 100,
		wettimetotal = 100,
		dampnessmin = 0,
		dampnessmax = 100,
		dampnesstotal = 100,
		roomwidthmin = 0,
		roomwidthmax = 100,
		roomwidthtotal = 100,
		roomsizemin = 0,
		roomsizemax = 100,
		roomsizetotal = 100
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
		if (static_api_ptr_t<dsp_config_manager>()->core_query_dsp(guid_freeverb, preset2)) {
			bool enabled;
			dsp_reverb::parse_preset(drytime, wettime, dampness, roomwidth, roomsize, enabled, m_initData);

			slider_drytime.SetPos((double)(100 * drytime));
			slider_wettime.SetPos((double)(100 * wettime));
			slider_dampness.SetPos((double)(100 * dampness));
			slider_roomwidth.SetPos((double)(100 * roomwidth));
			slider_roomsize.SetPos((double)(100 * roomsize));

			RefreshLabel(drytime, wettime, dampness, roomwidth, roomsize);
		}
	}

	BOOL OnInitDialog(CWindow, LPARAM)
	{
		slider_drytime = GetDlgItem(IDC_DRYTIME);
		slider_drytime.SetRange(0, drytimetotal);
		slider_wettime = GetDlgItem(IDC_WETTIME);
		slider_wettime.SetRange(0, wettimetotal);
		slider_dampness = GetDlgItem(IDC_DAMPING);
		slider_dampness.SetRange(0, dampnesstotal);
		slider_roomwidth = GetDlgItem(IDC_ROOMWIDTH);
		slider_roomwidth.SetRange(0, roomwidthtotal);
		slider_roomsize = GetDlgItem(IDC_ROOMSIZE);
		slider_roomsize.SetRange(0, roomsizetotal);

		{
			bool enabled;
			dsp_reverb::parse_preset(drytime,wettime,dampness,roomwidth,roomsize,enabled, m_initData);

			slider_drytime.SetPos( (double)(100*drytime));
			slider_wettime.SetPos( (double)(100*wettime));
			slider_dampness.SetPos( (double)(100*dampness));
			slider_roomwidth.SetPos( (double)(100*roomwidth));
			slider_roomsize.SetPos( (double)(100*roomsize));

			RefreshLabel( drytime,wettime, dampness, roomwidth,roomsize);

		}
		return TRUE;
	}

	void OnButton( UINT, int id, CWindow )
	{
		EndDialog( id );
	}

	void OnHScroll( UINT nSBCode, UINT nPos, CScrollBar pScrollBar )
	{
        drytime = slider_drytime.GetPos()/100.0;
		wettime = slider_wettime.GetPos()/100.0;
		dampness = slider_dampness.GetPos()/100.0;
		roomwidth = slider_roomwidth.GetPos()/100.0;
		roomsize = slider_roomsize.GetPos()/100.0;
		if (LOWORD(nSBCode) != SB_THUMBTRACK)
		{
			dsp_preset_impl preset;
			dsp_reverb::make_preset(drytime,wettime,dampness,roomwidth,roomsize,true, preset );
			m_callback.on_preset_changed( preset );
		}
		RefreshLabel( drytime,wettime, dampness, roomwidth,roomsize);
	}

	void RefreshLabel(float  drytime,float wettime, float dampness, float roomwidth,float roomsize )
	{
		pfc::string_formatter msg; 
		msg << "Dry Time: ";
		msg << pfc::format_int( 100*drytime ) << "%";
		::uSetDlgItemText( *this, IDC_DRYTIMEINFO, msg );
		msg.reset();
		msg << "Wet Time: ";
		msg << pfc::format_int( 100*wettime ) << "%";
		::uSetDlgItemText( *this, IDC_WETTIMEINFO, msg );
		msg.reset();
		msg << "Damping: ";
		msg << pfc::format_int( 100*dampness ) << "%";
		::uSetDlgItemText( *this, IDC_DAMPINGINFO, msg );
		msg.reset();
		msg << "Room Width: ";
		msg << pfc::format_int( 100*roomwidth ) << "%";
		::uSetDlgItemText( *this, IDC_ROOMWIDTHINFO, msg );
		msg.reset();
		msg << "Room Size: ";
		msg << pfc::format_int( 100*roomsize ) << "%";
		::uSetDlgItemText( *this, IDC_ROOMSIZEINFO, msg );
	}
	const dsp_preset & m_initData; // modal dialog so we can reference this caller-owned object.
	dsp_preset_edit_callback & m_callback;
	float  drytime, wettime, dampness, roomwidth, roomsize;
	CTrackBarCtrl slider_drytime,slider_wettime,slider_dampness,slider_roomwidth,slider_roomsize;
};
static void RunDSPConfigPopup( const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback )
{
	CMyDSPPopupReverb popup( p_data, p_callback );
	if ( popup.DoModal(p_parent) != IDOK ) p_callback.on_preset_changed( p_data );
}

static dsp_factory_t<dsp_reverb> g_dsp_reverb_factory;