#ifndef f_VD2_VDLIB_JOB_H
#define f_VD2_VDLIB_JOB_H

#include <vd2/system/VDString.h>
#include <vd2/system/log.h>

class VDJob;

class IVDJobQueue {
public:
	virtual bool IsLocal(const VDJob *job) const = 0;
	virtual void Refresh(VDJob *job) = 0;
	virtual void Run(VDJob *job) = 0;
	virtual void Reload(VDJob *job) = 0;
};

class VDJob {
public:
	enum {
		kStateWaiting		= 0,
		kStateInProgress	= 1,
		kStateCompleted		= 2,
		kStatePostponed		= 3,
		kStateAborted		= 4,
		kStateError			= 5,
		kStateAborting		= 6,
		kStateStarting		= 7,
		kStateCount			= 8
	};

	IVDJobQueue	*mpJobQueue;

	uint32		mCreationRevision;
	uint32		mChangeRevision;
	uint64		mId;
	uint64		mDateStart;		///< Same units as NT FILETIME.
	uint64		mDateEnd;		///< Same units as NT FILETIME.

	typedef VDAutoLogger::tEntries tLogEntries;
	tLogEntries	mLogEntries;

	/////
	VDJob();
	~VDJob();

	bool operator==(const VDJob& job) const;

	bool	IsLocal() const { return !mpJobQueue || mpJobQueue->IsLocal(this); }

	const char *	GetName() const				{ return mName.c_str(); }
	void			SetName(const char *name)	{ mName = name; }

	const char *	GetInputFile() const			{ return mInputFile.c_str(); }
	void			SetInputFile(const char *file)	{ mInputFile = file; }
	void			SetInputFile(const wchar_t *file);

	const char *	GetOutputFile() const			{ return mOutputFile.c_str(); }
	void			SetOutputFile(const char *file)	{ mOutputFile = file; }
	void			SetOutputFile(const wchar_t *file);

	const char *	GetError() const				{ return mError.c_str(); }
	void			SetError(const char *err)		{ mError = err; }

	const char *	GetRunnerName() const			{ return mRunnerName.c_str(); }
	uint64			GetRunnerId() const				{ return mRunnerId; }

	bool	IsRunning() const { return mState == kStateInProgress || mState == kStateAborting; }
	bool	IsModified() const { return mbModified; }
	void	SetModified(bool mod) { mbModified = mod; }

	int		GetState() const { return mState; }
	void	SetState(int state);

	uint64	GetId() const { return mId; }

	void	SetRunner(uint64 id, const char *name);

	bool	IsReloadMarkerPresent() const { return mbContainsReloadMarker; }

	void	SetScript(const void *script, uint32 len, bool reloadable);
	const char *GetScript() const { return mScript.c_str(); }

	void Refresh();
	void Run();
	void Reload();

	bool Merge(const VDJob& src);

	VDStringA	ToString() const;

	uint64		mRunnerId;
	VDStringA	mRunnerName;
protected:
	VDStringA	mName;
	VDStringA	mInputFile;
	VDStringA	mOutputFile;
	VDStringA	mError;
	VDStringA	mScript;
	int			mState;
	bool		mbContainsReloadMarker;
	bool		mbModified;
};

#endif
