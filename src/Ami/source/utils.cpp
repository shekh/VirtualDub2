#include <stdarg.h>

#include "utils.h"
#include "lexer.h"

///////////////////////////////////////////////////////////////////////////
//
//	utils
//
///////////////////////////////////////////////////////////////////////////

std::string ANSIify(const std::wstring& unicode) {
	std::string ansi;

	// ugh

	std::wstring::const_iterator it = unicode.begin(), itEnd = unicode.end();

	for(; it!=itEnd; ++it) {
		char buf[8];

		int bytes = wctomb(buf, *it);

		if (bytes<0)
			ansi += '?';
		else
			ansi.append(buf, bytes);
	}

	return ansi;
}

///////////////////////////////////////////////////////////////////////////
//
//	error handling
//
///////////////////////////////////////////////////////////////////////////

void warning(const char *format, ...) {
	va_list val;

	printf("%s(%d) : warning: ", lexfilename(), lexlineno());
	va_start(val, format);
	vprintf(format, val);
	va_end(val);
	putchar('\n');
}

void fatal(const char *format, ...) {
	va_list val;

	printf("%s(%d): error: ", lexfilename(), lexlineno());
	va_start(val, format);
	vprintf(format, val);
	va_end(val);
	putchar('\n');

	exit(10);
}

// :)

void fatal_internal(const char *fname, const int line) {
	fatal("INTERNAL COMPILER ERROR\n"
		"        (compiler file '%s', line %d)\n", fname, line);
}


namespace nsSCSU {
	enum {
		kSQx		= 0x01,		// 01-08 ch		Quote from window n

		kSDX		= 0x0B,		// 0B hb lb		Define extended

		kSQU		= 0x0E,		// 0E hb lb		Quote Unicode
		kSCU		= 0x0F,		//				Switch to Unicode mode

		kSCx		= 0x10,		// 10-17		Change to window n
		kSDx		= 0x18,		// 18-1F b		Define window n as OffsetTable[b]

		kUCx		= 0xE0,		// E0-E7		Change to window n
		kUDx		= 0xE8,		// E8-EF b		Define window n as OffsetTable[b]
		kUQU		= 0xF0,		// F0			Quote
		kUDX		= 0xF1,		// F1			Define extended

	};
}

// My SCSU encoder sucks, but oh well.  We deliberately use unsigned here
// instead of wchar_t to head off possible wraparound issues when choosing
// a window.

std::basic_string<unsigned char> ConvertToSCSU(const std::wstring& s) {
	using namespace nsSCSU;

	std::wstring::const_iterator it = s.begin(), itEnd = s.end();
	const unsigned static_windows[8]={0x0000,0x0080,0x0100,0x0300,0x2000,0x2080,0x2100,0x3000};
	unsigned dynamic_windows[8]={0x0080,0x00c0,0x0400,0x0600,0x0900,0x3040,0x30A0,0xFF00};
	int current_dynwnd = 0;
	int next_redefine = 0;
	std::basic_string<unsigned char> out;

	while(it!=itEnd) {
		unsigned ch = *it;
		unsigned lookahead = 0xFFFF0000;	// something not remotely close to anything else

		++it;

		if (it != itEnd)
			lookahead = *it;

		if (ch >= 0x10000)
			fatal("Unicode surrogate characters are not supported");

		// Determine if the character can be encoded directly in static window 0.
		// We can pass 00, 09, 0A, 0D, and 20-7F.

		if (ch < 0x100 && (ch >= 0x20 || (0x00002601UL & (1<<ch)))) {
			out += (unsigned char)ch;
		}

		// Try the current dynamic window.

		else if (ch >= dynamic_windows[current_dynwnd] && ch < dynamic_windows[current_dynwnd] + 0x80) {
			out += (unsigned char)(0x80 + (ch - dynamic_windows[current_dynwnd]));
		}

		// Check the static windows.

		else {
			int i;

			for(i=0; i<8; ++i) {
				unsigned offset = (ch - static_windows[i]);

				if (offset < 0x80) {		// unsigned comparison trick for 0 <= offset <= 0x80
					// If the next character is also from the same window, don't
					// use a static escape.  Instead, force a dynamic change.
					//
					// BTW, we can't do this for 00-7F since the window can't be placed that low....

					if (i && !((lookahead ^ ch) & ~0x7f))
						goto force_dynamic;		// yeah, yeah

					// Write a SQx tag followed by the offset.

					out += (unsigned char)(kSQx + i);
					out += (unsigned char)offset;
					break;
				}
			}

			if (i >= 8) {
				// Now check if one of the dynamic windows has the character we want.

				for(i=0; i<8; ++i) {
					unsigned offset = (ch - dynamic_windows[i]);

					if (offset < 0x80) {		// unsigned comparison trick for 0 <= offset <= 0x80

						// If the next character is from the same window, do a dynamic
						// window switch, else do a quote.

						if (lookahead >= dynamic_windows[i] && lookahead < dynamic_windows[i]+0x80) {
							out += (unsigned char)(kSCx + i);
							current_dynwnd = i;
						} else
							out += (unsigned char)(kSQx + i);

						out += (unsigned char)(offset + 0x80);
						break;
					}
				}

				if (i >= 8) {
force_dynamic:
					// Okay, no window contains what we want.  Our solution: choose a dynamic
					// window in round-robin order, redefine it, and switch to it.  If we can't
					// shift a window into position, just quote it.  The characters we can't
					// window are U+3400 through U+BFFF.

					unsigned new_window;
					unsigned char id;

					if (ch >= 0x3400 && ch < 0xBFFF) {
						out += (unsigned char)(kSQU);
						out += (unsigned char)(ch>>8);
						out += (unsigned char)(ch&0xff);
						continue;
					} else if (ch >= 0x00c0 && ch < 0x0140) {
						new_window = 0x00c0;
						id = (unsigned char)0xF9;
					} else if (ch >= 0x0250 && ch < 0x02d0) {
						new_window = 0x0250;
						id = (unsigned char)0xFA;
					} else if (ch >= 0x0370 && ch < 0x03f0) {
						new_window = 0x0370;
						id = (unsigned char)0xFB;
					} else if (ch >= 0x0530 && ch < 0x05b0) {
						new_window = 0x0530;
						id = (unsigned char)0xFC;
					} else if (ch >= 0x3040 && ch < 0x30c0) {
						new_window = 0x3040;
						id = (unsigned char)0xFD;
					} else if (ch >= 0x30a0 && ch < 0x3120) {
						new_window = 0x30a0;
						id = (unsigned char)0xFE;
					} else if (ch >= 0xff60 && ch < 0xffe0) {
						new_window = 0xff60;
						id = (unsigned char)0xFF;
					} else if (ch <  0x3400) {
						new_window = ch & 0xff80;
						id = (unsigned char)(ch >> 7);
					} else {
						new_window = 0xff80;
						id = (unsigned char)((ch >> 7) - 0x158);
					}

					out += (unsigned char)(kSDx + next_redefine);
					out += id;
					out += (unsigned char)(0x80 + (ch - new_window));

					dynamic_windows[next_redefine] = new_window;

					current_dynwnd = next_redefine;
					next_redefine = (next_redefine+1) & 7;
				}
			}
		}
	}

	return out;
}
