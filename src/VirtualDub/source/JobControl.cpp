#include "stdafx.h"
#include "JobControl.h"
#include <vd2/system/filesys.h>
#include <vd2/system/file.h>
#include <vd2/system/hash.h>
#include <vd2/system/registry.h>
#include <vd2/system/thread.h>
#include <vd2/system/time.h>
#include <vd2/system/w32assist.h>
#include <vd2/VDLib/Job.h>
#include <hash_map>

#include "misc.h"
#include "oshelper.h"
#include "script.h"
#include "dub.h"
#include "project.h"
#include "command.h"

extern const char g_szError[];
extern HWND g_hWnd;
extern VDProject *g_project;
extern bool g_exitOnDone;

HWND g_hwndJobs;

bool g_fJobMode;

VDJobQueue g_VDJobQueue;

static const char g_szRegKeyShutdownWhenFinished[] = "Shutdown after jobs finish";
static const char g_szRegKeyShutdownMode[] = "Shutdown mode";

IVDJobQueueStatusCallback *g_pVDJobQueueStatusCallback;

void JobPositionCallback(VDPosition start, VDPosition cur, VDPosition end, int progress, bool fast_update, void *cookie) {
	if (g_pVDJobQueueStatusCallback && !fast_update)
		g_pVDJobQueueStatusCallback->OnJobProgressUpdate(*g_VDJobQueue.GetCurrentlyRunningJob(), (float)progress / 8192.0f);
}


bool VDUIRequestSystemShutdown(VDGUIHandle hParent);

///////////////////////////////////////////////////////////////////////////////

VDJobQueue::VDJobQueue() 
	: mJobCount(0)
	, mJobNumber(1)
	, mpRunningJob(NULL)
	, mbRunning(false)
	, mbRunAll(false)
	, mbRunAllStop(false)
	, mbModified(false)
	, mbBlocked(false)
	, mbOrderModified(false)
	, mbAutoRun(false)
	, mbDistributedMode(false)
	, mJobIdToRun(0)
	, mLastSignature(0)
	, mLastRevision(0)
{
	char name[MAX_COMPUTERNAME_LENGTH + 1];
	DWORD len = MAX_COMPUTERNAME_LENGTH + 1;

	mComputerName = "unnamed";

	mRunnerId = (uint32)GetCurrentProcessId();
	mBaseSignature = (uint64)mRunnerId << 32;
	if (GetComputerName(name, &len)) {
		mComputerName = name;

		uint64 computerHash = (uint64)VDHashString32(name) << 32;
		mBaseSignature ^= computerHash;
		mRunnerId += computerHash;
	}
}

VDJobQueue::~VDJobQueue() {
	Shutdown();
}

void VDJobQueue::Shutdown() {
	mpRunningJob = NULL;

	while(!mJobQueue.empty()) {
		delete mJobQueue.back();
		mJobQueue.pop_back();
	}

	mJobCount = 0;
}

const wchar_t *VDJobQueue::GetJobFilePath() const {
	return mJobFilePath.c_str();
}

const wchar_t *VDJobQueue::GetDefaultJobFilePath() const {
	return mDefaultJobFilePath.c_str();
}

void VDJobQueue::SetJobFilePath(const wchar_t *path, bool enableDistributedMode, bool enableAutoUpdate) {
	if (mpRunningJob) {
		VDASSERT(!"Can't change job file path while job is running.");
		return;
	}

	SetAutoUpdateEnabled(false);

	Shutdown();

	if (path)
		mJobFilePath = path;
	else
		mJobFilePath = mDefaultJobFilePath;

	SetAutoUpdateEnabled(enableAutoUpdate);
	mLastSignature = 0;
	mLastRevision = 0;
	mbDistributedMode = enableDistributedMode;

	ListLoad(NULL, false);

	if (g_pVDJobQueueStatusCallback) {
		g_pVDJobQueueStatusCallback->OnJobQueueReloaded();
		g_pVDJobQueueStatusCallback->OnJobQueueStatusChanged(GetQueueStatus());
	}
}

void VDJobQueue::SetDefaultJobFilePath(const wchar_t *path) {
	mDefaultJobFilePath = path;
}

int32 VDJobQueue::GetJobIndexById(uint64 id) const {
	int32 index = 0;
	JobQueue::const_iterator it(mJobQueue.begin()), itEnd(mJobQueue.end());
	for(; it != itEnd; ++it, ++index) {
		VDJob *job = *it;

		if (job->mId == id)
			return index;
	}

	return -1;
}

VDJob *VDJobQueue::GetJobById(uint64 id) const {
	JobQueue::const_iterator it(mJobQueue.begin()), itEnd(mJobQueue.end());
	for(; it != itEnd; ++it) {
		VDJob *job = *it;

		if (job->mId == id)
			return job;
	}

	return NULL;
}

VDJob *VDJobQueue::ListGet(int index) {
	if ((unsigned)index >= mJobQueue.size())
		return NULL;

	return mJobQueue[index];
}

