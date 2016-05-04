#include "stdafx.h"
#include <vd2/system/profile.h>
#include <vd2/Riza/videocodec.h>
#include <../Kasumi/h/uberblit_rgb64.h>
#include "ThreadedVideoCompressor.h"

enum {
	// This is to work around an XviD decode bug (see VideoSource.h).
	kDecodeOverflowWorkaroundSize = 16,

	kReasonableBFrameBufferLimit = 100
};

///////////////////////////////////////////////////////////////////////////

void VDRenderOutputBufferTracker::Init(void *base, const VDPixmap& px) {
	vdrefptr<VDRenderOutputBuffer> buf(new VDRenderOutputBuffer(this));

	buf->Init(base, px);
}

void VDRenderOutputBufferTracker::Init(int count, const VDPixmapLayout& layout) {
	VDRenderBufferAllocator<VDRenderOutputBuffer>::Init(count);

	for(int i=0; i<count; ++i) {
		vdrefptr<VDRenderOutputBuffer> buf(new VDRenderOutputBuffer(this));

		buf->Init(layout);
	}
}

///////////////////////////////////////////////////////////////////////////

VDRenderOutputBuffer::VDRenderOutputBuffer(VDRenderOutputBufferTracker *tracker)
	: mpTracker(tracker)
{
}

VDRenderOutputBuffer::~VDRenderOutputBuffer() {
}

int VDRenderOutputBuffer::Release() {
	int rc = --mRefCount;

	if (!rc) {
		if (!mpTracker->FreeFrame(this))
			delete this;
	}

	return rc;
}

void VDRenderOutputBuffer::Init(void *base, const VDPixmap& px) {
	mpBase = base;
	mPixmap = px;
}

void VDRenderOutputBuffer::Init(const VDPixmapLayout& layout) {
	mBuffer.init(layout);
	mPixmap = mBuffer;
	mpBase = mBuffer.base();
}

///////////////////////////////////////////////////////////////////////////

class VDRenderPostCompressionBuffer;

void VDRenderPostCompressionBufferAllocator::Init(int count, uint32 auxsize) {
	VDRenderBufferAllocator<VDRenderPostCompressionBuffer>::Init(count);

	for(int i=0; i<count; ++i) {
		vdrefptr<VDRenderPostCompressionBuffer> buf(new VDRenderPostCompressionBuffer(this));

		buf->mOutputBuffer.resize(auxsize);
	}
}

VDRenderPostCompressionBuffer::VDRenderPostCompressionBuffer(VDRenderPostCompressionBufferAllocator *tracker)
	: mpTracker(tracker)
{
}

VDRenderPostCompressionBuffer::~VDRenderPostCompressionBuffer() {
}

int VDRenderPostCompressionBuffer::Release() {
	int rc = --mRefCount;

	if (!rc) {
		if (!mpTracker->FreeFrame(this))
			delete this;
	}

	return rc;
}

///////////////////////////////////////////////////////////////////////////

class VDThreadedVideoCompressorSlave : public VDThread {
public:
	VDThreadedVideoCompressorSlave();
	~VDThreadedVideoCompressorSlave();

	void Init(VDThreadedVideoCompressor *parent, IVDVideoCompressor *compressor);
	void Shutdown();

protected:
	void ThreadRun();

	VDThreadedVideoCompressor *mpParent;
	IVDVideoCompressor *mpCompressor;
	VDPixmapBuffer repack_buffer;
};

/////////

VDThreadedVideoCompressor::VDThreadedVideoCompressor()
	: mpThreads(NULL)
	, mThreadCount(0)
	, mbClientFlushInProgress(false)
	, mBarrier(0)
	, mFrameSkipCounter(0)
	, mpAllocator(new VDRenderPostCompressionBufferAllocator)
	, mFramesSubmitted(0)
	, mFramesProcessed(0)
	, mFramesBufferedInFlush(0)
	, mbFlushInProgress(false)
	, mbLoopDetectedDuringFlush(false)
	, mInputBufferCount(0)
	, mPriority(VDThread::kPriorityDefault)
{
}

VDThreadedVideoCompressor::~VDThreadedVideoCompressor() {
}

VDThreadedVideoCompressor::FlushStatus VDThreadedVideoCompressor::GetFlushStatus() {
	uint32 result = 0;

	vdsynchronized(mMutex) {
		if (mbLoopDetectedDuringFlush)
			result |= kFlushStatusLoopingDetected;
	}

	return (FlushStatus)result;
}

