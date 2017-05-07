//	VirtualDub - Video processing and capture application
//	A/V interface library
//	Copyright (C) 1998-2004 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include <stdafx.h>
#include <vd2/system/thread.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/system/math.h>
#include <vd2/system/profile.h>
#include <vd2/system/time.h>
#include <vd2/VDCapture/capresync.h>
#include <vd2/Priss/convert.h>

///////////////////////////////////////////////////////////////////////////

extern "C" __declspec(align(16)) const sint16 gVDCaptureAudioResamplingKernel[32][8] = {
	{+0x0000,+0x0000,+0x0000,+0x4000,+0x0000,+0x0000,+0x0000,+0x0000 },
	{-0x000a,+0x0052,-0x0179,+0x3fe2,+0x019f,-0x005b,+0x000c,+0x0000 },
	{-0x0013,+0x009c,-0x02cc,+0x3f86,+0x0362,-0x00c0,+0x001a,+0x0000 },
	{-0x001a,+0x00dc,-0x03f9,+0x3eef,+0x054a,-0x012c,+0x002b,+0x0000 },
	{-0x001f,+0x0113,-0x0500,+0x3e1d,+0x0753,-0x01a0,+0x003d,+0x0000 },
	{-0x0023,+0x0141,-0x05e1,+0x3d12,+0x097c,-0x021a,+0x0050,-0x0001 },
	{-0x0026,+0x0166,-0x069e,+0x3bd0,+0x0bc4,-0x029a,+0x0066,-0x0001 },
	{-0x0027,+0x0182,-0x0738,+0x3a5a,+0x0e27,-0x031f,+0x007d,-0x0002 },
	{-0x0028,+0x0197,-0x07b0,+0x38b2,+0x10a2,-0x03a7,+0x0096,-0x0003 },
	{-0x0027,+0x01a5,-0x0807,+0x36dc,+0x1333,-0x0430,+0x00af,-0x0005 },
	{-0x0026,+0x01ab,-0x083f,+0x34db,+0x15d5,-0x04ba,+0x00ca,-0x0007 },
	{-0x0024,+0x01ac,-0x085b,+0x32b3,+0x1886,-0x0541,+0x00e5,-0x0008 },
	{-0x0022,+0x01a6,-0x085d,+0x3068,+0x1b40,-0x05c6,+0x0101,-0x000b },
	{-0x001f,+0x019c,-0x0846,+0x2dfe,+0x1e00,-0x0644,+0x011c,-0x000d },
	{-0x001c,+0x018e,-0x0819,+0x2b7a,+0x20c1,-0x06bb,+0x0136,-0x0010 },
	{-0x0019,+0x017c,-0x07d9,+0x28e1,+0x2380,-0x0727,+0x014f,-0x0013 },
	{-0x0016,+0x0167,-0x0788,+0x2637,+0x2637,-0x0788,+0x0167,-0x0016 },
	{-0x0013,+0x014f,-0x0727,+0x2380,+0x28e1,-0x07d9,+0x017c,-0x0019 },
	{-0x0010,+0x0136,-0x06bb,+0x20c1,+0x2b7a,-0x0819,+0x018e,-0x001c },
	{-0x000d,+0x011c,-0x0644,+0x1e00,+0x2dfe,-0x0846,+0x019c,-0x001f },
	{-0x000b,+0x0101,-0x05c6,+0x1b40,+0x3068,-0x085d,+0x01a6,-0x0022 },
	{-0x0008,+0x00e5,-0x0541,+0x1886,+0x32b3,-0x085b,+0x01ac,-0x0024 },
	{-0x0007,+0x00ca,-0x04ba,+0x15d5,+0x34db,-0x083f,+0x01ab,-0x0026 },
	{-0x0005,+0x00af,-0x0430,+0x1333,+0x36dc,-0x0807,+0x01a5,-0x0027 },
	{-0x0003,+0x0096,-0x03a7,+0x10a2,+0x38b2,-0x07b0,+0x0197,-0x0028 },
	{-0x0002,+0x007d,-0x031f,+0x0e27,+0x3a5a,-0x0738,+0x0182,-0x0027 },
	{-0x0001,+0x0066,-0x029a,+0x0bc4,+0x3bd0,-0x069e,+0x0166,-0x0026 },
	{-0x0001,+0x0050,-0x021a,+0x097c,+0x3d12,-0x05e1,+0x0141,-0x0023 },
	{+0x0000,+0x003d,-0x01a0,+0x0753,+0x3e1d,-0x0500,+0x0113,-0x001f },
	{+0x0000,+0x002b,-0x012c,+0x054a,+0x3eef,-0x03f9,+0x00dc,-0x001a },
	{+0x0000,+0x001a,-0x00c0,+0x0362,+0x3f86,-0x02cc,+0x009c,-0x0013 },
	{+0x0000,+0x000c,-0x005b,+0x019f,+0x3fe2,-0x0179,+0x0052,-0x000a },
};

#ifdef _M_IX86
	extern "C" void __cdecl vdasm_capture_resample16_MMX(sint16 *d, int stride, const sint16 *s, uint32 count, uint64 accum, sint64 inc);
#endif

