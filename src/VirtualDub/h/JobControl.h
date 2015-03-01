#ifndef f_VD2_JOBCONTROL_H
#define f_VD2_JOBCONTROL_H

#ifdef _MSC_VER
	#pragma once
#endif

#include <vd2/system/vdstl.h>
#include <vd2/system/filewatcher.h>
#include <vd2/system/log.h>
#include <vd2/system/time.h>
#include <vd2/VDLib/Job.h>

class VDJob;
class IVDStream;

enum VDJobQueueStatus {
	kVDJQS_Idle,
	kVDJQS_Running,
	kVDJQS_Blocked
};

class IVDJobQueueStatusCallback {
public:
	virtual void OnJobQueueStatusChanged(VDJobQueueStatus status) = 0;
	virtual void OnJobAdded(const VDJob& job, int index) = 0;
	virtual void OnJobRemoved(const VDJob& job, int index) = 0;
	virtual void OnJobUpdated(const VDJob& job, int index) = 0;
	virtual void OnJobStarted(const VDJob& job) = 0;
	virtual void OnJobEnded(const VDJob& job) = 0;
	virtual void OnJobProgressUpdate(const VDJob& job, float completion) = 0;
	virtual void OnJobQueueReloaded() = 0;
};

class VDJobQueue : protected IVDFileWatcherCallback, protected IVDTimerCallback, public IVDJobQueue {
	VDJobQueue(const VDJobQueue&);
	VDJobQueue& operator=(const VDJobQueue&);
public:
	VDJobQueue();
	~VDJobQueue();

	void Shutdown();

	void SetJobFilePath(const wchar_t *path, bool enableDistributedMode, bool enableAutoUpdate);
	void SetDefaultJobFilePath(const wchar_t *path);
	const wchar_t *GetJobFilePath() const;
	const wchar_t *GetDefaultJobFilePath() const;

	int32 GetJobIndexById(uint64 id) const;
	VDJob *GetJobById(uint64 id) const;
	VDJob *ListGet(int index);
	int ListFind(VDJob *vdj_find);
	long ListSize();
	void ListClear(bool force_no_update = false);

	void Refresh(VDJob *job);
	void Add(VDJob *job, bool force_no_update);
	void Delete(VDJob *job, bool force_no_update);
	void Run(VDJob *job);
	void Reload(VDJob *job);
	void Transform(int fromState, int toState);

	void ListLoad(const wchar_t *lpszName, bool skipIfSignatureSame);

	bool IsModified() {
		return mbModified;
	}

	void SetModified();

	bool Flush(const wchar_t *lpfn =NULL);

	void RunAllStart();
	bool RunAllNext();
	void RunAllStop();

	void Swap(int x, int y);

	bool IsLocal(const VDJob *job) const;

	bool IsRunInProgress() const {
		return mbRunning;
	}

	bool IsRunAllInProgress() const {
		return mbRunAll;
	}

	VDJobQueueStatus GetQueueStatus() const;

	int GetPendingJobCount() const;

	VDJob *GetCurrentlyRunningJob() {
		return mpRunningJob;
	}

	uint64 GetUniqueId();

	const char *GetRunnerName() const;
	uint64 GetRunnerId() const;

	bool IsAutoUpdateEnabled() const;
	void SetAutoUpdateEnabled(bool enabled);

	bool PollAutoRun();
	bool IsAutoRunEnabled() const;
	void SetAutoRunEnabled(bool autorun);

	void SetBlocked(bool blocked);
	void SetCallback(IVDJobQueueStatusCallback *cb);

protected:
	typedef vdfastvector<VDJob *> JobQueue;

	bool Load(IVDStream *stream, bool skipIfSignatureSame);
	void Save(IVDStream *stream, uint64 signature, uint32 revision, bool resetJobRevisions);

	void NotifyStatus();
	uint64 CreateListSignature();
	bool OnFileUpdated(const wchar_t *path);
	void TimerCallback();

	JobQueue mJobQueue;

	uint32	mJobCount;
	uint32	mJobNumber;
	VDJob	*mpRunningJob;
	bool	mbRunning;
	bool	mbRunAll;
	bool	mbRunAllStop;
	bool	mbModified;
	bool	mbBlocked;
	bool	mbOrderModified;
	bool	mbAutoRun;
	bool	mbDistributedMode;

	uint64	mJobIdToRun;

	VDStringA	mComputerName;
	uint64	mBaseSignature;
	uint64	mRunnerId;
	uint64	mLastSignature;
	uint32	mLastRevision;

	VDStringW		mJobFilePath;
	VDStringW		mDefaultJobFilePath;

	VDFileWatcher	mFileWatcher;
	VDLazyTimer		mFlushTimer;

	struct RetryTimer : public IVDTimerCallback {
		VDLazyTimer	mTimer;
		bool		mbRetryOK;
		uint32		mLastPeriod;

		RetryTimer() : mbRetryOK(true), mLastPeriod(0) {}

		void TimerCallback();
	} mRetryTimer;
};

#endif
