#include "ruleset.h"
#include "runtime_dasm.h"
#include "utils.h"

char *apply_ruleset(VDDisassemblyContext *pContext, const ruleset *rs, int *sp, char *hp, const uint8 *source, int bytes, const uint8 *&source_end);
char *VDDisasmMatchRule(VDDisassemblyContext *pContext, const unsigned char *source, const unsigned char *pattern, int pattern_len, int bytes, int *sp, char *hp, const unsigned char *&source_end);

void *VDDisasmDecompress(void *_dst, const unsigned char *src, int src_len) {
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
		const unsigned char *result_cache[4][2];

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

			packctl = *src++;
			packsrc = packctl >> 6;

			prematch = (packctl & 7);
			postmatch = ((packctl>>3) & 7);
			literal = (*src++ - 1);

			*dst++ = prematch + postmatch + literal;

			const unsigned char *result_start = dst;

			for(cnt=0; cnt<prematch; ++cnt)
				*dst++ = result_cache[packsrc][0][cnt];

			for(cnt=0; cnt<literal; ++cnt)
				*dst++ = *src++;

			for(cnt=0; cnt<postmatch; ++cnt)
				*dst++ = result_cache[packsrc][1][cnt-postmatch];

			// cycle result cache

			for(cnt=3; cnt>0; --cnt) {
				result_cache[cnt][0] = result_cache[cnt-1][0];
				result_cache[cnt][1] = result_cache[cnt-1][1];
			}

			result_cache[0][0] = result_start;
			result_cache[0][1] = dst;
		}

		src += 2;

		*dst++ = 0;
		*dst++ = 0;
	}

	prstab[0] = prstab[rulesets];

	return dst;
}

long VDDisasmPack32(const int *src) {
	return src[0] + (src[1]<<8) + (src[2]<<16) + (src[3]<<24);
}