namespace {
	uint64 resample16(sint16 *d, int stride, const sint16 *s, uint32 count, uint64 accum, sint64 inc) {
#ifdef _M_IX86
		if (MMX_enabled) {
			vdasm_capture_resample16_MMX(d, stride, s, count, accum, inc);
			return accum + inc*count;
		}
#endif

		do {
			const sint16 *s2 = s + (accum >> 32);
			const sint16 *f = gVDCaptureAudioResamplingKernel[(uint32)accum >> 27];

			accum += inc;

			uint32 v= (sint32)s2[0]*(sint32)f[0]
					+ (sint32)s2[1]*(sint32)f[1]
					+ (sint32)s2[2]*(sint32)f[2]
					+ (sint32)s2[3]*(sint32)f[3]
					+ (sint32)s2[4]*(sint32)f[4]
					+ (sint32)s2[5]*(sint32)f[5]
					+ (sint32)s2[6]*(sint32)f[6]
					+ (sint32)s2[7]*(sint32)f[7]
					+ 0x20002000;

			v >>= 14;

			if (v >= 0x10000)
				v = ~v >> 31;

			*d = (sint16)(v - 0x8000);
			d += stride;
		} while(--count);

		return accum;
	}
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

void VDCaptureAudioRateEstimator::Reset() {
	mX = 0;
	mY = 0;
	mX2 = 0;
	mXY = 0;
	mSamples = 0;
}

void VDCaptureAudioRateEstimator::AddSample(sint64 x, sint64 y) {
	++mSamples;

	vdint128 x2;
	x2.setSquare(x);

	mX	+= x;
	mX2	+= x2;
	mY	+= y;
	mXY	+= (vdint128)x * (vdint128)y;
}

bool VDCaptureAudioRateEstimator::GetSlope(double& slope) const {
	if (mSamples < 4)
		return false;

	const double x	= (double)mX;
	const double y	= (double)mY;
	const double x2	= mX2;
	const double xy	= mXY;
	const double n	= mSamples;

	slope = (n*xy - x*y) / (n*x2 - x*x);

	return true;
}

bool VDCaptureAudioRateEstimator::GetXIntercept(double slope, double& xintercept) const {
	xintercept = ((double)mX - (double)mY/slope) / mSamples;
	return true;
}

bool VDCaptureAudioRateEstimator::GetYIntercept(double slope, double& yintercept) const {
	yintercept = ((double)mY - (double)mX*slope) / mSamples;
	return true;
}

///////////////////////////////////////////////////////////////////////////

namespace {
	template<unsigned N>
	class VDCaptureWindowedRegressionEstimator {
	public:
		VDCaptureWindowedRegressionEstimator() { Reset(); }

		void Reset();
		void AddSample(sint64 x, sint64 y);
		bool GetSlope(double& slope) const;
		bool GetXIntercept(double slope, double& xintercept) const;
		bool GetYIntercept(double slope, double& yintercept) const;

	protected:
		struct Sample {
			sint64		x;
			sint64		y;
			vdint128	x2;
			vdint128	xy;
		};

		int		mSamples;
		int		mWindowOffset;

		Sample	mTotal;
		Sample	mWindow[N];
	};

	template<unsigned N>
	void VDCaptureWindowedRegressionEstimator<N>::Reset() {
		mSamples = 0;
		mWindowOffset = 0;

		memset(&mTotal, 0, sizeof mTotal + sizeof mWindow);
	}

	template<unsigned N>
	void VDCaptureWindowedRegressionEstimator<N>::AddSample(sint64 x, sint64 y) {
		Sample& sa = mWindow[mWindowOffset];

		if (++mSamples > N) {
			--mSamples;

			mTotal.x -= sa.x;
			mTotal.y -= sa.y;
			mTotal.x2 -= sa.x2;
			mTotal.xy -= sa.xy;
		}

		vdint128 x2;
		x2.setSquare(x);

		sa.x = x;
		sa.y = y;
		sa.x2 = x2;
		sa.xy = (vdint128)x * (vdint128)y;

		mTotal.x += sa.x;
		mTotal.y += sa.y;
		mTotal.x2 += sa.x2;
		mTotal.xy += sa.xy;

		if (++mWindowOffset >= N)
			mWindowOffset = 0;
	}

	template<unsigned N>
	bool VDCaptureWindowedRegressionEstimator<N>::GetSlope(double& slope) const {
		if (mSamples < N)
			return false;

		const double x	= (double)mTotal.x;
		const double y	= (double)mTotal.y;
		const double x2	= mTotal.x2;
		const double xy	= mTotal.xy;
		const double n	= mSamples;

		slope = (n*xy - x*y) / (n*x2 - x*x);

		return true;
	}

	template<unsigned N>
	bool VDCaptureWindowedRegressionEstimator<N>::GetXIntercept(double slope, double& xintercept) const {
		xintercept = ((double)mTotal.x - (double)mTotal.y/slope) / mSamples;
		return true;
	}

	template<unsigned N>
	bool VDCaptureWindowedRegressionEstimator<N>::GetYIntercept(double slope, double& yintercept) const {
		yintercept = ((double)mTotal.y - (double)mTotal.x*slope) / mSamples;
		return true;
	}
}

///////////////////////////////////////////////////////////////////////////

namespace {
	template<class T, unsigned N>
	class MovingAverage {
	public:
		MovingAverage()
			: mPos(0)
			, mSum(0)
			, mbInited(false)
		{
			for(unsigned i=0; i<N; ++i)
				mArray[i] = 0;
		}