void VDThreadedVideoCompressor::SetPriority(int priority) {
	if (mPriority != priority) {
		mPriority = priority;

		if (mpThreads) {
			for(int i=0; i<mThreadCount; ++i)
				mpThreads[i].ThreadSetPriority(priority);
		}
	}
}

void VDThreadedVideoCompressor::Init(int threads, IVDVideoCompressor *pBaseCompressor) {
	Shutdown();

	uint32 compsize = pBaseCompressor->GetMaxOutputSize() + kDecodeOverflowWorkaroundSize;

	mpAllocator->Init(threads ? threads * 2 + 1 : 1, compsize);

	mpBaseCompressor = pBaseCompressor;
	mbInErrorState = false;
	mInputBufferCount.Reset(0);
	mNextInputFrameNumber = 0;
	mNextOutputFrameNumber = 0;
	mNextOutputAllocIndex = 0;
	mThreadCount = threads;
	mpThreads = new VDThreadedVideoCompressorSlave[threads];

	if (threads) {
		for(int i=0; i<threads; ++i)
			mpThreads[i].ThreadSetPriority(mPriority);

		mpThreads[0].Init(this, pBaseCompressor);

		for(int i=1; i<threads; ++i) {
			IVDVideoCompressor *vc;

			pBaseCompressor->Clone(&vc);

			mClonedCodecs.push_back(vc);

			mpThreads[i].Init(this, vc);
		}
	}
}

void VDThreadedVideoCompressor::Shutdown() {
	vdsynchronized(mMutex) {
		mbInErrorState = true;
	}

	mpAllocator->Shutdown();

	if (mpThreads) {
		for(int i=0; i<mThreadCount; ++i) {
			mInputBufferCount.Post();
		}

		delete[] mpThreads;
		mpThreads = NULL;
	}

	FlushInputQueue();
	FlushOutputQueue();

	while(!mClonedCodecs.empty()) {
		IVDVideoCompressor *vc = mClonedCodecs.back();
		mClonedCodecs.pop_back();

		vc->Stop();
		delete vc;
	}
}

void VDThreadedVideoCompressor::SetFlush(bool flush, VDRenderOutputBuffer *flushBuffer) {
	if (mbClientFlushInProgress == flush)
		return;

	mbClientFlushInProgress = flush;
	if (flush) {
		vdsynchronized(mMutex) {
			mbFlushInProgress = true;
			mpFlushBuffer = flushBuffer;

			for(int i=0; i<mThreadCount; ++i)
				mInputBufferCount.Post();
		}
	} else {
		// flush the input queue to stop new frames from starting
		FlushInputQueue();

		// flush the output queue to free up output buffers
		FlushOutputQueue();

		// barrier all threads
		for(int i=0; i<mThreadCount; ++i)
			mBarrier.Wait();

		// flush the output queue again for any frames that went through since the initial flush
		FlushOutputQueue();

		// reset frame numbers, since we may have dropped some frames between the queues
		mNextInputFrameNumber = 0;
		mNextOutputFrameNumber = 0;
		mNextOutputAllocIndex = 0;

		// clear the flush flag
		vdsynchronized(mMutex) {
			mbFlushInProgress = false;
			mbLoopDetectedDuringFlush = false;
		}

		// release all threads
		for(int i=0; i<mThreadCount; ++i)
			mBarrier.Post();
	}
}

void VDThreadedVideoCompressor::SkipFrame() {
	if (mThreadCount)
		++mFrameSkipCounter;
	else
		mpBaseCompressor->SkipFrame();
}

void VDThreadedVideoCompressor::Restart() {
	// barrier all threads
	for(int i=0; i<mThreadCount; ++i)
		mBarrier.Wait();

	mpBaseCompressor->Restart();

	// release all threads
	for(int i=0; i<mThreadCount; ++i)
		mBarrier.Post();
}

bool VDThreadedVideoCompressor::ExchangeBuffer(VDRenderOutputBuffer *buffer, VDRenderPostCompressionBuffer **ppOutBuffer) {
	bool success = false;

	if (mThreadCount) {
		vdsynchronized(mMutex) {
			if (mbInErrorState)
				throw mError;

			if (buffer) {
				buffer->AddRef();

				mInputBuffer.push_back(buffer);
				mInputBufferCount.Post();

				if (!mbFlushInProgress)
					++mFramesSubmitted;

				// extend queue for frame we are pushing in
				OutputEntry oe = {NULL, false};
				mOutputBuffer.push_back(oe);
			}

			if (ppOutBuffer) {
				if (!mOutputBuffer.empty() && mOutputBuffer.front().mbCompleted) {
					*ppOutBuffer = mOutputBuffer.front().mpBuffer;
					mOutputBuffer.pop_front();
					++mNextOutputFrameNumber;
					--mNextOutputAllocIndex;
					success = true;
				}
			}
		}
	} else {
		if (buffer) {
			if (!mbFlushInProgress)
				++mFramesSubmitted;

			if (!ProcessFrame(buffer, mpBaseCompressor, NULL, 0, NULL, NULL)) {
				if (mbInErrorState)
					throw mError;

				return false;
			}
		}

		if (ppOutBuffer) {
			if (!mOutputBuffer.empty() && mOutputBuffer.front().mbCompleted) {
				*ppOutBuffer = mOutputBuffer.front().mpBuffer;
				mOutputBuffer.pop_front();
				success = true;
			}
		}
	}

	return success;
}

