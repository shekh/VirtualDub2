#include "runtime_tracedec.h"

namespace {
	bool VDDisasmMatchRule(VDTracedecContext *pContext, const unsigned char *source, const unsigned char *pattern, int pattern_len, int bytes, const unsigned char *&source_end);

	bool VDDisasmApplyRuleset(VDTracedecContext *pContext, const unsigned char *rs, const unsigned char *source, int bytes, const uint8 *&source_end) {
		while(rs[0] || rs[1]) {
			const unsigned char *src_end;
			const unsigned char *result = rs + rs[0] + 1;
			const unsigned char *match_next = result + 1;

			if (VDDisasmMatchRule(pContext, source, rs+1, rs[0]>>1, bytes, src_end)) {
				uint8 flags = *result;

				if (flags & 0x80) {
					switch(flags) {
						case 0x80:	pContext->bSizeOverride = true; break;
						case 0x81:	pContext->bAddressOverride = true; break;
						case 0x82:	pContext->bRepnePrefix = true; break;
						case 0x83:	pContext->bRepePrefix = true; break;
					}
				} else {
					pContext->mFlags |= flags;
					pContext->bFinal = true;
				}

				source_end = src_end;
				return true;
			}

			rs = match_next;
		}

		return false;
	}

	bool VDDisasmMatchRule(VDTracedecContext *pContext, const unsigned char *source, const unsigned char *pattern, int pattern_len, int bytes, const unsigned char *&source_end) {
		while(bytes && pattern_len) {
			if (!pattern[1] && pattern[0]) {
				if (pattern[0] & 0x80) {
					int count = pattern[0] & 0x3f;

					if (pattern[0] & 0x40)
						--source;
				
					const uint8 *src_end;

					if (!VDDisasmApplyRuleset(pContext, pContext->pRuleSystem[count+1], source, bytes, src_end))
						return false;

					source = src_end;
				} else if (pattern[0] < 16) {
					if (pattern[0] > bytes)
						return false;

					source += pattern[0];
					bytes -= pattern[0]-1;
				} else {
					switch(pattern[0]) {
					case 16:	if (!pContext->bSizeOverride)		return false;	break;
					case 17:	if (!pContext->bAddressOverride)	return false;	break;
					case 18:	if (!pContext->bRepnePrefix)		return false;	break;
					case 19:	if (!pContext->bRepePrefix)			return false;	break;
					case 20:	if (pContext->pszSegmentOverride)	return false;	break;
					case 21:	if (!(pContext->rex & 8))			return false;	break;
					}
				}
			} else {
				uint8 b = *source++;

				if ((b & pattern[1]) != pattern[0])
					return false;
			}
			pattern += 2;
			--bytes;
			--pattern_len;
		}

		if (!pattern_len) {
			source_end = source;
			return true;
		}

		return false;
	}
}

bool VDTraceObjectCode(VDTracedecContext *pvdc, const uint8 *source, int bytes, VDTracedecResult& res) {
	const uint8 *src2 = source;
	const uint8 *src_end;

	pvdc->bAddressOverride = false;
	pvdc->bSizeOverride = false;
	pvdc->bRepePrefix = false;
	pvdc->bRepnePrefix = false;
	pvdc->bFinal = false;
	pvdc->mFlags = 0;
	pvdc->pszSegmentOverride = NULL;
	pvdc->rex = 0;

	do {
		if (VDDisasmApplyRuleset(pvdc, pvdc->pRuleSystem[0], src2, bytes, src_end) && pvdc->bFinal) {
			res.mbIsCall	= 0 != (pvdc->mFlags & 0x01);
			res.mbIsJcc		= 0 != (pvdc->mFlags & 0x02);
			res.mbIsJmp		= 0 != (pvdc->mFlags & 0x04);
			res.mbIsReturn	= 0 != (pvdc->mFlags & 0x08);
			res.mbIsInvalid	= 0 != (pvdc->mFlags & 0x10);
			res.mbTargetValid = false;
			res.mRelTargetSize = -1;

			if (res.mbIsCall || res.mbIsJcc || res.mbIsJmp) {
				switch(pvdc->mFlags & 0x60) {
					case 0x00:
						break;
					case 0x20:
						res.mBranchTarget = (pvdc->physToVirtOffset + (ptrdiff_t)src_end) + ((sint8 *)src_end)[-1];
						res.mbTargetValid = true;
						res.mRelTargetSize = 1;
						break;
					case 0x40:
						res.mBranchTarget = (pvdc->physToVirtOffset + (ptrdiff_t)src_end) + ((sint16 *)src_end)[-1];
						res.mbTargetValid = true;
						res.mRelTargetSize = 2;
						break;
					case 0x60:
						res.mBranchTarget = (pvdc->physToVirtOffset + (ptrdiff_t)src_end) + ((sint32 *)src_end)[-1];
						res.mbTargetValid = true;
						res.mRelTargetSize = 4;
						break;
				}
			}

			res.mInsnLength = src_end - source;
			return true;
		}

		bytes -= (src_end - src2);
		src2 = src_end;
	} while(bytes);

	return false;
}

void *VDTracedecDecompress(void *_dst, const unsigned char *src, int src_len) {
	const unsigned char *src_limit = src + src_len;
	unsigned char *dst = (unsigned char *)_dst;

	// read ruleset count
	int rulesets = *src++;
	unsigned char **prstab = (unsigned char **)dst;

	dst += sizeof(unsigned char *) * (rulesets + 1);

	// decompress rulesets sequentially
	for(int rs=0; rs<rulesets; ++rs) {
		prstab[rs+1] = dst;

		const unsigned char *pattern_cache[4][2];

		while(src[0] || src[1]) {
			unsigned char packctl;
			int packsrc, cnt;

			// read pack control uint8 and copy prematch-literal-postmatch for pattern
			packctl = *src++;
			packsrc = packctl >> 6;

			int prematch = (packctl & 7) * 2;
			int postmatch = ((packctl>>3) & 7) * 2;
			int literal = (*src++ - 1) * 2;

			*dst++ = literal + prematch + postmatch;

			const unsigned char *pattern_start = dst;

			for(cnt=0; cnt<prematch; ++cnt)
				*dst++ = pattern_cache[packsrc][0][cnt];

			for(cnt=0; cnt<literal; ++cnt)
				*dst++ = *src++;

			for(cnt=0; cnt<postmatch; ++cnt)
				*dst++ = pattern_cache[packsrc][1][cnt-postmatch];

			// cycle pattern cache

			for(cnt=3; cnt>0; --cnt) {
				pattern_cache[cnt][0] = pattern_cache[cnt-1][0];
				pattern_cache[cnt][1] = pattern_cache[cnt-1][1];
			}

			pattern_cache[0][0] = pattern_start;
			pattern_cache[0][1] = dst;

			// read pack control uint8 and copy prematch-literal-postmatch for result
			*dst++ = *src++;
		}

		src += 2;

		*dst++ = 0;
		*dst++ = 0;
	}

	prstab[0] = prstab[rulesets];

	return dst;
}