void VDDisasmExpandRule(VDDisassemblyContext *pContext, char *s, const unsigned char *result, const int *sp_base, const unsigned char *source) {
	static const char *const reg64[16]={"rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi","r8" ,"r9" ,"r10" ,"r11" ,"r12" ,"r13" ,"r14" ,"r15" };
	static const char *const reg32[16]={"eax","ecx","edx","ebx","esp","ebp","esi","edi","r8d","r9d","r10d","r11d","r12d","r13d","r14d","r15d"};
	static const char *const reg16[16]={ "ax", "cx", "dx", "bx", "sp", "bp", "si", "di","r8w","r9w","r10w","r11w","r12w","r13w","r14w","r15w"};
	static const char *const reg8a[16]={ "al"," cl", "dl", "bl","spl","bpl","sil","dil","r8b","r9b","r10b","r11b","r12b","r13b","r14b","r15b"};
	static const char *const reg8 [16]={"al","cl","dl","bl","ah","ch","dh","bh"};
	static const char *const regmmx[8]={"mm0","mm1","mm2","mm3","mm4","mm5","mm6","mm7"};
	static const char *const regxmm[16]={"xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7","xmm8","xmm9","xmm10","xmm11","xmm12","xmm13","xmm14","xmm15"};
	static const char *const regcrn[8]={"cr0","cr1","cr2","cr3","cr4","cr5","cr6","cr7"};
	static const char *const regdrn[8]={"dr0","dr1","dr2","dr3","dr4","dr5","dr6","dr7"};
	static const char *const regseg[8]={"es","cs","ss","ds","fs","gs","?6s","?7s"};
	static const char *const regf[8]={"st(0)","st(1)","st(2)","st(3)","st(4)","st(5)","st(6)","st(7)"};

	static const char *const *const sStaticLabels[]={
		reg32,
		reg16,
		reg8,
		regmmx,
		regxmm,
		regcrn,
		regdrn,
		regseg,
		regf
	};

	const unsigned char *result_limit = result + result[0]+1;

	++result;

	while(result < result_limit) {
		char c = *result++;

		if ((unsigned char)(c&0x7f) < 32) {
			if (c & 0x80) {
				c &= 0x7f;
				*s++ = ' ';
			}

			static const unsigned char static_bitfields[8]={
				0x80, 0x30, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00
			};

			unsigned char control_byte = (unsigned char)*result++;
			unsigned char bitfield = static_bitfields[control_byte >> 5];

			if (!bitfield)
				bitfield = (unsigned char)*result++;

			int bf_start = bitfield & 15;
			int bf_siz = bitfield>>4;
			const char *arg_s = (const char *)sp_base[c-1];
			int arg = (sp_base[c-1] >> bf_start) & ((1<<bf_siz)-1);

			control_byte &= 0x1f;

			if (control_byte < 10) {
				s = strtack(s, sStaticLabels[control_byte-1][arg]);
			} else {
				long symoffset = 0;

				switch(control_byte) {
				case kTarget_r1632:
					s = strtack(s, reg32[arg] + pContext->bSizeOverride);
					break;
				case kTarget_rmx:
					s = strtack(s, (pContext->bSizeOverride ? regxmm : regmmx)[arg]);
					break;
				case kTarget_lx:
					symoffset = VDDisasmPack32(sp_base + c - 1);
					s += sprintf(s, "%08lxh", symoffset);
					break;
				case kTarget_hx:
					s += sprintf(s, "%02x%02xh"
								, (unsigned char)sp_base[c]
								, (unsigned char)sp_base[c-1]
								);
					break;
				case kTarget_x:
					s += sprintf(s, "0%02xh" + ((unsigned char)arg < 0xa0), arg);
					break;
				case kTarget_lo:
					symoffset = VDDisasmPack32(sp_base + c - 1);
					s += sprintf(s, "%c%02lxh", symoffset<0 ? '-' : '+', abs(symoffset));
					break;
				case kTarget_ho:
					{
						short x =  ((unsigned char)sp_base[c  ] << 8)
								+  (unsigned char)sp_base[c-1];

						s += sprintf(s, "%c%02lxh", x<0 ? '-' : '+', abs(x));
					}
					break;
				case kTarget_o:
					s += sprintf(s, "%c%02xh", arg&0x80?'-':'+', abs((signed char)arg));
					break;
				case kTarget_la:
					symoffset = (long)source + VDDisasmPack32(sp_base + c - 1) + pContext->physToVirtOffset;
					s += sprintf(s, "%08lxh", symoffset);
					break;
				case kTarget_ha:
					symoffset = (long)source + (signed short)(sp_base[c-1] + (sp_base[c]<<8)) + pContext->physToVirtOffset;
					s += sprintf(s, "%08lxh", symoffset);
					break;
				case kTarget_a:
					symoffset = (long)source + (signed char)arg + pContext->physToVirtOffset;
					s += sprintf(s, "%08lxh", symoffset);
					break;
				case kTarget_s:
					s = strtack(s, arg_s);
					break;
				case kTarget_r3264:
					s = strtack(s, (pContext->rex & 8 ? reg32 : reg64)[arg + ((pContext->rex & 0x04) << 1)]);
					break;
				case kTarget_r163264:
					s = strtack(s, (pContext->rex & 8 ? reg64 : pContext->bSizeOverride ? reg16 : reg32)[arg + ((pContext->rex & 0x04) << 1)]);
					break;
				case kTarget_ext:
					switch(*result++) {
					case kTarget_ext_r3264rexX:
						s = strtack(s, (pContext->bAddressOverride ? reg32 : reg64)[arg + ((pContext->rex & 0x02) << 2)]);
						break;
					case kTarget_ext_r3264rexB:
						s = strtack(s, (pContext->bAddressOverride ? reg32 : reg64)[arg + ((pContext->rex & 0x01) << 3)]);
						break;
					case kTarget_ext_r163264rexB:
						s = strtack(s, (pContext->rex & 8 ? reg64 : pContext->bSizeOverride ? reg16 : reg32)[arg + ((pContext->rex & 0x01) << 3)]);
						break;
					}
					break;
				}

				if (symoffset && pContext->pSymLookup) {
					symoffset = pContext->pSymLookup((unsigned long)symoffset, s+2, 128);

					if (symoffset >= 0) {
						s[0] = ' ';
						s[1] = '(';
						s += 2;
						while(*s)
							++s;
						if (symoffset)
							s += sprintf(s, "+%02xh", symoffset);
						*s++ = ')';
					}
				}
			}
		} else if ((unsigned char)c >= 0xe0) {
			switch(c) {
			case kTarget_ap:
				if (pContext->pszSegmentOverride) {
					s = strtack(s, pContext->pszSegmentOverride);
					*s++ = ':';
				}
				break;
			case kTarget_p_cs:	pContext->pszSegmentOverride = regseg[1];	break;
			case kTarget_p_ss:	pContext->pszSegmentOverride = regseg[2];	break;
			case kTarget_p_ds:	pContext->pszSegmentOverride = regseg[3];	break;
			case kTarget_p_es:	pContext->pszSegmentOverride = regseg[0];	break;
			case kTarget_p_fs:	pContext->pszSegmentOverride = regseg[4];	break;
			case kTarget_p_gs:	pContext->pszSegmentOverride = regseg[5];	break;
			case kTarget_p_66:	pContext->bSizeOverride = true;				break;
			case kTarget_p_67:	pContext->bAddressOverride = true;			break;
			case kTarget_p_F2:	pContext->bRepnePrefix = true;				break;
			case kTarget_p_F3:	pContext->bRepePrefix = true;				break;
			case kTarget_p_rex:	pContext->rex = sp_base[0];					break;
			}
		} else
			*s++ = c;
	}

	*s = 0;
}

