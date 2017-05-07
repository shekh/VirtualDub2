#ifndef CAPVUMETER_H
#define CAPVUMETER_H

#include <vd2/system/vdtypes.h>

void VDComputeWavePeaks(const void *p, unsigned depth, unsigned channels, unsigned count, float* peak);

class VDINTERFACE IVDUICaptureVumeter {
public:
	enum { kTypeID = 'cpvu' };

	virtual void SetPeakLevels(int count, float* peak, int mask) = 0;
};

#endif