int VDJobQueue::ListFind(VDJob *vdj_find) {
	JobQueue::const_iterator it(std::find(mJobQueue.begin(), mJobQueue.end(), vdj_find));

	if (it == mJobQueue.end())
		return -1;

	return it - mJobQueue.begin();
}

long VDJobQueue::ListSize() {
	return mJobCount;
}

// VDJobQueue::ListClear()
//
// Clears all jobs from the list.

void VDJobQueue::ListClear(bool force_no_update) {
	for(int i=mJobQueue.size()-1; i>=0; --i) {
		VDJob *vdj = mJobQueue[i];

		if (vdj->GetState() != VDJob::kStateInProgress) {
			VDASSERT(vdj->mpJobQueue == this);
			vdj->mpJobQueue = NULL;

			mJobQueue.erase(mJobQueue.begin() + i);
			--mJobCount;
			
			delete vdj;
		}
	}

	if (g_pVDJobQueueStatusCallback)
		g_pVDJobQueueStatusCallback->OnJobQueueReloaded();

	if (!force_no_update)
		SetModified();
}

void VDJobQueue::Refresh(VDJob *job) {
	int index = ListFind(job);

	if (index>=0) {
		if (g_pVDJobQueueStatusCallback)
			g_pVDJobQueueStatusCallback->OnJobUpdated(*job, index);
	}
}

void VDJobQueue::Add(VDJob *job, bool force_no_update) {
	VDASSERT(!job->mpJobQueue);
	job->mpJobQueue = this;

	if (!*job->GetName()) {
		VDStringA name;
		name.sprintf("Job %d", mJobNumber++);
		job->SetName(name.c_str());
	}

	if (job->mId == 0)
		job->mId = GetUniqueId();

	job->mCreationRevision = 0;
	job->mChangeRevision = 0;

	mJobQueue.push_back(job);
	++mJobCount;

	if (g_pVDJobQueueStatusCallback)
		g_pVDJobQueueStatusCallback->OnJobAdded(*job, mJobCount - 1);

	if (!force_no_update) SetModified();
}

void VDJobQueue::Delete(VDJob *job, bool force_no_update) {
	VDASSERT(job->mpJobQueue == this);
	job->mpJobQueue = NULL;

	int index = ListFind(job);

	if (index >= 0) {
		if (g_pVDJobQueueStatusCallback)
			g_pVDJobQueueStatusCallback->OnJobRemoved(*job, index);

		VDStringA dataSubdir(job->GetProjectSubdir());
		VDProject::DeleteData(mJobFilePath,VDTextU8ToW(dataSubdir));
	}

	mJobQueue.erase(mJobQueue.begin() + index);
	--mJobCount;
	
	if (!force_no_update) SetModified();
}