void VDThreadedVideoCompressor::RunSlave(IVDVideoCompressor *compressor, VDPixmapBuffer& repack_buffer) {
	VDRTProfileChannel	profchan("VideoCompressor");

	FrameTrackingQueue frameTrackingQueue;
	bool flushing = false;
	bool flushMode = false;
	bool resetCodec = false;

	for(;;) {
		if (!flushing) {
			mBarrier.Post();
			mInputBufferCount.Wait();
			mBarrier.Wait();
		}

		vdrefptr<VDRenderOutputBuffer> buffer;
		int framesToSkip = 0;
		sint32 frameNumber = 0;

		vdsynchronized(mMutex) {
			if (mbInErrorState)
				break;

			if (mbFlushInProgress != flushMode) {
				flushMode = mbFlushInProgress;

				if (!flushMode)
					resetCodec = true;
			}

			// allocate new frames for output buffers in sequential order; we have to
			// do this to ensure we don't have a deadlock with newer frames taking all
			// of the slots before an older frame can be allocated
			while(mNextOutputAllocIndex < mOutputBuffer.size()) {
				uint32 minAlloc = 0;
				if (!frameTrackingQueue.empty())
					minAlloc = frameTrackingQueue.front() - mNextOutputFrameNumber;
				else if (!mInputBuffer.empty())
					minAlloc = mNextInputFrameNumber - mNextOutputFrameNumber;

				if (mNextOutputAllocIndex > minAlloc)
					break;

				vdrefptr<VDRenderPostCompressionBuffer> outputBuffer;

				mMutex.Unlock();

				mpAllocator->AllocFrame(-1, ~outputBuffer);

				mMutex.Lock();

				if (!outputBuffer)
					break;

				if (mNextOutputAllocIndex >= mOutputBuffer.size())
					break;

				VDASSERT(!mOutputBuffer[mNextOutputAllocIndex].mpBuffer);
				mOutputBuffer[mNextOutputAllocIndex].mpBuffer = outputBuffer.release();
				++mNextOutputAllocIndex;
			}

			if (!mInputBuffer.empty()) {
				buffer.set(mInputBuffer.front());
				mInputBuffer.pop_front();

				framesToSkip = mFrameSkipCounter.xchg(0);

				frameNumber = mNextInputFrameNumber++;
			} else if (mpFlushBuffer && !frameTrackingQueue.empty()) {
				buffer = mpFlushBuffer;
				frameNumber = -1;
				flushing = true;
			}
		}

		// If we are coming off a flush, it's possible that we don't have a frame here.
		if (!buffer) {
			VDASSERT(!framesToSkip);
			flushing = false;
			continue;
		}

		if (resetCodec) {
			resetCodec = false;
			frameTrackingQueue.clear();

			if (compressor != mpBaseCompressor)
				compressor->Restart();
		}

		while(framesToSkip--)
			compressor->SkipFrame();

		if (!ProcessFrame(buffer, compressor, &profchan, frameNumber, &frameTrackingQueue, &repack_buffer))
			break;
	}
}

