#include "stdafx.h"

#include <windows.h>
#include <vd2/plugin/vdplugin.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/system/debug.h>
#include <vd2/Riza/audiocodec.h>
#include <vd2/plugin/vdinputdriver.h>
#include "AVIOutputPlugin.h"
#include "plugins.h"
#include "project.h"

extern VDProject *g_project;
static tVDOutputDrivers g_VDOutputDrivers;
static tVDAudioEncList g_VDAudioEnc;

///////////////////////////////////////////////////////////////////////////

class VDOutputDriverContextImpl : public VDXInputDriverContext, public vdrefcounted<IVDPluginCallbacks> {
public:
	VDOutputDriverContextImpl(const VDPluginDescription *);
	~VDOutputDriverContextImpl(){}

	void BeginExternalCall();
	void EndExternalCall();

public:
	virtual void * VDXAPIENTRY GetExtendedAPI(const char *pExtendedAPIName);
	virtual void VDXAPIENTRYV SetError(const char *format, ...);
	virtual void VDXAPIENTRY SetErrorOutOfMemory();
	virtual uint32 VDXAPIENTRY GetCPUFeatureFlags();

	VDStringW	mName;
	MyError		mError;
};

VDOutputDriverContextImpl::VDOutputDriverContextImpl(const VDPluginDescription *pInfo) {
	mName.sprintf(L"Output driver plugin \"%S\"", pInfo->mName.c_str());
	mpCallbacks = this;
}

void VDOutputDriverContextImpl::BeginExternalCall() {
	mError.clear();
}

void VDOutputDriverContextImpl::EndExternalCall() {
	if (mError.gets()) {
		MyError tmp;
		tmp.TransferFrom(mError);
		throw tmp;
	}
}

void * VDXAPIENTRY VDOutputDriverContextImpl::GetExtendedAPI(const char *pExtendedAPIName) {
	if (strcmp(pExtendedAPIName,"IFilterModPixmap")==0)
		return &g_project->filterModPixmap;
	return NULL;
}

void VDXAPIENTRYV VDOutputDriverContextImpl::SetError(const char *format, ...) {
	va_list val;
	va_start(val, format);
	mError.vsetf(format, val);
	va_end(val);
}

void VDXAPIENTRY VDOutputDriverContextImpl::SetErrorOutOfMemory() {
	MyMemoryError e;
	mError.TransferFrom(e);
}

uint32 VDXAPIENTRY VDOutputDriverContextImpl::GetCPUFeatureFlags() {
	return CPUGetEnabledExtensions();
}

#define vdwithoutputplugin(context) switch(VDExternalCodeBracket _exbracket = ((context)->BeginExternalCall(), VDExternalCodeBracketLocation((context)->mName.c_str(), __FILE__, __LINE__))) while((context)->EndExternalCall(), false) case false: default:

class AVIOutputPluginStream;

class AVIOutputPlugin : public IVDMediaOutputPlugin {
private:
	bool		mbInitialized;
	struct StreamInfo {
		AVIOutputPluginStream	*mpStream;
		bool	mbIsVideo;

		StreamInfo();
		~StreamInfo();
	};

	typedef std::list<StreamInfo> tStreams;
	tStreams mStreams;

	IVDMediaOutputStream	*mpFirstVideoStream;
	IVDMediaOutputStream	*mpFirstAudioStream;

	typedef std::map<uint32, VDStringA> tTextInfo;
	tTextInfo	mTextInfo;
	vdrefptr<IVDOutputDriver> driver;
	VDString format;

public:
	AVIOutputPlugin(IVDOutputDriver* driver, const char* format);
	virtual ~AVIOutputPlugin();

	void *AsInterface(uint32 id);

	IVDMediaOutput *AsIMediaOutput() { return this; }

	void setTextInfo(uint32 ckid, const char *text);

	IVDMediaOutputStream *createAudioStream();
	IVDMediaOutputStream *createVideoStream();
	IVDMediaOutputStream *getAudioOutput() { return mpFirstAudioStream; }
	IVDMediaOutputStream *getVideoOutput() { return mpFirstVideoStream; }

	bool init(const wchar_t *szFile);

	void partialWriteChunkBegin(int nStream, uint32 flags, uint32 cbBuffer){}
	void partialWriteChunk(int nStream, const void *pBuffer, uint32 cbBuffer){}
	void partialWriteChunkEnd(int nStream){}

	void finalize();

	vdrefptr<IVDXOutputFile> outFile;
	VDOutputDriverContextImpl* mpContext;
};

class AVIOutputPluginStream : public AVIOutputStream {
public:
	AVIOutputPluginStream(AVIOutputPlugin *pParent, int nStream);