		T operator()(T v) {
			if (!mbInited) {
				for(int i=0; i<N; ++i)
					mArray[i] = v;

				mbInited = true;
				return v;
			}

			mArray[mPos] = v;
			if (++mPos >= N)
				mPos = 0;

			return operator()();
		}

		T operator()() const {
			// Note: This is an O(n) sum rather than an O(1) sum table approach
			// because the latter is unsafe with floating-point values.
			T sum(0);
			for(unsigned i=0; i<N; ++i)
				sum += mArray[i];

			return sum * (T(1)/N);
		}

	protected:
		unsigned mPos;
		bool mbInited;
		T mSum;
		T mArray[N];
	};

	class VDCaptureVideoTimingController {
	public:
		void Init(double fps, IVDCaptureProfiler *pProfiler);

		sint64	Run(sint64 timeIn);
		void	PostUpdate(double frameError);

		double	GetProjectionRate() const { return mProjectionRate; }

	protected:
		sint64		mFirstTime;
		sint64		mAccumTime;
		sint64		mLastTime;
		double		mPeriod;
		sint64		mPeriodInt;
		double		mInvPeriod;
		sint64		mDropThreshold;
		int			mProportionalControllerKillTime;
		int			mIntegralControllerKillTime;

		double		mProjectedTime;
		double		mProjectionRate;
		double		mPeriodAverage;
		double		mOffsetErrorAverage;

		IVDCaptureProfiler *mpProfiler;
		int			mProfileResampleRate;
		int			mProfileOffsetError;
	};

	void VDCaptureVideoTimingController::Init(double fps, IVDCaptureProfiler *pProfiler) {
		mAccumTime		= -1;
		mLastTime		= -1;
		mPeriod			= 1000000.0 / fps;
		mPeriodInt		= VDRoundToInt64(mPeriod);
		mInvPeriod		= fps / 1000000.0;
		mDropThreshold	= mPeriodInt*3/4;
		mIntegralControllerKillTime = 0;
		mProportionalControllerKillTime = 0;

		mProjectedTime		= 0;
		mProjectionRate		= 1.0;
		mPeriodAverage		= 1.0;
		mOffsetErrorAverage	= 0;

		mpProfiler		= pProfiler;
		mProfileResampleRate = -1;
		if (pProfiler) {
			mProfileResampleRate	= pProfiler->RegisterStatsChannel("Video resampling rate");
			mProfileOffsetError		= pProfiler->RegisterStatsChannel("Video offset error");
		}
	}

	sint64 VDCaptureVideoTimingController::Run(sint64 t) {
		// Apply smoothing.
		if (mLastTime < 0) {
			mLastTime = 0;
			mFirstTime = t;
			mAccumTime = 0;
			mProjectedTime = 0;
			return 0;
		}

		t -= mFirstTime;

		sint64 delta = t - (mLastTime + mPeriodInt);

		// Check if we have a large or small delta that indicates an irregularity
		// in the stream timing. If one is present, most likely due to a drop, do
		// not average the delta -- pass it right through -- and temporarily kill
		// the proportional controller to prevent it from kicking the resampling
		// factor.
		if (abs((long)delta) < mDropThreshold)
			delta = (t - mPeriodInt - mAccumTime)/32;
		else
			mProportionalControllerKillTime = 8;

		delta += mPeriodInt;

		mAccumTime += delta;

		mLastTime = t;
		t = mAccumTime;

		// Forward project next time.
		double projectedDelta = delta * mProjectionRate;
		mProjectedTime += projectedDelta;
		t = VDRoundToInt64(mProjectedTime);

		// Compute period error and average
		if (!mProportionalControllerKillTime)
			mPeriodAverage += (delta*mInvPeriod - mPeriodAverage) * 0.001;
		double periodError = mPeriodAverage * mProjectionRate - 1.0;

//		VDDEBUG2("periodError = %g %g\n", periodError, delta*mInvPeriod);

		// Apply proportional controller. The proportional controller, classically,
		// is responsible for killing slope errors. Our process value is a bit
		// noisy for that, but here it mainly helps kill the oscillation from the
		// integral controller.
		const double Kp = -0.05;

		if (mProportionalControllerKillTime)
			--mProportionalControllerKillTime;
		else
			mProjectionRate += Kp * periodError;

		return t;
	}

	void VDCaptureVideoTimingController::PostUpdate(double offsetError) {
		// Compute smoothed offset error
		offsetError = (mOffsetErrorAverage += (offsetError - mOffsetErrorAverage)*0.1);
		if (mpProfiler)
			mpProfiler->AddDataPoint(mProfileOffsetError, (float)offsetError);

		// Apply integral controller. The integral controller, classically, is
		// responsible for pushing the steady-state error to zero. In our case
		// we use a modified I controller that is based off of the offset error
		// instead of the period error instead, since we want to push the frame
		// offset error to zero.
		const double Ki = -0.001;

		mProjectionRate += Ki * offsetError;

		if (mProjectionRate < 0.8)
			mProjectionRate = 0.8;

		if (mProjectionRate > 1.2)
			mProjectionRate = 1.2;

		if (mpProfiler) {
			// Use a log scale such at a +1.0 delta is 1% faster/slower.
			mpProfiler->AddDataPoint(mProfileResampleRate, (float)log(mProjectionRate) * 100.49917080713052880106636866079f);
		}
	}

