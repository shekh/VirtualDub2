#include <vd2/system/math.h>
#include <vd2/system/int128.h>
#include <vd2/system/fraction.h>
#include "test.h"

namespace {
	vduint128 rand128_64(vduint128 v) {
		return vduint128(v.getLo(), ~(v.getHi() ^ (uint64)(v >> (126 - 64)) ^ (uint64)(v >> (101 - 64)) ^ (uint64)(v >> (99 - 64))));
	}

	vduint128 rand128(vduint128 v) {
		return rand128_64(rand128_64(v));
	}

	vduint128 slowmul(vduint128 x, vduint128 y) {
		vdint128 shifter;
		shifter.q[0] = x.getLo();
		shifter.q[1] = x.getHi();
		vduint128 result(0);
		vdint128 zero(0);

		for(int i=0; i<128; ++i) {
			result += result;
			if (shifter < zero)
				result += y;
			shifter += shifter;
		}

		return result;
	}
}

DEFINE_TEST(Math) {
	TEST_ASSERT(VDRoundToInt(-2.00f) ==-2);
	TEST_ASSERT(VDRoundToInt(-1.51f) ==-2);
	TEST_ASSERT(VDRoundToInt(-1.49f) ==-1);
	TEST_ASSERT(VDRoundToInt(-1.00f) ==-1);
	TEST_ASSERT(VDRoundToInt(-0.51f) ==-1);
	TEST_ASSERT(VDRoundToInt(-0.49f) == 0);
	TEST_ASSERT(VDRoundToInt( 0.00f) == 0);
	TEST_ASSERT(VDRoundToInt( 0.49f) == 0);
	TEST_ASSERT(VDRoundToInt( 0.51f) == 1);
	TEST_ASSERT(VDRoundToInt( 1.00f) == 1);
	TEST_ASSERT(VDRoundToInt( 1.49f) == 1);
	TEST_ASSERT(VDRoundToInt( 1.51f) == 2);
	TEST_ASSERT(VDRoundToInt( 2.00f) == 2);

	TEST_ASSERT(VDFloorToInt(-2.0f) == -2);
	TEST_ASSERT(VDFloorToInt(-1.5f) == -2);
	TEST_ASSERT(VDFloorToInt(-1.0f) == -1);
	TEST_ASSERT(VDFloorToInt(-0.5f) == -1);
	TEST_ASSERT(VDFloorToInt( 0.0f) ==  0);
	TEST_ASSERT(VDFloorToInt( 0.5f) ==  0);
	TEST_ASSERT(VDFloorToInt( 1.0f) ==  1);
	TEST_ASSERT(VDFloorToInt( 1.5f) ==  1);
	TEST_ASSERT(VDFloorToInt( 2.0f) ==  2);

	TEST_ASSERT(VDCeilToInt(-2.0f) == -2);
	TEST_ASSERT(VDCeilToInt(-1.5f) == -1);
	TEST_ASSERT(VDCeilToInt(-1.0f) == -1);
	TEST_ASSERT(VDCeilToInt(-0.5f) ==  0);
	TEST_ASSERT(VDCeilToInt( 0.0f) ==  0);
	TEST_ASSERT(VDCeilToInt( 0.5f) ==  1);
	TEST_ASSERT(VDCeilToInt( 1.0f) ==  1);
	TEST_ASSERT(VDCeilToInt( 1.5f) ==  2);
	TEST_ASSERT(VDCeilToInt( 2.0f) ==  2);

	TEST_ASSERT(VDUMul64x64To128(1, 1) == vduint128(1));
	TEST_ASSERT(VDUMul64x64To128(3, 7) == vduint128(21));
	TEST_ASSERT(VDUMul64x64To128(0xFFFFFFFF, 0xFFFFFFFF) == vduint128(0xFFFFFFFE00000001));
	TEST_ASSERT(VDUMul64x64To128(0x123456789ABCDEF0, 0xBAADF00DDEADBEEF) == vduint128(0x0D4665441D7CFEBC, 0xD182EA976BFA4210));
	TEST_ASSERT(VDUMul64x64To128(0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF) == vduint128(0xFFFFFFFFFFFFFFFE, 0x0000000000000001));

	uint64 quotient;
	uint64 remainder;
	TEST_ASSERT(((quotient = VDUDiv128x64To64(vduint128(21), 7, remainder)), quotient == 3 && remainder == 0));
	TEST_ASSERT(((quotient = VDUDiv128x64To64(vduint128(27), 7, remainder)), quotient == 3 && remainder == 6));
	TEST_ASSERT(((quotient = VDUDiv128x64To64(vduint128(0xFFFFFFFFFFFFFFFE, 0x0000000000000001), 0xFFFFFFFFFFFFFFFF, remainder)), quotient == 0xFFFFFFFFFFFFFFFF && remainder == 0));
	TEST_ASSERT(((quotient = VDUDiv128x64To64(vduint128(0xFFFFFFFFFFFFFFFF, 0x0000000000000000), 0xFFFFFFFFFFFFFFFF, remainder)), quotient == 0xFFFFFFFFFFFFFFFF && remainder == 0xFFFFFFFFFFFFFFFF));
	TEST_ASSERT(((quotient = VDUDiv128x64To64(vduint128(0x123456789ABCDEF0, 0xBAADF00DDEADBEEF), 0xFEDCBA9876543210, remainder)), quotient == 0x1249249249249238 && remainder == 0xA72CB7FA5D75AB6F));

	TEST_ASSERT(VDFraction(3,2) * VDFraction(2,3) == VDFraction(1,1));
	TEST_ASSERT(VDFraction(3,2) * VDFraction(5,8) == VDFraction(15,16));
	TEST_ASSERT(VDFraction(15,16) / VDFraction(5,8) == VDFraction(3,2));
	TEST_ASSERT(VDFraction(1024,768) == VDFraction(4,3));
	TEST_ASSERT(VDFraction(0xFFFFFFFC/3,0xFFFFFFFF) * VDFraction(0xFFFFFFFF,0xFFFFFFFC) == VDFraction(1,3));

	TEST_ASSERT(VDMulDiv64(-10000000000000000, -10000000000000000, -10000000000000000) == -10000000000000000);
	TEST_ASSERT(VDMulDiv64(-1000000000000, -100000, 17) == 5882352941176471);

	vduint128 seed(0);

	for(int i=0; i<10000; ++i) {
		vduint128 x(seed);	seed = rand128(seed);
		vduint128 y(seed);	seed = rand128(seed);
		vduint128 p(x*y);
		vduint128 q(slowmul(x, y));

		TEST_ASSERT(p == q);
	}

	return 0;
}