void VDJobQueue::Run(VDJob *job) {
	uint64 id = job->mId;

	if (mbDistributedMode) {
		try {
			Flush();
		} catch(const MyError&) {
			if (!mRetryTimer.mLastPeriod)
				mRetryTimer.mLastPeriod = 100;
			else {
				mRetryTimer.mLastPeriod += mRetryTimer.mLastPeriod;
				if (mRetryTimer.mLastPeriod > 1000)
					mRetryTimer.mLastPeriod = 1000;
			}

			mRetryTimer.mbRetryOK = false;
			mRetryTimer.mTimer.SetOneShot(&mRetryTimer, mRetryTimer.mLastPeriod);
			return;
		}
	}

	job = GetJobById(id);
	if (!job || job->IsRunning())
		return;

	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);

	VDASSERT(!mbDistributedMode || !job->IsModified());

	uint64 oldDateStart = job->mDateStart;
	uint64 oldDateEnd = job->mDateEnd;
	int oldState = job->GetState();
	uint64 oldRunner = job->GetRunnerId();
	VDStringA oldRunnerName(job->GetRunnerName());

	job->mDateStart = ((uint64)ft.dwHighDateTime << 32) + (uint32)ft.dwLowDateTime;
	job->mDateEnd = 0;
	job->SetRunner(mRunnerId, mComputerName.c_str());

	if (mbDistributedMode) {
		job->SetState(VDJob::kStateStarting);

		bool flushOK = false;

		try {
			if (Flush())
				flushOK = true;
		} catch(const MyError&) {
			// ignore
		}

		if (!flushOK) {
			//VDDEBUG("[%d] Failed to commit, rolling back...\n", VDGetCurrentProcessId());
			job->SetState(oldState);
			job->SetRunner(oldRunner, oldRunnerName.c_str());
			job->mDateStart = oldDateStart;
			job->mDateEnd = oldDateEnd;
			job->SetModified(false);
			job->Refresh();

			if (!mRetryTimer.mLastPeriod)
				mRetryTimer.mLastPeriod = 100;
			else {
				mRetryTimer.mLastPeriod += mRetryTimer.mLastPeriod;
				if (mRetryTimer.mLastPeriod > 1000)
					mRetryTimer.mLastPeriod = 1000;
			}

			mRetryTimer.mbRetryOK = false;
			mRetryTimer.mTimer.SetOneShot(&mRetryTimer, mRetryTimer.mLastPeriod);
			return;
		}
	} else {
		job->SetState(VDJob::kStateInProgress);

		try {
			Flush();
		} catch(const MyError&) {
			// ignore
		}
	}

	mRetryTimer.mbRetryOK = true;
	mRetryTimer.mLastPeriod = 0;

	job = GetJobById(id);

	if (!job || !job->IsLocal())
		return;

	VDASSERT(job->GetState() != VDJob::kStateStarting);

	if (!job->IsRunning())
		return;

	mpRunningJob = job;

	job->Refresh();

	if (g_pVDJobQueueStatusCallback) {
		NotifyStatus();
		g_pVDJobQueueStatusCallback->OnJobStarted(*job);
		g_pVDJobQueueStatusCallback->OnJobProgressUpdate(*job, 0.0f);
	}

	try {
		g_fJobMode = true;

		VDAutoLogger logger(kVDLogWarning);

		if (g_project) {
			VDStringA subDir(job->GetProjectSubdir());
			g_project->SaveProjectPath(mJobFilePath, VDTextU8ToW(subDir), true);
		}
		RunScriptMemory(job->GetScript(), job->GetScriptLine(), false);

		job->mLogEntries = logger.GetEntries();
	} catch(const MyUserAbortError&) {
		job->SetState(VDJob::kStateAborted);
	} catch(const MyError& err) {
		job->SetState(VDJob::kStateError);
		job->SetError(err.gets());
	}

	if (g_project) {
		g_project->Close();
		g_project->SetAudioSourceNormal(0);
		g_project->mProjectFilename.clear();
	}

	g_fJobMode = false;

	if (g_pVDJobQueueStatusCallback)
		g_pVDJobQueueStatusCallback->OnJobEnded(*job);
	mpRunningJob = NULL;

	if (job->GetState() == VDJob::kStateInProgress)
		job->SetState(VDJob::kStateCompleted);

	GetSystemTimeAsFileTime(&ft);

	job->mDateEnd = ((uint64)ft.dwHighDateTime << 32) + (uint32)ft.dwLowDateTime;
	SetModified();

	job->Refresh();

	try {
		Flush();
	} catch(const MyError&) {
		// Eat errors from the job flush.  The job queue on disk may be messed
		// up, but as long as our in-memory queue is OK, we can at least finish
		// remaining jobs.
	}
}

void VDJobQueue::Reload(VDJob *job) {
	// safety guard
	if (mbRunning)
		return;

	mbRunning	= true;
	mbRunAllStop		= false;

	bool wasEnabled = !(GetWindowLong(g_hwndJobs, GWL_STYLE) & WS_DISABLED);
	if (g_hwndJobs)
		EnableWindow(g_hwndJobs, FALSE);

	MyError err;
	try {
		g_project->OpenJob(mJobFilePath.c_str(), job);
	} catch(MyError& e) {
		err.TransferFrom(e);
	}

	mbRunning = false;

	// re-enable job window
	if (g_hwndJobs && wasEnabled)
		EnableWindow(g_hwndJobs, TRUE);

	if (err.gets())
		err.post(g_hwndJobs, g_szError);
}

void VDJobQueue::ReloadAutosave() {
	autoSave.Load(g_project);
}

void VDJobQueue::Transform(int fromState, int toState) {
	bool modified = false;

	for(uint32 i=0; i<mJobCount; ++i) {
		VDJob *job = mJobQueue[i];

		if (job->GetState() == fromState) {
			job->SetState(toState);
			modified = true;
			job->Refresh();
		}
	}

	SetModified();
}

// VDJobQueue::ListLoad()
//
// Loads the list from a file.

static char *findcmdline(char *s) {
	while(isspace((unsigned char)*s)) ++s;

	if (s[0] != '/' || s[1] != '/')
		return NULL;

	s+=2;

	while(isspace((unsigned char)*s)) ++s;
	if (*s++ != '$') return NULL;

	return s;
}

static void strgetarg(VDStringA& str, const char *s) {
	const char *t = s;

	if (*t == '"') {
		s = ++t;
		while(*s && *s!='"') ++s;
	} else
		while(*s && !isspace((unsigned char)*s)) ++s;

	str.assign(t, s);
}

static void strgetarg2(VDStringA& str, const char *s) {
	static const char hexdig[]="0123456789ABCDEF";
	vdfastvector<char> buf;
	char stopchar = 0;

	if (*s == '"') {
		++s;
		stopchar = '"';
	}

	buf.reserve(strlen(s));

	while(char c = *s++) {
		if (c == stopchar)
			break;

		if (c=='\\') {
			switch(c=*s++) {
			case 'a': c='\a'; break;
			case 'b': c='\b'; break;
			case 'f': c='\f'; break;
			case 'n': c='\n'; break;
			case 'r': c='\r'; break;
			case 't': c='\t'; break;
			case 'v': c='\v'; break;
			case 'x':
				c = (char)(strchr(hexdig,toupper(s[0]))-hexdig);
				c = (char)((c<<4) | (strchr(hexdig,toupper(s[1]))-hexdig));
				s += 2;
				break;
			}
		}

		if (!c)
			break;

		buf.push_back(c);
	}

	str.assign(buf.data(), buf.size());
}

