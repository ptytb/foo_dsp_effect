#define _WIN32_WINNT 0x0501
#include "../SDK/foobar2000.h"
#include "../ATLHelpers/ATLHelpers.h"
#define M_PI       3.14159265358979323846

class dsp_loudspeakereq : public dsp_impl_base {	
	// Contains duplicate of chunk
	pfc::array_t<audio_sample> buffer;
	// f[k-2], f[k-1] and f[k] - original audio samples
	pfc::array_t<double> fk[3];	
	// y[k-2], y[k-1] and y[k] - filtered audio samples
	pfc::array_t<double> yk[3];		
	// y[k-2], y[k-1] and y[k] - mechanical model output
	pfc::array_t<double> yk_m[3];
	// Last values from previous chunk, similar to above
	pfc::array_t<double> fk_s[3];		
	pfc::array_t<double> yk_s[3];
	pfc::array_t<double> yk_m_s[3];
	// Filter coefficients, audio- and mechanical filter
	double a[3], b[3], a_m[3], b_m[3];	
	// Presets
	bool eq_activated, mp_activated;
	int fc, fcnew;
	double q_tc, q_tcnew;
	bool auto_pregain;
	double pregain;
	int mp_gain,fs;

	bool q_gain;
	double offset_adjustment_speed;
	
public:
	dsp_loudspeakereq( dsp_preset const & in ) : eq_activated(0),mp_activated (0),fc (100),fcnew(40),q_tc  ((t_int32)(1000*1/sqrt(2.0))), q_tcnew ((t_int32)(1000*1/sqrt(2.0))),auto_pregain( 1),pregain( 160 ),mp_gain(100)
	{
		parse_preset( eq_activated,mp_activated,fc,fcnew,q_tc, q_tcnew,auto_pregain,pregain,mp_gain, in );
		offset_adjustment_speed = 0.95;
	}
	// Function for retriving GUID of plugin
	static GUID g_get_guid() {		
		// {430A98EA-2703-4b02-B25E-2C7485845D4E}
		static const GUID guid = { 0x430a98ea, 0x2703, 0x4b02, { 0xb2, 0x5e, 0x2c, 0x74, 0x85, 0x84, 0x5d, 0x4e } };
		return guid;
	}
	// Function for retrieving name of plugin
    static void g_get_name( pfc::string_base & p_out ) { p_out = "Loudspeaker Equalizer"; }
	// Main class responsible for modifying audio chunks
	virtual bool on_chunk(audio_chunk * chunk, abort_callback & p_abort) {
		unsigned calculate_block = 2;
		unsigned channel_count = chunk->get_channels();
		t_size sample_count = chunk->get_sample_count();
		static int fcnew_offset;
		// Arrays initialized according to the number of channels */
		//Old check to see if array needs reinitialization
		if (channel_count != yk[2].get_size()) {   
			for (unsigned i = 0; i<3; i++) { 
				yk[i].set_size(channel_count);
				yk[i].fill<double>(0);
				fk[i].set_size(channel_count);
				fk[i].fill<double>(0);
				yk_s[i].set_size(channel_count);
				yk_s[i].fill<double>(0);
				fk_s[i].set_size(channel_count);
				fk_s[i].fill<double>(0);
				yk_m[i].set_size(channel_count);
				yk_m[i].fill<double>(0);
				yk_m_s[i].set_size(channel_count);
				yk_m_s[i].fill<double>(0);
			}
			// Restarting any previous mechanical protection
		}
		// Mechanical protection activated, new chunk calculation started
		if (fcnew_offset > 0) {		
			if (fcnew_offset < 2)  // Difference between fcnew and fcnew+offset
				fcnew_offset = 0;	// is miniscule, assuming it's zero
			// Reduce fcnew_offset by multiplication with speed
			else
				fcnew_offset = (int)(fcnew_offset*offset_adjustment_speed);
			calculate_coefficients(fs, fc, fcnew+fcnew_offset, q_tc, q_tcnew, a, b, a_m, b_m);
		}
		// stores address of chunk containing current audio
		audio_sample * current = chunk->get_data();	
		// IIR filter coefficients are calculated according to the sampling frequency
		if (fs != chunk->get_sample_rate()) {
			fs = chunk->get_sample_rate();
			calculate_coefficients(fs, fc, fcnew, q_tc, q_tcnew, a, b, a_m, b_m);
		}
		// Last values of previous chunk is stores, in case IIR filter is to be restarted
		for (unsigned i = 0; i<3; i++) {
			for (unsigned channel = 0; channel < channel_count; channel++) {
				fk_s[i][channel] = fk[i][channel];								
				yk_s[i][channel] = yk[i][channel];
				yk_m_s[i][channel] = yk_m[i][channel];
			}
		}
		// duplicate of chunk address
		audio_sample * current_start = current;		
		// Create copy of audio chunk, source of IIR filter calculations
		buffer.set_size(sample_count*channel_count);
		for (t_size sample = 0; sample < channel_count*sample_count; sample++) {
			buffer[sample] = current[sample];
		}
		// Main loop where audio is modified through the recursive IIR filter
		// sample the calculation reached
		while(calculate_block) {
			// Set conditions to initial state when IIR filter started
			for (unsigned i = 0; i<3; i++) { 
				for (unsigned channel_begin = 0; channel_begin < channel_count; channel_begin++) {
					fk[i][channel_begin] = fk_s[i][channel_begin];								
					yk[i][channel_begin] = yk_s[i][channel_begin];
					yk_m[i][channel_begin] = yk_m_s[i][channel_begin];
				}
			}
			// go to the beginning of the output audio
			//current = current_start;
			int sample_index = 0;
			// no max excursion determined yet
			double mp_max = 0;
			for (t_size sample = 0; sample < sample_count; sample++) {
				for (unsigned channel = 0; channel < channel_count; channel++) {
					// k = k + 1
					// old f[k-1] = new f[k-2]
					fk[2][channel] = fk[1][channel];	
					// old f[k]   = new f[k-1]
					fk[1][channel] = fk[0][channel];
					// Pregain is applied
					fk[0][channel] = (double)(buffer[sample_index] / pregain);
					if (eq_activated) {
						yk[0][channel] = -a[1]*yk[1][channel] -a[2]*yk[2][channel] + 
							b[0]*fk[0][channel] + b[1]*fk[1][channel] + b[2]*fk[2][channel];
					}
					else { // No equalizing is applied
						yk[0][channel] = fk[0][channel];
					}
					if (mp_activated) {
						// Filter modelling mechanical excursion
						yk_m[0][channel] = -a_m[1]*yk_m[1][channel] -a_m[2]*yk_m[2][channel] + 
							b_m[0]*yk[0][channel] + b_m[1]*yk[1][channel] + b_m[2]*yk[2][channel];
						// Save highest excursion during chunk
						if (abs(yk_m[0][channel]) > mp_max)
							mp_max = abs(yk_m[0][channel]);
						yk_m[2][channel] = yk_m[1][channel];
						yk_m[1][channel] = yk_m[0][channel];
					}
					current[sample_index] = (audio_sample)yk[0][channel];
					// old y[k-1] = y[k-2] new
					yk[2][channel] = yk[1][channel];	
					// old y[k] = y[k-1] new
					yk[1][channel] = yk[0][channel];
					sample_index++;
				}
			} 
			// Chunk has now been fully processed
			if (mp_activated && mp_max*mp_gain/100.0 > 1) {
				if (fcnew + fcnew_offset < fc) {
					fcnew_offset += (int)ceil(mp_max*mp_gain/100.0);
					calculate_coefficients(fs, fc, fcnew+fcnew_offset, q_tc, q_tcnew, a, b, a_m, b_m);
				}
				else 
					calculate_block = 0;
			}
			else 
				calculate_block = 0;
		}
		return true;
	}
	void on_endofplayback( abort_callback & ) { }
	void on_endoftrack( abort_callback & ) { }
	void flush()
	{
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
		int tc = ((t_int32)(1000*1/sqrt(2.0)));
		make_preset(0,0,100,40,tc,tc,1, 160,100, p_out );
		return true;
	}
	static void g_show_config_popup( const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback )
	{
		
	}
	static bool g_have_config_popup() { return true; }
	static void make_preset(bool  eq_activated, bool  mp_activated,int  fc, int fcnew,double  q_tc,double  q_tcnew,	bool  auto_pregain, double  pregain, int  mp_gain, dsp_preset & out)
	{
		dsp_preset_builder parser; 
		parser << eq_activated; 
		parser << mp_activated;
		parser << fc;
		parser << fcnew;
		parser << q_tc;
		parser << q_tcnew;
		parser << auto_pregain;
		parser << pregain;
		parser << mp_gain;
		parser.finish( g_get_guid(), out );
	}                        
	void parse_preset(bool & eq_activated, bool & mp_activated,int & fc, int & fcnew,double & q_tc,double & q_tcnew,	bool & auto_pregain, double & pregain, int & mp_gain, const dsp_preset & in)
	{
		try
		{
			dsp_preset_parser parser(in);
			parser >> eq_activated; 
			parser >> mp_activated;
			parser >> fc;
			parser >> fcnew;
			parser >> q_tc;
			parser >> q_tcnew;
			parser >> auto_pregain;
			parser >> pregain;
			parser >> mp_gain;

			// Automatically calculate the resulting pre-gain as a result of fc, fcnew and q_tcnew
			calculate_coefficients(44100, fc, fcnew, q_tc, q_tcnew, a, b, a_m, b_m);
		}
		catch(exception_io_data) {eq_activated = 0;mp_activated = 0;fc = 100;fcnew = 40;q_tc =((t_int32)(1000*1/sqrt(2.0))); q_tcnew = ((t_int32)(1000*1/sqrt(2.0)));auto_pregain = 1;pregain = 160; mp_gain = 100;}
	}
	static void calculate_coefficients(int fs, t_int32 fc, t_int32 fcnew, double q_tc, double q_tcnew, double * a, double * b, double * a_m, double * b_m) {
		double timeunit, wc, wcnew, k_z;
		// Tidsinterval
		timeunit = 1.0/fs;	
		// Original frekvens prewarpet i rad/s
		wc = 2/timeunit*tan(fc*timeunit/2)*2*M_PI;	
		// Ønsket frekvens prewarpet i rad/s
		wcnew = 2/timeunit*tan(fcnew*timeunit/2)*2*M_PI;
		// constant for b[0], b[1] and b[2] of mechanical filter
		k_z = (wc*wc*timeunit*timeunit*q_tc)/(4*q_tc+wc*wc*timeunit*timeunit*q_tc+2*wc*timeunit);
		a[0] = q_tc*(4*q_tcnew+wcnew*wcnew*timeunit*timeunit*q_tcnew+2*wcnew*timeunit);
		a[1] = q_tc*(-8*q_tcnew+2*wcnew*wcnew*timeunit*timeunit*q_tcnew)/a[0];
		a[2] = q_tc*(-2*wcnew*timeunit+4*q_tcnew+wcnew*wcnew*timeunit*timeunit*q_tcnew)/a[0];
		b[0] = q_tcnew*(4*q_tc+wc*wc*timeunit*timeunit*q_tc+2*wc*timeunit)/a[0];
		b[1] = q_tcnew*(-8*q_tc+2*wc*wc*timeunit*timeunit*q_tc)/a[0];
		b[2] = q_tcnew*(-2*wc*timeunit+4*q_tc+wc*wc*timeunit*timeunit*q_tc)/a[0];
		a[0] = 1;
		a_m[0] = 1;
		a_m[1] = (2*q_tc*(-4+wc*wc*timeunit*timeunit))/(4*q_tc+wc*wc*timeunit*timeunit*q_tc+2*wc*timeunit);
		a_m[2] = (-2*wc*timeunit+4*q_tc+wc*wc*timeunit*timeunit*q_tc)/(4*q_tc+wc*wc*timeunit*timeunit*q_tc+2*wc*timeunit);
		b_m[0] = 1*k_z;
		b_m[1] = 2*k_z;
		b_m[2] = 1*k_z;
	}
};
static dsp_factory_t<dsp_loudspeakereq> foo_dsp_loudspeakereq;

