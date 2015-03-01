#include <vd2/system/vdstl.h>
#include <vd2/VDLib/Job.h>
#include "test.h"

namespace {
	enum JobState {
		waiting		= VDJob::kStateWaiting,
		inprogress	= VDJob::kStateInProgress,
		completed	= VDJob::kStateCompleted,
		postponed	= VDJob::kStatePostponed,
		aborted		= VDJob::kStateAborted,
		error		= VDJob::kStateError,
		aborting	= VDJob::kStateAborting,
		count		= VDJob::kStateCount			
	};

	VDJob j(JobState state, int changeRev, bool modified) {
		VDJob job;

		job.SetState(state);
		job.mChangeRevision = changeRev;
		job.SetModified(modified);

		return job;
	}

	VDJob operator+(const VDJob& x, const VDJob& y) {
		VDJob t(x);

		t.Merge(y);
		return t;
	}

	bool operator<<=(const VDJob& x, const VDJob& y) {
		return	x.GetState() == y.GetState() &&
				x.mChangeRevision == y.mChangeRevision;
	}
}

DEFINE_TEST(DistributedJobQueue) {
	TEST_ASSERT(j(waiting, 10, false) + j(postponed, 11, false) <<= j(postponed, 11, false));
	TEST_ASSERT(j(postponed, 10, true) + j(waiting, 10, false) <<= j(postponed, 10, false));
	TEST_ASSERT(j(postponed, 10, true) + j(completed, 10, false) <<= j(postponed, 10, false));
	TEST_ASSERT(j(postponed, 10, true) + j(completed, 11, false) <<= j(completed, 11, false));
	TEST_ASSERT(j(postponed, 10, true) + j(completed, 10, false) <<= j(postponed, 10, false));

	return 0;
}
