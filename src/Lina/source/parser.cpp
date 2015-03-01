#pragma warning(disable: 4786)	// STFU

#include <string>
#include <list>
#include <map>
#include <stdarg.h>
#include "document.h"
#include "parser.h"

///////////////////////////////////////////////////////////////////////////////

struct FileContext {
	TreeLocation *tagloc;
	FILE *f;
	std::string name;
	int lineno;
};

std::string g_name;
int g_line = 1;
FILE *g_file;
TreeLocation *g_pTagLocation;
std::list<FileContext> g_fileStack;
std::list<TreeLocation> g_tagLocations;

///////////////////////////////////////////////////////////////////////////////

void error(const char *format, ...) {
	va_list val;

	printf("%s(%d): Error! ", g_name.c_str(), g_line);

	va_start(val, format);
	vprintf(format, val);
	va_end(val);
	putchar('\n');
	exit(10);
}

void unexpected(int c) {
	error("unexpected character '%c'", (char)c);
}

TreeParser::TreeParser(TreeDocument *doc)
	: mpDocument(doc)
{
}

void TreeParser::ParseFile(const char *fname) {
	int c;

	PushFile(fname);

	while((c = Next()) != EOF) {
		if (c == '<') {
			TreeNode *p = ParseTag();
			if (p && !p->mbIsText) {
				if (!mpDocument->mpRoot)
					mpDocument->mpRoot = p;
				else
					error("multiple high-level tags detected (first is <%s>, second is <%s>)", mpDocument->mpRoot->mName.c_str(), p->mName.c_str());
			}
		}
	}
}

void TreeParser::PushFile(const char *fname) {
	FILE *f = fopen(fname, "r");
	if (!f)
		error("cannot open \"%s\"", fname);

	if (g_file) {
		FileContext fc;

		fc.f = g_file;
		fc.name = g_name;
		fc.lineno = g_line;
		fc.tagloc = g_pTagLocation;
		g_fileStack.push_back(fc);
	}

	TreeLocation tl;
	tl.mName = fname;
	g_tagLocations.push_back(tl);

	g_pTagLocation = &g_tagLocations.back();
	g_file = f;
	g_name = fname;
	g_line = 1;

	printf("Processing: %s\n", fname);
}

bool TreeParser::PopFile() {
	if (g_file)
		fclose(g_file);

	if (g_fileStack.empty()) {
		g_file = NULL;
	} else {
		const FileContext& fc = g_fileStack.back();
		g_file = fc.f;
		g_name = fc.name;
		g_line = fc.lineno;
		g_pTagLocation = fc.tagloc;
		g_fileStack.pop_back();
	}
	return g_file!=0;
}

int TreeParser::Next() {
	int c;

	do {
		c = getc(g_file);
	} while(c == EOF && PopFile());

	if (c == '\n')
		++g_line;

	return c;
}

int TreeParser::NextRequired() {
	int c = Next();
	if (c == EOF)
		error("unexpected end of file");
	return c;
}

bool istagchar(int c) {
	return isalnum(c) || c=='-' || c==':';
}

bool issafevaluechar(int c) {
	return isalnum(c) || c=='-';
}

TreeNode *TreeParser::AllocNode() {
	TreeNode *t = mpDocument->AllocNode();

	t->mpLocation = g_pTagLocation;
	t->mLineno = g_line;
	return t;
}

TreeNode *TreeParser::ParseInline(int& c) {
	TreeNode& tag = *AllocNode();
	bool last_was_space = false;

	tag.mbIsText = true;
	tag.mbIsControl = false;

	do {
		tag.mName += c;
		c = NextRequired();
	} while(c != '<');

	return &tag;
}

// parse_tag
//
// Assumes that starting < has already been parsed.
TreeNode *TreeParser::ParseTag() {
	TreeNode& tag = *AllocNode();
	bool closed = false;
	int c;

	tag.mbIsControl = false;
	tag.mbIsText = false;

	c = NextRequired();
	if (isspace(c))
		do {
			c = NextRequired();
		} while(isspace(c));

	if (c=='?' || c=='!') {
		tag.mbIsText = true;
		tag.mbIsControl = true;
		tag.mName = "<";
		tag.mName += c;

		int bracket_count = 1;
		do {
			c = NextRequired();
			tag.mName += c;

			if (c == '<')
				++bracket_count;
			else if (c == '>')
				--bracket_count;
		} while(bracket_count);
		return &tag;
	} else if (c == '/') {
		tag.mName += c;
		c = NextRequired();
	}

	do {
		tag.mName += tolower(c);
		c = NextRequired();
	} while(istagchar(c));

	if (tag.mName[0] == '/')
		closed = true;

	// backwards compatibility
	std::string::size_type pos = 0;
	if (closed)
		pos = 1;

	if (!tag.mName.compare(pos, 2, "w:"))
		tag.mName.replace(pos, 2, "lina:");

	while(c != '>') {
		if (c == '/' || c=='?') {
			closed = true;
			c = NextRequired();
		} else if (istagchar(c)) {
			tag.mAttribs.push_back(TreeAttribute());
			TreeAttribute& att = tag.mAttribs.back();

			do {
				att.mName += tolower(c);
				c = NextRequired();
			} while(istagchar(c));

			while(isspace(c))
				c = NextRequired();

			att.mbNoValue = true;

			if (c == '=') {
				att.mbNoValue = false;
				do {
					c = NextRequired();
				} while(isspace(c));

				if (c == '"') {
					c = NextRequired();
					while(c != '"') {
						att.mValue += c;
						c = NextRequired();
					}
					c = NextRequired();
				} else {
					do {
						att.mValue += c;
						c = NextRequired();
					} while(istagchar(c));
				}
			}

		} else if (isspace(c)) {
			c = NextRequired();
		} else
			unexpected(c);
	}

	if (!closed) {
		c = NextRequired();
		for(;;) {
			TreeNode *p;
			if (c == '<') {
				p = ParseTag();

				if (p && !p->mName.empty() && p->mName[0] == '/') {
					if ((std::string("/") + tag.mName) != p->mName)
						error("closing tag <%s> doesn't match opening tag <%s> on line %d", p->mName.c_str(), tag.mName.c_str(), tag.mLineno);
					break;
				}
				c = NextRequired();
			} else {
				p = ParseInline(c);
			}

			if (p)
				tag.mChildren.push_back(p);
		}
	}

	// Check for a macro or include and whisk it away if so.

	if (tag.mName == "lina:macro") {
		const TreeAttribute *a = tag.Attrib("name");

		if (!a)
			error("macro definition must have NAME attribute");

		mpDocument->mMacros[a->mValue] = &tag;

		return NULL;
	} else if (tag.mName == "lina:include") {
		const TreeAttribute *a = tag.Attrib("file");

		if (!a)
			error("<lina:include> must specify FILE");

		PushFile(a->mValue.c_str());

		return NULL;
	}

	return &tag;
}
