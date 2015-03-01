#include <stdio.h>
#include <list>
#include <vector>

#include "lexer.h"
#include "utils.h"

FILE *g_file;
bool g_bUnicode;
std::string g_filename;
int g_lineno;
int g_commentlineno;
bool g_bInComment;
std::vector<wint_t> g_backstack;
int g_pushedToken;
int g_intVal;
std::wstring g_identifier;

struct IncludeEntry {
	FILE *f;
	std::string filename;
	int lineno;
	bool bUnicode;
};

std::list<IncludeEntry> g_includeStack;

static const struct {
	const wchar_t *name;
	const char *ansi_name;
	int token;
} g_keywords[]={
	{ L"include",			"include",				kTokenInclude },
	{ L"enum",				"enum",					kTokenEnum },
	{ L"let",				"let",					kTokenLet },
	{ L"declare",			"declare",				kTokenDeclare },
	{ L"using",				"using",				kTokenUsing },
	{ L"now",				"now",					kTokenNow },

	{ L"label",				"label",				kTokenLabel },
	{ L"edit",				"edit",					kTokenEdit },
	{ L"editInt",			"editInt",				kTokenEditInt },
	{ L"button",			"button",				kTokenButton },
	{ L"checkBox",			"checkBox",				kTokenCheckBox },
	{ L"listBox",			"listBox",				kTokenListBox },
	{ L"comboBox",			"comboBox",				kTokenComboBox },
	{ L"listView",			"listView",				kTokenListView },
	{ L"trackBar",			"trackBar",				kTokenTrackbar },
	{ L"fileControl",		"fileControl",			kTokenFileControl },
	{ L"set",				"set",					kTokenSet },
	{ L"pageset",			"pageset",				kTokenPageSet },
	{ L"grid",				"grid",					kTokenGrid },
	{ L"option",			"option",				kTokenOption },
	{ L"group",				"group",				kTokenGroup },
	{ L"splitter",			"splitter",				kTokenSplitter },
	{ L"textedit",			"textedit",				kTokenTextEdit },
	{ L"textarea",			"textarea",				kTokenTextArea },
	{ L"hotkey",			"hotkey",				kTokenHotkey },
	{ L"customWindow",		"customWindow",			kTokenCustomWindow },

	{ L"listitem",			"listitem",				kTokenListItem },
	{ L"page",				"page",					kTokenPage },
	{ L"row",				"row",					kTokenRow },
	{ L"column",			"column",				kTokenColumn },
	{ L"nextrow",			"nextrow",				kTokenNextRow },

	{ L"stringSet",			"stringSet",			kTokenStringSet },
	{ L"message",			"message",				kTokenMessage },
	{ L"override",			"override",				kTokenOverride },
	{ L"dialog",			"dialog",				kTokenDialog },
	{ L"template",			"template",				kTokenTemplate },

	{ L"marginl",			"marginl",				kTokenMarginL },
	{ L"margint",			"margint",				kTokenMarginT },
	{ L"marginr",			"marginr",				kTokenMarginR },
	{ L"marginb",			"marginb",				kTokenMarginB },
	{ L"padl",				"padl",					kTokenPadL },
	{ L"padt",				"padt",					kTokenPadT },
	{ L"padr",				"padr",					kTokenPadR },
	{ L"padb",				"padb",					kTokenPadB },
	{ L"minw",				"minw",					kTokenMinW },
	{ L"minh",				"minh",					kTokenMinH },
	{ L"maxw",				"maxw",					kTokenMaxW },
	{ L"maxh",				"maxh",					kTokenMaxH },
	{ L"align",				"align",				kTokenAlign },
	{ L"valign",			"valign",				kTokenVAlign },
	{ L"spacing",			"spacing",				kTokenSpacing },
	{ L"aspect",			"aspect",				kTokenAspect },
	{ L"affinity",			"affinity",				kTokenAffinity },
	{ L"rowspan",			"rowspan",				kTokenRowSpan },
	{ L"colspan",			"colspan",				kTokenColSpan },

	{ L"vertical",			"vertical",				kTokenVertical },
	{ L"raised",			"raised",				kTokenRaised },
	{ L"sunken",			"sunken",				kTokenSunken },
	{ L"child",				"child",				kTokenChild },
	{ L"multiline",			"multiline",			kTokenMultiline },
	{ L"readonly",			"readonly",				kTokenReadonly },
	{ L"checkable",			"checkable",			kTokenCheckable },
	{ L"noheader",			"noheader",				kTokenNoHeader },
	{ L"default",			"default",				kTokenDefault },

	{ L"enable",			"enable",				kTokenEnable },
	{ L"value",				"value",				kTokenValue },

	{ L"left",				"left",					kTokenLeft },
	{ L"center",			"center",				kTokenCenter },
	{ L"right",				"right",				kTokenRight },
	{ L"top",				"top",					kTokenTop },
	{ L"bottom",			"bottom",				kTokenBottom },
	{ L"fill",				"fill",					kTokenFill },
	{ L"expand",			"expand",				kTokenExpand },
	{ L"link",				"link",					kTokenLink },
	{ L"addColumn",			"addColumn",			kTokenAddColumn },
};