void VDJobQueue::ListLoad(const wchar_t *fileName, bool merge) {
	vdautoptr<VDJob> job;

	// Try to create VirtualDub.jobs in the same directory as VirtualDub.

	bool usingGlobalFile = false;
	if (!fileName) {
		usingGlobalFile = true;

		fileName = mJobFilePath.c_str();
	}

	try {
		VDFileStream fileStream(fileName);

		if (!Load(&fileStream, merge) || !merge) {
			mbOrderModified = false;
			mbModified = false;
		}
	} catch(const MyError& e) {
		if (!usingGlobalFile)
			throw MyError("Failure loading job list: %s.", e.c_str());
	}
}

void VDJobQueue::LoadProject(const wchar_t *fileName) {
	VDFileStream fileStream(fileName);
	Load(&fileStream,false);
}

bool VDJobQueue::Load(IVDStream *stream, bool merge) {
	JobQueue newJobs;
	vdautoptr<VDJob> job;

	bool modified = false;
	try {
		bool script_capture = false;
		bool script_reloadable = false;
		vdfastvector<char> script;
		int script_line = -1;

		uint64 newSignature		= mBaseSignature;
		uint32 newRevision		= 1;

		VDTextStream input(stream);

		vdfastvector<char> linebuffer;
		int input_line = -1;

		for(;;) {
			// read in the line

			const char *line = input.GetNextLine();
			if (!line)
				break;

			linebuffer.assign(line, line+strlen(line)+1);
			input_line++;

			char *s = linebuffer.data();

			// scan for a command

			if (s = findcmdline(linebuffer.data())) {
				char *t = s;

				while(isalpha((unsigned char)*t) || *t=='_') ++t;

				if (*t) *t++=0;
				while(isspace((unsigned char)*t)) ++t;

				if (!_stricmp(s, "signature")) {
					uint64 sig;
					uint32 revision;

					if (2 != sscanf(t, "%llx %x", &sig, &revision))
						throw MyError("invalid signature");

					if (merge && sig == mLastSignature && revision == mLastRevision)
						return false;

					newSignature = sig;
					newRevision = revision;
				} else if (!_stricmp(s, "job")) {
					job = new_nothrow VDJob;
					if (!job) throw MyError("out of memory");

					job->mpJobQueue			= this;
					job->mCreationRevision	= newRevision;
					job->mChangeRevision	= newRevision;

					VDStringA name;
					strgetarg(name, t);
					job->SetName(name.c_str());

				} else if (!_stricmp(s, "data")) {

					VDStringA subdir;
					strgetarg(subdir, t);
					job->SetProjectSubdir(subdir.c_str());

				} else if (!_stricmp(s, "location")) {

					VDStringA dir;
					strgetarg(dir, t);
					job->SetProjectDir(dir.c_str());

				} else if (!_stricmp(s, "input")) {

					VDStringA inputFile;
					strgetarg(inputFile, t);
					job->SetInputFile(inputFile.c_str());

				} else if (!_stricmp(s, "output")) {

					VDStringA outputFile;
					strgetarg(outputFile, t);
					job->SetOutputFile(outputFile.c_str());

				} else if (!_stricmp(s, "error")) {

					VDStringA error;
					strgetarg2(error, t);
					job->SetError(error.c_str());

				} else if (!_stricmp(s, "state")) {

					job->SetState(atoi(t));

				} else if (!_stricmp(s, "id")) {

					uint64 id;
					if (1 != sscanf(t, "%llx", &id))
						throw MyError("invalid ID");

					job->mId = id;

				} else if (!_stricmp(s, "runner_id")) {

					uint64 id;
					if (1 != sscanf(t, "%llx", &id))
						throw MyError("invalid runner ID");

					job->mRunnerId = id;

				} else if (!_stricmp(s, "runner_name")) {

					strgetarg2(job->mRunnerName, t);

				} else if (!_stricmp(s, "revision")) {

					uint32 createrev, changerev;

					if (2 != sscanf(t, "%x %x", &createrev, &changerev))
						throw MyError("invalid revisions");

					job->mCreationRevision = createrev;
					job->mChangeRevision = changerev;

				} else if (!_stricmp(s, "start_time")) {
					uint32 lo, hi;

					if (2 != sscanf(t, "%08lx %08lx", &hi, &lo))
						throw MyError("invalid start time");

					job->mDateStart = ((uint64)hi << 32) + lo;
				} else if (!_stricmp(s, "end_time")) {
					uint32 lo, hi;

					if (2 != sscanf(t, "%08lx %08lx", &hi, &lo))
						throw MyError("invalid start time");

					job->mDateEnd = ((uint64)hi << 32) + lo;

				} else if (!_stricmp(s, "script")) {

					script_capture = true;
					script_reloadable = false;
					script.clear();
					script_line = input_line;

				} else if (!_stricmp(s, "endjob")) {
					if (script_capture) {
						job->SetScript(script.begin(), script.size(), script_line, script_reloadable);
						script_capture = false;
						script_line = -1;
					}

					job->SetModified(false);

					// Check if the job is running and if the process corresponds to the same machine as
					// us. If so, check if the process is still running and if not, mark the job as aborted.

					int state = job->GetState();
					if (state == VDJob::kStateInProgress || state == VDJob::kStateAborting) {
						if (job->mRunnerId && job->mRunnerId != mRunnerId) {
							uint32 machineid = (uint32)(job->mRunnerId >> 32);
							if (machineid == (uint32)(mRunnerId >> 32)) {
								uint32 pid = (uint32)job->mRunnerId;
								HANDLE hProcess = OpenProcess(SYNCHRONIZE, FALSE, pid);

								bool processExists = false;
								if (hProcess) {
									if (WAIT_TIMEOUT == WaitForSingleObject(hProcess, 0))
										processExists = true;

									CloseHandle(hProcess);
								}

								if (!processExists)
									job->SetState(VDJob::kStateAborted);
							}
						} else {
							if (!mpRunningJob || mpRunningJob->mId != job->mId)
								job->SetState(VDJob::kStateAborted);
						}
					}

					newJobs.push_back(job);
					job.release();

				} else if (!_stricmp(s, "logent")) {

					int severity;
					char dummyspace;
					int pos;

					if (2 != sscanf(t, "%d%c%n", &severity, &dummyspace, &pos))
						throw MyError("invalid log entry");

					job->mLogEntries.push_back(VDJob::tLogEntries::value_type(severity, VDTextAToW(t + pos, -1).c_str()));
				}
			}

			// add commands and blank lines, how else it should report errors
			if (script_capture) {
				s = linebuffer.data();
				script.insert(script.end(), s, s+strlen(s));
				script.push_back('\r');
				script.push_back('\n');

				// check for reload marker
				while(isspace((unsigned char)*s)) ++s;
				if (s[0] == '/' && s[1] == '/' && strstr(s, "$reloadstop"))
					script_reloadable = true;
			}
		}

		if (merge) {
			// Merge the in-memory and on-disk job queues.
			typedef stdext::hash_map<uint64, int> JobQueueLookup;

			//VDDEBUG("[%d] Merging job queues - last rev %d (order%s modified), new rev %d\n", VDGetCurrentProcessId(), mLastRevision, mbOrderModified ? "" : " not", newRevision);

			bool useJobsFromDstQueue = mbOrderModified;
			JobQueue dstQueue;
			JobQueue srcQueue;
			JobQueue dumpQueue;
			uint32 srcRevision;

			if (useJobsFromDstQueue) {
				dstQueue = mJobQueue;
				srcQueue = newJobs;
				srcRevision = newRevision;
			} else {
				dstQueue = newJobs;
				srcQueue = mJobQueue;
				srcRevision = mLastRevision;
			}

			// build lookup for src queue
			JobQueueLookup srcJobLookup;

			for(JobQueue::const_iterator it(srcQueue.begin()), itEnd(srcQueue.end()); it!=itEnd; ++it) {
				VDJob *job = *it;

				if (job->mId)
					srcJobLookup.insert(JobQueueLookup::value_type(job->mId, it - srcQueue.begin()));
			}

			// iterate over dst queue
			vdfastvector<uint32> jobIndicesToDelete;
			for(JobQueue::iterator it(dstQueue.begin()), itEnd(dstQueue.end()); it!=itEnd; ++it) {
				VDJob *job = *it;

				if (!job->mId)
					continue;

				JobQueueLookup::const_iterator itCrossJob(srcJobLookup.find(job->mId));
				if (itCrossJob != srcJobLookup.end()) {
					int crossIndex = itCrossJob->second;
					VDJob *crossJob = srcQueue[crossIndex];

					VDASSERT(crossJob);
					if (crossJob) {
						if (useJobsFromDstQueue) {
							modified |= job->Merge(*crossJob);
							dumpQueue.push_back(crossJob);
						} else {
							modified |= crossJob->Merge(*job);
							*it = crossJob;
							dumpQueue.push_back(job);
						}

						//VDDEBUG("[%d] %s\n", VDGetCurrentProcessId(), job->ToString().c_str());

						srcQueue[crossIndex] = NULL;
					}
				} else if (job->mCreationRevision < srcRevision) {
					dumpQueue.push_back(job);
					*it = NULL;

					if (useJobsFromDstQueue)
						modified = true;
				}
			}

			// compact destination queue by removing null job entries
			dstQueue.erase(std::remove_if(dstQueue.begin(), dstQueue.end(), std::bind2nd(std::equal_to<VDJob *>(), (VDJob *)NULL)), dstQueue.end());

			// merge any new jobs from the on-disk version
			for(JobQueue::iterator it(srcQueue.begin()), itEnd(srcQueue.end()); it!=itEnd; ++it) {
				VDJob *newJob = *it;

				if (newJob && (!newJob->mCreationRevision || newJob->mCreationRevision > mLastRevision)) {
					dstQueue.push_back(newJob);
					*it = NULL;

					if (useJobsFromDstQueue)
						modified = true;
				}
			}

			// swap over queues
			mJobQueue.swap(dstQueue);
			mJobCount = mJobQueue.size();

			mLastSignature = newSignature;
			mLastRevision = newRevision;

			newJobs.clear();

			while(!srcQueue.empty()) {
				VDJob *job = srcQueue.back();
				srcQueue.pop_back();
				if (job) {
					job->mpJobQueue = NULL;
					delete job;
				}
			}

			while(!dumpQueue.empty()) {
				VDJob *job = dumpQueue.back();
				dumpQueue.pop_back();
				if (job) {
					job->mpJobQueue = NULL;
					delete job;
				}
			}
		} else {
			// swap over queues
			mJobQueue.swap(newJobs);
			mJobCount = mJobQueue.size();

			mLastSignature = newSignature;
			mLastRevision = newRevision;

			while(!newJobs.empty()) {
				VDJob *job = newJobs.back();
				newJobs.pop_back();
				if (job) {
					job->mpJobQueue = NULL;
					delete job;
				}
			}
		}

		// assign IDs and revision IDs to any jobs that are missing them
		JobQueue::iterator it(mJobQueue.begin()), itEnd(mJobQueue.end());
		for(; it != itEnd; ++it) {
			VDJob *job = *it;

			VDASSERT(job->mpJobQueue == this);

			if (!job->mId)
				job->mId = VDJobQueue::GetUniqueId();

			if (!job->mCreationRevision)
				job->mCreationRevision = newRevision;

			if (!job->mChangeRevision)
				job->mChangeRevision = newRevision;
		}

		// notify
		if (g_pVDJobQueueStatusCallback)
			g_pVDJobQueueStatusCallback->OnJobQueueReloaded();

		// check if the current job is now marked as Aborting -- if so, issue an abort
		if (mpRunningJob && mpRunningJob->GetState() == VDJob::kStateAborting && g_dubber) {
			g_dubber->Abort();
		}

	} catch(const MyError&) {
		while(!newJobs.empty()) {
			VDJob *job = newJobs.back();
			newJobs.pop_back();
			delete job;
		}

		throw;
	}

	return modified;
}

