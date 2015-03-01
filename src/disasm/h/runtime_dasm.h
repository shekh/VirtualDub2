#ifndef f_VD2_DISASM_RUNTIME_DASM_H
#define f_VD2_DISASM_RUNTIME_DASM_H

#include <vd2/system/vdtypes.h>
#include <stddef.h>

struct VDDisassemblyContext {
	const unsigned char **pRuleSystem;
	long (*pSymLookup)(unsigned long virtAddr, char *buf, int buf_len);

	bool bSizeOverride;			// 66
	bool bAddressOverride;		// 67
	bool bRepnePrefix;			// F2
	bool bRepePrefix;			// F3
	unsigned char	rex;
	const char *pszSegmentOverride;

	ptrdiff_t	physToVirtOffset;

	char	heap[2048];
	int		stack[32];
};

void VDDisassemble(VDDisassemblyContext *pvdc, const uint8 *source, int bytes);

#endif