enum { kKeywordCount = sizeof g_keywords / sizeof g_keywords[0] };


///////////////////////////////////////////////////////////////////////////
//
//	lexical analyzer
//
///////////////////////////////////////////////////////////////////////////

const std::wstring& lexident() {
	return g_identifier;
}

int lexint() {
	return g_intVal;
}

const char *lexfilename() {
	return g_filename.c_str();
}

int lexlineno() {
	return g_lineno;
}

bool lexisunicode() {
	return g_bUnicode;
}

void lextestunicode() {
	int c = getc(g_file);

	g_bUnicode = false;

	if (c == 0xFF) {
		int d = getc(g_file);

		if (d == 0xFE) {
			g_bUnicode = true;
		} else {
			lexungetc(d);
			lexungetc(c);
		}
	} else
		lexungetc(c);
}

void lexopen(const char *fn) {
	g_filename = fn;
	g_file = fopen(fn, "rb");
	if (!g_file)
		fatal("cannot open input file %s", g_filename.c_str());
	g_lineno = 1;
	lextestunicode();
}

void lexinclude(const std::string& filename) {
	FILE *f = fopen(filename.c_str(), "rb");

	if (!f)
		fatal("Cannot open include file \"%s\"", filename.c_str());

	g_includeStack.push_front(IncludeEntry());
	g_includeStack.front().f = g_file;
	g_includeStack.front().filename = g_filename;
	g_includeStack.front().lineno = g_lineno;
	g_includeStack.front().bUnicode = g_bUnicode;

	g_file = f;
	g_filename = filename;
	g_lineno = 1;

	lextestunicode();

	printf("Ami: Including file \"%s\" (%s)\n", g_filename.c_str(), g_bUnicode ? "Unicode" : "ANSI");
}

wint_t lexrawgetc() {
	wint_t c;
	
	if (g_backstack.empty()) {
		for(;;) {
			if (g_bUnicode) {
				c = getwc(g_file);
			} else
				c = getc(g_file);

			if (c != WEOF || g_includeStack.empty())
				break;

			fclose(g_file);
			g_file = g_includeStack.front().f;
			g_filename = g_includeStack.front().filename;
			g_lineno = g_includeStack.front().lineno;
			g_bUnicode = g_includeStack.front().bUnicode;
			g_includeStack.pop_front();
		}
	} else {
		c = g_backstack.back();
		g_backstack.pop_back();
	}
	return c;
}

// We can't just use unget[w]c() here, because the Unicode escape mechanism
// may cause more than one character to be pushed back.

void lexungetc(wint_t c) {
	if (c < 0)
		return;

	g_backstack.push_back(c);
}

wint_t lexgetc() {
	wint_t c = lexrawgetc();

	// check if it is a Unicode escape

	if (c == '\\') {
		c = lexrawgetc();

		if (c == 'u') {
			// Unicode escape!

			wint_t c1 = lexrawgetc();
			wint_t c2 = lexrawgetc();
			wint_t c3 = lexrawgetc();
			wint_t c4 = lexrawgetc();

			if (c1 == WEOF || c2 == WEOF || c3 == WEOF || c4 == WEOF)
				fatal("EOF encountered in Unicode escape");

			if (!iswxdigit(c1) || !iswxdigit(c2) || !iswxdigit(c3) || !iswxdigit(c4))
				fatal("Non-hex digit encountered in Unicode escape");

			c1 -= '0';
			if (c1 > 10)
				c1 -= 6;
			c2 -= '0';
			if (c2 > 10)
				c2 -= 6;
			c3 -= '0';
			if (c3 > 10)
				c3 -= 6;
			c4 -= '0';
			if (c4 > 10)
				c4 -= 6;

			return (c1<<12) + (c2<<8) + (c3<<4) + c4;
		} else
			lexungetc(c);

		return '\\';
	}

	return c;
}

