#include <stdio.h>

#include "ruleset.h"
#include "utils.h"

void parse_ia(tRuleSystem& rsys, FILE *f) {
	char linebuf[4096];
	ruleset		*pRuleset = NULL;

	while(fgets(linebuf, sizeof linebuf, f)) {
		strtrim(linebuf);

		if (!linebuf[0] || linebuf[0] == '#')
			continue;

		puts(linebuf);

		if (linebuf[0] == '%') {			// ruleset definition
			strtrim(linebuf+1);

			ruleset r;
			r.name = linebuf+1;
			rsys.push_back(r);
			pRuleset = &rsys.back();
		} else {							// rule definition

			if (!pRuleset)
				oops("Not in ruleset:\n>%s\n", linebuf);

			rule r;

			r.rule_line = linebuf;
			r.argcount = 0;

			// Find colon

			char *colon = linebuf;

			while(*colon != ':') {
				if (!*colon)
					oops("Colon missing in rule:\n>%s\n", linebuf);

				++colon;
			}

			// Nuke colon

			*colon++ = 0;

			// Parse tokens until colon is found

			static const char whitespace[]=" \t\n\v";
			const char *token = strtok(linebuf, whitespace);

			std::vector<bool> argumentTypeStack;		// true if arg is a string

			if (token) do {
				if (*token == '*') {						// any character
					if (!r.match_stream.empty() && !r.match_stream.rbegin()->second && r.match_stream.rbegin()->first < 15)
						++r.match_stream.rbegin()->first;
					else {
						r.match_stream.push_back(std::pair<uint8, uint8>(1,0));
					}

					argumentTypeStack.push_back(false);
					++r.argcount;
				} else if (*token == '[') {
					if (!strcmp(token+1, "66]"))
						r.match_stream.push_back(std::pair<uint8, uint8>(16,0));
					else if (!strcmp(token+1, "67]"))
						r.match_stream.push_back(std::pair<uint8, uint8>(17,0));
					else if (!strcmp(token+1, "F2]"))
						r.match_stream.push_back(std::pair<uint8, uint8>(18,0));
					else if (!strcmp(token+1, "F3]"))
						r.match_stream.push_back(std::pair<uint8, uint8>(19,0));
					else if (!strcmp(token+1, "!s]"))
						r.match_stream.push_back(std::pair<uint8, uint8>(20,0));
					else if (!strcmp(token+1, "q]"))
						r.match_stream.push_back(std::pair<uint8, uint8>(21,0));
					else
						oops("unknown prefix match token '%s'\n", token);
				} else if (isxdigit((unsigned char)token[0]) && isxdigit((unsigned char)token[1])
						&& (token[2] == '-' || !token[2])) {		// match character
					int byteval, byteend;
					int c;

					c = sscanf(token, "%x-%x", &byteval, &byteend);

					if (byteval < 0 || byteval >= 256)
						oops("uint8 start value out of range\n");

					if (c<2) {
						byteend = byteval;
					} else if (byteend != byteval) {
						if (byteend < 0 || byteend >= 256)
							oops("uint8 end value out of range\n");
					}

					r.match_stream.push_back(std::pair<uint8, uint8>(byteval, ~(byteval ^ byteend)));
					argumentTypeStack.push_back(false);
					++r.argcount;

				} else {									// macro invocation
					tRuleSystem::iterator it = rsys.begin();
					tRuleSystem::iterator itEnd = rsys.end();
					int index = 128;

					if (*token == '!') {	// reuse last uint8 char
						index = 192;
						++token;
					}

					for(; it!=itEnd; ++it, ++index) {
						if (!_stricmp((*it).name.c_str(), token))
							break;
					}

					if (it == itEnd)
						oops("unknown ruleset '%s'\n", token);

					r.match_stream.push_back(std::pair<uint8, uint8>(index, 0));
					r.argcount += 2;
					argumentTypeStack.push_back(false);
					argumentTypeStack.push_back(true);
				}
			} while(token = strtok(NULL, whitespace));

			// match sequence parsed -- parse the result string.

			char *s = colon;

			for(;;) {
				while(*s && strchr(whitespace, *s))
					++s;

				if (!*s || *s == '#')
					break;

				if (*s == '"') {	// string literal
					const char *start = ++s;

					while(*s != '"') {
						if (!*s)
							oops("unterminated string constant\n");

						++s;
					}
					
					r.result.append(start, s-start);
					++s;
				} else if (*s == '$') {	// macro expansion
					++s;

					if (!_strnicmp(s, "p_cs", 4)) {
						r.result += kTarget_p_cs;
						s += 4;
					} else if (!_strnicmp(s, "p_ss", 4)) {
						r.result += kTarget_p_ss;
						s += 4;
					} else if (!_strnicmp(s, "p_ds", 4)) {
						r.result += kTarget_p_ds;
						s += 4;
					} else if (!_strnicmp(s, "p_es", 4)) {
						r.result += kTarget_p_es;
						s += 4;
					} else if (!_strnicmp(s, "p_fs", 4)) {
						r.result += kTarget_p_fs;
						s += 4;
					} else if (!_strnicmp(s, "p_gs", 4)) {
						r.result += kTarget_p_gs;
						s += 4;
					} else if (!_strnicmp(s, "p_66", 4)) {
						r.result += kTarget_p_66;
						r.is_66 = true;
						s += 4;
					} else if (!_strnicmp(s, "p_67", 4)) {
						r.result += kTarget_p_67;
						r.is_67 = true;
						s += 4;
					} else if (!_strnicmp(s, "p_F2", 4)) {
						r.result += kTarget_p_F2;
						r.is_f2 = true;
						s += 4;
					} else if (!_strnicmp(s, "p_F3", 4)) {
						r.result += kTarget_p_F3;
						r.is_f3 = true;
						s += 4;
					} else if (!_strnicmp(s, "ap", 2)) {
						r.result += kTarget_ap;
						s += 2;
					} else if (!_strnicmp(s, "p_rex", 5)) {
						r.result += kTarget_p_rex;
						s += 5;
					} else if (!_strnicmp(s, "return", 6)) {
						s += 6;
						r.is_return = true;
					} else if (!_strnicmp(s, "call", 4)) {
						s += 4;
						r.is_call = true;
					} else if (!_strnicmp(s, "jmp", 3)) {
						s += 3;
						r.is_jump = true;
					} else if (!_strnicmp(s, "jcc", 3)) {
						s += 3;
						r.is_jcc = true;
					} else if (!_strnicmp(s, "imm8", 4)) {
						s += 4;
						r.is_imm8 = true;
					} else if (!_strnicmp(s, "imm16", 5)) {
						s += 5;
						r.is_imm16 = true;
					} else if (!_strnicmp(s, "imm32", 5)) {
						s += 5;
						r.is_imm32 = true;
					} else if (!_strnicmp(s, "invalid", 7)) {
						s += 7;
						r.is_invalid = true;
					} else {
						unsigned long id = strtoul(s, &s, 10);

						if (!id || (int)id > r.argcount)
							oops("macro argument $%lu out of range\n", id);

						if (!r.result.empty() && *r.result.rbegin() == ' ')
							*r.result.rbegin() = (char)(id + 0x80);
						else
							r.result += (char)id;

						int firstbit = 0;
						int lastbit = 7;

						if (*s == '[') {
							++s;

							firstbit = strtol(s, &s, 10);

							if (*s++ != '-')
								oops("macro argument bitfield range missing '-'\n");

							lastbit = strtol(s, &s, 10);

							if (firstbit < 0 || lastbit > 7 || firstbit > lastbit)
								oops("invalid bitfield %d-%d\n", firstbit, lastbit);

							if (*s++ != ']')
								oops("invalid bitfield\n");
						}

						if (!*s)
							oops("macro expansion missing format\n");

						char *t = s;

						while(*t && !isspace((unsigned char)*t))
							++t;

						*t = 0;

						char control_byte;
						char ext_byte = 0;

						if (!_stricmp(s, "r32")) {
							control_byte = kTarget_r32;
						} else if (!_stricmp(s, "r16")) {
							control_byte = kTarget_r16;
						} else if (!_stricmp(s, "r1632")) {
							control_byte = kTarget_r1632;
						} else if (!_stricmp(s, "r8")) {
							control_byte = kTarget_r8;
						} else if (!_stricmp(s, "rm")) {
							control_byte = kTarget_rm;
						} else if (!_stricmp(s, "rx")) {
							control_byte = kTarget_rx;
						} else if (!_stricmp(s, "rmx")) {
							control_byte = kTarget_rmx;
						} else if (!_stricmp(s, "rc")) {
							control_byte = kTarget_rc;
						} else if (!_stricmp(s, "rd")) {
							control_byte = kTarget_rd;
						} else if (!_stricmp(s, "rs")) {
							control_byte = kTarget_rs;
						} else if (!_stricmp(s, "rf")) {
							control_byte = kTarget_rf;
						} else if (!_stricmp(s, "x")) {
							control_byte = kTarget_x;
						} else if (!_stricmp(s, "hx")) {
							control_byte = kTarget_hx;
						} else if (!_stricmp(s, "lx")) {
							control_byte = kTarget_lx;
						} else if (!_stricmp(s, "o")) {
							control_byte = kTarget_o;
						} else if (!_stricmp(s, "ho")) {
							control_byte = kTarget_ho;
						} else if (!_stricmp(s, "lo")) {
							control_byte = kTarget_lo;
						} else if (!_stricmp(s, "a")) {
							control_byte = kTarget_a;
						} else if (!_stricmp(s, "ha")) {
							control_byte = kTarget_ha;
						} else if (!_stricmp(s, "la")) {
							control_byte = kTarget_la;
						} else if (!_stricmp(s, "s")) {
							control_byte = kTarget_s;
						} else if (!_stricmp(s, "r3264")) {
							control_byte = kTarget_r3264;
						} else if (!_stricmp(s, "r163264")) {
							control_byte = kTarget_r163264;
						} else if (!_stricmp(s, "r3264rexX")) {
							control_byte = kTarget_ext;
							ext_byte = kTarget_ext_r3264rexX;
						} else if (!_stricmp(s, "r3264rexB")) {
							control_byte = kTarget_ext;
							ext_byte = kTarget_ext_r3264rexB;
						} else if (!_stricmp(s, "r163264rexB")) {
							control_byte = kTarget_ext;
							ext_byte = kTarget_ext_r163264rexB;
						} else {
							oops("unknown macro expansion mode: '%s'\n", s);
						}

						if (argumentTypeStack[id-1] != (control_byte == kTarget_s))
							oops("bad argument type: $%d (not a %s)\n", id, argumentTypeStack[id-1] ? "uint8" : "string");

						if (firstbit == 0 && lastbit == 2) {
							r.result += (char)(control_byte + 0x20);
						} else if (firstbit == 3 && lastbit == 5) {
							r.result += (char)(control_byte + 0x40);
						} else if (firstbit != 0 || lastbit != 7) {
							r.result += (char)(control_byte + 0xe0);
							r.result += (char)((lastbit+1-firstbit)*16 + firstbit);
						} else {
							r.result += (char)control_byte;
						}

						if (ext_byte)
							r.result += (char)ext_byte;

						s = t+1;
					}
				} else
					oops("indecipherable result string\n");
			}

			pRuleset->rules.push_back(r);
		}
	}
}
