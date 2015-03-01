#ifndef f_LINA_PARSER_H
#define f_LINA_PARSER_H

void error(const char *format, ...);

class TreeParser {
public:
	TreeParser(TreeDocument *doc);

	void ParseFile(const char *fname);

protected:
	void PushFile(const char *fname);
	bool PopFile();
	int Next();
	int NextRequired();
	TreeNode *AllocNode();
	TreeNode *ParseInline(int& c);
	TreeNode *ParseTag();

protected:
	TreeDocument *mpDocument;
};

#endif
