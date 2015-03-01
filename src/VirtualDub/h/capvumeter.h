#ifndef CAPVUMETER_H
#define CAPVUMETER_H

#include <vd2/system/vdtypes.h>

void VDComputeWavePeaks(const void *p, unsigned depth, unsigned channels, unsigned count, float& l, float& r);

class VDINTERFACE IVDUICaptureVumeter {
public:
	enum { kTypeID = 'cpvu' };

	virtual void SetPeakLevels(float l, float r) = 0;
};

#endif