	void write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 samples);
	void write(const void *pBuffer, uint32 cbBuffer, IVDXOutputFile::PacketInfo& info);
	void partialWriteBegin(uint32 flags, uint32 bytes, uint32 samples);
	void partialWrite(const void *pBuffer, uint32 cbBuffer);
	void partialWriteEnd();
	bool isAVIFile(){ return false; }

	uint32 plugin_id;

protected:
	AVIOutputPlugin *const mpParent;
	const int mStream;
};

AVIOutputPluginStream::AVIOutputPluginStream(AVIOutputPlugin *pParent, int nStream)
	: mpParent(pParent)
	, mStream(nStream)
{
}

void AVIOutputPluginStream::write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 samples) {
	IVDXOutputFile::PacketInfo info;
	info.flags = flags;
	info.samples = samples;
	write(pBuffer,cbBuffer,info);
}

void AVIOutputPluginStream::write(const void *pBuffer, uint32 cbBuffer, IVDXOutputFile::PacketInfo& info) {
	vdwithoutputplugin(mpParent->mpContext) {
		mpParent->outFile->Write(plugin_id,pBuffer,cbBuffer,info);
	}
	/*
	partialWriteBegin(flags, cbBuffer, samples);
	partialWrite(pBuffer, cbBuffer);
	partialWriteEnd();
	*/
}

void AVIOutputPluginStream::partialWriteBegin(uint32 flags, uint32 bytes, uint32 samples) {
	mpParent->partialWriteChunkBegin(mStream, flags, bytes);
}

void AVIOutputPluginStream::partialWrite(const void *pBuffer, uint32 cbBuffer) {
	mpParent->partialWriteChunk(mStream, pBuffer, cbBuffer);
}

void AVIOutputPluginStream::partialWriteEnd() {
	mpParent->partialWriteChunkEnd(mStream);
}

IVDMediaOutputPlugin *VDCreateMediaOutputPlugin(IVDOutputDriver* driver, const char* format) {
	return new AVIOutputPlugin(driver,format);
}

AVIOutputPlugin::AVIOutputPlugin(IVDOutputDriver* driver, const char* format)
{
	mpFirstVideoStream = 0;
	mpFirstAudioStream = 0;
	mbInitialized = false;
	this->driver = driver;
	this->format = format;
	mpContext = driver->GetContext();
	vdwithoutputplugin(mpContext) {
		driver->GetDriver()->CreateOutputFile(~outFile);
	}
}

AVIOutputPlugin::~AVIOutputPlugin() {
}

AVIOutputPlugin::StreamInfo::StreamInfo() {
	mpStream = 0;
}

AVIOutputPlugin::StreamInfo::~StreamInfo() {
	delete mpStream;
}

void *AVIOutputPlugin::AsInterface(uint32 id) {
	if (id == IVDMediaOutput::kTypeID)
		return static_cast<IVDMediaOutput *>(this);

	return NULL;
}

IVDMediaOutputStream *AVIOutputPlugin::createVideoStream() {
	mStreams.resize(mStreams.size() + 1);
	StreamInfo& stream = mStreams.back();

	if (!(stream.mpStream = new_nothrow AVIOutputPluginStream(this, mStreams.size() - 1)))
		throw MyMemoryError();

	stream.mbIsVideo = true;

	if (!mpFirstVideoStream)
		mpFirstVideoStream = stream.mpStream;

	return stream.mpStream;
}

IVDMediaOutputStream *AVIOutputPlugin::createAudioStream() {
	mStreams.resize(mStreams.size() + 1);
	StreamInfo& stream = mStreams.back();

	if (!(stream.mpStream = new_nothrow AVIOutputPluginStream(this, mStreams.size() - 1)))
		throw MyMemoryError();

	stream.mbIsVideo = false;

	if (!mpFirstAudioStream)
		mpFirstAudioStream = stream.mpStream;

	return stream.mpStream;
}

void AVIOutputPlugin::setTextInfo(uint32 ckid, const char *text) {
	mTextInfo[ckid] = text;
}

bool AVIOutputPlugin::init(const wchar_t *szFile) {
	VDASSERT(!mStreams.empty());
	vdwithoutputplugin(mpContext) {
		outFile->Init(szFile,format.c_str());
	}

	tStreams::iterator it(mStreams.begin()), itEnd(mStreams.end());

	for(; it != itEnd; ++it) {
		StreamInfo& stream = *it;
		AVIOutputPluginStream* s = stream.mpStream;

		if(stream.mbIsVideo){
			vdwithoutputplugin(mpContext) {
				s->plugin_id = outFile->CreateStream(kVDXST_Video);
				outFile->SetVideo(s->plugin_id,s->getStreamInfo(),s->getFormat(),s->getFormatLen());
			}
		} else {
			vdwithoutputplugin(mpContext) {
				s->plugin_id = outFile->CreateStream(kVDXST_Audio);
				outFile->SetAudio(s->plugin_id,s->getStreamInfo(),s->getFormat(),s->getFormatLen());
			}
		}
	}

	mbInitialized = true;
	return true;
}