	class VDPIDController {
	public:
		VDPIDController() { Reset(); }

		void Reset();

		void SetControlValueRange(double cvMin, double cvMax) {
			mCVMin = cvMin;
			mCVMax = cvMax;
		}

		void SetControlValue(double cv) {
			mCV = mCVPD = cv;
			mCVI = 0;
		}

		void SetSetPoint(double sp) {
			mSP = sp;
		}

		void SetIntegralLimit() {
			mILimit = 1e+30f;
		}

		void SetIntegralLimit(double limit) {
			mILimit = limit;
		}

		void SetProportionalHysteresis(double hys, double hysbleed) {
			mPHysteresis = hys;
			mPHysBleed = hysbleed;
		}

		void SetProportionalLimit(double limit) {
			mPLimit = limit;
		}

		void SetResponse(double p, double i, double d);

		double Run(double pv, double pvi, double t);

		double GetControlValue() const { return mCV; }

	protected:
		int		mPreload;		///< Delay before starting controller to prime process value history.
		double	mCVPD;			///< Current control value (proportional/derivative).
		double	mCVI;			///< Current control value (integral).
		double	mCV;			///< Current control valeu (P+I+D).
		double	mCVMin;			///< Hard minimum for control value.
		double	mCVMax;			///< Hard maximum for control value.
		double	mPHysteresis;	///< Proportional controller hysteresis.
		double	mPHysBleed;		///< Proportional controller hysteresis bleed/interval.
		double	mPHysAccum;		///< Proportional controller hysteresis accumulator.
		double	mPLimit;		///< Proportional controller limit.
		double	mILimit;		///< Integral controller limit.
		double	mSP;			///< Set point.
		double	mKp;			///< Proportional control constant.
		double	mKi;			///< Integral control constant.
		double	mKd;			///< Derivative control constant.
		double	mPV1;			///< Previous process value.
		double	mPV2;			///< Prev-prev process value.
	};

	void VDPIDController::Reset() {
		mPreload = 2;
		mCVPD	= 0;
		mCVI	= 0;
		mCVMin	= 0;
		mCVMax	= 0;
		mPHysteresis = 0;
		mPHysAccum = 0;
		mPHysBleed = 0;
		mPLimit	= 1e+20f;
		mILimit	= 1e+30f;
		mSP		= 0;
		mKp		= 0;
		mKi		= 0;
		mKd		= 0;
		mPV1	= 0;
		mPV2	= 0;
	}

	void VDPIDController::SetResponse(double p, double i, double d) {
		mKp = p;
		mKi = i;
		mKd = d;
	}

	double VDPIDController::Run(double pv0, double pvi, double t) {
		if (mPreload) {
			--mPreload;
			return mCVPD + mCVI;
		}

		// Apply hysteresis to the PV input to the P controller to avoid
		// bumping the CV on routine noise. Apply a limiter to prevent
		// a hard kick to the error from trashing the output.
		double Pk = 0;
		double PLimit = mPLimit * t;
		mPHysAccum += (mPV1 - pv0);
		if (mPHysAccum < -mPHysteresis) {
			Pk = mPHysAccum + mPHysteresis;
			if (Pk < -PLimit)
				Pk = -PLimit;
		} else if (mPHysAccum > +mPHysteresis) {
			Pk = mPHysAccum - mPHysteresis;
			if (Pk > PLimit)
				Pk = PLimit;
		}

		mPHysAccum -= Pk;

		Pk += mPHysAccum * mPHysBleed;
		mPHysAccum *= (1.0 - mPHysBleed);

		Pk *= mKp;

		double Ik = (mKi * t) * (mSP - pvi);
		double Dk = (mKd / t) * (2*mPV1 - (pv0 + mPV2));

		mPV2 = mPV1;
		mPV1 = pv0;

		mCVPD += Pk + Dk;
		mCVI += Ik;

		double IToPD = mCVI * 0.01f;
		mCVPD += IToPD;
		mCVI -= IToPD;

		// clamp proportional/derivative contribution
		if (!(mCVPD >= mCVMin))
			mCVPD = mCVMin;
		if (!(mCVPD <= mCVMax))
			mCVPD = mCVMax;

		// clamp integral contribution

		if (fabs(mCVI) > mILimit)
			mCVI = (mCVI<0) ? -mILimit : mILimit;

		// clamp output
		mCV = mCVPD + mCVI;

		if (!(mCV >= mCVMin))
			mCV = mCVMin;
		if (!(mCV <= mCVMax))
			mCV = mCVMax;

		return mCV;
	}
}

///////////////////////////////////////////////////////////////////////////

class VDCaptureResyncFilter : public IVDCaptureResyncFilter {
public:
	VDCaptureResyncFilter();
	~VDCaptureResyncFilter();

	void SetChildCallback(IVDCaptureDriverCallback *pChild);
	void SetProfiler(IVDCaptureProfiler *pProfiler);
	void SetVideoRate(double fps);
	void SetAudioRate(double bytesPerSec);
	void SetAudioChannels(int chans);
	void SetAudioFormat(VDAudioSampleType type);
	void SetResyncMode(Mode mode);
	void EnableVideoTimingCorrection(bool en);
	void EnableVideoDrops(bool enable);
	void EnableVideoInserts(bool enable);
	void SetVideoInsertLimit(int insertLimit);
	void SetFixedAudioLatency(int latencyInMilliseconds);
	void SetLimitedAutoAudioLatency(int samples);
	void SetAutoAudioLatency();
	void EnableAudioClock(bool enable);