void VDJobQueue::SetModified() {
	mbModified = true;

	mFlushTimer.SetOneShot(this, 1000);
}

// VDJobQueue::Flush()
//
// Flushes the job list out to disk.
//
// We store the job list in a file called VirtualDub.jobs.  It's actually a
// human-readable, human-editable Sylia script with extra comments to tell
// VirtualDub about each of the scripts.

bool VDJobQueue::Flush(const wchar_t *fileName) {
	// Try to create VirtualDub.jobs in the same directory as VirtualDub.

	bool usingGlobalFile = false;
	if (!fileName) {
		usingGlobalFile = true;

		fileName = mJobFilePath.c_str();
	}

	if (usingGlobalFile && mbDistributedMode) {
		try {
			VDFileStream outputStream(fileName, nsVDFile::kReadWrite | nsVDFile::kDenyAll | nsVDFile::kOpenAlways);

			Load(&outputStream, true);

			for(JobQueue::iterator it(mJobQueue.begin()), itEnd(mJobQueue.end()); it != itEnd; ++it) {
				VDJob *job = *it;

				VDASSERT(job->mpJobQueue == this);

				if (job->GetState() == VDJob::kStateStarting && job->IsLocal()) {
					job->SetState(VDJob::kStateInProgress);
					job->Refresh();
				}
			}

			uint64 signature = CreateListSignature();
			outputStream.seek(0);
			outputStream.truncate();

			uint32 revision = mLastRevision + 1;

			Save(&outputStream, signature, revision, false);

			mbModified = false;
			mbOrderModified = false;
			mLastSignature = signature;
			mLastRevision = revision;

			JobQueue::const_iterator it(mJobQueue.begin()), itEnd(mJobQueue.end());
			for(; it != itEnd; ++it) {
				VDJob *job = *it;

				if (!job->mCreationRevision)
					job->mCreationRevision = revision;

				if (!job->mChangeRevision)
					job->mChangeRevision = revision;

				job->SetModified(false);
			}
		} catch(const MyError&) {
			return false;
		}
	} else {
		VDFileStream outputStream(fileName, nsVDFile::kWrite | nsVDFile::kDenyAll | nsVDFile::kCreateAlways);
		
		Save(&outputStream, 0, 1, true);

		mbModified = false;
		mbOrderModified = false;
	}

	return true;
}