wint_t lexgetescape() {
	wint_t c = lexgetc();

	if (c < 0)
		fatal("Newline found in escape sequence");

	switch(c) {
	case '0':	return 0;
	case 'a':	return '\a';
	case 'b':	return '\b';
	case 'f':	return '\f';
	case 'n':	return '\n';
	case 'r':	return '\r';
	case 't':	return '\t';
	case 'v':	return '\v';
	case 'x':
		{
			wint_t c1 = lexgetc();
			wint_t c2 = lexgetc();

			if (c1 == WEOF || c2 == WEOF)
				fatal("Newline found in escape sequence");
				
			if (!iswxdigit(c1) || !iswxdigit(c2))
				fatal("Invalid \\x escape sequence");

			c1 -= '0';
			if (c1 > 10)
				c1 -= 6;

			c2 -= '0';
			if (c2 > 10)
				c2 -= 6;

			return (c1<<4) + c2;
		}

	case '"':	return '"';
	case '\'':	return '\'';
	default:
		if (c<0x100 && isprint(c))
			fatal("Invalid escape sequence \\%c", c);
		else
			fatal("Invalid escape sequence \\\\u%04x", c);
	}

	return 0;	// this won't actually ever get executed...
}

void lexpush(int token) {
	if (g_pushedToken)
		fatal_internal(__FILE__, __LINE__);

	g_pushedToken = token;
}

