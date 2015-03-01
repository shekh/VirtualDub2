#include <vd2/system/atomic.h>
#include <vd2/system/thread.h>
#include <vd2/system/vdalloc.h>
#include "test.h"

class ThreadLog {
public:
	typedef int value_type;
	typedef const value_type *const_iterator;

	enum { N = 1000000 };

	ThreadLog();

	const_iterator begin() const { return mLog; }
	const_iterator end() const { int n = mPos; return mLog + (n > N ? N : n); }

	void Add(int code);

protected:
	VDAtomicInt mPos;
	int mLog[N];
};

ThreadLog::ThreadLog()
	: mPos(0)
{
	memset(mLog, 0, sizeof mLog);
}

void ThreadLog::Add(int code) {
	int pos = mPos++;

	if (pos < N)
		mLog[pos] = code;
}

///////////////////////////////////////////////////////////////////////////////

class SemaphoreTestThread : public VDThread {
public:
	void Init(ThreadLog *log, VDSemaphore *sema, int mode);

	void ThreadRun();

protected:
	ThreadLog *mpLog;
	VDSemaphore *mpSema;
	int mMode;
};

void SemaphoreTestThread::Init(ThreadLog *log, VDSemaphore *sema, int mode) {
	mpLog = log;
	mpSema = sema;
	mMode = mode;
}

void SemaphoreTestThread::ThreadRun() {
	switch(mMode) {
	case 0:
		for(int i=0; i<10000; ++i) {
			mpLog->Add(+1);
			mpSema->Post();

			for(int j=rand() & 1023; j; --j)
				volatile int k = 1/j;
		}
		break;

	case 1:
		for(int i=0; i<10000; ++i) {
			mpSema->Wait();
			mpLog->Add(-1);

			for(int j=rand() & 1023; j; --j)
				volatile int k = 1/j;
		}
		break;

	case 2:
		for(int i=0; i<10000; ++i) {
			while(!mpSema->TryWait())
				VDThreadSleep(1);

			mpLog->Add(-1);

			for(int j=rand() & 1023; j; --j)
				volatile int k = 1/j;
		}
		break;

	case 3:
		for(int i=0; i<10000; ++i) {
			while(!mpSema->Wait(1))
				;

			mpLog->Add(-1);

			for(int j=rand() & 1023; j; --j)
				volatile int k = 1/j;
		}
		break;
	}
}

static bool RunSemaphoreTest() {
	for(int phase=0; phase<3; ++phase) {
		SemaphoreTestThread threads[100];
		VDSemaphore sema(0);
		vdautoptr<ThreadLog> log(new ThreadLog);


		switch(phase) {
			case 0:
				for(int i=0; i<100; ++i) {
					threads[i].Init(log, &sema, (i&1) != 0);
				}
				break;

			case 1:
				for(int i=0; i<100; ++i) {
					threads[i].Init(log, &sema, (i&1) ? 2 : 0);
				}
				break;

			case 2:
				for(int i=0; i<100; ++i) {
					threads[i].Init(log, &sema, (i&1) ? 3 : 0);
				}
				break;
		}

		for(int i=0; i<100; ++i)
			threads[i].ThreadStart();

		for(int i=0; i<100; ++i)
			threads[i].ThreadWait();

		ThreadLog::const_iterator it(log->begin()), itEnd(log->end());
		int count = 0;
		for(; it!=itEnd; ++it) {
			int code = *it;

			count += code;
			if (count < 0) {
				printf("Semaphore inconsistency detected.\n");
				return false;
			}
		}

		if (count) {
			printf("Semaphore inconsistency detected.\n");
			return false;
		}
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////

DEFINE_TEST(Synchronization) {
	if (!RunSemaphoreTest())
		return false;

	return true;
}
