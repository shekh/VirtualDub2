#ifndef f_AVIOUTPUTPLUGIN_H
#define f_AVIOUTPUTPLUGIN_H

#ifdef _MSC_VER
	#pragma once
#endif

#include "AVIOutput.h"

class IVDMediaOutput;
struct VDPluginDescription;
class IVDXOutputFileDriver;
class VDOutputDriverContextImpl;
class IVDXAudioEnc;
class IVDAudioCodec;
struct VDWaveFormat;

class VDINTERFACE IVDOutputDriver : public IVDRefCount {
public:
	virtual VDPluginDescription* GetDesc() = 0;
	virtual const wchar_t *	GetSignatureName() = 0;
	virtual IVDXOutputFileDriver * GetDriver() = 0;
	virtual VDOutputDriverContextImpl* GetContext() = 0;
	virtual uint32 GetFormatCaps(int i) = 0;
};

class VDINTERFACE IVDAudioEnc : public IVDRefCount {
public:
	virtual const wchar_t *	GetName() = 0;
	virtual const char *	GetSignatureName() = 0;
	virtual IVDXAudioEnc * GetDriver() = 0;
	virtual VDOutputDriverContextImpl* GetContext() = 0;
};

typedef std::vector<vdrefptr<IVDOutputDriver> > tVDOutputDrivers;
typedef std::vector<vdrefptr<IVDAudioEnc> > tVDAudioEncList;

void VDInitOutputDrivers();
void VDShutdownOutputDrivers();
void VDGetOutputDrivers(tVDOutputDrivers& l);
IVDOutputDriver *VDGetOutputDriverByName(const wchar_t *name);
void VDInitAudioEnc();
void VDShutdownAudioEnc();
void VDGetAudioEncList(tVDAudioEncList& l);
IVDAudioEnc *VDGetAudioEncByName(const char *name);

class IVDMediaOutputPlugin : public IVDMediaOutput {
public:
	virtual void setTextInfo(uint32 ckid, const char *text) = 0;
};

IVDMediaOutputPlugin *VDCreateMediaOutputPlugin(IVDOutputDriver* driver, const char* format);
IVDAudioCodec *VDCreateAudioCompressorPlugin(const VDWaveFormat *srcFormat, const char *pSignatureName, vdblock<char>& config, bool throwIfNotFound);

#endif