	void GetStatus(VDCaptureResyncStatus&);

	void CapBegin(sint64 global_clock);
	void CapEnd(const MyError *pError);
	bool CapEvent(nsVDCapture::DriverEvent event, int data);
	void CapProcessData(int stream, const void *data, uint32 size, sint64 timestamp, bool key, sint64 global_clock);

protected:
	void ResampleAndDispatchAudio(const void *data, uint32 size, bool key, sint64 global_clock, double resampling_rate);
	void UnpackSamples(sint16 *dst, ptrdiff_t dstStride, const void *src, uint32 samples, uint32 channels);

	IVDCaptureDriverCallback *mpCB;
	IVDCaptureProfiler *mpProfiler;

	bool		mbAdjustVideoTime;
	bool		mbAllowDrops;
	bool		mbAllowInserts;
	int			mInsertLimit;
	Mode		mMode;

	bool		mbEnableFixedAudioLatency;
	bool		mbEnableAudioClock;
	int			mAutoAudioLatencyLimit;
	double		mFixedAudioLatency;

	double		mLastSyncError;

	sint64		mGlobalClockBase;
	sint64		mVideoLastTime;
	sint64		mVideoLastRawTime;
	sint64		mVideoLastAccurateTime;
	sint64		mVideoTimingWrapAdjust;
	sint64		mVideoTimingAdjust;
	sint64		mVideoFramesWritten;
	sint64		mAudioBytes;
	sint64		mAudioWrittenBytes;
	double		mInvAudioRate;
	double		mVideoRate;
	double		mInvVideoRate;
	double		mAudioRate;
	VDCaptureVideoTimingController	mVideoTimingController;
	VDPIDController		mAudioResamplingController;
	int			mChannels;
	VDCaptureAudioRateEstimator	mVideoRealRateEstimator;
	VDCaptureAudioRateEstimator	mAudioRealRateEstimator;
	VDCaptureAudioRateEstimator	mAudioRelativeRateEstimator;
	VDCaptureWindowedRegressionEstimator<11>	mVideoWindowedRateEstimator;
	VDCaptureWindowedRegressionEstimator<11>	mAudioRelativeWindowedRateEstimator;
	VDCriticalSection	mcsLock;

	tpVDConvertPCM	mpAudioDecoder16;
	tpVDConvertPCM	mpAudioEncoder16;
	uint32		mBytesPerInputSample;

	sint32		mInputLevel;
	uint32		mAccum;
	vdblock<sint16>	mInputBuffer;
	vdblock<sint16>	mOutputBuffer;
	vdblock<char>	mEncodingBuffer;

	MovingAverage<double, 8>	mCurrentLatencyAverage;
	MovingAverage<double, 8>	mAudioStartAverage;

