#ifndef f_PLUGINS_H
#define f_PLUGINS_H

#include <vd2/system/vdtypes.h>
#include <vd2/system/VDString.h>
#include <vd2/plugin/vdvideofilt.h>
#include <vector>
#include <map>

class VDExternalModule;
struct VDXPluginInfo;
typedef VDXPluginInfo VDPluginInfo;

struct VDPluginDescription {
	VDStringW			mName;
	VDStringW			mAuthor;
	VDStringW			mDescription;
	uint32				mVersion;
	uint32				mType;
	VDExternalModule	*mpModule;
	const VDPluginInfo	*mpInfo;
	const VDPluginInfo	*mpShadowedInfo;
	bool				mbHasStaticAbout;
	bool				mbHasStaticConfigure;
};

struct VDXFilterModule {		// formerly FilterModule
	struct VDXFilterModule *next, *prev;
	VDXHINSTANCE			hInstModule;
	VDXFilterModuleInitProc	initProc;
	FilterModModuleInitProc	filterModInitProc;
	VDXFilterModuleDeinitProc	deinitProc;
};

class VDExternalModule {
public:
	VDExternalModule(const VDStringW& filename);
	~VDExternalModule();

	bool Lock();
	void Unlock();

	bool IsConfigureSupported() const;
	bool IsAboutSupported() const;

	int GetVideoFilterAPIVersion() const { return mVFHighVersion; }

	const VDStringW& GetFilename() const { return mFilename; }
	VDXFilterModule& GetFilterModuleInfo() { return mModuleInfo; }

protected:
	void DisconnectOldPlugins();
	void ReconnectOldPlugins();
	bool ReconnectPlugins();

	VDStringW		mFilename;
	HMODULE			mhModule;
	int				mModuleRefCount;
	int				mVFHighVersion;		// video filter high version (from module init)
	VDXFilterModule	mModuleInfo;
};

void					VDDeinitPluginSystem();

bool					VDAddPluginModule(const wchar_t *pFilename);
void					VDAddInternalPlugins(const VDPluginInfo *const *ppInfo);

VDExternalModule *		VDGetExternalModuleByFilterModule(const VDXFilterModule *);

VDPluginDescription *	VDGetPluginDescription(const wchar_t *pName, uint32 mType);
void					VDEnumeratePluginDescriptions(std::vector<VDPluginDescription *>& plugins, uint32 type);

void					VDLoadPlugins(const VDStringW& path, int& succeeded, int& failed);
const VDPluginInfo *	VDLockPlugin(VDPluginDescription *pDesc);
void					VDUnlockPlugin(VDPluginDescription *pDesc);

void					VDConnectPluginDescription(const VDPluginInfo *pInfo, VDExternalModule *pModule);

class VDPluginPtr {
public:
	VDPluginPtr() : mpDesc(NULL) {}
	VDPluginPtr(VDPluginDescription *pDesc);
	VDPluginPtr(const VDPluginPtr& src);
	~VDPluginPtr();

	VDPluginPtr& operator=(const VDPluginPtr& src);
	VDPluginPtr& operator=(VDPluginDescription *pDesc);

	bool operator!() const { return !mpDesc; }
	VDPluginDescription *operator->() const { return mpDesc; }
	VDPluginDescription& operator*() const { return *mpDesc; }

protected:
	VDPluginDescription *mpDesc;
};

union VDPluginConfigVariantData {
	uint32	vu32;
	sint32	vs32;
	uint64	vu64;
	sint64	vs64;
	double	vfd;
	struct NarrowString {
		char *s;
	} vsa;
	struct WideString {
		wchar_t *s;
	} vsw;
	struct Block {
		uint32 len;
		char *s;
	} vb;
};

class VDPluginConfigVariant {
public:
	enum {
		kTypeInvalid	= 0,
		kTypeU32		= 1,
		kTypeS32,
		kTypeU64,
		kTypeS64,
		kTypeDouble,
		kTypeAStr,
		kTypeWStr,
		kTypeBlock
	};

	VDPluginConfigVariant() : mType(kTypeInvalid) {}
	VDPluginConfigVariant(const VDPluginConfigVariant&);
	~VDPluginConfigVariant();

	VDPluginConfigVariant& operator=(const VDPluginConfigVariant&);

	unsigned GetType() const { return mType; }

	void Clear();

	void SetU32(uint32 v) { Clear(); mType = kTypeU32; mData.vu32 = v; }
	void SetS32(sint32 v) { Clear(); mType = kTypeS32; mData.vs32 = v; }
	void SetU64(uint64 v) { Clear(); mType = kTypeU64; mData.vu64 = v; }
	void SetS64(sint64 v) { Clear(); mType = kTypeS64; mData.vs64 = v; }
	void SetDouble(double v) { Clear(); mType = kTypeDouble; mData.vfd = v; }
	void SetAStr(const char *s);
	void SetWStr(const wchar_t *s);
	void SetBlock(const void *s, unsigned b);

	const uint32& GetU32() const { return mData.vu32; }
	const sint32& GetS32() const { return mData.vs32; }
	const uint64& GetU64() const { return mData.vu64; }
	const sint64& GetS64() const { return mData.vs64; }
	const double& GetDouble() const { return mData.vfd; }
	const char *GetAStr() const { return mData.vsa.s; }
	const wchar_t *GetWStr() const { return mData.vsw.s; }
	const void *GetBlockPtr() const { return mData.vb.s; }
	const unsigned GetBlockLen() const { return mData.vb.len; }

protected:
	unsigned mType;

	VDPluginConfigVariantData	mData;
};

typedef std::map<unsigned, VDPluginConfigVariant> VDPluginConfig;

#endif