void AVIOutputPlugin::finalize() {
	VDDEBUG("AVIOutputPlugin: Beginning finalize.\n");
	vdwithoutputplugin(mpContext) {
		outFile->Finalize();
	}
	VDDEBUG("AVIOutputPlugin: Finalize successful.\n");
}

class VDOutputDriverPlugin : public vdrefcounted<IVDOutputDriver> {
public:
	VDOutputDriverPlugin(VDPluginDescription *pDesc);
	~VDOutputDriverPlugin();

	const wchar_t *	GetSignatureName() {
		return mpDef->mpDriverTagName;
	}

	IVDXOutputFileDriver * GetDriver() {
		return plugin;
	}

	VDOutputDriverContextImpl* GetContext() {
		return &context;
	}

	VDPluginDescription *mpDesc;
	const VDPluginInfo *info;
	VDXOutputDriverDefinition *mpDef;
	vdrefptr<IVDXOutputFileDriver> plugin;
	VDOutputDriverContextImpl context;
};

VDOutputDriverPlugin::VDOutputDriverPlugin(VDPluginDescription *desc)
	:context(desc)
{
	mpDesc = desc;
	info = VDLockPlugin(desc);
	mpDef = (VDXOutputDriverDefinition*)info->mpTypeSpecificInfo;
	mpDef->mpCreate(&context, ~plugin);
}

VDOutputDriverPlugin::~VDOutputDriverPlugin() {
	plugin = 0;
	VDUnlockPlugin(mpDesc);
}

void VDInitOutputDrivers() {
	g_VDOutputDrivers.clear();

	std::vector<VDPluginDescription *> plugins;
	VDEnumeratePluginDescriptions(plugins, kVDXPluginType_Output);

	while(!plugins.empty()) {
		VDPluginDescription *desc = plugins.back();
		g_VDOutputDrivers.push_back(vdrefptr<IVDOutputDriver>(new VDOutputDriverPlugin(desc)));
		plugins.pop_back();
	}
}

void VDShutdownOutputDrivers() {
	g_VDOutputDrivers.clear();
}

void VDGetOutputDrivers(tVDOutputDrivers& l) {
	for(tVDOutputDrivers::const_iterator it(g_VDOutputDrivers.begin()), itEnd(g_VDOutputDrivers.end()); it!=itEnd; ++it) {
		l.push_back(*it);
	}
}

IVDOutputDriver *VDGetOutputDriverByName(const wchar_t *name) {
	for(tVDOutputDrivers::const_iterator it(g_VDOutputDrivers.begin()), itEnd(g_VDOutputDrivers.end()); it!=itEnd; ++it) {
		IVDOutputDriver *pDriver = *it;

		const wchar_t *dvname = pDriver->GetSignatureName();

		if (dvname && !_wcsicmp(name, dvname))
			return pDriver;
	}

	return NULL;
}

//----------------------------------------------------------------------------------------------

class VDAudioPluginCodec : public IVDAudioCodec {
public:
	VDPluginDescription *mpDesc;
	vdrefptr<IVDXAudioEnc> plugin;
	VDOutputDriverContextImpl context;

	VDAudioPluginCodec(VDPluginDescription *pDesc);
	~VDAudioPluginCodec();

	IVDXAudioEnc * GetDriver() {
		return plugin;
	}

	virtual void Shutdown(){}

	virtual bool IsEnded() const { return false; }

	virtual unsigned	GetInputLevel() const {
		return plugin->GetInputLevel(); 
	}
	virtual unsigned	GetInputSpace() const { 
		return plugin->GetInputSpace();
	}
	virtual unsigned	GetOutputLevel() const {
		return plugin->GetOutputLevel();
	}
	virtual const VDWaveFormat *GetOutputFormat() const {
		return (VDWaveFormat*)plugin->GetOutputFormat();
	}
	virtual unsigned	GetOutputFormatSize() const {
		return plugin->GetOutputFormatSize();
	}

	virtual void		Restart(){}
	virtual bool		Convert(bool flush, bool requireOutput){
		return plugin->Convert(flush,requireOutput);
	}

