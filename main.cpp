#include "../SDK/foobar2000.h"
#include "SoundTouch/SoundTouch.h"
#include "dsp_guids.h"
#define MYVERSION "0.21"

static pfc::string_formatter g_get_component_about()
{
	pfc::string_formatter about;
	about << "A special effect DSP for foobar2000 1.3 ->\n";
	about << "Written by mudlord.\n";
	about << "Portions by Jon Watte, Jezar Wakefield, Chris Snowhill, Gian-Carlo Pascutto.\n";
	about << "Using SoundTouch library version "<< SOUNDTOUCH_VERSION << "\n";
	about << "SoundTouch (c) Olli Parviainen\n";
	return about;
}

// {35FDAA75-AAC9-4EFE-9091-D12E7398EF4A}
static const GUID g_mainmenu_group_id =
{ 0x35fdaa75, 0xaac9, 0x4efe,{ 0x90, 0x91, 0xd1, 0x2e, 0x73, 0x98, 0xef, 0x4a } };
static mainmenu_group_popup_factory g_mainmenu_group(g_mainmenu_group_id,
	mainmenu_groups::view, mainmenu_commands::sort_priority_base, "Effect DSP");

class mainmenu_commands_sample : public mainmenu_commands {
	dsp_chain_config_impl chain;

public:
	void execute(t_uint32 p_index, service_ptr_t<service_base> p_callback) {
		get_command_count(); // Gotta refresh first
		switch (p_index)
		{
		case 0:
			::ChorusMainMenuWindow();
			break;
		case 1:
			::DynamicsMainMenuWindow();
			break;
		case 2:
			::EchoMainMenuWindow();
			break;
		case 3:
			::IIRMainMenuWindow();
			break;
		case 4:
			::PhaserMainMenuWindow();
			break;
		case 5:
			::PitchMainMenuWindow();
			break;
		case 6:
			::ReverbMainMenuWindow();
			break;
		case 7:
			::TremeloMainMenuWindow();
			break;
		case 8:
			::VibratoMainMenuWindow();
			break;
		case 9:
			::WahMainMenuWindow();
			break;
		}
	}

	static const int cmd_max = 32;

	t_uint32 get_command_count()
	{
		return 10;
	}

	GUID get_parent() { return g_mainmenu_group_id; }

	GUID get_command(t_uint32 p_index) {
		GUID base =
		{ 0xf53517a1, 0x615e, 0x46da,{ 0xb1, 0x7, 0xa5, 0x31, 0x99, 0xb8, 0x6b, 0xc9 } };
		base.Data3 = base.Data3 + p_index;
		return base;
	}

	void get_name(t_uint32 p_index, pfc::string_base & p_out) {
		switch (p_index)
		{
		case 0:
			p_out.reset();
			p_out += "Chorus";
			break;
		case 1:
			p_out.reset();
			p_out += "Dynamics Compressor";
			break;
		case 2:
			p_out.reset();
			p_out += "Echo";
			break;
		case 3:
			p_out.reset();
			p_out += "IIR Filter";
			break;
		case 4:
			p_out.reset();
			p_out += "Phaser";
			break;
		case 5:
			p_out.reset();
			p_out += "Pitch/Tempo/Playback Rate Shift";
			break;
		case 6:
			p_out.reset();
			p_out += "Reverb";
			break;
		case 7:
			p_out.reset();
			p_out += "Tremolo";
			break;
		case 8:
			p_out.reset();
			p_out += "Vibrato";
			break;
		case 9:
			p_out.reset();
			p_out += "WahWah";
			break;
		}
		
	}

	bool get_description(t_uint32 p_index, pfc::string_base & p_out) {

		pfc::string8 text;
		p_out.reset();
		switch (p_index)
		{
		case 0:
			p_out = "Opens a window for chorus control.";
			break;
		case 1:
			p_out = "Opens a window for realtime dynamics compression control.";
			break;
		case 2:
			p_out = "Opens a window for echo adjustment.";
			break;
		case 3:
			p_out = "Opens a window for realtime IIR filtering control.";
			break;
		case 4:
			p_out = "Opens a window for cycling phase modulation control.";
			break;
		case 5:
			p_out = "Opens a window for pitch/tempo/playback rate control.";
			break;
		case 6:
			p_out = "Opens a window for reverberation adjustment.";
			break;
		case 7:
			p_out = "Opens a window for tremolo control.";
			break;
		case 8:
			p_out = "Opens a window for vibrato control.";
			break;
		case 9:
			p_out = "Opens a window for wah effect control.";
			break;
		}
		return true;
	}

	bool get_display(
		t_uint32 p_index,
		pfc::string_base & p_text,
		t_uint32 & p_flags)
	{
		switch (p_index)
		{
		case 0:
			p_text = "Chorus";
			break;
		case 1:
			p_text = "Dynamics Compressor";
			break;
		case 2:
			p_text = "Echo";
			break;
		case 3:
			p_text = "IIR Filter";
			break;
		case 4:
			p_text = "Phaser";
			break;
		case 5:
			p_text = "Pitch/Tempo/Playback Rate";
			break;
		case 6:
			p_text = "Reverb";
			break;
		case 7:
			p_text = "Tremolo";
			break;
		case 8:
			p_text = "Vibrato";
			break;
		case 9:
			p_text = "WahWah";
			break;
		}
		return true;
	}
};
static mainmenu_commands_factory_t < mainmenu_commands_sample >g_mainmenu_commands_sample_factory;


DECLARE_COMPONENT_VERSION_COPY(
"Effect DSP",
MYVERSION,
g_get_component_about()
);
VALIDATE_COMPONENT_FILENAME("foo_dsp_effect.dll");