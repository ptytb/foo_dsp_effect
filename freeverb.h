#ifndef FREEVERB_H
#define FREEVERB_H

// FIXME: Fix this really ugly hack
inline float undenormalise(void *sample) {
	if (((*(unsigned int*)sample) &  0x7f800000) == 0)
		return 0.0f;
	return *(float*)sample;
}


class comb {
public:
	comb();
	void setbuffer(float *buf, int size);
	inline float process(float inp);
	void mute();
	void setdamp(float val);
	float getdamp();
	void setfeedback(float val);
	float getfeedback();
private:
	float feedback;
	float filterstore;
	float damp1;
	float damp2;
	float *buffer;
	int bufsize;
	int bufidx;
};

inline float comb::process(float input) {
	float output;

	output = buffer[bufidx];
	undenormalise(&output);

	filterstore = (output * damp2) + (filterstore * damp1);
	undenormalise(&filterstore);

	buffer[bufidx] = input + (filterstore * feedback);

	if (++bufidx >= bufsize)
		bufidx = 0;

	return output;
}

class allpass {
public:
	allpass();
	void setbuffer(float *buf, int size);
	inline float process(float inp);
	void mute();
	void setfeedback(float val);
	float getfeedback();
private:
	float feedback;
	float *buffer;
	int bufsize;
	int bufidx;
};

inline float allpass::process(float input) {
	float output;
	float bufout;

	bufout = buffer[bufidx];
	undenormalise(&bufout);

	output = -input + bufout;
	buffer[bufidx] = input + (bufout * feedback);

	if (++bufidx >= bufsize)
		bufidx = 0;

	return output;
}


const int	numcombs	= 8;
const int	numallpasses	= 4;
const float	muted		= 0;
const float	fixedgain	= 0.015f;
const float	scalewet	= 3;
const float	scaledry	= 2;
const float	scaledamp	= 0.4f;
const float	scaleroom	= 0.28f;
const float	offsetroom	= 0.7f;
const float	initialroom	= 0.5f;
const float	initialdamp	= 0.5f;
const float	initialwet	= 1 / scalewet;
const float	initialdry	= 0;
const float	initialwidth	= 1;
const float	initialmode	= 0;
const float	freezemode	= 0.5f;

const int combtuningL1		= 1116;
const int combtuningL2		= 1188;
const int combtuningL3		= 1277;
const int combtuningL4		= 1356;
const int combtuningL5		= 1422;
const int combtuningL6		= 1491;
const int combtuningL7		= 1557;
const int combtuningL8		= 1617;
const int allpasstuningL1	= 556;
const int allpasstuningL2	= 441;
const int allpasstuningL3	= 341;
const int allpasstuningL4	= 225;


class revmodel {
public:
	revmodel();
	void mute();
	float revmodel::processsample(float in);
	void setroomsize(float value);
	float getroomsize();
	void setdamp(float value);
	float getdamp();
	void setwet(float value);
	float getwet();
	void setdry(float value);
	float getdry();
	void setwidth(float value);
	float getwidth();
	void setmode(float value);
	float getmode();
private:
	void update();

	float gain;
	float roomsize, roomsize1;
	float damp, damp1;
	float wet, wet1, wet2;
	float dry;
	float width;
	float mode;

	comb combL[numcombs];


	allpass	allpassL[numallpasses];

	float bufcombL1[combtuningL1];
	float bufcombL2[combtuningL2];
	float bufcombL3[combtuningL3];
	float bufcombL4[combtuningL4];
	float bufcombL5[combtuningL5];
	float bufcombL6[combtuningL6];
	float bufcombL7[combtuningL7];
	float bufcombL8[combtuningL8];

	float bufallpassL1[allpasstuningL1];
	float bufallpassL2[allpasstuningL2];
	float bufallpassL3[allpasstuningL3];
	float bufallpassL4[allpasstuningL4];
};

#endif
