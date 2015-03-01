#include <string.h>
#include <vd2/Riza/audioformat.h>

namespace nsVDWinFormats {
	bool Guid::operator==(const Guid& r) const {
		return !memcmp(this, &r, 16);
	}

	extern const Guid kKSDATAFORMAT_SUBTYPE_PCM={
		kWAVE_FORMAT_PCM, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71
	};
};
