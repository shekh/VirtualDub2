#include <math.h>
#include <vd2/system/Fraction.h>
#include "test.h"

DEFINE_TEST(Fraction) {
	TEST_ASSERT(VDFraction(1,1) * VDFraction(2,3) == VDFraction(2,3));
	TEST_ASSERT(VDFraction(3,1) * VDFraction(2,3) == VDFraction(2,1));
	TEST_ASSERT(VDFraction(0x80000000, 0x80000000) == VDFraction(1,1));
	TEST_ASSERT(VDFraction(0xF0000000, 0x08000000) == VDFraction(30,1));

	TEST_ASSERT(VDFraction(0x08000000, 0xF0000000).scale64r ( 10000000000000i64) ==  333333333333i64);
	TEST_ASSERT(VDFraction(0x08000000, 0xF0000000).scale64t ( 10000000000000i64) ==  333333333333i64);
	TEST_ASSERT(VDFraction(0x08000000, 0xF0000000).scale64u ( 10000000000000i64) ==  333333333334i64);
	TEST_ASSERT(VDFraction(0xF0000000, 0x08000000).scale64ir( 10000000000000i64) ==  333333333333i64);
	TEST_ASSERT(VDFraction(0xF0000000, 0x08000000).scale64it( 10000000000000i64) ==  333333333333i64);
	TEST_ASSERT(VDFraction(0xF0000000, 0x08000000).scale64iu( 10000000000000i64) ==  333333333334i64);

	TEST_ASSERT(VDFraction(0x08000000, 0xF0000000).scale64r ( 20000000000000i64) ==  666666666667i64);
	TEST_ASSERT(VDFraction(0x08000000, 0xF0000000).scale64t ( 20000000000000i64) ==  666666666666i64);
	TEST_ASSERT(VDFraction(0x08000000, 0xF0000000).scale64u ( 20000000000000i64) ==  666666666667i64);
	TEST_ASSERT(VDFraction(0xF0000000, 0x08000000).scale64ir( 20000000000000i64) ==  666666666667i64);
	TEST_ASSERT(VDFraction(0xF0000000, 0x08000000).scale64it( 20000000000000i64) ==  666666666666i64);
	TEST_ASSERT(VDFraction(0xF0000000, 0x08000000).scale64iu( 20000000000000i64) ==  666666666667i64);

	TEST_ASSERT(VDFraction(0x08000000, 0xF0000000).scale64r ( 30000000000000i64) == 1000000000000i64);
	TEST_ASSERT(VDFraction(0x08000000, 0xF0000000).scale64t ( 30000000000000i64) == 1000000000000i64);
	TEST_ASSERT(VDFraction(0x08000000, 0xF0000000).scale64u ( 30000000000000i64) == 1000000000000i64);
	TEST_ASSERT(VDFraction(0xF0000000, 0x08000000).scale64ir( 30000000000000i64) == 1000000000000i64);
	TEST_ASSERT(VDFraction(0xF0000000, 0x08000000).scale64it( 30000000000000i64) == 1000000000000i64);
	TEST_ASSERT(VDFraction(0xF0000000, 0x08000000).scale64iu( 30000000000000i64) == 1000000000000i64);

	TEST_ASSERT(VDFraction(0x08000000, 0xF0000000).scale64r (-10000000000000i64) == -333333333333i64);
	TEST_ASSERT(VDFraction(0x08000000, 0xF0000000).scale64t (-10000000000000i64) == -333333333333i64);
	TEST_ASSERT(VDFraction(0x08000000, 0xF0000000).scale64u (-10000000000000i64) == -333333333333i64);
	TEST_ASSERT(VDFraction(0xF0000000, 0x08000000).scale64ir(-10000000000000i64) == -333333333333i64);
	TEST_ASSERT(VDFraction(0xF0000000, 0x08000000).scale64it(-10000000000000i64) == -333333333333i64);
	TEST_ASSERT(VDFraction(0xF0000000, 0x08000000).scale64iu(-10000000000000i64) == -333333333333i64);

	TEST_ASSERT(VDFraction(0x08000000, 0xF0000000).scale64r (-20000000000000i64) == -666666666667i64);
	TEST_ASSERT(VDFraction(0x08000000, 0xF0000000).scale64t (-20000000000000i64) == -666666666666i64);
	TEST_ASSERT(VDFraction(0x08000000, 0xF0000000).scale64u (-20000000000000i64) == -666666666666i64);
	TEST_ASSERT(VDFraction(0xF0000000, 0x08000000).scale64ir(-20000000000000i64) == -666666666667i64);
	TEST_ASSERT(VDFraction(0xF0000000, 0x08000000).scale64it(-20000000000000i64) == -666666666666i64);
	TEST_ASSERT(VDFraction(0xF0000000, 0x08000000).scale64iu(-20000000000000i64) == -666666666666i64);

	TEST_ASSERT(VDFraction(0x08000000, 0xF0000000).scale64r (-30000000000000i64) ==-1000000000000i64);
	TEST_ASSERT(VDFraction(0x08000000, 0xF0000000).scale64t (-30000000000000i64) ==-1000000000000i64);
	TEST_ASSERT(VDFraction(0x08000000, 0xF0000000).scale64u (-30000000000000i64) ==-1000000000000i64);
	TEST_ASSERT(VDFraction(0xF0000000, 0x08000000).scale64ir(-30000000000000i64) ==-1000000000000i64);
	TEST_ASSERT(VDFraction(0xF0000000, 0x08000000).scale64it(-30000000000000i64) ==-1000000000000i64);
	TEST_ASSERT(VDFraction(0xF0000000, 0x08000000).scale64iu(-30000000000000i64) ==-1000000000000i64);

	TEST_ASSERT(VDFraction(1, 1).scale64r (1) == 1);
	TEST_ASSERT(VDFraction(1, 1).scale64t (1) == 1);
	TEST_ASSERT(VDFraction(1, 1).scale64u (1) == 1);
	TEST_ASSERT(VDFraction(1, 1).scale64ir(1) == 1);
	TEST_ASSERT(VDFraction(1, 1).scale64it(1) == 1);
	TEST_ASSERT(VDFraction(1, 1).scale64iu(1) == 1);
	TEST_ASSERT(VDFraction(1, 1).scale64r (-1) == -1);
	TEST_ASSERT(VDFraction(1, 1).scale64t (-1) == -1);
	TEST_ASSERT(VDFraction(1, 1).scale64u (-1) == -1);
	TEST_ASSERT(VDFraction(1, 1).scale64ir(-1) == -1);
	TEST_ASSERT(VDFraction(1, 1).scale64it(-1) == -1);
	TEST_ASSERT(VDFraction(1, 1).scale64iu(-1) == -1);

	// check for broken carry
	TEST_ASSERT(VDFraction(0xFFFFFFFF, 0xFFFFFFFF).scale64r(0x7FFFFFFFFFFFFFFFi64) == 0x7FFFFFFFFFFFFFFFi64);

	// check fraction conversion
	VDFraction frac;
	TEST_ASSERT(frac.Parse("0") && frac == VDFraction(0, 1));
	TEST_ASSERT(frac.Parse("1") && frac == VDFraction(1, 1));
	TEST_ASSERT(frac.Parse(" 1") && frac == VDFraction(1, 1));
	TEST_ASSERT(frac.Parse(" 1 ") && frac == VDFraction(1, 1));
	TEST_ASSERT(frac.Parse("4294967295") && frac == VDFraction(0xFFFFFFFFUL, 1));
	TEST_ASSERT(!frac.Parse("4294967296"));
	TEST_ASSERT(!frac.Parse(" 1x"));
	TEST_ASSERT(!frac.Parse("x1"));

	// check continued fraction approximations
	frac = VDFraction(30000, 1001);
	TEST_ASSERT(frac * frac * frac == VDFraction(2596703098, 96463));

	TEST_ASSERT(frac.Parse("3.14159265358979324") && frac == VDFraction(4030016662, 1282794145));

	// check overflow and underflow behavior
	frac = VDFraction(0x80000000, 1);
	TEST_ASSERT(frac * frac * frac * frac == VDFraction(0xFFFFFFFFUL, 1));

	frac = VDFraction(1, 0x80000000);
	TEST_ASSERT(frac * frac * frac * frac == VDFraction(0, 1));

	frac = VDFraction(16, 15);
	TEST_ASSERT(frac * frac * frac * frac * frac * frac * frac * frac == VDFraction(2569952127, 1533540481));

	// check conversion from double
	TEST_ASSERT(0.0 == VDFraction(0.0).asDouble());
	TEST_ASSERT(1.0 / 4294967295.0 == VDFraction(1.0 / 4294967295.0).asDouble());
	TEST_ASSERT(4294967295.0 == VDFraction(4294967295.0).asDouble());
	for(int i=-31; i<=31; ++i) {
		double x = ldexp(1.0, i);
		VDFraction frac = VDFraction(x);
		double y = frac.asDouble();
		TEST_ASSERT(x == y);
	}

	double epsilon = ldexp(1.0, -51);
	for(int i=0; i<100000; ++i) {
		unsigned n = rand() ^ (rand() << 10) ^ (rand() << 20);
		unsigned d = rand() ^ (rand() << 10) ^ (rand() << 20);

		if (!d)
			continue;

		double x = (double)n / (double)d;
		VDFraction frac = VDFraction(x);
		double y = frac.asDouble();
		TEST_ASSERT(fabs(x - y) < x * epsilon);
	}

	return 0;
}