int lex() {
	wint_t c;

	if (g_pushedToken) {
		int t = g_pushedToken;

		g_pushedToken = 0;
		return t;
	}

	for(;;) {
		c = lexgetc();

		if (c == WEOF) {
			if (ferror(g_file))
				fatal("Read error");

			if (g_bInComment)
				fatal("EOF found while parsing comment starting at line %d", g_commentlineno);

			return kTokenEOF;
		}

		// process EOLs

		if (c == '\r') {
			c = lexgetc();

			if (c != '\n')
				lexungetc(c);

			++g_lineno;
			continue;
		} else if (c == '\n') {
			++g_lineno;
			continue;
		}

		// discard whitespace and ^Z

		if (iswspace(c) || c == 26)
			continue;

		// are we in a comment?

		if (g_bInComment) {
			if (c == '/') {
				c = lexgetc();

				if (c == '*')
					fatal("C-style comment already started at line %d", g_commentlineno);
				else if (c == '/') {
					do {
						c = lexgetc();
					} while(c != '\r' && c != '\n');
				}

				lexungetc(c);
			} else if (c == '*') {
				c = lexgetc();

				if (c == '/')
					g_bInComment = false;
				else
					lexungetc(c);
			}
		} else {
			switch(c) {

			// single character, non-overloaded tokens
			case '~':
			case ',':
			case ':':
			case ';':
			case '{':
			case '}':
			case '(':
			case ')':
			case '[':
			case ']':
			case '@':
				return c;

			case '+':
				c = lexgetc();
				if (c == '+')
					return kTokenPlusPlus;
				lexungetc(c);
				return '+';

			case '-':
				c = lexgetc();
				if (c == '-')
					return kTokenMinusMinus;
				lexungetc(c);
				return '-';

			// these must be overloaded for comments
			case '*':
				c = lexgetc();
				if (c == '/')
					fatal("'*/' found outside of C-style comment");

				lexungetc(c);
				return '*';

			case '/':
				c = lexgetc();
				if (c == '*') {
					g_commentlineno = g_lineno;
					g_bInComment = true;
					continue;
				} else if (c == '/') {
					do {
						c = lexgetc();
					} while(c != '\r' && c != '\n');

					lexungetc(c);
					continue;
				}
				lexungetc(c);
				return '/';

			case '=':
				c = lexgetc();
				if (c == '=')
					return kTokenEQ;
				lexungetc(c);
				return '=';

			case '!':
				c = lexgetc();
				if (c == '=')
					return kTokenNE;
				lexungetc(c);
				return '!';

			case '<':
				c = lexgetc();
				if (c == '=')
					return kTokenLE;
				lexungetc(c);
				return '<';

			case '>':
				c = lexgetc();
				if (c == '=')
					return kTokenGE;
				lexungetc(c);
				return '>';

			case '&':
				c = lexgetc();
				if (c == '&')
					return kTokenLogicalAnd;
				lexungetc(c);
				break;

			case '|':
				c = lexgetc();
				if (c == '|')
					return kTokenLogicalOr;
				lexungetc(c);
				break;

			case '\'':
				g_intVal = 0;
				c = lexgetc();
				if (c == '\'')
					fatal("Empty character literal constant");

				{
					int count = 0;
					do {
						if (++count > 4)
							fatal("Character constant too large");

						if (c == '\\') {
							c = lexgetescape();
						}
						if (c == '\r' || c == '\n')
							fatal("Newline found in character constant");

						g_intVal = (g_intVal << 8) + c;
						c = lexgetc();
					} while(c != '\'');
				}
				return kTokenInteger;

			case '"':
				g_identifier.resize(0);
				for(;;) {
					c = lexgetc();
					if (c == '\\') {
						c = lexgetescape();
						g_identifier += (wchar_t) c;
						continue;
					}

					if (c == '\r' || c=='\n')
						fatal("Newline found in string constant");

					if (c == '"')
						break;

					g_identifier += (wchar_t)c;
				}

				return kTokenString;

			default:
				if (iswdigit(c)) {
					// check for a leading zero -- we don't care about octal, but
					// we do care about hex....

					int v = 0;

					if (c == '0') {
						c = lexgetc();

						if (c == 'x') {
							// hex constant!

							for(;;) {
								c = lexgetc();
								if (!iswxdigit(c))
									break;
								c = toupper(c) - 0x30;

								if (c>10)
									c -= 6;

								v = (v<<4) + c;
							}

							// absorb (and ignore) C/C++ number suffixes

							if (c == 'u' || c=='U')
								c = lexgetc();

							if (c == 'l' || c=='L')
								c = lexgetc();

							lexungetc(c);

							g_intVal = v;
							return kTokenInteger;
						} else {
							lexungetc(c);
							c = '0';
						}
					}

					// decimal constant

					do {
						v = v*10 + (c - '0');

						c = lexgetc();
					} while(iswdigit(c));

					// absorb (and ignore) C/C++ number suffixes

					if (c == 'u' || c=='U')
						c = lexgetc();

					if (c == 'l' || c=='L')
						c = lexgetc();

					lexungetc(c);

					g_intVal = v;
					return kTokenInteger;
				}

				// Not a number.  Constant?

				if (((unsigned)c < 0x100 && isalpha(c)) || c=='_') {
					g_identifier.resize(0);

					do {
						g_identifier += (wchar_t)c;
						c = lexgetc();
					} while(c=='_' || ((unsigned)c<0x100 && iswalnum(c)));

					lexungetc(c);

					// Check keywords.

					for(int i=0; i<kKeywordCount; ++i)
						if (!_wcsicmp(g_identifier.c_str(), g_keywords[i].name))
							return g_keywords[i].token;

					return kTokenIdentifier;
				}
			}

			// Whoops.

			if (c < 0x100 && isprint(c))
				fatal("Unrecognized character '%c'", c);
			else
				fatal("Unrecognized character \\u%04x", c);
		}
	}
}

std::string lextokenname(int token, bool expand) {
	if (token < 0)
		return "end of file";

	switch(token) {
	case kTokenInteger:		return "integer";
	case kTokenString:		return "string literal";
	case kTokenIdentifier:	return expand ? std::string("identifier '") + ANSIify(g_identifier) + "'" : "identifier";
	case kTokenPlusPlus:	return "'++'";
	case kTokenMinusMinus:	return "'--'";
	case kTokenEQ:			return "'=='";
	case kTokenNE:			return "'!='";
	case kTokenLE:			return "'<='";
	case kTokenGE:			return "'>='";
	case kTokenLogicalAnd:	return "'&&'";
	case kTokenLogicalOr:	return "'||'";
	default:
		{
			for(int i=0; i<kKeywordCount; ++i)
				if (g_keywords[i].token == token)
					return std::string("keyword '") + g_keywords[i].ansi_name + '\'';
		}
		if (token >= 255)
			fatal_internal(__FILE__, __LINE__);

		return std::string("'") + (char)token + '\'';
	}
}

