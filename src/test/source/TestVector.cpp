#include <vd2/system/vdstl.h>
#include "test.h"

#define CHECK(cond) if (!(cond)) { VDASSERT((cond)); return 5; } else

template<class C>
int RunVectorTest(bool checkCapacity) {
	C v;

	CHECK(v.empty());
	CHECK(v.size() == 0);
	CHECK(!checkCapacity || v.capacity() == 0);

	v.push_back(1);
	CHECK(v[0] == 1);
	CHECK(!v.empty());
	CHECK(v.size() == 1);

	v.push_back(2);
	CHECK(v[0] == 1);
	CHECK(v[1] == 2);
	CHECK(!v.empty());
	CHECK(v.size() == 2);
	CHECK(!checkCapacity || v.capacity() >= 2);

	v.swap(C());
	CHECK(v.size() == 0);
	v.reserve(2);
	CHECK(v.size() == 0);
	CHECK(!checkCapacity || v.capacity() == 2);
	static const int k[2]={10,11};
	v.assign(k, k+2);
	v.insert(v.begin(), v[1]);
	CHECK(v[0] == 11);
	CHECK(v[1] == 10);
	CHECK(v[2] == 11);
	CHECK(v.size() == 3);
	CHECK(!checkCapacity || v.capacity() >= 3);

	// range erase
	v.clear();
	v.push_back(1);
	v.push_back(2);
	v.push_back(3);
	v.erase(v.begin(), v.end());
	CHECK(v.size() == 0);

	v.clear();
	v.push_back(1);
	v.push_back(2);
	v.push_back(3);
	v.erase(v.begin() + 1, v.begin() + 1);
	CHECK(v.size() == 3);
	CHECK(v[0] == 1 && v[1] == 2 && v[2] == 3);

	v.clear();
	v.push_back(1);
	v.push_back(2);
	v.push_back(3);
	v.erase(v.begin() + 1, v.begin() + 2);
	CHECK(v.size() == 2);
	CHECK(v[0] == 1 && v[1] == 3);

	v.clear();
	v.push_back(1);
	v.push_back(2);
	v.push_back(3);
	v.erase(v.begin() + 1, v.begin() + 3);
	CHECK(v.size() == 1);
	CHECK(v[0] == 1);

	v.clear();
	v.push_back(1);
	v.push_back(2);
	v.push_back(3);
	v.erase(v.begin() + 0, v.begin() + 2);
	CHECK(v.size() == 1);
	CHECK(v[0] == 3);

	return 0;
}

DEFINE_TEST(FastVector) {
	int e = RunVectorTest<vdfastvector<int> >(true);
	e += RunVectorTest<vdfastfixedvector<int, 1> >(false);
	e += RunVectorTest<vdfastfixedvector<int, 2> >(false);
	e += RunVectorTest<vdfastfixedvector<int, 4> >(false);
	e += RunVectorTest<vdfastfixedvector<int, 8> >(false);
	return e;
}
