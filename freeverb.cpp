#include "freeverb.h"

comb::comb() {
	filterstore = 0;
	bufidx = 0;
}

void comb::setbuffer(float *buf, int size) {
	buffer = buf;
	bufsize = size;
}

void comb::mute() {
	for (int i = 0; i < bufsize; i++)
		buffer[i] = 0;
}

void comb::setdamp(float val) {
	damp1 = val;
	damp2 = 1 - val;
}

float comb::getdamp() {
	return damp1;
}

void comb::setfeedback(float val) {
	feedback = val;
}

float comb::getfeedback() {
	return feedback;
}



allpass::allpass() {
	bufidx = 0;
}

void allpass::setbuffer(float *buf, int size) {
	buffer = buf;
	bufsize = size;
}

void allpass::mute() {
	for (int i = 0; i < bufsize; i++)
		buffer[i] = 0;
}

void allpass::setfeedback(float val) {
	feedback = val;
}

float allpass::getfeedback() {
	return feedback;
}



revmodel::revmodel() {
	combL[0].setbuffer(bufcombL1,combtuningL1);
	combL[1].setbuffer(bufcombL2,combtuningL2);
	combL[2].setbuffer(bufcombL3,combtuningL3);
	combL[3].setbuffer(bufcombL4,combtuningL4);
	combL[4].setbuffer(bufcombL5,combtuningL5);
	combL[5].setbuffer(bufcombL6,combtuningL6);
	combL[6].setbuffer(bufcombL7,combtuningL7);
	combL[7].setbuffer(bufcombL8,combtuningL8);
	allpassL[0].setbuffer(bufallpassL1,allpasstuningL1);
	allpassL[1].setbuffer(bufallpassL2,allpasstuningL2);
	allpassL[2].setbuffer(bufallpassL3,allpasstuningL3);
	allpassL[3].setbuffer(bufallpassL4,allpasstuningL4);
	allpassL[0].setfeedback(0.5f);
	allpassL[1].setfeedback(0.5f);
	allpassL[2].setfeedback(0.5f);
	allpassL[3].setfeedback(0.5f);
	setwet(initialwet);
	setroomsize(initialroom);
	setdry(initialdry);
	setdamp(initialdamp);
	setwidth(initialwidth);
	setmode(initialmode);
	mute();
}

void revmodel::mute() {
	int i;

	if (getmode() >= freezemode)
		return;

	for (i = 0; i < numcombs; i++) {
		combL[i].mute();
	}

	for (i = 0; i < numallpasses; i++) {
		allpassL[i].mute();
	}
}

float revmodel::processsample(float in)
{
	float samp = in;
	float mono_out = 0.0f;
	float mono_in = samp;
	float input = (mono_in) * gain;
	for(int i=0; i<numcombs; i++)
	{
		mono_out += combL[i].process(input);
	}
	for(int i=0; i<numallpasses; i++)
	{
		mono_out = allpassL[i].process(mono_out);
	}
	samp = mono_in * dry + mono_out * wet1;
	return samp;
}

void revmodel::update() {
	int i;
	wet1 = wet * (width / 2 + 0.5f);

	if (mode >= freezemode) {
		roomsize1 = 1;
		damp1 = 0;
		gain = muted;
	} else {
		roomsize1 = roomsize;
		damp1 = damp;
		gain = fixedgain;
	}

	for (i = 0; i < numcombs; i++) {
		combL[i].setfeedback(roomsize1);
	}

	for (i = 0; i < numcombs; i++) {
		combL[i].setdamp(damp1);
	}
}

void revmodel::setroomsize(float value) {
	roomsize = (value * scaleroom) + offsetroom;
	update();
}

float revmodel::getroomsize() {
	return (roomsize - offsetroom) / scaleroom;
}

void revmodel::setdamp(float value) {
	damp = value * scaledamp;
	update();
}

float revmodel::getdamp() {
	return damp / scaledamp;
}

void revmodel::setwet(float value) {
	wet = value * scalewet;
	update();
}

float revmodel::getwet() {
	return wet / scalewet;
}

void revmodel::setdry(float value) {
	dry = value * scaledry;
}

float revmodel::getdry() {
	return dry / scaledry;
}

void revmodel::setwidth(float value) {
	width = value;
	update();
}

float revmodel::getwidth() {
	return width;
}

void revmodel::setmode(float value) {
	mode = value;
	update();
}

float revmodel::getmode() {
	if (mode >= freezemode)
		return 1;
	else
		return 0;
}
