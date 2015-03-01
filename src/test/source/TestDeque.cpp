#include <vd2/system/vdstl.h>
#include "test.h"

#define CHECK(cond) if (!(cond)) { VDASSERT((cond)); return 5; } else

DEFINE_TEST(FastDeque) {
	vdfastdeque<int> v;

	CHECK(v.empty());
	CHECK(v.size() == 0);

	v.push_back(1);
	CHECK(v[0] == 1);
	CHECK(!v.empty());
	CHECK(v.size() == 1);

	v.push_back(2);
	CHECK(v[0] == 1);
	CHECK(v[1] == 2);
	CHECK(!v.empty());
	CHECK(v.size() == 2);

	v.clear();
	CHECK(v.empty());
	CHECK(v.size() == 0);

	for(int i=0; i<128; ++i) {
		v.push_back(i);
		CHECK(!v.empty());
		CHECK(v.size() == i + 1);
		CHECK(v.front() == 0);
		CHECK(v.back() == i);

		vdfastdeque<int>::iterator it(v.begin());
		vdfastdeque<int>::const_iterator it2(static_cast<const vdfastdeque<int>&>(v).begin());
		for(int j=0; j<=i; ++j) {
			CHECK(*it == j);
			CHECK(*it2 == j);
			++it;
			++it2;
		}

		vdfastdeque<int>::iterator itEnd(v.end());
		vdfastdeque<int>::const_iterator it2End(static_cast<const vdfastdeque<int>&>(v).end());
		CHECK(it == itEnd);
		CHECK(it2 == it2End);
		CHECK(it == it2End);
		CHECK(it2 == itEnd);
	}

	for(int i=0; i<128; ++i) {
		vdfastdeque<int>::iterator it(v.begin());
		vdfastdeque<int>::const_iterator it2(static_cast<const vdfastdeque<int>&>(v).begin());
		for(int j=0; j<128-i; ++j) {
			CHECK(*it == i+j);
			CHECK(*it2 == i+j);
			++it;
			++it2;
		}

		vdfastdeque<int>::iterator itEnd(v.end());
		vdfastdeque<int>::const_iterator it2End(static_cast<const vdfastdeque<int>&>(v).end());
		CHECK(it == itEnd);
		CHECK(it2 == it2End);
		CHECK(it == it2End);
		CHECK(it2 == itEnd);

		for(int j=127-i; j>=0; --j) {
			CHECK(v[j] == i+j);
		}

		CHECK(!v.empty());
		CHECK(v.size() == 128 - i);
		CHECK(v.front() == i);
		CHECK(v.back() == 127);
		v.pop_front();
	}

	CHECK(v.empty());
	CHECK(v.size() == 0);
	CHECK(v.begin() == v.end());

	return 0;
}

