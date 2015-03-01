// IDCT test module for VirtualDub
//
// This code is based on a test harness released into the public domain by
// Tom Lane, the comment block of which is reproduced below:
//
	/*
	 * ieeetest.c --- test IDCT code against the IEEE Std 1180-1990 spec
	 *
	 * Note that this does only one pass of the test.
	 * Six invocations of ieeetest are needed to complete the entire spec.
	 * The shell script "doieee" performs the complete test.
	 *
	 * Written by Tom Lane (tgl@cs.cmu.edu).
	 * Released to public domain 11/22/93.
	 */
//
// As Mr. Lane graciously released his test harness into the public domain,
// I think it only fair that I do the same for mine that is based on his.
// Note that this applies only to this module, and not to other parts of
// VirtualDub.
//
// Modified by Avery Lee <phaeron@virtualdub.org>
// Released to public domain 06/07/2003.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <vd2/Meia/MPEGIDCT.h>

namespace {
	typedef int DCTELEM;

	inline DCTELEM fastround(double v) {
		return v>0 ? (DCTELEM)(v+0.5) : -(DCTELEM)(0.5-v);
	}

	// Pseudo-random generator specified by IEEE 1180
	class IEEE1180Random {
	public:
		IEEE1180Random() : seed(1) {}

		long seed;

		long operator()(long L, long H) {
		  static double z = (double) 0x7fffffff;

		  long i,j;
		  double x;

		  seed = (seed * 1103515245) + 12345;
		  i = seed & 0x7ffffffe;
		  x = ((double) i) / z;
		  x *= (L+H+1);
		  j = (long)x;
		  return j-L;
		}
	};

	class IDCTTest {
	public:
		IDCTTest(const VDMPEGIDCTSet& idct);

		bool Test(VDIDCTComplianceResult& result);

	protected:
		bool RunSingleTest(long minpix, long maxpix, long sign, long niters, VDIDCTTestResult& result);
		void RunTestIDCT(DCTELEM block[64]);
		void ReferenceFDCT(DCTELEM block[8][8]);
		void ReferenceIDCT(DCTELEM block[8][8]);

		const VDMPEGIDCTSet& mIDCT;
		double	mBasis[8][8];		// mBasis[a][b] = C(b)/2 * cos[(2a+1)b*pi/16]
	};

	IDCTTest::IDCTTest(const VDMPEGIDCTSet& idct) : mIDCT(idct) {
		// init DCT basis function lookup table
		for(int a=0; a<8; ++a) {
			for(int b=0; b<8; ++b) {
				double tmp = cos(((a+a+1)*b) * (3.14159265358979323846 / 16.0));
				if(b==0)
					tmp /= sqrt(2.0);
				mBasis[a][b] = tmp * 0.5;
			}
		}
	}

	bool IDCTTest::Test(VDIDCTComplianceResult& result) {
		bool passed;
		
		passed  = RunSingleTest(-256, 255,  1, 10000, result.tests[0]);
		passed &= RunSingleTest(  -5,   5,  1, 10000, result.tests[1]);
		passed &= RunSingleTest(-300, 300,  1, 10000, result.tests[2]);
		passed &= RunSingleTest(-256, 255, -1, 10000, result.tests[3]);
		passed &= RunSingleTest(  -5,   5, -1, 10000, result.tests[4]);
		passed &= RunSingleTest(-300, 300, -1, 10000, result.tests[5]);

		/* test for 0 input giving 0 output */
		DCTELEM testout[64]={0};
		RunTestIDCT(testout);
		result.mbZeroTestOK = true;
		for (int i = 0; i < 64; i++) {
			if (testout[i]) {
				result.mbZeroTestOK = false;
				passed = false;
				break;
			}
		}

		return result.mbPassed = passed;
	}

