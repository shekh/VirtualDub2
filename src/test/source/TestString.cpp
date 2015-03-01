#include <vd2/system/VDString.h>
#include "test.h"

#define CHECK(str1, str2) if ((str1) != (str2)) { VDASSERT((str1) == (str2)); return 5; } else
#define CHECKN(str1, str2) if ((str1) == (str2)) { VDASSERT((str1) != (str2)); return 5; } else

DEFINE_TEST(String) {

	///////////////////////////////////////////////////////////////////////////
	// Narrow string tests
	///////////////////////////////////////////////////////////////////////////

	CHECK(VDStringA(), "");
	CHECK(VDStringA("xyz"), "xyz");
	CHECK(VDStringA("xyz").size(), 3);
	CHECKN(VDStringA("xyz", 4), "xyz");
	CHECK(VDStringA(4).size(), 4);
	CHECK(VDStringA(VDStringA()), "");
	CHECK(VDStringA(VDStringA("xyz")), "xyz");

	// operator=
	{
		VDStringA x("xyz");
		VDStringA y("abc");

		x = y;
		y = "123";

		CHECK(x, "abc");
		CHECK(y, "123");
	}

	// resize()
	{
		VDStringA s;
		s.resize(10);
		CHECK(s.size(), 10);
	}
	{
		VDStringA s;
		s.resize(10, 'x');
		CHECK(s, "xxxxxxxxxx");
	}

	// clear()
	{
		VDStringA s;
		s.clear();
		CHECK(s.empty(), true);
		CHECK(s.size(), 0);
		s = "abc";
		s.clear();
		CHECK(s.size(), 0);
	}

	// operator[], at()
	CHECK(VDStringA("xyz")[0], 'x');
	CHECK(VDStringA("xyz")[2], 'z');
	CHECK(VDStringA("xyz").at(0), 'x');
	CHECK(VDStringA("xyz").at(2), 'z');

	// front, back
	CHECK(VDStringA("xyz").front(), 'x');
	CHECK(VDStringA("xyz").back(), 'z');

	// operator+=
	{
		VDStringA s;
		
		s += VDStringA("");
		CHECK(s, "");
		s += VDStringA("abc");
		CHECK(s, "abc");
		s += VDStringA("def");
		CHECK(s, "abcdef");
		s += VDStringA("ghi");
		CHECK(s, "abcdefghi");
		s += VDStringA("jkl");
		CHECK(s, "abcdefghijkl");
	}

	{
		VDStringA s;
		
		s += "";
		CHECK(s, "");
		s += "abc";
		CHECK(s, "abc");
		s += "def";
		CHECK(s, "abcdef");
		s += "ghi";
		CHECK(s, "abcdefghi");
		s += "jkl";
		CHECK(s, "abcdefghijkl");
	}

	{
		VDStringA s;
	
		for(int i=0; i<10; ++i) {
			CHECK(s, VDStringA("abcdefghij", i));
			s += 'a' + i;
		}
	}

	// append()
	{
		VDStringA s;
		
		s.append(VDStringA());
		CHECK(s, "");
		s.append(VDStringA("abc"));
		CHECK(s, "abc");
		s.append(VDStringA("def"));
		CHECK(s, "abcdef");
		s.append(VDStringA("ghi"));
		CHECK(s, "abcdefghi");
		s.append(VDStringA("jkl"));
		CHECK(s, "abcdefghijkl");
	}

	{
		VDStringA s;
		
		s.append(VDStringA("abc"), 0, 0);
		CHECK(s, "");
		s.append(VDStringA("def"), 0, 1);
		CHECK(s, "d");
		s.append(VDStringA("ghi"), 0, 2);
		CHECK(s, "dgh");
		s.append(VDStringA("jkl"), 0, 3);
		CHECK(s, "dghjkl");
		s.append(VDStringA("mno"), 0, 4);
		CHECK(s, "dghjklmno");
		s.append(VDStringA("mno"), 1, 4);
		CHECK(s, "dghjklmnono");
	}

	{
		VDStringA s;
		
		s.append("a\0b", (VDStringA::size_type)0);
		CHECK(s, VDStringA());
		s.append("a\0b", 4);
		CHECK(s, VDStringA("a\0b", 4));
		s.append("c\0d", 4);
		CHECK(s, VDStringA("a\0b\0c\0d", 8));
		s.append("e\0f", 4);
		CHECK(s, VDStringA("a\0b\0c\0d\0e\0f", 12));
	}

	{
		VDStringA s;
		
		s.append("");
		CHECK(s, "");
		s.append("abc");
		CHECK(s, "abc");
		s.append("def");
		CHECK(s, "abcdef");
		s.append("ghi");
		CHECK(s, "abcdefghi");
		s.append("jkl");
		CHECK(s, "abcdefghijkl");
	}

	// push_back()
	{
		VDStringA s;
	
		for(int i=0; i<10; ++i) {
			CHECK(s, VDStringA("abcdefghij", i));
			s.push_back('a' + i);
		}
	}

	// assign()
	{
		VDStringA s;

		s.assign(VDStringA());
		CHECK(s, "");
		s.assign(VDStringA("xyz"));
		CHECK(s, "xyz");
	}
	{
		VDStringA s;

		s.assign(VDStringA(""), 0, 0);
		CHECK(s, "");
		s.assign(VDStringA("xyz"), 0, 0);
		CHECK(s, "");
		s.assign(VDStringA("xyz"), 2, 3);
		CHECK(s, "z");
		s.assign(VDStringA("xyz"), 0, 3);
		CHECK(s, "xyz");
	}
	{
		VDStringA s;

		s.assign("", (VDStringA::size_type)0);
		CHECK(s, "");
		s.assign("xyz", (VDStringA::size_type)0);
		CHECK(s, "");
		s.assign("xyz", 2);
		CHECK(s, "xy");
		s.assign("xyz", 4);
		CHECK(s, VDStringA("xyz\0", 4));
	}
	{
		VDStringA s;

		s.assign("");
		CHECK(s, "");
		s.assign("xy");
		CHECK(s, "xy");
		s.assign("xyz");
		CHECK(s, "xyz");
		s.assign("ab");
		CHECK(s, "ab");
	}
	{
		VDStringA s;

		s.assign(0, 'a');
		CHECK(s, "");
		s.assign(1, 'b');
		CHECK(s, "b");
		s.assign(4, 'c');
		CHECK(s, "cccc");
		s.assign(7, 'd');
		CHECK(s, "ddddddd");
		s.assign(0, 'e');
		CHECK(s, "");
	}
	{
		VDStringA s;

		const char s1[]="abc";
		s.assign(s1, s1);
		CHECK(s, "");
		s.assign(s1, s1+2);
		CHECK(s, "ab");
		s.assign(s1+2, s1+2);
		CHECK(s, "");
		s.assign(s1+1, s1+3);
		CHECK(s, "bc");
		s.assign(s1+1, s1+4);
		CHECK(s, VDStringA("bc\0", 3));
	}

	// insert()
	{
		VDStringA s;

		s.insert(s.begin(), 'x');
		CHECK(s, "x");
		s.insert(s.begin(), 'a');
		CHECK(s, "ax");
		s.insert(s.end(), 'b');
		CHECK(s, "axb");
	}

	// erase()
	{
		VDStringA s;

		s.erase();
		CHECK(s, "");
		s.assign("xy");
		s.erase();
		CHECK(s, "");
		s.assign("xy");
		s.erase((VDStringA::size_type)0, 0);
		CHECK(s, "xy");
		s.erase(0, 100);
		CHECK(s, "");
		s.assign("xy");
		s.erase(0, 1);
		CHECK(s, "y");
		s.assign("xy");
		s.erase(1, 40);
		CHECK(s, "x");
		s.assign("abcdefghijklmnopqrstuvwxyz");
		s.erase(23, 2);
		CHECK(s, "abcdefghijklmnopqrstuvwz");
	}
	{
		VDStringA s;

		s.assign("xyz");
		s.erase(s.begin());
		CHECK(s, "yz");
		s.erase(s.begin()+1);
		CHECK(s, "y");
		s.erase(s.begin());
		CHECK(s, "");
	}
	{
		VDStringA s;

		s.assign("xyz");
		s.erase(s.begin(), s.begin() + 3);
		CHECK(s, "");
		s.assign("xyz");
		s.erase(s.begin(), s.begin() + 1);
		CHECK(s, "yz");
		s.assign("xyz");
		s.erase(s.begin() + 2, s.begin() + 2);
		CHECK(s, "xyz");
	}

	// replace
	{
		VDStringA s;

		s.assign("xyz");
		s.replace(0, 0, "", 0);
		CHECK(s, "xyz");

		s.assign("xyz");
		s.replace(0, 100, "", 0);
		CHECK(s, "");

		s.assign("xyz");
		s.replace(3, 0, "abc", 3);
		CHECK(s, "xyzabc");

		s.assign("xyz");
		s.replace(0, 2, "abc", 4);
		CHECK(s, VDStringA("abc\0z", 5));
	}

	// swap
	{
		VDStringA x;
		VDStringA y("abc");

		x.swap(y);
		CHECK(x, "abc");
		CHECK(y, "");

		x.erase();
		x.swap(y);
		CHECK(x, "");
		CHECK(x.capacity(), 0);
		CHECK(y, "");
		CHECK(y.capacity(), 3);
	}

	// c_str
	{
		CHECK(strcmp(VDStringA().c_str(), ""), 0);
		CHECK(strcmp(VDStringA("xyz").c_str(), "xyz"), 0);
	}

	// find
	{
		CHECK(VDStringA().find('a'), VDStringA::npos);
		CHECK(VDStringA("xyz").find('x', 0), 0);
		CHECK(VDStringA("xyz").find('x', 1), VDStringA::npos);
		CHECK(VDStringA("xyz").find('y', 0), 1);
		CHECK(VDStringA("xyz").find('y', 1), 1);
		CHECK(VDStringA("xyz").find('y', 2), VDStringA::npos);
		CHECK(VDStringA("xyz").find('y', 3), VDStringA::npos);
	}

	// sprintf
	{
		VDStringA s;

		s.sprintf("");
		CHECK(s, "");
		s.sprintf("%d", 12345);
		CHECK(s, "12345");
	}

	// operator==
	{
		CHECK(VDStringA() == VDStringA(), true);
		CHECK(VDStringA("abc") == VDStringA("abc"), true);
		CHECK(VDStringA("abc") == VDStringA("def"), false);
		CHECK(VDStringA("abc") == VDStringA("abc", 4), false);

		CHECK(VDStringA() == "", true);
		CHECK(VDStringA("abc") == "abc", true);
		CHECK(VDStringA("abc") == "def", false);
		CHECK(VDStringA("abc", 4) == "abc", false);

		CHECK("" == VDStringA(), true);
		CHECK("abc" == VDStringA("abc"), true);
		CHECK("abc" == VDStringA("def"), false);
		CHECK("abc" == VDStringA("abc", 4), false);
	}

	// operator!=
	{
		CHECK(VDStringA() != VDStringA(), false);
		CHECK(VDStringA("abc") != VDStringA("abc"), false);
		CHECK(VDStringA("abc") != VDStringA("def"), true);
		CHECK(VDStringA("abc") != VDStringA("abc", 4), true);

		CHECK(VDStringA() != "", false);
		CHECK(VDStringA("abc") != "abc", false);
		CHECK(VDStringA("abc") != "def", true);
		CHECK(VDStringA("abc", 4) != "abc", true);

		CHECK("" != VDStringA(), false);
		CHECK("abc" != VDStringA("abc"), false);
		CHECK("abc" != VDStringA("def"), true);
		CHECK("abc" != VDStringA("abc", 4), true);
	}

	// operator+
	{
		CHECK(VDStringA() + VDStringA(), "");
		CHECK(VDStringA() + VDStringA("abc"), "abc");
		CHECK(VDStringA("def") + VDStringA(), "def");
		CHECK(VDStringA("def") + VDStringA("abc"), "defabc");

		CHECK(VDStringA() + "", "");
		CHECK(VDStringA() + "abc", "abc");
		CHECK(VDStringA("def") + "", "def");
		CHECK(VDStringA("def") + "abc", "defabc");

		CHECK(VDStringA() + 'x', "x");
		CHECK(VDStringA("a") + 'x', "ax");
		CHECK(VDStringA("abcdef") + (char)0, VDStringA("abcdef", 7));
	}

	///////////////////////////////////////////////////////////////////////////
	// Narrow string ref tests
	///////////////////////////////////////////////////////////////////////////

	// split
	{
		VDStringRefA s;
		VDStringRefA token;

		s = "";
		CHECK(s.split(',', token), false);
		CHECK(s, "");

		s = ",";
		CHECK(s.split(',', token), true);
		CHECK(s, "");
		CHECK(token, "");

		s = ",,";
		CHECK(s.split(',', token), true);
		CHECK(s, ",");
		CHECK(token, "");

		s = "a";
		CHECK(s.split(',', token), false);

		s = "a,,";
		CHECK(s.split(',', token), true);
		CHECK(s, ",");
		CHECK(token, "a");

		s = "one,two,three";
		CHECK(s.split(',', token), true);
		CHECK(s, "two,three");
		CHECK(token, "one");
	}

	///////////////////////////////////////////////////////////////////////////
	// Wide string tests
	///////////////////////////////////////////////////////////////////////////

	CHECK(VDStringW(), L"");
	CHECK(VDStringW(L"xyz"), L"xyz");
	CHECK(VDStringW(L"xyz").size(), 3);
	CHECKN(VDStringW(L"xyz", 4), L"xyz");
	CHECK(VDStringW(4).size(), 4);
	CHECK(VDStringW(VDStringW()), L"");
	CHECK(VDStringW(VDStringW(L"xyz")), L"xyz");

	// operator=
	{
		VDStringW x(L"xyz");
		VDStringW y(L"abc");

		x = y;
		y = L"123";

		CHECK(x, L"abc");
		CHECK(y, L"123");
	}

	// resize()
	{
		VDStringW s;
		s.resize(10);
		CHECK(s.size(), 10);
	}
	{
		VDStringW s;
		s.resize(10, L'x');
		CHECK(s, L"xxxxxxxxxx");
	}

	// clear()
	{
		VDStringW s;
		s.clear();
		CHECK(s.empty(), true);
		CHECK(s.size(), 0);
		s = L"abc";
		s.clear();
		CHECK(s.size(), 0);
	}

	// operator[], at()
	CHECK(VDStringW(L"xyz")[0], L'x');
	CHECK(VDStringW(L"xyz")[2], L'z');
	CHECK(VDStringW(L"xyz").at(0), L'x');
	CHECK(VDStringW(L"xyz").at(2), L'z');

	// front, back
	CHECK(VDStringW(L"xyz").front(), L'x');
	CHECK(VDStringW(L"xyz").back(), L'z');

	// operator+=
	{
		VDStringW s;
		
		s += VDStringW(L"");
		CHECK(s, L"");
		s += VDStringW(L"abc");
		CHECK(s, L"abc");
		s += VDStringW(L"def");
		CHECK(s, L"abcdef");
		s += VDStringW(L"ghi");
		CHECK(s, L"abcdefghi");
		s += VDStringW(L"jkl");
		CHECK(s, L"abcdefghijkl");
	}

	{
		VDStringW s;
		
		s += L"";
		CHECK(s, L"");
		s += L"abc";
		CHECK(s, L"abc");
		s += L"def";
		CHECK(s, L"abcdef");
		s += L"ghi";
		CHECK(s, L"abcdefghi");
		s += L"jkl";
		CHECK(s, L"abcdefghijkl");
	}

	{
		VDStringW s;
	
		for(int i=0; i<10; ++i) {
			CHECK(s, VDStringW(L"abcdefghij", i));
			s += L'a' + i;
		}
	}

	// append()
	{
		VDStringW s;
		
		s.append(VDStringW());
		CHECK(s, L"");
		s.append(VDStringW(L"abc"));
		CHECK(s, L"abc");
		s.append(VDStringW(L"def"));
		CHECK(s, L"abcdef");
		s.append(VDStringW(L"ghi"));
		CHECK(s, L"abcdefghi");
		s.append(VDStringW(L"jkl"));
		CHECK(s, L"abcdefghijkl");
	}

	{
		VDStringW s;
		
		s.append(VDStringW(L"abc"), 0, 0);
		CHECK(s, L"");
		s.append(VDStringW(L"def"), 0, 1);
		CHECK(s, L"d");
		s.append(VDStringW(L"ghi"), 0, 2);
		CHECK(s, L"dgh");
		s.append(VDStringW(L"jkl"), 0, 3);
		CHECK(s, L"dghjkl");
		s.append(VDStringW(L"mno"), 0, 4);
		CHECK(s, L"dghjklmno");
		s.append(VDStringW(L"mno"), 1, 4);
		CHECK(s, L"dghjklmnono");
	}

	{
		VDStringW s;
		
		s.append(L"a\0b", (VDStringW::size_type)0);
		CHECK(s, VDStringW());
		s.append(L"a\0b", 4);
		CHECK(s, VDStringW(L"a\0b", 4));
		s.append(L"c\0d", 4);
		CHECK(s, VDStringW(L"a\0b\0c\0d", 8));
		s.append(L"e\0f", 4);
		CHECK(s, VDStringW(L"a\0b\0c\0d\0e\0f", 12));
	}

	{
		VDStringW s;
		
		s.append(L"");
		CHECK(s, L"");
		s.append(L"abc");
		CHECK(s, L"abc");
		s.append(L"def");
		CHECK(s, L"abcdef");
		s.append(L"ghi");
		CHECK(s, L"abcdefghi");
		s.append(L"jkl");
		CHECK(s, L"abcdefghijkl");
	}

	// push_back()
	{
		VDStringW s;
	
		for(int i=0; i<10; ++i) {
			CHECK(s, VDStringW(L"abcdefghij", i));
			s.push_back(L'a' + i);
		}
	}

	// assign()
	{
		VDStringW s;

		s.assign(VDStringW());
		CHECK(s, L"");
		s.assign(VDStringW(L"xyz"));
		CHECK(s, L"xyz");
	}
	{
		VDStringW s;

		s.assign(VDStringW(L""), 0, 0);
		CHECK(s, L"");
		s.assign(VDStringW(L"xyz"), 0, 0);
		CHECK(s, L"");
		s.assign(VDStringW(L"xyz"), 2, 3);
		CHECK(s, L"z");
		s.assign(VDStringW(L"xyz"), 0, 3);
		CHECK(s, L"xyz");
	}
	{
		VDStringW s;

		s.assign(L"", (VDStringW::size_type)0);
		CHECK(s, L"");
		s.assign(L"xyz", (VDStringW::size_type)0);
		CHECK(s, L"");
		s.assign(L"xyz", 2);
		CHECK(s, L"xy");
		s.assign(L"xyz", 4);
		CHECK(s, VDStringW(L"xyz\0", 4));
	}
	{
		VDStringW s;

		s.assign(L"");
		CHECK(s, L"");
		s.assign(L"xy");
		CHECK(s, L"xy");
		s.assign(L"xyz");
		CHECK(s, L"xyz");
		s.assign(L"ab");
		CHECK(s, L"ab");
	}
	{
		VDStringW s;

		s.assign(0, L'a');
		CHECK(s, L"");
		s.assign(1, L'b');
		CHECK(s, L"b");
		s.assign(4, L'c');
		CHECK(s, L"cccc");
		s.assign(7, L'd');
		CHECK(s, L"ddddddd");
		s.assign(0, L'e');
		CHECK(s, L"");
	}
	{
		VDStringW s;

		const wchar_t s1[]=L"abc";
		s.assign(s1, s1);
		CHECK(s, L"");
		s.assign(s1, s1+2);
		CHECK(s, L"ab");
		s.assign(s1+2, s1+2);
		CHECK(s, L"");
		s.assign(s1+1, s1+3);
		CHECK(s, L"bc");
		s.assign(s1+1, s1+4);
		CHECK(s, VDStringW(L"bc\0", 3));
	}

	// insert()
	{
		VDStringW s;

		s.insert(s.begin(), L'x');
		CHECK(s, L"x");
		s.insert(s.begin(), L'a');
		CHECK(s, L"ax");
		s.insert(s.end(), L'b');
		CHECK(s, L"axb");
	}

	// erase()
	{
		VDStringW s;

		s.erase();
		CHECK(s, L"");
		s.assign(L"xy");
		s.erase();
		CHECK(s, L"");
		s.assign(L"xy");
		s.erase((VDStringW::size_type)0, 0);
		CHECK(s, L"xy");
		s.erase(0, 100);
		CHECK(s, L"");
		s.assign(L"xy");
		s.erase(0, 1);
		CHECK(s, L"y");
		s.assign(L"xy");
		s.erase(1, 40);
		CHECK(s, L"x");
		s.assign(L"abcdefghijklmnopqrstuvwxyz");
		s.erase(23, 2);
		CHECK(s, L"abcdefghijklmnopqrstuvwz");
	}
	{
		VDStringW s;

		s.assign(L"xyz");
		s.erase(s.begin());
		CHECK(s, L"yz");
		s.erase(s.begin()+1);
		CHECK(s, L"y");
		s.erase(s.begin());
		CHECK(s, L"");
	}
	{
		VDStringW s;

		s.assign(L"xyz");
		s.erase(s.begin(), s.begin() + 3);
		CHECK(s, L"");
		s.assign(L"xyz");
		s.erase(s.begin(), s.begin() + 1);
		CHECK(s, L"yz");
		s.assign(L"xyz");
		s.erase(s.begin() + 2, s.begin() + 2);
		CHECK(s, L"xyz");
	}

	// replace
	{
		VDStringW s;

		s.assign(L"xyz");
		s.replace(0, 0, L"", 0);
		CHECK(s, L"xyz");

		s.assign(L"xyz");
		s.replace(0, 100, L"", 0);
		CHECK(s, L"");

		s.assign(L"xyz");
		s.replace(3, 0, L"abc", 3);
		CHECK(s, L"xyzabc");

		s.assign(L"xyz");
		s.replace(0, 2, L"abc", 4);
		CHECK(s, VDStringW(L"abc\0z", 5));
	}

	// swap
	{
		VDStringW x;
		VDStringW y(L"abc");

		x.swap(y);
		CHECK(x, L"abc");
		CHECK(y, L"");

		x.erase();
		x.swap(y);
		CHECK(x, L"");
		CHECK(x.capacity(), 0);
		CHECK(y, L"");
		CHECK(y.capacity(), 3);
	}

	// c_str
	{
		CHECK(wcscmp(VDStringW().c_str(), L""), 0);
		CHECK(wcscmp(VDStringW(L"xyz").c_str(), L"xyz"), 0);
	}

	// find
	{
		CHECK(VDStringW().find(L'a'), VDStringW::npos);
		CHECK(VDStringW(L"xyz").find(L'x', 0), 0);
		CHECK(VDStringW(L"xyz").find(L'x', 1), VDStringW::npos);
		CHECK(VDStringW(L"xyz").find(L'y', 0), 1);
		CHECK(VDStringW(L"xyz").find(L'y', 1), 1);
		CHECK(VDStringW(L"xyz").find(L'y', 2), VDStringW::npos);
		CHECK(VDStringW(L"xyz").find(L'y', 3), VDStringW::npos);
		CHECK(VDStringW(L"xyz").find(L'z', 0), 2);
		CHECK(VDStringW(L"xyz").find(L'z', 1), 2);
		CHECK(VDStringW(L"xyz").find(L'z', 2), 2);
		CHECK(VDStringW(L"xyz").find(L'z', 3), VDStringW::npos);
	}

	// sprintf
	{
		VDStringW s;

		s.sprintf(L"");
		CHECK(s, L"");
		s.sprintf(L"%d", 12345);
		CHECK(s, L"12345");
	}

	// operator==
	{
		CHECK(VDStringW() == VDStringW(), true);
		CHECK(VDStringW(L"abc") == VDStringW(L"abc"), true);
		CHECK(VDStringW(L"abc") == VDStringW(L"def"), false);
		CHECK(VDStringW(L"abc") == VDStringW(L"abc", 4), false);

		CHECK(VDStringW() == L"", true);
		CHECK(VDStringW(L"abc") == L"abc", true);
		CHECK(VDStringW(L"abc") == L"def", false);
		CHECK(VDStringW(L"abc", 4) == L"abc", false);

		CHECK(L"" == VDStringW(), true);
		CHECK(L"abc" == VDStringW(L"abc"), true);
		CHECK(L"abc" == VDStringW(L"def"), false);
		CHECK(L"abc" == VDStringW(L"abc", 4), false);
	}

	// operator!=
	{
		CHECK(VDStringW() != VDStringW(), false);
		CHECK(VDStringW(L"abc") != VDStringW(L"abc"), false);
		CHECK(VDStringW(L"abc") != VDStringW(L"def"), true);
		CHECK(VDStringW(L"abc") != VDStringW(L"abc", 4), true);

		CHECK(VDStringW() != L"", false);
		CHECK(VDStringW(L"abc") != L"abc", false);
		CHECK(VDStringW(L"abc") != L"def", true);
		CHECK(VDStringW(L"abc", 4) != L"abc", true);

		CHECK(L"" != VDStringW(), false);
		CHECK(L"abc" != VDStringW(L"abc"), false);
		CHECK(L"abc" != VDStringW(L"def"), true);
		CHECK(L"abc" != VDStringW(L"abc", 4), true);
	}

	// operator+
	{
		CHECK(VDStringW() + VDStringW(), L"");
		CHECK(VDStringW() + VDStringW(L"abc"), L"abc");
		CHECK(VDStringW(L"def") + VDStringW(), L"def");
		CHECK(VDStringW(L"def") + VDStringW(L"abc"), L"defabc");

		CHECK(VDStringW() + L"", L"");
		CHECK(VDStringW() + L"abc", L"abc");
		CHECK(VDStringW(L"def") + L"", L"def");
		CHECK(VDStringW(L"def") + L"abc", L"defabc");

		CHECK(VDStringW() + L'x', L"x");
		CHECK(VDStringW(L"a") + L'x', L"ax");
		CHECK(VDStringW(L"abcdef") + (wchar_t)0, VDStringW(L"abcdef", 7));
	}

	///////////////////////////////////////////////////////////////////////////
	// Wide string ref tests
	///////////////////////////////////////////////////////////////////////////

	// split
	{
		VDStringRefW s;
		VDStringRefW token;

		s = L"";
		CHECK(s.split(L',', token), false);
		CHECK(s, L"");

		s = L",";
		CHECK(s.split(L',', token), true);
		CHECK(s, L"");
		CHECK(token, L"");

		s = L",,";
		CHECK(s.split(L',', token), true);
		CHECK(s, L",");
		CHECK(token, L"");

		s = L"a";
		CHECK(s.split(L',', token), false);

		s = L"a,,";
		CHECK(s.split(L',', token), true);
		CHECK(s, L",");
		CHECK(token, L"a");

		s = L"one,two,three";
		CHECK(s.split(L',', token), true);
		CHECK(s, L"two,three");
		CHECK(token, L"one");
	}

	return 0;
}
