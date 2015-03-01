#include <vd2/system/filesys.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/file.h>
#include "test.h"

DEFINE_TEST(BufferedStream) {
	typedef vdfastvector<uint8> Data;
	Data tempstream(1000000);

	Data::iterator it(tempstream.begin()), itEnd(tempstream.end());
	for(; it!=itEnd; ++it) {
		*it = (uint8)rand();
	}

	VDMemoryStream ms(tempstream.data(), tempstream.size());
	VDBufferedStream bs(&ms, 256);

	char tmpbuf[1024];

	// test for 1.7.4 bug
	bs.Seek(0);
	bs.Read(tmpbuf, 256);
	TEST_ASSERT(!memcmp(tmpbuf, tempstream.data() + 0, 256));
	bs.Read(tmpbuf, 256);
	TEST_ASSERT(!memcmp(tmpbuf, tempstream.data() + 256, 256));
	bs.Seek(0);
	bs.Read(tmpbuf, 256);
	TEST_ASSERT(!memcmp(tmpbuf, tempstream.data() + 0, 256));

	// random test
	uint32 pos = (uint32)bs.Pos();
	for(uint32 i=0; i<20; ++i) {
		for(uint32 j=0; j<50000; ++j) {
			uint32 len = rand() & 1023;

			if (1000000 - pos < len || (rand() & 1)) {
				do {
					pos = (rand() ^ (rand() << 14)) % 1000000;
				} while(pos + len > 1000000);

				bs.Seek(pos);
			}

			bs.Read(tmpbuf, len);
			TEST_ASSERT(!memcmp(tmpbuf, tempstream.data() + pos, len));
			pos += len;
		}

		tmpbuf[0] = '[';
		for(uint32 j=0; j<=i; ++j)
			tmpbuf[j+1] = '*';
		for(uint32 j=i+1; j<20; ++j)
			tmpbuf[j+1] = ' ';
		tmpbuf[21] = ']';
		tmpbuf[22] = '\r';
		tmpbuf[23] = 0;
		fputs(tmpbuf, stderr);
		fflush(stderr);
	}
	fputc('\n', stderr);

	return 0;
}