	bool IDCTTest::RunSingleTest(long minpix, long maxpix, long sign, long niters, VDIDCTTestResult& result) {
		memset(&result, 0, sizeof result);

		result.mRangeLow	= minpix;
		result.mRangeHigh	= maxpix;
		result.mSign		= sign;
		result.mIterations	= niters;
		
		long curiter;
		int i;
		double max, total;
		DCTELEM   block[64];	/* random source data */
		DCTELEM   refcoefs[64]; /* coefs from reference FDCT */
		DCTELEM   refout[64];	/* output from reference IDCT */
		DCTELEM   testout[64]; /* output from test IDCT */
		
		/* Loop once per generated random-data block */
		
		IEEE1180Random randgen;
		
		for (curiter = 0; curiter < niters; curiter++) {
			
			/* generate a pseudo-random block of data */
			for (i = 0; i < 64; i++)
				block[i] = (DCTELEM) (randgen(-minpix,maxpix) * sign);
			
			/* perform reference FDCT */
			memcpy(refcoefs, block, sizeof(DCTELEM)*64);
			ReferenceFDCT((DCTELEM (*)[8])refcoefs);
			/* clip */
			for (i = 0; i < 64; i++) {
				if (refcoefs[i] < -2048) refcoefs[i] = -2048;
				else if (refcoefs[i] > 2047) refcoefs[i] = 2047;
			}
			
			/* perform reference IDCT */
			memcpy(refout, refcoefs, sizeof(DCTELEM)*64);
			ReferenceIDCT((DCTELEM (*)[8])refout);
			/* clip */
			for (i = 0; i < 64; i++) {
				if (refout[i] < -256) refout[i] = -256;
				else if (refout[i] > 255) refout[i] = 255;
			}
			
			/* perform test IDCT */
			memcpy(testout, refcoefs, sizeof(DCTELEM)*64);
			RunTestIDCT(testout);
			/* clip */
			for (i = 0; i < 64; i++) {
				if (testout[i] < -256) testout[i] = -256;
				else if (testout[i] > 255) testout[i] = 255;
			}
			
			/* accumulate error stats */
			for (i = 0; i < 64; i++) {
				register int err = testout[i] - refout[i];
				result.mMeanErrors[i] += err;
				result.mSquaredErrors[i] += err * err;
				if (err < 0) err = -err;
				if (result.mMaxErrors[i] < err) result.mMaxErrors[i] = err;
			}
		}
				
		for (i = 0; i < 64; i++) {
			if (result.mMaxErrors[i] > result.mMaximumError)
				result.mMaximumError = result.mMaxErrors[i];
		}
		result.mbMaxErrorOK = result.mMaximumError <= 1;
		
		max = total = 0.0;
		for (i = 0; i < 64; i++) {
			double err = result.mSquaredErrors[i] /= (double) niters;
			total += err;
			if (max < err) max = err;
		}
		total /= 64.0;

		result.mWorstSquaredError = max;
		result.mAverageSquaredError = total;
		result.mbWorstSquaredErrorOK = (max <= 0.06);
		result.mbAverageSquaredErrorOK = (total <= 0.02);
		
		max = total = 0.0;
		for (i = 0; i < 64; i++) {
			double err = result.mMeanErrors[i] /= (double) niters;
			total += err;
			if (err < 0.0) err = -err;
			if (max < err) max = err;
		}
		total /= 64.0;
		result.mWorstError = max;
		result.mAverageError = total;
		result.mbWorstErrorOK = (max <= 0.015);
		result.mbAverageErrorOK = (total <= 0.0015);

		return result.mbMaxErrorOK && result.mbWorstSquaredErrorOK && result.mbAverageSquaredErrorOK && result.mbWorstErrorOK && result.mbAverageErrorOK;
	}

