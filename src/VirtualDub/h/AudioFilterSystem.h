#ifndef f_AUDIOFILTERSYSTEM_H
#define f_AUDIOFILTERSYSTEM_H

#include <list>
#include <vector>
#include <map>
#include <vd2/system/VDScheduler.h>
#include <vd2/system/VDString.h>
#include <vd2/system/thread.h>
#include <vd2/plugin/vdplugin.h>
#include <vd2/plugin/vdaudiofilt.h>
#include "plugins.h"

struct VDXPluginInfo;
typedef VDXPluginInfo VDPluginInfo;

struct VDPluginDescription;
struct VDAudioFilterDefinition;
class VDAudioFilterInstance;

class IVDAudioFilterInstance {
public:
	virtual const VDPluginInfo *GetPluginInfo() = 0;
	virtual const VDAudioFilterDefinition *GetDefinition() = 0;
	virtual bool Configure(VDGUIHandle hParent) = 0;
	virtual void DeserializeConfig(const VDPluginConfig&) = 0;
	virtual void SerializeConfig(VDPluginConfig&) = 0;
	virtual void SetConfigVal(unsigned idx, const VDPluginConfigVariant& var) = 0;
	virtual void *GetObject() = 0;
	virtual sint64 GetPosition() = 0;
	virtual sint64 GetLength() = 0;

	virtual const VDXWaveFormat *GetOutputPinFormat(int outputPin) = 0;
	virtual bool GetInputPinConnection(unsigned inputPin, IVDAudioFilterInstance*& pFilt, unsigned& outputPin) = 0;
	virtual bool GetOutputPinConnection(unsigned outputPin, IVDAudioFilterInstance*& pFilt, unsigned& inputPin) = 0;
};

struct VDAudioFilterGraph {
	struct FilterEntry {
		VDStringW	mFilterName;
		int			mInputPins;
		int			mOutputPins;
		VDPluginConfig	mConfig;
	};

	struct FilterConnection {
		int		filt;
		int		pin;
	};

	typedef std::list<FilterEntry> FilterList;
	typedef std::vector<FilterConnection> FilterConnectionList;

	FilterList				mFilters;
	FilterConnectionList	mConnections;
};

class VDAudioFilterSystem {
public:
	VDAudioFilterSystem();
	~VDAudioFilterSystem();

	void SetScheduler(VDScheduler *pIOScheduler, VDScheduler *pFastScheduler = NULL);
	VDScheduler *GetIOScheduler() { return mpIOScheduler; }

	IVDAudioFilterInstance *Create(VDPluginDescription *);
	void Destroy(IVDAudioFilterInstance *);

	void Disconnect(IVDAudioFilterInstance *pFilter, unsigned outputPin);
	void Connect(IVDAudioFilterInstance *, unsigned nPinOut, IVDAudioFilterInstance *pFilterIn, unsigned nPinIn);

	void LoadFromGraph(const VDAudioFilterGraph& graph, std::vector<IVDAudioFilterInstance *>& filterPtrs);

	void Clear();
	void Prepare();
	void Start();
	void Stop();

	void Seek(sint64 us);

	IVDAudioFilterInstance *GetClock();

protected:
	void Unprepare(VDAudioFilterInstance *pInst);
	void TryPrepare(VDAudioFilterInstance *pInst);
	void Prepare(VDAudioFilterInstance *pInst, bool mustSucceed);

	VDScheduler		*mpIOScheduler;
	VDScheduler		*mpFastScheduler;

	VDAudioFilterInstance	*mpClock;

	typedef std::list<VDAudioFilterInstance *> tFilterList;
	tFilterList		mFilters;
	tFilterList		mStartedFilters;

	VDCriticalSection	mcsStateChange;

	void SortFilter(tFilterList& newList, VDAudioFilterInstance *pFilt);
	void Sort();
	void Suspend();
	void Resume();
};

void VDAddAudioFilter(const VDAudioFilterDefinition *);

#endif