void VDJobQueue::Save(IVDStream *stream, uint64 signature, uint32 revision, bool resetJobRevisions) {
	VDTextOutputStream output(stream);

	output.PutLine("// VirtualDub job list (Sylia script format)");
	output.PutLine("// This is a program generated file -- edit at your own risk.");
	output.PutLine("//");
	output.FormatLine("// $signature %llx %x", signature, revision);
	output.FormatLine("// $numjobs %d", ListSize());
	output.PutLine("//");
	output.PutLine("");

	JobQueue::const_iterator it(mJobQueue.begin()), itEnd(mJobQueue.end());

	for(; it != itEnd; ++it) {
		VDJob *vdj = *it;
		const int state = vdj->GetState();

		output.FormatLine("// $job \"%s\""		, vdj->GetName());
		output.FormatLine("// $data \"%s\""		, vdj->GetProjectSubdir());
		output.FormatLine("// $input \"%s\""	, vdj->GetInputFile());
		output.FormatLine("// $output \"%s\""	, vdj->GetOutputFile());
		output.FormatLine("// $state %d"		, state);
		output.FormatLine("// $id %llx"			, vdj->mId);

		if (!resetJobRevisions) {
			if (vdj->IsModified())
				vdj->mChangeRevision = revision;

			output.FormatLine("// $revision %x %x", vdj->mCreationRevision ? vdj->mCreationRevision : revision, vdj->mChangeRevision);
		}

		if (state == VDJob::kStateInProgress || state == VDJob::kStateAborting || state == VDJob::kStateCompleted || state == VDJob::kStateError) {
			output.FormatLine("// $runner_id %llx", vdj->mRunnerId);
			output.FormatLine("// $runner_name \"%s\"", VDEncodeScriptString(VDStringSpanA(vdj->GetRunnerName())).c_str());
		}

		output.FormatLine("// $start_time %08lx %08lx", (unsigned long)(vdj->mDateStart >> 32), (unsigned long)vdj->mDateStart);
		output.FormatLine("// $end_time %08lx %08lx", (unsigned long)(vdj->mDateEnd >> 32), (unsigned long)vdj->mDateEnd);

		for(VDJob::tLogEntries::const_iterator it(vdj->mLogEntries.begin()), itEnd(vdj->mLogEntries.end()); it!=itEnd; ++it) {
			const VDJob::tLogEntries::value_type& ent = *it;
			output.FormatLine("// $logent %d %s", ent.severity, VDTextWToA(ent.text).c_str());
		}

		if (state == VDJob::kStateError)
			output.FormatLine("// $error \"%s\"", VDEncodeScriptString(VDStringSpanA(vdj->GetError())).c_str());

		output.PutLine("// $script");
		output.PutLine("");

		// Dump script

		const char *s = vdj->GetScript();

		while(*s) {
			const char *t = s;
			char c;

			while((c=*t) && c!='\r' && c!='\n')
				++t;

			if (t>s)
				output.Write(s, t-s);

			output.PutLine();

			// handle CR, CR/LF, LF, and NUL terminators

			if (*t == '\r') ++t;
			if (*t == '\n') ++t;

			s=t;
		}

		// Next...

		output.PutLine("");
		output.PutLine("// $endjob");
		output.PutLine("//");
		output.PutLine("//--------------------------------------------------");
	}

	output.PutLine("// $done");

	output.Flush();
}

