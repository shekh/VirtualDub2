#ifndef f_VD2_DISASM_RUNTIME_TRACEDEC_H
#define f_VD2_DISASM_RUNTIME_TRACEDEC_H

#include <vd2/system/vdtypes.h>
#include <stddef.h>

struct VDTracedecContext {
	const unsigned char **pRuleSystem;

	bool bSizeOverride;			// 66
	bool bAddressOverride;		// 67
	bool bRepnePrefix;			// F2
	bool bRepePrefix;			// F3
	bool bFinal;
	unsigned char	rex;
	const char *pszSegmentOverride;

	uint8		mFlags;

	ptrdiff_t	physToVirtOffset;
};

struct VDTracedecResult {
	ptrdiff_t	mInsnLength;
	ptrdiff_t	mBranchTarget;
	bool		mbIsJmp;
	bool		mbIsJcc;
	bool		mbIsCall;
	bool		mbIsReturn;
	bool		mbIsInvalid;
	bool		mbTargetValid;
	int			mRelTargetSize;
};

void VDTraceObjectCode(VDTracedecContext *pvdc, const uint8 *source, int bytes);
void *VDTracedecDecompress(void *_dst, const unsigned char *src, int src_len);

#endif
