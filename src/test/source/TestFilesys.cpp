#include <vd2/system/filesys.h>
#include "test.h"

DEFINE_TEST(Filesys) {

#define TEST(fn, x, y1, y2) \
	TEST_ASSERT(!strcmp(fn(x), y2));	\
	TEST_ASSERT(!wcscmp(fn(L##x), L##y2));	\
	TEST_ASSERT(fn##Left(VDStringA(x))==y1);	\
	TEST_ASSERT(fn##Right(VDStringA(x))==y2);	\
	TEST_ASSERT(fn##Left(VDStringW(L##x))==L##y1);	\
	TEST_ASSERT(fn##Right(VDStringW(L##x))==L##y2)

	TEST(VDFileSplitPath, "", "", "");
	TEST(VDFileSplitPath, "x", "", "x");
	TEST(VDFileSplitPath, "x\\y", "x\\", "y");
	TEST(VDFileSplitPath, "x\\y\\z", "x\\y\\", "z");
	TEST(VDFileSplitPath, "x\\", "x\\", "");
	TEST(VDFileSplitPath, "x\\y\\z\\", "x\\y\\z\\", "");
	TEST(VDFileSplitPath, "c:", "c:", "");
	TEST(VDFileSplitPath, "c:x", "c:", "x");
	TEST(VDFileSplitPath, "c:\\", "c:\\", "");
	TEST(VDFileSplitPath, "c:\\x", "c:\\", "x");
	TEST(VDFileSplitPath, "c:\\x\\", "c:\\x\\", "");
	TEST(VDFileSplitPath, "c:\\x\\", "c:\\x\\", "");
	TEST(VDFileSplitPath, "c:x\\y", "c:x\\", "y");
	TEST(VDFileSplitPath, "\\\\server\\share\\", "\\\\server\\share\\", "");
	TEST(VDFileSplitPath, "\\\\server\\share\\x", "\\\\server\\share\\", "x");
#undef TEST

#define TEST(fn, x, y1, y2)	\
	VDASSERT(!strcmp(fn(x), y2));	\
	VDASSERT(!wcscmp(fn(L##x), L##y2));	\
	VDASSERT(fn(VDStringA(x))==y1);	\
	VDASSERT(fn(VDStringW(L##x))==L##y1)

	TEST(VDFileSplitRoot, "", "", "");
	TEST(VDFileSplitRoot, "c:", "c:", "");
	TEST(VDFileSplitRoot, "c:x", "c:", "x");
	TEST(VDFileSplitRoot, "c:x\\", "c:", "x\\");
	TEST(VDFileSplitRoot, "c:x\\y", "c:", "x\\y");
	TEST(VDFileSplitRoot, "c:\\", "c:\\", "");
	TEST(VDFileSplitRoot, "c:\\x", "c:\\", "x");
	TEST(VDFileSplitRoot, "c:\\x\\", "c:\\", "x\\");
	TEST(VDFileSplitRoot, ".", "", ".");
	TEST(VDFileSplitRoot, "..", "", "..");
	TEST(VDFileSplitRoot, "\\", "", "\\");
	TEST(VDFileSplitRoot, "\\x", "", "\\x");
	TEST(VDFileSplitRoot, "\\x\\", "", "\\x\\");
	TEST(VDFileSplitRoot, "\\x\\y", "", "\\x\\y");
	TEST(VDFileSplitRoot, "\\\\server\\share", "\\\\server\\share", "");
	TEST(VDFileSplitRoot, "\\\\server\\share\\", "\\\\server\\share\\", "");
	TEST(VDFileSplitRoot, "\\\\server\\share\\x", "\\\\server\\share\\", "x");
	TEST(VDFileSplitRoot, "\\\\server\\share\\x\\", "\\\\server\\share\\", "x\\");
	TEST(VDFileSplitRoot, "\\\\server\\share\\x\\y", "\\\\server\\share\\", "x\\y");
#undef TEST

	// WILDCARD TESTS

	// Basic non-wildcard tests
	TEST_ASSERT(VDFileWildMatch(L"", L"random.bin") == false);
	TEST_ASSERT(VDFileWildMatch(L"random.bin", L"random.bin") == true);
	TEST_ASSERT(VDFileWildMatch(L"random.bin", L"randum.bin") == false);
	TEST_ASSERT(VDFileWildMatch(L"random.bin", L"randum.bi") == false);
	TEST_ASSERT(VDFileWildMatch(L"random.bin", L"randum.binx") == false);
	TEST_ASSERT(VDFileWildMatch(L"random.bin", L"RANDOM.BIN") == true);
	TEST_ASSERT(VDFileWildMatch(L"random.bin", L"xrandom.bin") == false);

	// ? tests
	TEST_ASSERT(VDFileWildMatch(L"random.b?n", L"random.bin") == true);
	TEST_ASSERT(VDFileWildMatch(L"random.bin?", L"random.bin") == false);
	TEST_ASSERT(VDFileWildMatch(L"?random.bin", L"random.bin") == false);

	// * tests
	TEST_ASSERT(VDFileWildMatch(L"*", L"random.bin") == true);
	TEST_ASSERT(VDFileWildMatch(L"random*", L"random.bin") == true);
	TEST_ASSERT(VDFileWildMatch(L"random.bin*", L"random.bin") == true);
	TEST_ASSERT(VDFileWildMatch(L"*random.bin", L"random.bin") == true);
	TEST_ASSERT(VDFileWildMatch(L"random*.bin", L"random.bin") == true);
	TEST_ASSERT(VDFileWildMatch(L"random*bin", L"random.bin") == true);
	TEST_ASSERT(VDFileWildMatch(L"random**bin", L"random.bin") == true);
	TEST_ASSERT(VDFileWildMatch(L"random*bin", L"random.bin.bin") == true);
	TEST_ASSERT(VDFileWildMatch(L"random*bin", L"random.ban.bin") == true);
	TEST_ASSERT(VDFileWildMatch(L"ran*?*bin", L"random.bin") == true);
	TEST_ASSERT(VDFileWildMatch(L"*.bin", L"random.bin") == true);
	TEST_ASSERT(VDFileWildMatch(L"*n*", L"random.bin") == true);
	TEST_ASSERT(VDFileWildMatch(L"*om*and*", L"random.bin") == false);

	// Path canonicalization tests
	TEST_ASSERT(VDFileGetCanonicalPath(L"") == L".");
	TEST_ASSERT(VDFileGetCanonicalPath(L".") == L".");
	TEST_ASSERT(VDFileGetCanonicalPath(L"c:") == L"c:");
	TEST_ASSERT(VDFileGetCanonicalPath(L"c:\\") == L"c:\\");
	TEST_ASSERT(VDFileGetCanonicalPath(L"c:\\\\") == L"c:\\");
	TEST_ASSERT(VDFileGetCanonicalPath(L"\\\\server\\share") == L"\\\\server\\share");
	TEST_ASSERT(VDFileGetCanonicalPath(L"\\\\server\\share\\") == L"\\\\server\\share");
	TEST_ASSERT(VDFileGetCanonicalPath(L"c:/") == L"c:\\");
	TEST_ASSERT(VDFileGetCanonicalPath(L"c:/path") == L"c:\\path");
	TEST_ASSERT(VDFileGetCanonicalPath(L"c:/path/") == L"c:\\path");
	TEST_ASSERT(VDFileGetCanonicalPath(L"c:/path/path2") == L"c:\\path\\path2");
	TEST_ASSERT(VDFileGetCanonicalPath(L"c:/path/path2;1") == L"c:\\path\\path2;1");
	TEST_ASSERT(VDFileGetCanonicalPath(L"c:/path\\path2/;1") == L"c:\\path\\path2;1");
	TEST_ASSERT(VDFileGetCanonicalPath(L"c:path") == L"c:path");
	TEST_ASSERT(VDFileGetCanonicalPath(L"c:path/") == L"c:path");
	TEST_ASSERT(VDFileGetCanonicalPath(L"c:path/path2") == L"c:path\\path2");
	TEST_ASSERT(VDFileGetCanonicalPath(L"c:path/path2;1") == L"c:path\\path2;1");
	TEST_ASSERT(VDFileGetCanonicalPath(L"c:path\\path2/;1") == L"c:path\\path2;1");
	TEST_ASSERT(VDFileGetCanonicalPath(L"c:/path1/./path2/../path3") == L"c:\\path1\\path3");
	TEST_ASSERT(VDFileGetCanonicalPath(L"c:/path1/./path2/../../../../path3") == L"c:\\path3");
	TEST_ASSERT(VDFileGetCanonicalPath(L"..") == L"..");
	TEST_ASSERT(VDFileGetCanonicalPath(L"../..") == L"..\\..");
	TEST_ASSERT(VDFileGetCanonicalPath(L"../x/..") == L"..");
	TEST_ASSERT(VDFileGetCanonicalPath(L".\\x") == L"x");
	TEST_ASSERT(VDFileGetCanonicalPath(L".\\x\\y") == L"x\\y");

	// Path relativization tests
	TEST_ASSERT(VDFileIsRelativePath(L"c:\\") == false);
	TEST_ASSERT(VDFileIsRelativePath(L"c:\\foo") == false);
	TEST_ASSERT(VDFileIsRelativePath(L"c:\\foo\\") == false);
	TEST_ASSERT(VDFileIsRelativePath(L"c:\\foo\\bar") == false);
	TEST_ASSERT(VDFileIsRelativePath(L"c:\\foo\\bar\\") == false);
	TEST_ASSERT(VDFileIsRelativePath(L"\\\\server\\share") == false);
	TEST_ASSERT(VDFileIsRelativePath(L"\\\\server\\share\\") == false);
	TEST_ASSERT(VDFileIsRelativePath(L"c:") == true);
	TEST_ASSERT(VDFileIsRelativePath(L".") == true);
	TEST_ASSERT(VDFileIsRelativePath(L"..") == true);
	TEST_ASSERT(VDFileIsRelativePath(L"x") == true);
	TEST_ASSERT(VDFileIsRelativePath(L"x\\") == true);
	TEST_ASSERT(VDFileIsRelativePath(L"/x") == true);
	TEST_ASSERT(VDFileIsRelativePath(L"/x/") == true);

	TEST_ASSERT(VDFileGetRelativePath(L"c:/program files/my stuff", L"c:/program files/my stuff", true) == L".");
	TEST_ASSERT(VDFileGetRelativePath(L"c:/program files/my stuff", L"c:/program files/my stuff/foo.exe", true) == L"foo.exe");
	TEST_ASSERT(VDFileGetRelativePath(L"c:/program files/my stuff", L"c:/program files/my stuff/subdir/foo.exe", true) == L"subdir\\foo.exe");
	TEST_ASSERT(VDFileGetRelativePath(L"c:/program files/my stuff", L"c:/program files/other stuff/foo.exe", true) == L"..\\other stuff\\foo.exe");
	TEST_ASSERT(VDFileGetRelativePath(L"c:/program files/my stuff", L"c:/program files/other stuff/foo.exe", false) == L"");
	TEST_ASSERT(VDFileGetRelativePath(L"c:/program files/my stuff", L"c:/program files/my stuff/../other stuff/foo.exe", true) == L"..\\other stuff\\foo.exe");
	TEST_ASSERT(VDFileGetRelativePath(L"c:/program files/my stuff", L"c:/program files/my stuff/../other stuff/../MY STUFF/subdir/foo.exe", true) == L"subdir\\foo.exe");
	TEST_ASSERT(VDFileGetRelativePath(L"C:/PROGRAM FILES/MY STUFF", L"c:/program files/my stuff/foo.exe;1", true) == L"foo.exe;1");
	TEST_ASSERT(VDFileGetRelativePath(L"C:/PROGRAM FILES/MY STUFF", L"d:/program files/my stuff/foo.exe", true) == L"");
	TEST_ASSERT(VDFileGetRelativePath(L"C:/PROGRAM FILES/MY STUFF", L"program files/my stuff/foo.exe", true) == L"");
	TEST_ASSERT(VDFileGetRelativePath(L"PROGRAM FILES/MY STUFF", L"c:/program files/my stuff/foo.exe", true) == L"");

	return 0;
}

