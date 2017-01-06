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

class VDINTERFACE IVDOutputDriver : public IVDRefCount {
public:
	virtual const wchar_t *	GetSignatureName() = 0;
	virtual IVDXOutputFileDriver * GetDriver() = 0;
  virtual VDOutputDriverContextImpl* GetContext() = 0;
};

typedef std::vector<vdrefptr<IVDOutputDriver> > tVDOutputDrivers;

void VDInitOutputDrivers();
void VDShutdownOutputDrivers();
void VDGetOutputDrivers(tVDOutputDrivers& l);
IVDOutputDriver *VDGetOutputDriverByName(const wchar_t *name);

class IVDMediaOutputPlugin : public IVDMediaOutput {
public:
	virtual void setTextInfo(uint32 ckid, const char *text) = 0;
};

IVDMediaOutputPlugin *VDCreateMediaOutputPlugin(IVDOutputDriver* driver, const char* format);

#endif