void VDJobQueue::RunAllStart() {
	if (mbRunning || mbRunAll)
		return;

	if (inputAVI && g_project) {
		autoSave.Save(g_project);
	}

	mbRunning		= true;
	mbRunAll		= true;
	mbRunAllStop	= false;
	mRetryTimer.mbRetryOK = true;
	mRetryTimer.mLastPeriod = 0;

	NotifyStatus();

	ShowWindow(g_hWnd, SW_MINIMIZE);
}

bool VDJobQueue::RunAllNext() {
	JobQueue::const_iterator it(mJobQueue.begin()), itEnd(mJobQueue.end());
	for(; it != itEnd; ++it) {
		VDJob *testJob = *it;

		if (testJob->GetState() == VDJob::kStateWaiting) {
			mJobIdToRun = testJob->GetId();
			return true;
		}
	}

	mbRunning = false;
	mbRunAll = false;
	mbRunAllStop = false;
	NotifyStatus();

	VDRegistryAppKey key;
	if (key.getBool(g_szRegKeyShutdownWhenFinished)) {
		if (g_hwndJobs)
			EnableWindow(g_hwndJobs, FALSE);

		VDSystemShutdownMode shutdownMode = (VDSystemShutdownMode)key.getInt(g_szRegKeyShutdownMode);

		bool do_shutdown = VDUIRequestSystemShutdown((VDGUIHandle)g_hWnd);

		if (g_hwndJobs)
			EnableWindow(g_hwndJobs, TRUE);

		if (do_shutdown) {
			if (!VDInitiateSystemShutdownWithUITimeout(shutdownMode,
				L"VirtualDub is shutting down the system after finishing the job queue at the user's request.",
				0))
			{
				VDInitiateSystemShutdown((VDSystemShutdownMode)key.getInt(g_szRegKeyShutdownMode));
			}

			PostQuitMessage(0);
			return false;
		}
	}

	if (!g_exitOnDone) ReloadAutosave();

	return false;
}

