//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2001 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "stdafx.h"

#include <windows.h>

#include "resource.h"
#include "disasm.h"

extern HINSTANCE g_hInst;

#ifdef _M_AMD64
	#define VD_PTR_08lx	"%08I64x"
#else
	#define VD_PTR_08lx	"%08x"
#endif

#define MAX_INSTRUCTIONS (1024)

// WARNING: This is called from crash-time conditions!  No malloc() or new!!!

#define malloc not_allowed_here
#define new not_allowed_here

// Also, we keep as much of our initialized data as possible as const.  That way,
// it is in a write-locked code segment, which can't be overwritten.

char *VDDisasmMatchRule(VDDisassemblyContext *pContext, const unsigned char *source, const unsigned char *pattern, int pattern_len, int bytes, ptrdiff_t *sp, char *hp, const unsigned char *&source_end);

char *strtack(char *s, const char *t) {
	while(*s = *t)
		++s, ++t;

	return s;
}

// 1-15 are static lookups
static const char kTarget_r32		= 1;
static const char kTarget_r16		= 2;
static const char kTarget_r8		= 3;
static const char kTarget_rm		= 4;
static const char kTarget_rx		= 5;
static const char kTarget_rc		= 6;
static const char kTarget_rd		= 7;
static const char kTarget_rs		= 8;
static const char kTarget_rf		= 9;

// 16-31 are dynamic translations
static const char kTarget_r1632		= 16;
static const char kTarget_rmx		= 17;
static const char kTarget_x			= 18;
static const char kTarget_hx		= 19;
static const char kTarget_lx		= 20;
static const char kTarget_s			= 21;
static const char kTarget_o			= 22;
static const char kTarget_ho		= 23;
static const char kTarget_lo		= 24;
static const char kTarget_a			= 25;
static const char kTarget_ha		= 26;
static const char kTarget_la		= 27;
static const char kTarget_r3264		= 28;
static const char kTarget_r163264	= 29;
static const char kTarget_ext		= 30;

static const char kTarget_ext_r3264rexX	= 1;
static const char kTarget_ext_r3264rexB = 2;

static const char kTarget_ap		= (char)224;
static const char kTarget_p_cs		= (char)225;
static const char kTarget_p_ss		= (char)226;
static const char kTarget_p_ds		= (char)227;
static const char kTarget_p_es		= (char)228;
static const char kTarget_p_fs		= (char)229;
static const char kTarget_p_gs		= (char)230;
static const char kTarget_p_66		= (char)231;
static const char kTarget_p_67		= (char)232;
static const char kTarget_p_F2		= (char)233;
static const char kTarget_p_F3		= (char)234;
static const char kTarget_p_rex		= (char)235;

#ifdef _DEBUG
#undef new
#undef malloc
#include <string>
std::string decode_pattern(const unsigned char *pat, int patlen) {
	char buf[16];
	std::string tmp;

	while(patlen > 0) {
		unsigned char c = *pat++;
		unsigned char d = *pat++;

		patlen -= 2;

		if (!tmp.empty())
			tmp += ' ';

		if (!d) {
			if (c & 0x80) {
				int count = c & 0x3f;


				if (c & 0x40)
					tmp += '!';

				sprintf(buf, "ruleset(%d)", count);

				tmp += buf;
			} else if (c < 16) {
				for(int i=0; i<c; ++i)
					tmp += i ? " *" : "*";
			} else {
				switch(c) {
				case 16:	tmp += "[66]"; break;
				case 17:	tmp += "[67]"; break;
				case 18:	tmp += "[F2]"; break;
				case 19:	tmp += "[F3]"; break;
				case 20:	tmp += "[!s]"; break;
				case 21:	tmp += "[q]"; break;
				}
			}
		} else {
			if (d != 0xff)
				sprintf(buf, "%02x-%02x", c, c|d);
			else
				sprintf(buf, "%02x", c);

			tmp += buf;
		}
	}

	return tmp;
}