bool VDThreadedVideoCompressor::ProcessFrame(VDRenderOutputBuffer *pBuffer, IVDVideoCompressor *pCompressor, VDRTProfileChannel *pProfileChannel, sint32 frameNumber, FrameTrackingQueue *frameTrackingQueue, VDPixmapBuffer* repack_buffer) {
	vdrefptr<VDRenderPostCompressionBuffer> pOutputBuffer;

	if (frameTrackingQueue) {
		uint32 outputFrameNumber = frameTrackingQueue->empty() ? frameNumber : frameTrackingQueue->front();

		vdsynchronized(mMutex) {
			uint32 offset = outputFrameNumber - mNextOutputFrameNumber;

			VDASSERT((sint32)offset >= 0);
			VDASSERT(offset < mOutputBuffer.size());

			VDASSERT(!mOutputBuffer[offset].mbCompleted);
			pOutputBuffer = mOutputBuffer[offset].mpBuffer;
		}

		VDASSERT(pOutputBuffer);
	} else {
		if (!mpAllocator->AllocFrame(-1, ~pOutputBuffer)) {
			VDASSERT(mbInErrorState);
			return false;
		}
	}

	bool isKey;
	uint32 packedSize;
	bool valid;

	VDPROFILEBEGIN("V-Format");
	VDPixmap& src = pBuffer->mPixmap;
	void* dst = pBuffer->mpBase;
	if (src.format==nsVDPixmap::kPixFormat_XRGB64) {
		// need another buffer for this
		if (!repack_buffer->data) repack_buffer->init(src.w,src.h,src.format);
		dst = repack_buffer->data;
		if (VDPixmap_X16R16G16B16_IsNormalized(src.info)) {
			VDPixmap_X16R16G16B16_to_b64a(*repack_buffer,src);
		} else {
			VDPixmap_X16R16G16B16_Normalize(*repack_buffer,src);
			VDPixmap_X16R16G16B16_to_b64a(*repack_buffer,*repack_buffer);
		}
	}
	VDPROFILEEND();

	VDPROFILEBEGIN("V-Compress");

	try {
		if (frameTrackingQueue && frameNumber >= 0)
			frameTrackingQueue->push_back(frameNumber);

		valid = pCompressor->CompressFrame(pOutputBuffer->mOutputBuffer.data(), dst, isKey, packedSize);

		if (!valid) {
			vdsynchronized(mMutex) {
				if (mbFlushInProgress) {
					if (++mFramesBufferedInFlush >= kReasonableBFrameBufferLimit) {
						mFramesBufferedInFlush = 0;
						mbLoopDetectedDuringFlush = true;
					}
				}
			}
		}

		// check if we got a delta frame in multithreaded mode
		if (!isKey && mThreadCount > 1 && packedSize > 0)
			throw MyError("The video compressor was unable to produce a key frame only sequence. Multithreaded compression must be disabled for this video codec and current compression settings.");

		VDPROFILEEND();
	} catch(MyError& e) {
		VDPROFILEEND();

		vdsynchronized(mMutex) {
			if (!mbInErrorState) {
				mError.TransferFrom(e);
				mbInErrorState = true;
			}
		}
		return false;
	}

	if (valid) {
		if (frameTrackingQueue) {
			frameNumber = frameTrackingQueue->front();
			frameTrackingQueue->pop_front();
		}

		pOutputBuffer->mOutputSize = packedSize;
		pOutputBuffer->mbOutputIsKey = isKey;

		vdsynchronized(mMutex) {
			mFramesBufferedInFlush = 0;

			if (frameTrackingQueue) {
				uint32 offset = frameNumber - mNextOutputFrameNumber;

				VDASSERT((sint32)offset >= 0);

				VDASSERT(mOutputBuffer[offset].mbCompleted == false);
				VDASSERT(mOutputBuffer[offset].mpBuffer == pOutputBuffer);

				mOutputBuffer[offset].mbCompleted = true;
			} else {
				OutputEntry oe = {pOutputBuffer, true};
				pOutputBuffer->AddRef();
				mOutputBuffer.push_back(oe);
			}

			mEventFrameComplete.Raise(this, false);
		}
	}

	return true;
}

void VDThreadedVideoCompressor::FlushInputQueue() {
	vdsynchronized(mMutex) {
		while(!mInputBuffer.empty()) {
			mInputBuffer.front()->Release();
			mInputBuffer.pop_front();
		}

		mpFlushBuffer = NULL;
	}
}

void VDThreadedVideoCompressor::FlushOutputQueue() {
	vdsynchronized(mMutex) {
		while(!mOutputBuffer.empty()) {
			VDRenderPostCompressionBuffer *buf = mOutputBuffer.front().mpBuffer;
			if (buf)
				buf->Release();

			mOutputBuffer.pop_front();
		}
	}
}

/////////

VDThreadedVideoCompressorSlave::VDThreadedVideoCompressorSlave()
	: VDThread("Video compressor")
{
}

VDThreadedVideoCompressorSlave::~VDThreadedVideoCompressorSlave() {
}

void VDThreadedVideoCompressorSlave::Init(VDThreadedVideoCompressor *parent, IVDVideoCompressor *compressor) {
	mpParent = parent;
	mpCompressor = compressor;

	ThreadStart();
}

void VDThreadedVideoCompressorSlave::ThreadRun() {
	mpParent->RunSlave(mpCompressor,repack_buffer);
}
