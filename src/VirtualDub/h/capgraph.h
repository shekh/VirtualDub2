#ifndef f_CAPGRAPH_H
#define f_CAPGRAPH_H

#include <vd2/system/vdtypes.h>

class IVDCaptureStatsCollector;

class VDINTERFACE IVDUICaptureGraph {
public:
	enum { kTypeID = 'cpgr' };

	virtual IVDCaptureProfiler *AsICaptureProfiler() = 0;
	virtual void Clear() = 0;
};

#endif