std::string decode_result(const unsigned char *result) {
	const unsigned char *result_limit = result + result[0]+1;
	std::string tmp;
	char buf[32];

	++result;

	while(result < result_limit) {
		char c = *result++;

		if ((unsigned char)(c&0x7f) < 32) {
			if (c & 0x80) {
				c &= 0x7f;
				tmp += ' ';
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

			sprintf(buf, "$%d", c);
			tmp += buf;

			if (bf_start > 0 || bf_siz < 8) {
				sprintf(buf, "[%d-%d]", bf_start, bf_start+bf_siz-1);
				tmp += buf;
			}

			control_byte &= 0x1f;

			if (control_byte < 10) {
				switch(control_byte-1) {
				case 0: tmp += "reg32"; break;
				case 1: tmp += "reg16"; break;
				case 2: tmp += "reg8"; break;
				case 3: tmp += "regmmx"; break;
				case 4: tmp += "regxmm"; break;
				case 5: tmp += "regcrn"; break;
				case 6: tmp += "regdrn"; break;
				case 7: tmp += "regseg"; break;
				case 8: tmp += "regf"; break;
				}
			} else {
				switch(control_byte) {
				case kTarget_r1632:		tmp += "r1632"; break;
				case kTarget_rmx:		tmp += "rmx"; break;
				case kTarget_lx:		tmp += "lx"; break;
				case kTarget_hx:		tmp += "hx"; break;
				case kTarget_x:			tmp += "x"; break;
				case kTarget_lo:		tmp += "lo"; break;
				case kTarget_ho:		tmp += "ho"; break;
				case kTarget_o:			tmp += "o"; break;
				case kTarget_la:		tmp += "la"; break;
				case kTarget_ha:		tmp += "ha"; break;
				case kTarget_a:			tmp += "a"; break;
				case kTarget_s:			tmp += "s"; break;
				case kTarget_r3264:		tmp += "r3264"; break;
				case kTarget_r163264:		tmp += "r163264"; break;
				case kTarget_ext:
					switch(*result++) {
					case kTarget_ext_r3264rexX:	tmp += "r3264rexX"; break;
					case kTarget_ext_r3264rexB:	tmp += "r3264rexB"; break;
					}
					break;
				}
			}
		} else if ((unsigned char)c >= 0xe0) {
			switch(c) {
			case kTarget_ap:	tmp += "$p_ap"; break;
			case kTarget_p_cs:	tmp += "$p_cs"; break;
			case kTarget_p_ss:	tmp += "$p_ss"; break;
			case kTarget_p_ds:	tmp += "$p_ds"; break;
			case kTarget_p_es:	tmp += "$p_es"; break;
			case kTarget_p_fs:	tmp += "$p_fs"; break;
			case kTarget_p_gs:	tmp += "$p_gs"; break;
			case kTarget_p_66:	tmp += "$p_66"; break;
			case kTarget_p_67:	tmp += "$p_67"; break;
			case kTarget_p_F2:	tmp += "$p_f2"; break;
			case kTarget_p_F3:	tmp += "$p_f3"; break;
			case kTarget_p_rex:	tmp += "$p_rex"; break;
			}
		} else
			tmp += c;
	}

	return tmp;
}

#endif


void *VDDisasmDecompress(void *_dst, const unsigned char *src, int /*src_len*/) {
	unsigned char *dst = (unsigned char *)_dst;

	// read ruleset count
	int rulesets = *src++;
	unsigned *prstab = (unsigned *)dst;

	dst += sizeof(unsigned) * (rulesets + 1);

	// decompress rulesets sequentially
	for(int rs=0; rs<rulesets; ++rs) {
		prstab[rs+1] = dst - (unsigned char *)_dst;

		const unsigned char *pattern_cache[4][2];
		const unsigned char *result_cache[4][2];

		while(src[0] || src[1]) {
			unsigned char packctl;
			int packsrc, cnt;

			// read pack control byte and copy prematch-literal-postmatch for pattern
			packctl = *src++;
			packsrc = packctl >> 6;

			int prematch = (packctl & 7) * 2;
			int postmatch = ((packctl>>3) & 7) * 2;
			int literal = (*src++ - 1) * 2;

			*dst++ = (unsigned char)(literal + prematch + postmatch);

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

			// read pack control byte and copy prematch-literal-postmatch for result

			packctl = *src++;
			packsrc = packctl >> 6;

			prematch = (packctl & 7);
			postmatch = ((packctl>>3) & 7);
			literal = (*src++ - 1);

			*dst++ = (unsigned char)(prematch + postmatch + literal);

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

long VDDisasmPack32(const ptrdiff_t *src) {
	return src[0] + (src[1]<<8) + (src[2]<<16) + (src[3]<<24);
}

void VDDisasmExpandRule(VDDisassemblyContext *pContext, char *s, const unsigned char *result, const ptrdiff_t *sp_base, const unsigned char *source) {
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
				sintptr addr = 0;

				switch(control_byte) {
				case kTarget_r1632:
					s = strtack(s, reg32[arg] + pContext->bSizeOverride);
					break;
				case kTarget_rmx:
					s = strtack(s, (pContext->bSizeOverride ? regxmm : regmmx)[arg]);
					break;
				case kTarget_lx:
					symoffset = VDDisasmPack32(sp_base + c - 1);
					s += sprintf(s, "%08lx", symoffset);
					break;
				case kTarget_hx:
					s += sprintf(s, "%02x%02x"
								, (unsigned char)sp_base[c]
								, (unsigned char)sp_base[c-1]
								);
					break;
				case kTarget_x:
					s += sprintf(s, "0%02xh" + ((unsigned char)arg < 0xa0), arg);
					break;
				case kTarget_lo:
					symoffset = VDDisasmPack32(sp_base + c - 1);
					s += sprintf(s, "%c%02lx", symoffset<0 ? '-' : '+', abs(symoffset));
					break;
				case kTarget_ho:
					{
						int x =  ((unsigned char)sp_base[c  ] << 8)
								+ (unsigned char)sp_base[c-1];

						s += sprintf(s, "%c%02lx", x<0 ? '-' : '+', abs(x));
					}
					break;
				case kTarget_o:
					s += sprintf(s, "%c%02xh", arg&0x80?'-':'+', abs((signed char)arg));
					break;
				case kTarget_la:
					addr = (sintptr)source + VDDisasmPack32(sp_base + c - 1) + pContext->physToVirtOffset;
					s += sprintf(s, VD_PTR_08lx, addr);
					break;
				case kTarget_ha:
					addr = (sintptr)source + (signed short)(sp_base[c-1] + (sp_base[c]<<8)) + pContext->physToVirtOffset;
					s += sprintf(s, VD_PTR_08lx, addr);
					break;
				case kTarget_a:
					addr = (sintptr)source + (signed char)arg + pContext->physToVirtOffset;
					s += sprintf(s, VD_PTR_08lx, addr);
					break;
				case kTarget_s:
					s = strtack(s, arg_s);
					break;
				case kTarget_r3264:
					s = strtack(s, (pContext->rex & 8 ? reg32 : reg64)[arg + ((pContext->rex & 0x04) << 1)]);
					break;
				case kTarget_r163264:
					s = strtack(s, (pContext->rex & 8 ? reg64 : pContext->bAddressOverride ? reg16 : reg32)[arg + ((pContext->rex & 0x04) << 1)]);
					break;
				case kTarget_ext:
					switch(*result++) {
					case kTarget_ext_r3264rexX:
						s = strtack(s, (pContext->bAddressOverride ? reg32 : reg64)[arg + ((pContext->rex & 0x02) << 2)]);
						break;
					case kTarget_ext_r3264rexB:
						s = strtack(s, (pContext->bAddressOverride ? reg32 : reg64)[arg + ((pContext->rex & 0x01) << 3)]);
						break;
					}
					break;
				}

				if (symoffset && pContext->pSymLookup) {
					symoffset = pContext->pSymLookup(pContext, (unsigned long)symoffset, s+2, 128);

					if (symoffset >= 0) {
						s[0] = ' ';
						s[1] = '(';
						s += 2;
						while(*s)
							++s;
						if (symoffset)
							s += sprintf(s, "+%02x", symoffset);
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
			case kTarget_p_rex:	pContext->rex = (unsigned char)sp_base[0];	break;
			}
		} else
			*s++ = c;
	}

	*s = 0;
}

char *VDDisasmApplyRuleset(VDDisassemblyContext *pContext, const unsigned char *rs, ptrdiff_t *sp, char *hp, const unsigned char *source, int bytes, const unsigned char *&source_end) {
	char *hpr;

	while(rs[0] || rs[1]) {
		const unsigned char *src_end;
		const unsigned char *result = rs + rs[0] + 1;
		const unsigned char *match_next = result + result[0] + 1;

		hpr = VDDisasmMatchRule(pContext, source, rs+1, rs[0]>>1, bytes, sp, hp, src_end);

		if (hpr) {
//			VDDEBUG("Rule match: addr=%p (end: %p), rule: %s\n", (uintptr)source + pContext->physToVirtOffset, (uintptr)source + pContext->physToVirtOffset + bytes, decode_pattern(rs+1, rs[0]).c_str());
//			VDDEBUG("Rule apply: addr=%p, result: %s\n", (uintptr)source + pContext->physToVirtOffset, decode_result(result).c_str());
			VDDisasmExpandRule(pContext, hpr, result, sp, src_end);

			source_end = src_end;
			return hpr;
		}

		rs = match_next;
	}

	return NULL;
}

char *VDDisasmMatchRule(VDDisassemblyContext *pContext, const unsigned char *source, const unsigned char *pattern, int pattern_len, int bytes, ptrdiff_t *sp, char *hp, const unsigned char *&source_end) {
	while(pattern_len) {
		if (!pattern[1] && pattern[0]) {
			if (pattern[0] & 0x80) {
				int count = pattern[0] & 0x3f;

				if (pattern[0] & 0x40) {
					--source;
					++bytes;
				}
			
				const unsigned char *src_end;

				hp = VDDisasmApplyRuleset(pContext, pContext->pRuleBase + pContext->pRuleSystemOffsets[count+1], sp, hp, source, bytes, src_end);

				if (!hp)
					return NULL;

				*sp++ = *source;
				*sp++ = (ptrdiff_t)hp;

				while(*hp++)
					;

				bytes -= (src_end - source);
				source = src_end;
			} else if (pattern[0] < 16) {
				if (pattern[0] > bytes)
					return NULL;

				for(int i=0; i<pattern[0]; ++i) {
					*sp++ = *source++;
				}

				bytes -= pattern[0];
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
			if (!bytes)
				return NULL;

			unsigned char b = *source++;

			if ((b & pattern[1]) != pattern[0])
				return NULL;

			*sp++ = b;
			--bytes;
		}
		pattern += 2;
		--pattern_len;
	}

	if (!pattern_len) {
//		VDDEBUG("success; bytes left = %d\n", bytes);
		source_end = source;
		return hp;
	}

	return NULL;
}

char *VDDisassemble(VDDisassemblyContext *pvdc, const unsigned char *source, int bytes, int& count) {
	const unsigned char *src2 = source;
	const unsigned char *src_end;
	char *s;

	pvdc->bAddressOverride = false;
	pvdc->bSizeOverride = false;
	pvdc->bRepePrefix = false;
	pvdc->bRepnePrefix = false;
	pvdc->pszSegmentOverride = NULL;
	pvdc->rex = 0;

	do {
		s = VDDisasmApplyRuleset(pvdc, pvdc->pRuleBase + pvdc->pRuleSystemOffsets[0], pvdc->stack, pvdc->heap, src2, bytes, src_end);

		bytes -= (src_end - src2);
		src2 = src_end;
	} while(!*s && bytes);

	if (!*s)
		return "???";

	count = src2 - source;

	char *dst = s, *dst_base;

	while(*dst++)
		;

	dst_base = dst;

	char *t = s;
	while(*t && *t != ' ')
		++t;

	if (*t)
		*t++ = 0;

	sprintf(dst, "%-6s %s", s, t);

	return dst_base;
}

bool VDDisasmInit(VDDisassemblyContext *pvdc, const char *pszFilename) {
	HANDLE h;

	pvdc->pRawBlock = NULL;
	pvdc->pExtraData = NULL;

	h = CreateFile(pszFilename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (h == INVALID_HANDLE_VALUE)
		return false;

	DWORD dwSize = GetFileSize(h, NULL), dwActual;

	pvdc->pRawBlock = VirtualAlloc(NULL, dwSize + 65536, MEM_COMMIT, PAGE_READWRITE);

	if (!pvdc->pRawBlock || !ReadFile(h, pvdc->pRawBlock, dwSize, &dwActual, NULL) || dwActual != dwSize) {
		CloseHandle(h);
		return false;
	}

	CloseHandle(h);

	const char *src = (const char *)pvdc->pRawBlock;

	if (src[0] != '[' || src[3] != '|')
		return false;

#ifdef _M_AMD64
	if (memcmp((char *)src + 6, "] VirtualDub disasm module (AMD64:", 34))
		return false;
#else
	if (memcmp((char *)src + 6, "] VirtualDub disasm module (IA32:", 33))
		return false;
#endif

	// Check version number

//	int write_version = (src[1]-'0')*10 + (src[2] - '0');
	int compat_version = (src[4]-'0')*10 + (src[5] - '0');

	if (compat_version > 2)
		return false;	// resource is too new for us to load


	long packSize = *(long *)((char *)pvdc->pRawBlock + 64);
	long depackSize = *(long *)((char *)pvdc->pRawBlock + 68);

	pvdc->pRuleBase = (const unsigned char *)pvdc->pRawBlock + dwSize;
	pvdc->pRuleSystemOffsets = (const unsigned *)pvdc->pRuleBase;
	pvdc->stack = (ptrdiff_t *)((char *)pvdc->pRuleBase + depackSize);
	pvdc->heap = (char *)(pvdc->stack + 32);

	pvdc->cbExtraData = dwSize - (packSize+72);
	pvdc->pExtraData = NULL;

	if (pvdc->cbExtraData)
		pvdc->pExtraData = (char *)pvdc->pRawBlock + 72 + packSize;

	VDDisasmDecompress((void *)pvdc->pRuleBase, (unsigned char *)pvdc->pRawBlock + 72, packSize);

	return true;
}

void VDDisasmDeinit(VDDisassemblyContext *pvdc) {
	if (pvdc->pRawBlock) {
		VirtualFree(pvdc->pRawBlock, 0, MEM_RELEASE);
		pvdc->pRawBlock = NULL;
	}
}

///////////////////////////////////////////////////////////////////////////////

static void SpliceProgramPath(char *buf, int bufsiz, const char *fn) {
	char tbuf[MAX_PATH];
	char *pszFile;

	GetModuleFileName(NULL, tbuf, sizeof tbuf);
	GetFullPathName(tbuf, bufsiz, buf, &pszFile);
	strcpy(pszFile, fn);
}

CodeDisassemblyWindow::CodeDisassemblyWindow(void *_code, long _length, void *_rbaseptr, void *_abaseptr)
: code(_code)
, rbase(_rbaseptr)
, abase(_abaseptr)
, length(_length)
, pFault(0)
{
//	lbents = new lbent[MAX_INSTRUCTIONS];

	char buf[MAX_PATH];

	vdc.pRuleBase = NULL;
	vdc.pSymLookup = NULL;
	vdc.physToVirtOffset = -(long)_rbaseptr;

	SpliceProgramPath(buf, sizeof buf, "ia32.vdi");
	if (!VDDisasmInit(&vdc, buf)) {
#ifdef __INTEL_COMPILER
		SpliceProgramPath(buf, sizeof buf, "VirtualDubP4.vdi");
		VDDisasmInit(&vdc, buf);
#elif defined(_M_AMD64)
		SpliceProgramPath(buf, sizeof buf, "VirtualDub64.vdi");
		VDDisasmInit(&vdc, buf);
#else
		SpliceProgramPath(buf, sizeof buf, "VirtualDub.vdi");
		if (!VDDisasmInit(&vdc, buf)) {
			SpliceProgramPath(buf, sizeof buf, "VirtualD.vdi");
			VDDisasmInit(&vdc, buf);
		}
#endif
	}

	lbents = (lbent *)VirtualAlloc(NULL, sizeof(lbent)*MAX_INSTRUCTIONS, MEM_COMMIT, PAGE_READWRITE);

	hFontMono = CreateFont(
			10,				// nHeight
			0,				// nWidth
			0,				// nEscapement
			0,				// nOrientation
			FW_DONTCARE,	// fnWeight
			FALSE,			// fdwItalic
			FALSE,			// fdwUnderline
			FALSE,			// fdwStrikeOut
			ANSI_CHARSET,	// fdwCharSet
			OUT_DEFAULT_PRECIS,		// fdwOutputPrecision
			CLIP_DEFAULT_PRECIS,	// fdwClipPrecision
			DEFAULT_QUALITY,		// fdwQuality
			DEFAULT_PITCH | FF_DONTCARE,	// fdwPitchAndFamily
			"Lucida Console"
			);

	if (!hFontMono) hFontMono = (HFONT)GetStockObject(ANSI_FIXED_FONT);

}

CodeDisassemblyWindow::~CodeDisassemblyWindow() {

	VDDisasmDeinit(&vdc);

	if (hFontMono) DeleteObject(hFontMono);

	if (lbents)
		VirtualFree(lbents, 0, MEM_RELEASE);
}

/////////////////////////////////////////////////////////////////////////


void CodeDisassemblyWindow::parse() {
	num_ents = 0;
	if (!vdc.pRuleBase)
		return;

	unsigned char *ip = (unsigned char *)code;
	unsigned char *ipend = ip + length;
	lbent *ipd = lbents;
	int	cnt=0;

	if (!ipd) {
		return;
	}

	while(ip < ipend && cnt++<MAX_INSTRUCTIONS) {
		int count;

		VDDisassemble(&vdc, ip, ipend - ip, count);
		ipd->ip = ip;
		ipd->len = count;

		ip += count;

		++ipd;
	}

	num_ents = ipd-lbents;
}

BOOL CodeDisassemblyWindow::post(HWND hWnd) {
	if (!lbents) return FALSE;

	DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_DISASM), hWnd, CodeDisassemblyWindow::DlgProc, (LPARAM)this);

	return TRUE;
}

long CodeDisassemblyWindow::getInstruction(char *buf, long val) {
	lbent *ipd;
	unsigned char *ip;

	if ((val >> 16) >= num_ents)
		return 0;

	ipd = &lbents[val >> 16];
	ip = ipd->ip;

	int count;

	uintptr virtAddr = (uintptr)ip - (uintptr)code + (uintptr)abase;
	int subIndex = val & 0xffff;
	int left = ipd->len - subIndex * 7;

	buf += sprintf(buf, subIndex ? "          " : VD_PTR_08lx": ", virtAddr);

	for(int i=0; i<7; ++i) {
		if (--left >= 0) {
			buf += wsprintf(buf, "%02x", (unsigned char)ip[subIndex*7+i]);
		} else {
			*buf++ = ' ';
			*buf++ = ' ';
		}
	}

	*buf++ = ' ';
	*buf++ = ' ';
	*buf = 0;

	if (!subIndex) {
		strcpy(buf, VDDisassemble(&vdc, ip, ipd->len, count));

		if (virtAddr == (uintptr)pFault)
			strcat(buf, "      <-- FAULT");
	}

	++subIndex;

	if (subIndex >= (ipd->len+6) / 7)
		return (val&0xffff0000) + 0x10000;
	else
		return val+1;
}

void CodeDisassemblyWindow::DoInitListbox(HWND hwndList) {
//	SendMessage(hwndList, LB_SETCOUNT, num_ents, 0);

	for(int i=0; i<num_ents; ++i)
		SendMessage(hwndList, LB_ADDSTRING, 0, (LPARAM)&lbents[i]);

	if (hFontMono)
		SendMessage(hwndList, WM_SETFONT, (WPARAM)hFontMono, MAKELPARAM(TRUE, 0));
}

BOOL CodeDisassemblyWindow::DoMeasureItem(LPARAM lParam) {
	LPMEASUREITEMSTRUCT pinfo = (LPMEASUREITEMSTRUCT)lParam;

	if (pinfo->CtlType != ODT_LISTBOX) return FALSE;

	lbent *pent = &lbents[pinfo->itemID];

	pinfo->itemHeight = 11 * ((pent->len + 6)/7);
	return TRUE;
}

BOOL CodeDisassemblyWindow::DoDrawItem(LPARAM lParam) {
	LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;
	lbent *ipd;

	if (lpdis->CtlType != ODT_LISTBOX) return FALSE;
	if (!(lpdis->itemAction & ODA_DRAWENTIRE)) return FALSE;

	// MSDN docs say that itemID can be -1 for an empty listbox or combobox that
	// needs a focus rect drawn.  Intel C/C++ caught the fact that itemID is a
	// UINT.  Hmm....
	if ((INT)lpdis->itemID < 0)
		return FALSE;

//	ipd = (lbent *)lpdis->itemData;
	ipd = &lbents[lpdis->itemID];

	if (ipd->ip) {
		SetBkMode(lpdis->hDC, TRANSPARENT);
		SetTextColor(lpdis->hDC, RGB(0x00,0x00,0x00));

		if (ipd->ip - (unsigned char *)code + (unsigned char *)abase == pFault) {
			HBRUSH hbr = CreateSolidBrush(RGB(0xff,0xa0,0x60));
			if (hbr) {
				FillRect(lpdis->hDC, &lpdis->rcItem, hbr);
				DeleteObject(hbr);
			}
		}

		int count;
		char *dst = buf;
		const unsigned char *src = (const unsigned char *)ipd->ip;
		int i, left = ipd->len;

		dst += sprintf(dst, VD_PTR_08lx": ", (size_t)(ipd->ip + vdc.physToVirtOffset));

		for(i=0; i<7; ++i)
			if (--left>=0) {
				dst += sprintf(dst, "%02x", *src++);
			} else {
				dst[0] = dst[1] = ' ';
				dst += 2;
			}

		*dst++ = ' ';
		*dst++ = ' ';

		strcpy(dst, VDDisassemble(&vdc, ipd->ip, ipd->len, count));

		int x = lpdis->rcItem.left;
		int y = lpdis->rcItem.top;

		ExtTextOut(lpdis->hDC, x, y, 0, NULL, buf, strlen(buf), NULL);

		while(left>0) {
			dst = buf+sprintf(buf, "          ");
			for(i=0; i<7; ++i) {
				if (--left>=0) {
					dst += sprintf(dst, "%02x", *src++);
				}
			}

			y += 11;
			ExtTextOut(lpdis->hDC, x, y, 0, NULL, buf, dst-buf, NULL);
		}
	}

	return TRUE;
}

INT_PTR CALLBACK CodeDisassemblyWindow::DlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	CodeDisassemblyWindow *thisPtr = (CodeDisassemblyWindow *)GetWindowLongPtr(hDlg, DWLP_USER);

	switch(msg) {

		case WM_INITDIALOG:
			SetWindowLongPtr(hDlg, DWLP_USER, lParam);
			thisPtr = (CodeDisassemblyWindow *)lParam;

			{
				HWND hwndList = GetDlgItem(hDlg, IDC_ASMBOX);

				thisPtr->DoInitListbox(hwndList);
			}
			return TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDCANCEL: case IDOK:
					EndDialog(hDlg, FALSE);
					return TRUE;
			}
			break;

		case WM_MEASUREITEM:
			return thisPtr->DoMeasureItem(lParam);

		case WM_DRAWITEM:
			return thisPtr->DoDrawItem(lParam);
	}

	return FALSE;
}
