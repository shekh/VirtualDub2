#include "stdafx.h"
#include <vd2/system/thread.h>
#include <vd2/VDLib/Job.h>

VDJob::VDJob()
	: mpJobQueue(NULL)
	, mState(VDJob::kStateWaiting)
	, mId(0)
	, mRunnerId(0)
	, mDateStart(0)
	, mDateEnd(0)
	, mbContainsReloadMarker(false)
{
}

VDJob::~VDJob() {
}

bool VDJob::operator==(const VDJob& job) const {
#define TEST(field) if (field != job.field) return false
	TEST(mCreationRevision);
	TEST(mChangeRevision);
	TEST(mId);
	TEST(mDateStart);
	TEST(mDateEnd);
	TEST(mRunnerId);
	TEST(mRunnerName);
	TEST(mName);
	TEST(mInputFile);
	TEST(mOutputFile);
	TEST(mError);
	TEST(mScript);
	TEST(mState);
	TEST(mRunnerId);
	TEST(mbContainsReloadMarker);
#undef TEST

	return true;
}

void VDJob::SetInputFile(const wchar_t *file) {
	mInputFile = VDTextWToA(file);
}

void VDJob::SetOutputFile(const wchar_t *file) {
	mOutputFile = VDTextWToA(file);
}

void VDJob::SetState(int state) {
	mState = state;
	mbModified = true;

	switch(state) {
	case kStateStarting:
	case kStateInProgress:
	case kStateAborted:
	case kStateAborting:
	case kStateCompleted:
	case kStateError:
		break;
	default:
		mRunnerName.clear();
		mRunnerId = 0;
		break;
	}
}

void VDJob::SetRunner(uint64 id, const char *name) {
	mRunnerId = id;
	mRunnerName = name;
}

void VDJob::SetScript(const void *script, uint32 len, bool reloadable) {
	mScript.assign((const char *)script, (const char *)script + len);
	mbContainsReloadMarker = reloadable;
}

void VDJob::Refresh() {
	if (mpJobQueue)
		mpJobQueue->Refresh(this);
}

void VDJob::Run() {
	if (mpJobQueue)
		mpJobQueue->Run(this);
}

void VDJob::Reload() {
	if (mpJobQueue)
		mpJobQueue->Reload(this);
}

bool VDJob::Merge(const VDJob& src) {
	const int rev1 = mChangeRevision;
	const int rev2 = src.mChangeRevision;
	const bool mod1 = mbModified;
	const bool mod2 = src.mChangeRevision > mChangeRevision;

	if (mChangeRevision < src.mChangeRevision)
		mChangeRevision = src.mChangeRevision;

	if (operator==(src))
		return false;

	// Okay, the jobs aren't the same. Which ones are modified?
	bool acceptTheirs = false;

	do {
		// only yours modified -- accept yours
		if (mod1 && !mod2) {
			acceptTheirs = false;
			break;
		}

		// only theirs modified -- accept theirs
		if (!mod1 && mod2) {
			acceptTheirs = true;
			break;
		}

		// Two cases left:
		//	- neither modified, but mismatch (bad, but should recover)
		//	- both modified, but mismatch (the dreaded three-way merge)

		// We resolve with the following priority:
		//
		//	Starting < Aborting < Aborted < Error < Waiting < Postponed < Completed < In Progress
		//
		// Ties are broken via Accept Theirs. This is critical for In Progress mode.

		static const int kPriority[]={
			4,
			7,
			6,
			5,
			2,
			3,
			1,
			0
		};

		VDASSERTCT(sizeof(kPriority)/sizeof(kPriority[0]) == VDJob::kStateCount);

		int pri1 = kPriority[mState];
		int pri2 = kPriority[src.mState];

		acceptTheirs = (pri1 <= pri2);

	} while(false);

	VDDEBUG("Yours[%3d%c]:  %s\n", rev1, mod1 ? '*' : ' ', ToString().c_str());
	VDDEBUG("Theirs[%3d%c]: %s\n", rev2, mod2 ? '*' : ' ', src.ToString().c_str());

	// We should never accept a local copy when it corresponds to an In Progress from a
	// different machine.
	VDASSERT(acceptTheirs || mState != kStateInProgress || (uint32)mRunnerId == VDGetCurrentProcessId());

	if (acceptTheirs) {
		mName		= src.mName;
		mError		= src.mError;
		mRunnerName	= src.mRunnerName;
		mState		= src.mState;
		mRunnerId	= src.mRunnerId;
		mDateStart	= src.mDateStart;
		mDateEnd	= src.mDateEnd;

		VDDEBUG("  Resolving accept theirs.\n");
		return true;
	}

	VDDEBUG("  Resolving accept yours.\n");
	return false;
}

VDStringA VDJob::ToString() const {
	VDStringA s;

	static const char *const kStateNames[]={
		"Waiting",
		"In progress",
		"Completed",
		"Postponed",
		"Aborted",
		"Error",
		"Aborting",
		"Starting",
	};

	VDASSERTCT(sizeof(kStateNames) / sizeof(kStateNames[0]) == kStateCount);

	s.sprintf("%s | %s | %-11s (%s:%d)", mInputFile.c_str(), mOutputFile.c_str(), kStateNames[mState], mRunnerName.c_str(), (uint32)mRunnerId);
	return s;
}
