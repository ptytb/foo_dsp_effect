#ifndef		DSP_GUIDS_H
#define		DSP_GUIDS_H

// {E6509452-690A-4aa6-B32A-EE29BC2D5FE5}
static const GUID guid_dynamics =
{ 0xe6509452, 0x690a, 0x4aa6,{ 0xb3, 0x2a, 0xee, 0x29, 0xbc, 0x2d, 0x5f, 0xe5 } };
static const GUID guid_echo = { 0xc2794c27, 0x2091, 0x460a,{ 0xa7, 0x5c, 0x1, 0x6, 0xc6, 0x6b, 0xa7, 0x96 } };
static const GUID guid_freeverb = { 0x97c60d5f, 0x3572, 0x4d35,{ 0x92, 0x60, 0xfd, 0xc, 0xf5, 0xdb, 0xa4, 0x80 } };
static const GUID guid_iir = { 0xfea092a6, 0xea54, 0x4f62,{ 0xb1, 0x80, 0x4c, 0x88, 0xb9, 0xeb, 0x2b, 0x67 } };
static const GUID guid_phaser = { 0x8b54d803, 0xefea, 0x4b6c,{ 0xb3, 0xbe, 0x92, 0x1f, 0x6a, 0xdc, 0x72, 0x21 } };
static const GUID guid_pitch ={ 0xa7fba855, 0x56d4, 0x46ac,{ 0x81, 0x16, 0x8b, 0x2a, 0x8d, 0xf2, 0xfb, 0x34 } };
static const GUID guid_tempo = { 0x44bcaca2, 0x9edd, 0x493a,{ 0xbb, 0x8f, 0x94, 0x74, 0xf4, 0xb5, 0xa7, 0x6b } };
static const GUID guid_pbrate ={ 0x8c12d81e, 0xbb88, 0x4056,{ 0xb4, 0xc0, 0xea, 0xfa, 0x4e, 0x9f, 0x3b, 0x95 } };
static const GUID guid_wahwah ={ 0x3e144cfa, 0xc63a, 0x4c12,{ 0x95, 0x3, 0xa5, 0xc8, 0x3c, 0x7e, 0x5c, 0xf8 } };

void DynamicsMainMenuWindow();
void EchoMainMenuWindow();
void IIRMainMenuWindow();
void PhaserMainMenuWindow();
void PitchMainMenuWindow();
void ReverbMainMenuWindow();
void WahMainMenuWindow();
#endif