char *VDDisasmApplyRuleset(VDDisassemblyContext *pContext, const unsigned char *rs, int *sp, char *hp, const unsigned char *source, int bytes, const uint8 *&source_end) {
	char *hpr;

	while(rs[0] || rs[1]) {
		const unsigned char *src_end;
		const unsigned char *result = rs + rs[0] + 1;
		const unsigned char *match_next = result + result[0] + 1;

		hpr = VDDisasmMatchRule(pContext, source, rs+1, rs[0]>>1, bytes, sp, hp, src_end);

		if (hpr) {
			VDDisasmExpandRule(pContext, hpr, result, sp, src_end);

			source_end = src_end;
			return hpr;
		}

		rs = match_next;
	}

	return NULL;
}

char *VDDisasmMatchRule(VDDisassemblyContext *pContext, const unsigned char *source, const unsigned char *pattern, int pattern_len, int bytes, int *sp, char *hp, const unsigned char *&source_end) {
	while(bytes && pattern_len) {
		if (!pattern[1] && pattern[0]) {
			if (pattern[0] & 0x80) {
				int count = pattern[0] & 0x3f;

				if (pattern[0] & 0x40) {
					--source;
					++bytes;
				}
			
				const uint8 *src_end;

				hp = VDDisasmApplyRuleset(pContext, pContext->pRuleSystem[count+1], sp, hp, source, bytes, src_end);

				if (!hp)
					return NULL;

				*sp++ = *source;
				*sp++ = (int)hp;

				while(*hp++);

				source = src_end;
			} else if (pattern[0] < 16) {
				if (pattern[0] > bytes)
					return NULL;

				for(int i=0; i<pattern[0]; ++i) {
					*sp++ = *source++;
				}

				bytes -= pattern[0]-1;
			} else {
				switch(pattern[0]) {
				case 16:	if (!pContext->bSizeOverride)		return NULL;	break;
				case 17:	if (!pContext->bAddressOverride)	return NULL;	break;
				case 18:	if (!pContext->bRepnePrefix)		return NULL;	break;
				case 19:	if (!pContext->bRepePrefix)			return NULL;	break;
				case 20:	if (pContext->pszSegmentOverride)	return NULL;	break;
				case 21:	if (!(pContext->rex & 8))			return NULL;	break;
				}
			}
		} else {
			uint8 b = *source++;

			if ((b & pattern[1]) != pattern[0])
				return NULL;

			*sp++ = b;
		}
		pattern += 2;
		--bytes;
		--pattern_len;
	}

	if (!pattern_len) {
		source_end = source;
		return hp;
	}

	return NULL;
}

void VDDisassemble(VDDisassemblyContext *pvdc, const uint8 *source, int bytes) {
	while(bytes > 0) {
		const uint8 *src2 = source;
		const uint8 *src_end;
		char *s;

		pvdc->bAddressOverride = false;
		pvdc->bSizeOverride = false;
		pvdc->bRepePrefix = false;
		pvdc->bRepnePrefix = false;
		pvdc->pszSegmentOverride = NULL;
		pvdc->rex = 0;

		do {
			s = VDDisasmApplyRuleset(pvdc, pvdc->pRuleSystem[0], pvdc->stack, pvdc->heap, src2, bytes, src_end);

			bytes -= (src_end - src2);
			src2 = src_end;
		} while(!*s && bytes);

		if (!bytes)
			break;

		int count = src_end - source;
		int linecnt;

		printf("%08lx: ", (unsigned long)source + pvdc->physToVirtOffset);

		for(linecnt=0; linecnt<7 && source < src_end; ++linecnt)
			printf("%02x", (unsigned char)*source++);

		char *t = s;
		while(*t && *t != ' ')
			++t;

		if (*t)
			*t++ = 0;

		printf("%*c%-7s%s\n", 2 + 2*(7-linecnt), ' ', s, t);

		// flush remaining bytes

		while(source < src_end) {
			printf("         ");
			for(linecnt=0; linecnt<7 && source < src_end; ++linecnt)
				printf(" %02x", (unsigned char)*source++);
			putchar('\n');
		}

		bytes -= count;
	}
}