	int			mProfileSyncError;
};

VDCaptureResyncFilter::VDCaptureResyncFilter()
	: mpCB(NULL)
	, mpProfiler(NULL)
	, mbAdjustVideoTime(true)
	, mbAllowDrops(true)
	, mbAllowInserts(true)
	, mInsertLimit(10)
	, mMode(kModeNone)
	, mbEnableFixedAudioLatency(false)
	, mbEnableAudioClock(true)
	, mAutoAudioLatencyLimit(30)
	, mVideoLastTime(0)
	, mVideoLastRawTime(0)
	, mVideoLastAccurateTime(0)
	, mVideoTimingWrapAdjust(0)
	, mVideoTimingAdjust(0)
	, mAudioBytes(0)
	, mAudioRate(0)
	, mInvAudioRate(0)
	, mVideoRate(1)
	, mInvVideoRate(1)
	, mChannels(0)
	, mpAudioDecoder16(NULL)
	, mpAudioEncoder16(NULL)
	, mBytesPerInputSample(0)
{
}

VDCaptureResyncFilter::~VDCaptureResyncFilter() {
}

IVDCaptureResyncFilter *VDCreateCaptureResyncFilter() {
	return new VDCaptureResyncFilter;
}

void VDCaptureResyncFilter::SetChildCallback(IVDCaptureDriverCallback *pChild) {
	mpCB = pChild;
}

void VDCaptureResyncFilter::SetProfiler(IVDCaptureProfiler *pProfiler) {
	mpProfiler = pProfiler;
}

void VDCaptureResyncFilter::SetVideoRate(double fps) {
	mVideoRate = fps;
	mInvVideoRate = 1.0 / fps;
}

void VDCaptureResyncFilter::SetAudioRate(double bytesPerSec) {
	mAudioRate = bytesPerSec;
	mInvAudioRate = 1.0 / bytesPerSec;
}

void VDCaptureResyncFilter::SetAudioChannels(int chans) {
	mChannels = chans;
}

void VDCaptureResyncFilter::SetAudioFormat(VDAudioSampleType type) {
	mpAudioDecoder16 = NULL;
	mpAudioEncoder16 = NULL;

	if (type != kVDAudioSampleType16S) {
		tpVDConvertPCMVtbl tbl = VDGetPCMConversionVtable();
		mpAudioDecoder16 = tbl[type][kVDAudioSampleType16S];
		mpAudioEncoder16 = tbl[kVDAudioSampleType16S][type];
	}

	mBytesPerInputSample = 1<<type;
}

void VDCaptureResyncFilter::SetResyncMode(Mode mode) {
	mMode = mode;
}

void VDCaptureResyncFilter::EnableVideoTimingCorrection(bool en) {
	mbAdjustVideoTime = en;
}

void VDCaptureResyncFilter::EnableVideoDrops(bool enable) {
	mbAllowDrops = enable;
}

void VDCaptureResyncFilter::EnableVideoInserts(bool enable) {
	mbAllowInserts = enable;
}

void VDCaptureResyncFilter::SetVideoInsertLimit(int insertLimit) {
	mInsertLimit = insertLimit;
}

void VDCaptureResyncFilter::SetFixedAudioLatency(int latencyInMilliseconds) {
	mbEnableFixedAudioLatency = true;
	mFixedAudioLatency = latencyInMilliseconds / 1000.0f;
}

void VDCaptureResyncFilter::SetLimitedAutoAudioLatency(int samples) {
	mbEnableFixedAudioLatency = false;
	mAutoAudioLatencyLimit = samples;
}

void VDCaptureResyncFilter::SetAutoAudioLatency() {
	mbEnableFixedAudioLatency = false;
	mAutoAudioLatencyLimit = 0;
}

void VDCaptureResyncFilter::EnableAudioClock(bool enable) {
	mbEnableAudioClock = enable;
}

void VDCaptureResyncFilter::GetStatus(VDCaptureResyncStatus& status) {
	vdsynchronized(mcsLock) {
		double rate, latency;
		status.mVideoTimingAdjust	= (sint32)mVideoTimingAdjust;
		status.mVideoResamplingRate	= (float)mVideoTimingController.GetProjectionRate();
		status.mAudioResamplingRate	= (float)mAudioResamplingController.GetControlValue();
		status.mCurrentLatency		= 0.f;
		status.mMeasuredLatency		= 0.f;

		if (mAudioRelativeRateEstimator.GetSlope(rate) && mAudioRelativeRateEstimator.GetXIntercept(rate, latency)) {
			status.mMeasuredLatency = (float)(mAudioStartAverage() * 1e+6f);
			status.mCurrentLatency	= (float)mLastSyncError;
		}
	}
}

void VDCaptureResyncFilter::CapBegin(sint64 global_clock) {
	mInputLevel = 0;
	mAccum = 0;
	mAudioWrittenBytes		= 0;
	mVideoLastTime			= 0;
	mVideoLastAccurateTime	= 0;
	mVideoLastRawTime		= 0;
	mVideoFramesWritten		= 0;

	mLastSyncError			= 0;

	mAudioResamplingController.SetControlValue(1.0);
	mAudioResamplingController.SetControlValueRange(0.1, 10.0);
	mAudioResamplingController.SetSetPoint(0);
	mAudioResamplingController.SetResponse(0.1, 0.01, 0);
	mAudioResamplingController.SetIntegralLimit(0.1f);				// in resampling ratio
	mAudioResamplingController.SetProportionalHysteresis(0.010, 0.1);	// apply 10ms of hysteresis, 10% bleedoff per interval
	mAudioResamplingController.SetProportionalLimit(1.0);			// limit to 1s of error response per 1s period

	mVideoTimingController.Init(mVideoRate, mpProfiler);

	mProfileSyncError = -1;
	if (mpProfiler)
		mProfileSyncError = mpProfiler->RegisterStatsChannel("Sync error");

	mInputBuffer.resize(4096 * mChannels);
	memset(mInputBuffer.data(), 0, mInputBuffer.size() * sizeof(mInputBuffer[0]));
	mOutputBuffer.resize(4096 * mChannels);

	if (mpAudioEncoder16)
		mEncodingBuffer.resize(4096 * mChannels * mBytesPerInputSample);

	mpCB->CapBegin(global_clock);
}

void VDCaptureResyncFilter::CapEnd(const MyError *pError) {
	mpCB->CapEnd(pError);
}

bool VDCaptureResyncFilter::CapEvent(nsVDCapture::DriverEvent event, int data) {
	return mpCB->CapEvent(event, data);
}

void VDCaptureResyncFilter::CapProcessData(int stream, const void *data, uint32 size, sint64 timestamp, bool key, sint64 global_clock)  {
	if (stream == 0) {
		// Correct for one form of the 71-minute bug.
		//
		// The video capture driver apparently computes a time in microseconds and then divides by
		// 1000 to convert to milliseconds, but doesn't compensate for when the microsecond counter
		// overflows past 2^32.  This results in a wraparound from 4294967ms (1h 11m 34s) to 0ms.
		// We must detect this and compensate for the wrap.
		//
		// Some Matrox drivers wrap at 2^31 too....

		if (timestamp < mVideoLastRawTime && timestamp < 10000000 && mVideoLastRawTime >= VD64(2138000000)) {

			// Perform sanity checks.  We should be within ten seconds of the last frame.
			sint64 bias;
			
			if (mVideoLastRawTime >= VD64(4285000000))
				bias = VD64(4294967296);	// 71 minute bug
			else
				bias = VD64(2147483648);	// 35 minute bug

			sint64 newtimestamp = timestamp + bias;

			if (newtimestamp < mVideoLastRawTime + 5000000 && newtimestamp >= mVideoLastRawTime - 5000000)
				mVideoTimingWrapAdjust += bias;
		}

		mVideoLastRawTime = timestamp;

		timestamp += mVideoTimingWrapAdjust;
	}

	double audioResamplingRate;
	bool frameKill = false;

	vdsynchronized(mcsLock) {
		if (stream == 0) {
			int vcount = mVideoRealRateEstimator.GetCount();

			// update video-to-real-time regression
			mVideoRealRateEstimator.AddSample(global_clock, timestamp);

			// apply video timing correction
			if (mbAdjustVideoTime)
				timestamp = mVideoTimingController.Run(timestamp);

			// apply video timing adjustment (AV sync by video time bump)
			mVideoLastTime = timestamp;
			timestamp += mVideoTimingAdjust;

			// compute frame error
			double frame = timestamp * mVideoRate * (1.0 / 1000000.0);
			double frameError = frame - mVideoFramesWritten;

			if (frameError < -0.75 && mbAllowDrops) {
				// don't flag event for null frames
				if (size)
					mpCB->CapEvent(nsVDCapture::kEventVideoFramesDropped, 1);
				frameKill = true;
			} else {
				double threshold = +0.75;
				if (!size)
					threshold += mInsertLimit;

				// Don't allow inserts before the first frame.
				if (frameError > threshold && mbAllowInserts && mVideoFramesWritten) {
					int framesToInsert = VDRoundToInt(frameError);

					if (framesToInsert > mInsertLimit)
						framesToInsert = mInsertLimit;

					mpCB->CapEvent(nsVDCapture::kEventVideoFramesInserted, framesToInsert);
					frameError -= framesToInsert;
					mVideoFramesWritten += framesToInsert;
				}

				if (size)
					++mVideoFramesWritten;
			}

			// don't pass through null frames
			if (!size)
				frameKill = true;

			// update video timing controller
			if (mbAdjustVideoTime)
				mVideoTimingController.PostUpdate(frameError);

			mVideoWindowedRateEstimator.AddSample(global_clock, timestamp);

			mVideoLastAccurateTime = global_clock;

		} else if (stream == 1 && mMode) {
			sint64 estimatedVideoTime;
			
			if (mbEnableAudioClock && timestamp >= 0)
				estimatedVideoTime = timestamp;
			else
				estimatedVideoTime = mVideoLastTime + (global_clock - mVideoLastAccurateTime);

			mAudioBytes += size;

			mAudioRelativeRateEstimator.AddSample(estimatedVideoTime, mAudioBytes);
			mAudioRealRateEstimator.AddSample(global_clock, mAudioBytes);

			double estimatedVideoTimeSlope;

			bool videoTimingOK = mVideoRealRateEstimator.GetSlope(estimatedVideoTimeSlope);

			if (videoTimingOK) {
				if (mMode == kModeResampleVideo) {
					int count = mAudioRelativeRateEstimator.GetCount();
					double rate;
					if (count > 8 && mAudioRelativeRateEstimator.GetSlope(rate)) {
						double adjustedRate = rate * ((double)(mVideoLastTime + mVideoTimingAdjust) / mVideoLastTime);
						double audioBytesPerSecond = adjustedRate * 1000000.0;
						double audioSecondsPerSecond = audioBytesPerSecond * mInvAudioRate;
						double errorSecondsPerSecond = audioSecondsPerSecond - 1.0;
						double errorMicroseconds = errorSecondsPerSecond * mVideoLastTime;
						double errorSeconds = errorMicroseconds / 1000000.0;
						double errorFrames = errorSeconds * mVideoRate;

						mLastSyncError = errorMicroseconds;

						if (fabs(errorFrames) >= 0.8) {
							sint32 errorAdj = -(sint32)(errorFrames / mVideoRate * 1000000.0);

							VDDEBUG("Applying delta of %d us -- total delta %d us\n", errorAdj, (sint32)mVideoTimingAdjust);
							mVideoTimingAdjust += errorAdj;
						}
					}
				} else if (mMode == kModeResampleAudio) {
					double videoRate, videoYIntercept;
					double audioRate, audioYIntercept;

					if (mVideoWindowedRateEstimator.GetSlope(videoRate) && mVideoWindowedRateEstimator.GetYIntercept(videoRate, videoYIntercept)
						&& mAudioRealRateEstimator.GetSlope(audioRate) && mAudioRealRateEstimator.GetYIntercept(audioRate, audioYIntercept)) {
						double videoTime;
						
						if (mbEnableAudioClock && timestamp >= 0)
							videoTime = (double)timestamp;
						else
							videoTime = videoYIntercept + videoRate * global_clock;

						// Assume we drop no audio and that audio rate is consistent.
						// Backproject with full global>audio estimator to determine audio pos at global time 0.
						// Compute appropriate audio position given video time, extrapolated using local global>video
						// estimator from global time, and add audio offset.
						// Use controller to drive byte delta to zero.

						double audioTimeAtVideoTimeZero;
						
						if (mbEnableFixedAudioLatency)
							audioTimeAtVideoTimeZero = mFixedAudioLatency;
						else {
							audioTimeAtVideoTimeZero = (audioYIntercept + audioRate * (global_clock - videoTime)) * mInvAudioRate;

							if (mAutoAudioLatencyLimit && mAudioRealRateEstimator.GetCount() >= mAutoAudioLatencyLimit) {
								mFixedAudioLatency = audioTimeAtVideoTimeZero;
								mbEnableFixedAudioLatency = true;
							}
						}

						double currentLatency = mCurrentLatencyAverage(videoTime * 1e-6 - (mAudioWrittenBytes+size) * mInvAudioRate);
						double latencyError = currentLatency + mAudioStartAverage(audioTimeAtVideoTimeZero);
						int count = mAudioRealRateEstimator.GetCount();

						mLastSyncError = latencyError * 1e+6f;

						if (mpProfiler)
							mpProfiler->AddDataPoint(mProfileSyncError, (float)(latencyError * mVideoRate));

						// interpolate gradually over 100 samples
						if (count < 100)
							latencyError *= count/100.0;

						mAudioResamplingController.Run(latencyError, latencyError, size * mInvAudioRate);
					}
				}
			}

			audioResamplingRate = mAudioResamplingController.GetControlValue();
		}
	}

	if (!frameKill) {
		if (stream == 1 && mMode == kModeResampleAudio)
			ResampleAndDispatchAudio(data, size, key, global_clock, audioResamplingRate);
		else
			mpCB->CapProcessData(stream, data, size, timestamp, key, global_clock);
	}
}

void VDCaptureResyncFilter::ResampleAndDispatchAudio(const void *data, uint32 size, bool key, sint64 global_clock, double audioResamplingRate) {
	int samples = size / (mChannels * mBytesPerInputSample);

	while(samples > 0) {
		int tc = 4096 - mInputLevel;

		VDASSERT(tc >= 0);
		if (tc > samples)
			tc = samples;

		int base = mInputLevel;

		samples -= tc;
		mInputLevel += tc;

		// resample
		uint32 inc = VDRoundToInt(audioResamplingRate * 65536.0);
		int limit = 0;

		const int chans = mChannels;

		VDPROFILEBEGIN("A-Copy");
		UnpackSamples(mInputBuffer.data() + base, 4096*sizeof(mInputBuffer[0]), data, tc, chans);
		VDPROFILEEND();

		if ((uint32)mInputLevel >= (mAccum>>16) + 8) {
			limit = ((mInputLevel << 16)-0x70000-mAccum + (inc-1)) / inc;
			if (limit > 4096)
				limit = 4096;

			uint32 accum0 = mAccum;

			VDPROFILEBEGIN("A-Filter");
			for(int chan=0; chan<chans; ++chan) {
				const sint16 *src = mInputBuffer.data() + 4096*chan;
				sint16 *dst = mOutputBuffer.data() + chan;

				mAccum = (uint32)(resample16(dst, chans, src, limit, (uint64)accum0<<16, (sint64)inc<<16) >> 16);

				int pos = mAccum >> 16;

				if (pos <= mInputLevel)
					memmove(&mInputBuffer[4096*chan], &mInputBuffer[4096*chan + pos], (mInputLevel - pos)*2);
			}
			VDPROFILEEND();
		}

		int shift = mAccum >> 16;
		if (shift > mInputLevel)
			shift = mInputLevel;
		mInputLevel -= shift;
		VDASSERT((unsigned)mInputLevel < 4096);
		mAccum -= shift << 16;

		const void *p = mOutputBuffer.data();
		uint32 size = limit * chans * 2;

		if (mpAudioEncoder16) {
			void *dst = mEncodingBuffer.data();

			mpAudioEncoder16(dst, p, limit * chans);

			size = limit * chans * mBytesPerInputSample;
			p = dst;
		}

		if (limit) {
			mAudioWrittenBytes += size;

			mpCB->CapProcessData(1, p, size, 0, key, global_clock);
		}

		data = (char *)data + mBytesPerInputSample*chans*tc;
	}
}

namespace {
	void strided_copy_16(sint16 *dst, ptrdiff_t dstStride, const sint16 *src, uint32 samples, uint32 channels) {
		dstStride -= samples * sizeof(sint16);
		for(uint32 ch=0; ch<channels; ++ch) {
			const sint16 *src2 = src++;

			for(uint32 s=0; s<samples; ++s) {
				*dst++ = *src2;
				src2 += channels;
			}

			dst = (sint16 *)((char *)dst + dstStride);
		}
	}
}

void VDCaptureResyncFilter::UnpackSamples(sint16 *dst, ptrdiff_t dstStride, const void *src, uint32 samples, uint32 channels) {
	if (!samples)
		return;

	if (!mpAudioDecoder16) {
		strided_copy_16(dst, dstStride, (const sint16 *)src, samples, channels);
		return;
	}

	sint16 buf[512];
	uint32 stripSize = 512 / channels;

	while(samples > 0) {
		uint32 tc = std::min<uint32>(stripSize, samples);

		mpAudioDecoder16(buf, src, tc * channels);

		strided_copy_16(dst, dstStride, buf, tc, channels);

		dst += tc;
		src = (const char *)src + tc*channels*mBytesPerInputSample;
		samples -= tc;
	}
}