	virtual void*   LockInputBuffer(unsigned& bytes){
		return plugin->LockInputBuffer(bytes);
	}
	virtual void		UnlockInputBuffer(unsigned bytes){
		plugin->UnlockInputBuffer(bytes);
	}
	virtual const void* LockOutputBuffer(unsigned& bytes){
		return plugin->LockInputBuffer(bytes);
	}
	virtual void		UnlockOutputBuffer(unsigned bytes){
		plugin->UnlockOutputBuffer(bytes);
	}
	virtual unsigned	CopyOutput(void *dst, unsigned bytes){
		sint64 duration;
		return plugin->CopyOutput(dst,bytes,duration);
	}
	virtual unsigned	CopyOutput(void *dst, unsigned bytes, sint64& duration){
		return plugin->CopyOutput(dst,bytes,duration);
	}
};

VDAudioPluginCodec::VDAudioPluginCodec(VDPluginDescription *desc)
	:context(desc)
{
	mpDesc = desc;
	const VDPluginInfo* info = VDLockPlugin(desc);
	VDXAudioEncDefinition* mpDef = (VDXAudioEncDefinition*)info->mpTypeSpecificInfo;
	mpDef->mpCreate(&context, ~plugin);
}

VDAudioPluginCodec::~VDAudioPluginCodec() {
	plugin = 0;
	VDUnlockPlugin(mpDesc);
}

class VDAudioEncPlugin : public vdrefcounted<IVDAudioEnc> {
public:
	VDAudioEncPlugin(VDPluginDescription *pDesc);
	~VDAudioEncPlugin();

	const wchar_t *	GetName() {
		return mpDef->mpDriverName;
	}

	const char *	GetSignatureName() {
		return mpDef->mpDriverTagName;
	}

	IVDXAudioEnc * GetDriver() {
		return plugin;
	}

	VDOutputDriverContextImpl* GetContext() {
		return &context;
	}

	VDAudioPluginCodec* CreateCodec() {
		VDAudioPluginCodec* codec = new VDAudioPluginCodec(mpDesc);
		return codec;
	}

	VDPluginDescription *mpDesc;
	const VDPluginInfo *info;
	VDXAudioEncDefinition *mpDef;
	vdrefptr<IVDXAudioEnc> plugin;
	VDOutputDriverContextImpl context;
};

VDAudioEncPlugin::VDAudioEncPlugin(VDPluginDescription *desc)
	:context(desc)
{
	mpDesc = desc;
	info = VDLockPlugin(desc);
	mpDef = (VDXAudioEncDefinition*)info->mpTypeSpecificInfo;
	mpDef->mpCreate(&context, ~plugin);
}

VDAudioEncPlugin::~VDAudioEncPlugin() {
	plugin = 0;
	VDUnlockPlugin(mpDesc);
}

IVDAudioCodec *VDCreateAudioCompressorPlugin(const VDWaveFormat *srcFormat, const char *pSignatureName, vdblock<char>& config) {
	VDAudioEncPlugin* driver = (VDAudioEncPlugin*)VDGetAudioEncByName(pSignatureName);
	if (!driver) {
		throw MyError(
			"Error initializing audio stream compression:\n"
			"Driver not found (%s).",
			pSignatureName
		);
	}

	VDAudioPluginCodec* codec = driver->CreateCodec();
	codec->GetDriver()->SetConfig(config.data(),config.size());
	codec->GetDriver()->SetInputFormat((VDXWAVEFORMATEX*)srcFormat);
	return codec;
}

void VDInitAudioEnc() {
	g_VDAudioEnc.clear();

	std::vector<VDPluginDescription *> plugins;
	VDEnumeratePluginDescriptions(plugins, kVDXPluginType_AudioEnc);

	while(!plugins.empty()) {
		VDPluginDescription *desc = plugins.back();
		g_VDAudioEnc.push_back(vdrefptr<IVDAudioEnc>(new VDAudioEncPlugin(desc)));
		plugins.pop_back();
	}
}

void VDShutdownAudioEnc() {
	g_VDAudioEnc.clear();
}

void VDGetAudioEncList(tVDAudioEncList& l) {
	for(tVDAudioEncList::const_iterator it(g_VDAudioEnc.begin()), itEnd(g_VDAudioEnc.end()); it!=itEnd; ++it) {
		l.push_back(*it);
	}
}

IVDAudioEnc *VDGetAudioEncByName(const char *name) {
	for(tVDAudioEncList::const_iterator it(g_VDAudioEnc.begin()), itEnd(g_VDAudioEnc.end()); it!=itEnd; ++it) {
		IVDAudioEnc *pDriver = *it;

		const char *dvname = pDriver->GetSignatureName();

		if (dvname && !_stricmp(name, dvname))
			return pDriver;
	}

	return NULL;
}