	void IDCTTest::RunTestIDCT(DCTELEM block[64]) {
		static const int zigzag_std[64]={
			 0,  1,  8, 16,  9,  2,  3, 10,
			17, 24, 32, 25, 18, 11,  4,  5,
			12, 19, 26, 33, 40, 48, 41, 34,
			27, 20, 13,  6,  7, 14, 21, 28,
			35, 42, 49, 56, 57, 50, 43, 36,
			29, 22, 15, 23, 30, 37, 44, 51,
			58, 59, 52, 45, 38, 31, 39, 46,
			53, 60, 61, 54, 47, 55, 62, 63,
		};

		int i;

		const int *scan = mIDCT.pAltScan?mIDCT.pAltScan:zigzag_std;
		if (mIDCT.pPrescaler) {
			int tmp[64];
			for(i=0; i<64; ++i) {
				const int dst = scan[i];
				tmp[dst] = (block[zigzag_std[i]] * mIDCT.pPrescaler[dst] + 128) >> 8;
			}

			mIDCT.pTest(tmp, 63);

			for(i=0; i<64; ++i)
				block[i] = tmp[i];
		} else {
			short tmp[64];
			for(i=0; i<64; ++i) {
				const int dst = scan[i];
				tmp[dst] = block[zigzag_std[i]];
			}

			mIDCT.pTest(tmp, 63);

			for(i=0; i<64; ++i)
				block[i] = tmp[i];
		}
	}

	// I hate waiting.
	void IDCTTest::ReferenceFDCT(DCTELEM block[8][8]) {
		int x,y;
		double tmp;
		double res[8][8];
		
		// row transform
		for (y=0; y<8; y++) {
			for (x=0; x<8; x++) {
				tmp = (double) block[y][0] * mBasis[0][x]
					+ (double) block[y][1] * mBasis[1][x]
					+ (double) block[y][2] * mBasis[2][x]
					+ (double) block[y][3] * mBasis[3][x]
					+ (double) block[y][4] * mBasis[4][x]
					+ (double) block[y][5] * mBasis[5][x]
					+ (double) block[y][6] * mBasis[6][x]
					+ (double) block[y][7] * mBasis[7][x];
				res[y][x] = tmp;
			}
		}
		
		// column transform
		for (x=0; x<8; x++) {
			for (y=0; y<8; y++) {
				tmp = res[0][x] * mBasis[0][y]
					+ res[1][x] * mBasis[1][y]
					+ res[2][x] * mBasis[2][y]
					+ res[3][x] * mBasis[3][y]
					+ res[4][x] * mBasis[4][y]
					+ res[5][x] * mBasis[5][y]
					+ res[6][x] * mBasis[6][y]
					+ res[7][x] * mBasis[7][y];
				block[y][x] = (DCTELEM) fastround(tmp);
			}
		}	
	}


	void IDCTTest::ReferenceIDCT(DCTELEM block[8][8]) {
		int x,y;
		double tmp;
		double res[8][8];
		
		// row transform
		for (y=0; y<8; y++) {
			for (x=0; x<8; x++) {
				tmp = (double) block[y][0] * mBasis[x][0]
					+ (double) block[y][1] * mBasis[x][1]
					+ (double) block[y][2] * mBasis[x][2]
					+ (double) block[y][3] * mBasis[x][3]
					+ (double) block[y][4] * mBasis[x][4]
					+ (double) block[y][5] * mBasis[x][5]
					+ (double) block[y][6] * mBasis[x][6]
					+ (double) block[y][7] * mBasis[x][7];
				res[y][x] = tmp;
			}
		}
		
		// column transform
		for (x=0; x<8; x++) {
			for (y=0; y<8; y++) {
				tmp = res[0][x] * mBasis[y][0]
					+ res[1][x] * mBasis[y][1]
					+ res[2][x] * mBasis[y][2]
					+ res[3][x] * mBasis[y][3]
					+ res[4][x] * mBasis[y][4]
					+ res[5][x] * mBasis[y][5]
					+ res[6][x] * mBasis[y][6]
					+ res[7][x] * mBasis[y][7];
				block[y][x] = (DCTELEM) fastround(tmp);
			}
		}	
	}
}

bool VDTestVideoIDCTCompliance(const VDMPEGIDCTSet& idct, VDIDCTComplianceResult& result) {
	return IDCTTest(idct).Test(result);
}