void VDJobQueue::RunAllStop() {
	mbRunAllStop = true;
}

void VDJobQueue::Swap(int x, int y) {
	uint32 size = mJobQueue.size();
	if ((unsigned)x < size && (unsigned)y < size)
		std::swap(mJobQueue[x], mJobQueue[y]);

	mbOrderModified = true;
}

bool VDJobQueue::IsLocal(const VDJob *job) const {
	return job->mRunnerId == mRunnerId;
}

VDJobQueueStatus VDJobQueue::GetQueueStatus() const {
	if (mbBlocked)
		return kVDJQS_Blocked;
	else if (mbRunning)
		return kVDJQS_Running;
	else
		return kVDJQS_Idle;
}

int VDJobQueue::GetPendingJobCount() const {
	int count = 0;

	JobQueue::const_iterator it(mJobQueue.begin()), itEnd(mJobQueue.end());
	for(; it != itEnd; ++it) {
		VDJob *job = *it;

		switch(job->GetState()) {
		case VDJob::kStateInProgress:
		case VDJob::kStateWaiting:
			++count;
			break;
		}
	}

	return count;
}

uint64 VDJobQueue::GetUniqueId() {
	uint64 id = mBaseSignature + (uint32)VDGetCurrentTick();

	while(id == 0 || id == (uint64)(sint64)-1 || GetJobById(id))
		--id;

	return id;
}

const char *VDJobQueue::GetRunnerName() const {
	return mComputerName.c_str();
}

uint64 VDJobQueue::GetRunnerId() const {
	return mRunnerId;
}

bool VDJobQueue::IsAutoUpdateEnabled() const {
	return mFileWatcher.IsActive();
}

void VDJobQueue::SetAutoUpdateEnabled(bool enabled) {
	if (mFileWatcher.IsActive() == enabled)
		return;

	if (enabled) {
		try {
			mFileWatcher.Init(mJobFilePath.c_str(), this);
		} catch(const MyError&) {
			// for now, eat the error
		}
	} else {
		mFileWatcher.Shutdown();
	}
}

bool VDJobQueue::PollAutoRun() {
	if (mbBlocked)
		return false;

	if (mbRunAllStop) {
		mJobIdToRun = 0;
		mbRunAll = false;
		mbRunning = false;
		mbRunAllStop = false;
		NotifyStatus();
		ReloadAutosave();
		return false;
	}

	if (mJobIdToRun) {
		if (!mRetryTimer.mbRetryOK)
			return false;

		VDJob *job = GetJobById(mJobIdToRun);
		if (job && job->GetState() == VDJob::kStateWaiting) {
			job->Run();
			return true;
		}

		mJobIdToRun = 0;
	}

	if (mbRunAll)
		return RunAllNext();

	if (mbRunning || !mbAutoRun)
		return false;

	JobQueue::const_iterator it(mJobQueue.begin()), itEnd(mJobQueue.end());
	for(; it!=itEnd; ++it) {
		VDJob *job = *it;

		if (job->GetState() == VDJob::kStateWaiting) {
			RunAllStart();
			return true;
		}
	}

	return false;
}

bool VDJobQueue::IsAutoRunEnabled() const {
	return mbAutoRun;
}

void VDJobQueue::SetAutoRunEnabled(bool autorun) {
	mbAutoRun = autorun;
}

void VDJobQueue::SetBlocked(bool blocked) {
	mbBlocked = blocked;

	NotifyStatus();
}

void VDJobQueue::SetCallback(IVDJobQueueStatusCallback *cb) {
	g_pVDJobQueueStatusCallback = cb;
}

void VDJobQueue::NotifyStatus() {
	if (!g_pVDJobQueueStatusCallback)
		return;

	g_pVDJobQueueStatusCallback->OnJobQueueStatusChanged(GetQueueStatus());
}

uint64 VDJobQueue::CreateListSignature() {
	for(;;) {
		uint64 sig = mBaseSignature + VDGetCurrentTick();

		if (sig != mLastSignature)
			return sig;

		::Sleep(1);
	}
}

bool VDJobQueue::OnFileUpdated(const wchar_t *path) {
	if (!VDDoesPathExist(path))
		return true;

	HANDLE hTest;
	
	if (VDIsWindowsNT())
		hTest = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	else
		hTest = CreateFileA(VDTextWToA(path).c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hTest == INVALID_HANDLE_VALUE) {
		DWORD err = GetLastError();

		// if we're getting access denied, keep polling
		if (err == ERROR_ACCESS_DENIED)
			return false;
	} else {
		ListLoad(NULL, true);
		CloseHandle(hTest);
	}

	return true;
}

void VDJobQueue::TimerCallback() {
	Flush();

	if (mbModified)
		SetModified();
}

void VDJobQueue::RetryTimer::TimerCallback() {
	mbRetryOK = true;
}
