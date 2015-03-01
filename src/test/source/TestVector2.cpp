#include <stdexcept>
#include <vd2/system/vdstl.h>
#include <vd2/system/VDString.h>
#include "test.h"

DEFINE_TEST(Vector) {
	vdvector<VDStringA> x;
	const vdvector<VDStringA>& xc = x;

	x.push_back(VDStringA("abc"));
	x.push_back(VDStringA("def"));
	x.push_back(VDStringA("ghi"));
	TEST_ASSERT(x[0] == "abc");
	TEST_ASSERT(x[1] == "def");
	TEST_ASSERT(x[2] == "ghi");
	TEST_ASSERT(x.at(0) == "abc");
	TEST_ASSERT(x.at(1) == "def");
	TEST_ASSERT(x.at(2) == "ghi");
	TEST_ASSERT(x.front() == "abc");
	TEST_ASSERT(x.back() == "ghi");
	TEST_ASSERT(x.size() == 3);
	TEST_ASSERT(x.capacity() >= 3);
	TEST_ASSERT(!x.empty());

	TEST_ASSERT(xc[0] == "abc");
	TEST_ASSERT(xc[1] == "def");
	TEST_ASSERT(xc[2] == "ghi");
	TEST_ASSERT(xc.at(0) == "abc");
	TEST_ASSERT(xc.at(1) == "def");
	TEST_ASSERT(xc.at(2) == "ghi");
	TEST_ASSERT(xc.front() == "abc");
	TEST_ASSERT(xc.back() == "ghi");
	TEST_ASSERT(xc.size() == 3);
	TEST_ASSERT(xc.capacity() >= 3);
	TEST_ASSERT(!xc.empty());

	x.insert(x.begin() + 1, VDStringA("foo1"));
	x.insert(x.begin() + 2, VDStringA("foo2"));
	x.insert(x.begin() + 3, VDStringA("foo3"));
	x.insert(x.begin() + 4, VDStringA("foo4"));
	TEST_ASSERT(x[0] == "abc");
	TEST_ASSERT(x[1] == "foo1");
	TEST_ASSERT(x[2] == "foo2");
	TEST_ASSERT(x[3] == "foo3");
	TEST_ASSERT(x[4] == "foo4");
	TEST_ASSERT(x[5] == "def");
	TEST_ASSERT(x[6] == "ghi");
	TEST_ASSERT(x.front() == "abc");
	TEST_ASSERT(x.back() == "ghi");
	TEST_ASSERT(x.size() == 7);
	TEST_ASSERT(x.capacity() >= 7);
	TEST_ASSERT(!x.empty());

	VDStringA ins[4]={
		VDStringA("ins1"),
		VDStringA("ins2"),
		VDStringA("ins3"),
		VDStringA("ins4"),
	};

	x.insert(x.end(), ins, ins+4);
	TEST_ASSERT(x[0] == "abc");
	TEST_ASSERT(x[1] == "foo1");
	TEST_ASSERT(x[2] == "foo2");
	TEST_ASSERT(x[3] == "foo3");
	TEST_ASSERT(x[4] == "foo4");
	TEST_ASSERT(x[5] == "def");
	TEST_ASSERT(x[6] == "ghi");
	TEST_ASSERT(x[7] == "ins1");
	TEST_ASSERT(x[8] == "ins2");
	TEST_ASSERT(x[9] == "ins3");
	TEST_ASSERT(x[10] == "ins4");
	TEST_ASSERT(x.front() == "abc");
	TEST_ASSERT(x.back() == "ins4");
	TEST_ASSERT(x.size() == 11);
	TEST_ASSERT(x.capacity() >= 11);
	TEST_ASSERT(!x.empty());

	x.insert(x.begin(), 2, VDStringA("x"));
	TEST_ASSERT(x[0] == "x");
	TEST_ASSERT(x[1] == "x");
	TEST_ASSERT(x[2] == "abc");
	TEST_ASSERT(x[3] == "foo1");
	TEST_ASSERT(x[4] == "foo2");
	TEST_ASSERT(x[5] == "foo3");
	TEST_ASSERT(x[6] == "foo4");
	TEST_ASSERT(x[7] == "def");
	TEST_ASSERT(x[8] == "ghi");
	TEST_ASSERT(x[9] == "ins1");
	TEST_ASSERT(x[10] == "ins2");
	TEST_ASSERT(x[11] == "ins3");
	TEST_ASSERT(x[12] == "ins4");
	TEST_ASSERT(x.front() == "x");
	TEST_ASSERT(x.back() == "ins4");
	TEST_ASSERT(x.size() == 13);
	TEST_ASSERT(x.capacity() >= 13);
	TEST_ASSERT(!x.empty());

	x.resize(6);
	TEST_ASSERT(x.front() == "x");
	TEST_ASSERT(x.back() == "foo3");
	TEST_ASSERT(x.size() == 6);
	TEST_ASSERT(x.capacity() >= 6);
	TEST_ASSERT(!x.empty());

	x.resize(7, VDStringA("q"));
	TEST_ASSERT(x.front() == "x");
	TEST_ASSERT(x.back() == "q");
	TEST_ASSERT(x.size() == 7);
	TEST_ASSERT(x.capacity() >= 7);
	TEST_ASSERT(!x.empty());

	x.reserve(0);
	TEST_ASSERT(x.front() == "x");
	TEST_ASSERT(x.back() == "q");
	TEST_ASSERT(x.size() == 7);
	TEST_ASSERT(x.capacity() >= 7);
	TEST_ASSERT(!x.empty());

	x.reserve(x.capacity() * 2);
	TEST_ASSERT(x.front() == "x");
	TEST_ASSERT(x.back() == "q");
	TEST_ASSERT(x.size() == 7);
	TEST_ASSERT(x.capacity() >= 7);
	TEST_ASSERT(!x.empty());

	x.pop_back();
	TEST_ASSERT(x.front() == "x");
	TEST_ASSERT(x.back() == "foo3");
	TEST_ASSERT(x.size() == 6);
	TEST_ASSERT(x.capacity() >= 6);
	TEST_ASSERT(!x.empty());

	x.erase(x.begin() + 3);
	TEST_ASSERT(x[0] == "x");
	TEST_ASSERT(x[1] == "x");
	TEST_ASSERT(x[2] == "abc");
	TEST_ASSERT(x[3] == "foo2");
	TEST_ASSERT(x[4] == "foo3");
	TEST_ASSERT(x.size() == 5);

	x.erase(x.begin() + 2, x.begin() + 2);
	TEST_ASSERT(x[0] == "x");
	TEST_ASSERT(x[1] == "x");
	TEST_ASSERT(x[2] == "abc");
	TEST_ASSERT(x[3] == "foo2");
	TEST_ASSERT(x[4] == "foo3");
	TEST_ASSERT(x.size() == 5);

	x.erase(x.begin() + 1, x.begin() + 3);
	TEST_ASSERT(x[0] == "x");
	TEST_ASSERT(x[1] == "foo2");
	TEST_ASSERT(x[2] == "foo3");
	TEST_ASSERT(x.size() == 3);

	vdvector<VDStringA> t;
	t.swap(x);
	TEST_ASSERT(t[0] == "x");
	TEST_ASSERT(t[1] == "foo2");
	TEST_ASSERT(t[2] == "foo3");
	TEST_ASSERT(t.size() == 3);
	TEST_ASSERT(x.size() == 0);
	t.swap(x);
	TEST_ASSERT(x[0] == "x");
	TEST_ASSERT(x[1] == "foo2");
	TEST_ASSERT(x[2] == "foo3");
	TEST_ASSERT(x.size() == 3);
	TEST_ASSERT(t.size() == 0);

	x.clear();
	TEST_ASSERT(x.size() == 0);
	TEST_ASSERT(x.capacity() >= 5);
	TEST_ASSERT(x.empty());

	x.push_back(VDStringA("a"));
	x.push_back(VDStringA("b"));
	x.push_back(VDStringA("c"));
	vdvector<VDStringA> c(x);
	TEST_ASSERT(c[0] == "a");
	TEST_ASSERT(c[1] == "b");
	TEST_ASSERT(c[2] == "c");
	TEST_ASSERT(c.size() == 3);
	TEST_ASSERT(c.capacity() == 3);

	x = vdvector<VDStringA>(5);
	TEST_ASSERT(x[0] == "");
	TEST_ASSERT(x[1] == "");
	TEST_ASSERT(x[2] == "");
	TEST_ASSERT(x[3] == "");
	TEST_ASSERT(x[4] == "");
	TEST_ASSERT(x.size() == 5);

	x = vdvector<VDStringA>(3, VDStringA("foo"));
	TEST_ASSERT(x[0] == "foo");
	TEST_ASSERT(x[1] == "foo");
	TEST_ASSERT(x[2] == "foo");
	TEST_ASSERT(x.size() == 3);

	x.push_back_as("abc");
	TEST_ASSERT(x[0] == "foo");
	TEST_ASSERT(x[1] == "foo");
	TEST_ASSERT(x[2] == "foo");
	TEST_ASSERT(x[3] == "abc");
	TEST_ASSERT(x.size() == 4);

	x.insert_as(x.begin() + 1, "def");
	TEST_ASSERT(x[0] == "foo");
	TEST_ASSERT(x[1] == "def");
	TEST_ASSERT(x[2] == "foo");
	TEST_ASSERT(x[3] == "foo");
	TEST_ASSERT(x[4] == "abc");
	TEST_ASSERT(x.size() == 5);

	x.resize(5);
	x.clear();
	x.insert(x.begin(), VDStringA("abc"));
	TEST_ASSERT(x.size() == 1);
	TEST_ASSERT(x[0] == "abc");
	x.insert(x.begin(), VDStringA("def"));
	TEST_ASSERT(x.size() == 2);
	TEST_ASSERT(x[0] == "def");
	TEST_ASSERT(x[1] == "abc");

	return 0;
}
