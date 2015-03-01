#include <vd2/Kasumi/pixel.h>
#include "test.h"

DEFINE_TEST(Pixel) {
	// 601 limited range
	TEST_ASSERT(VDConvertYCbCrToRGB(0x00, 0x80, 0x80, false, false) == 0x000000);
	TEST_ASSERT(VDConvertYCbCrToRGB(0x10, 0x80, 0x80, false, false) == 0x000000);
	TEST_ASSERT(VDConvertYCbCrToRGB(0x80, 0x80, 0x80, false, false) == 0x828282);
	TEST_ASSERT(VDConvertYCbCrToRGB(0xEB, 0x80, 0x80, false, false) == 0xFFFFFF);
	TEST_ASSERT(VDConvertYCbCrToRGB(0xFF, 0x80, 0x80, false, false) == 0xFFFFFF);

	// 601 full range
	TEST_ASSERT(VDConvertYCbCrToRGB(0x00, 0x80, 0x80, false, true) == 0x000000);
	TEST_ASSERT(VDConvertYCbCrToRGB(0x80, 0x80, 0x80, false, true) == 0x808080);
	TEST_ASSERT(VDConvertYCbCrToRGB(0xFF, 0x80, 0x80, false, true) == 0xFFFFFF);

	// 709 limited range
	TEST_ASSERT(VDConvertYCbCrToRGB(0x00, 0x80, 0x80, true, false) == 0x000000);
	TEST_ASSERT(VDConvertYCbCrToRGB(0x10, 0x80, 0x80, true, false) == 0x000000);
	TEST_ASSERT(VDConvertYCbCrToRGB(0x80, 0x80, 0x80, true, false) == 0x828282);
	TEST_ASSERT(VDConvertYCbCrToRGB(0xEB, 0x80, 0x80, true, false) == 0xFFFFFF);
	TEST_ASSERT(VDConvertYCbCrToRGB(0xFF, 0x80, 0x80, true, false) == 0xFFFFFF);

	// 709 full range
	TEST_ASSERT(VDConvertYCbCrToRGB(0x00, 0x80, 0x80, true, true) == 0x000000);
	TEST_ASSERT(VDConvertYCbCrToRGB(0x80, 0x80, 0x80, true, true) == 0x808080);
	TEST_ASSERT(VDConvertYCbCrToRGB(0xFF, 0x80, 0x80, true, true) == 0xFFFFFF);

	////////////////////////////////////

	TEST_ASSERT(VDConvertRGBToYCbCr(0x00, 0x00, 0x00, false, false) == 0x801080);
	TEST_ASSERT(VDConvertRGBToYCbCr(0x80, 0x80, 0x80, false, false) == 0x807E80);
	TEST_ASSERT(VDConvertRGBToYCbCr(0xFF, 0xFF, 0xFF, false, false) == 0x80EB80);

	TEST_ASSERT(VDConvertRGBToYCbCr(0x00, 0x00, 0x00, false, true) == 0x800080);
	TEST_ASSERT(VDConvertRGBToYCbCr(0x80, 0x80, 0x80, false, true) == 0x808080);
	TEST_ASSERT(VDConvertRGBToYCbCr(0xFF, 0xFF, 0xFF, false, true) == 0x80FF80);

	TEST_ASSERT(VDConvertRGBToYCbCr(0x00, 0x00, 0x00, true, false) == 0x801080);
	TEST_ASSERT(VDConvertRGBToYCbCr(0x80, 0x80, 0x80, true, false) == 0x807E80);
	TEST_ASSERT(VDConvertRGBToYCbCr(0xFF, 0xFF, 0xFF, true, false) == 0x80EB80);

	TEST_ASSERT(VDConvertRGBToYCbCr(0x00, 0x00, 0x00, true, true) == 0x800080);
	TEST_ASSERT(VDConvertRGBToYCbCr(0x80, 0x80, 0x80, true, true) == 0x808080);
	TEST_ASSERT(VDConvertRGBToYCbCr(0xFF, 0xFF, 0xFF, true, true) == 0x80FF80);

	return 0;
}

