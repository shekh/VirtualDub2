#pragma warning(disable: 4786)		// shut up

#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <string>
#include <vector>
#include <map>
#include <list>
#include <utility>

#include "bytecode.h"
#include "lexer.h"
#include "utils.h"
#include <vd2/system/vdstl.h>
#include <vd2/Dita/interface.h>

using namespace nsVDUI;
using namespace nsVDDitaBytecode;

///////////////////////////////////////////////////////////////////////////
//
//	globals
//
///////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////
//
//	parser globals
//
///////////////////////////////////////////////////////////////////////////

typedef std::map<int, std::wstring> tStringSet;
typedef std::map<int, tStringSet> tStringResource;
typedef std::map<std::wstring, int> tVarList;
typedef std::list<tVarList> tScopeList;
typedef std::vector<unsigned char> tDialogScript;
typedef std::map<int, tDialogScript> tDialogList;
typedef std::map<int, tDialogScript> tTemplateList;

tStringResource g_stringResource;
tScopeList		g_scopes;
tDialogList		g_dialogs;
tTemplateList	g_templates;
int				g_enableExprs;

///////////////////////////////////////////////////////////////////////////
//
//	parser
//
///////////////////////////////////////////////////////////////////////////

void expect(int found, int expected) {
	if (found != expected)
		fatal("Expected %s, found %s", lextokenname(expected, false).c_str(), lextokenname(found).c_str());
}

void expect(int token) {
	expect(lex(), token);
}

void reject(int token) {
	fatal("Unexpected %s", lextokenname(token).c_str());
}

void unexpected(int token, const char *expected) {
	fatal("Expected %s, found %s", expected, lextokenname(token).c_str());
}

void parse_push_scope() {
	g_scopes.push_front(tVarList());
}

void parse_pop_scope() {
	if (g_scopes.empty())
		fatal_internal(__FILE__, __LINE__);

	g_scopes.pop_front();
}

int parse_expression();

int *parse_lvalue_expression() {
	int t = lex();

	if (t != kTokenIdentifier) {
		fatal("Expected expression, found %s", lextokenname(t).c_str());
	}

	tScopeList::iterator it = g_scopes.begin(), itEnd = g_scopes.end();
	const std::wstring& ident = lexident();

	for(; it!=itEnd; ++it) {
		tVarList& varlist = *it;
		tVarList::iterator itV = varlist.find(ident);

		if (itV != varlist.end()) {
			return &(*itV).second;
		}
	}

	fatal("Undeclared identifier '%s'", ANSIify(ident).c_str());
}

int parse_prefix_expression() {
	int t = lex();

	switch(t) {
	case '(':
		{
			int v = parse_expression();
			expect(')');
			return v;
		}
		
	case '+':
		return parse_prefix_expression();
	case '-':
		return -parse_prefix_expression();
	case '~':
		return ~parse_prefix_expression();
	case kTokenInteger:
		return lexint();
	case kTokenPlusPlus:
		{
			int *p = parse_lvalue_expression();

			return ++*p;
		}
	case kTokenMinusMinus:
		{
			int *p = parse_lvalue_expression();

			return --*p;
		}
	case kTokenLeft:
	case kTokenTop:
		return 1;
	case kTokenCenter:
		return 2;
	case kTokenBottom:
	case kTokenRight:
		return 3;
	case kTokenFill:
		return 4;
	case kTokenExpand:
		return 0x100;
	default:
		lexpush(t);
		return *parse_lvalue_expression();
	}

	// shouldn't happen
	return 0;
}

// This is such a dirty hack.

int parse_postfix_expression() {
	int t = lex();

	if (t == kTokenIdentifier) {
		lexpush(t);
		int *p = parse_lvalue_expression();

		t = lex();

		if (t == kTokenPlusPlus)
			return (*p)++;
		else if (t == kTokenMinusMinus)
			return (*p)--;

		lexpush(t);
		return *p;
	}

	lexpush(t);

	return parse_prefix_expression();
}

int parse_multiplicative_expression() {
	int x = parse_postfix_expression();

	for(;;) {
		int t = lex();

		if (t == '*') {
			int y = parse_prefix_expression();

			x *= y;
		} else if (t == '/') {
			int y = parse_prefix_expression();

			if (!y)
				fatal("Divide by zero");

			x /= y;
		} else {
			lexpush(t);

			return x;
		}
	}
}

int parse_additive_expression() {
	int x = parse_multiplicative_expression();

	for(;;) {
		int t = lex();

		if (t == '+') {
			int y = parse_multiplicative_expression();

			x += y;
		} else if (t == '-') {
			int y = parse_multiplicative_expression();

			x -= y;
		} else {
			lexpush(t);

			return x;
		}
	}
}


int parse_expression() {
	return parse_additive_expression();
}

std::wstring parse_string_expression() {
	expect(kTokenString);

	std::wstring text(lexident());

	for(;;) {
		int t = lex();

		if (t == kTokenString)
			text += lexident();
		else {
			lexpush(t);
			return text;
		}
	}
}

void parse_let() {
	std::wstring name;

	expect(kTokenIdentifier);
	name = lexident();
	expect('=');
	int v = parse_expression();
	expect(';');

	tVarList& varlist = g_scopes.front();

	varlist[name] = v;
}

void parse_declare() {
	std::wstring name;

	expect(kTokenIdentifier);
	name = lexident();
	expect('=');
	int v = parse_expression();
	expect(';');

	tVarList& varlist = g_scopes.front();
	tVarList::iterator it = varlist.find(name);

	// only err if the second declaration is different

	if (it != varlist.end()) {
		if ((*it).second != v)
			fatal("Variable %s already declared with value %d", ANSIify(name).c_str(), (*it).second);

		return;
	}

	varlist[name] = v;
}

void parse_enum() {
	int eval = 0;
	int t = lex();

	if (t != kTokenIdentifier)
		lexpush(t);

	expect('{');

	for(;;) {
		int v;

		expect(kTokenIdentifier);
		std::wstring name(lexident());

		t = lex();

		if (t == '=') {
			v = eval = parse_expression();
		} else {
			v = eval++;
			lexpush(t);
		}

		tVarList& varlist = g_scopes.front();
		tVarList::iterator it = varlist.find(name);

		if (it != varlist.end()) {
			if ((*it).second != v)
				fatal("Variable %s already declared with value %d", ANSIify(name).c_str(), (*it).second);
		}
		
		varlist[name] = v;

		t = lex();

		if (t == '}')
			break;

		lexpush(t);
		expect(',');
	}

	expect(';');
}

void parse_common(int t) {
	switch(t) {
	case ';':
		break;
	case kTokenLet:
		parse_let();
		break;
	case kTokenDeclare:
		parse_declare();
		break;
	case kTokenEnum:
		parse_enum();
		break;
	default:
		fatal("Expected statement, found %s", lextokenname(t).c_str());
	}
}

void parse_message(tStringSet& set, int setid) {
	int t = lex();
	bool bAllowOverrides = false;

	if (t == kTokenOverride)
		bAllowOverrides = true;
	else
		lexpush(t);

	int id = parse_expression();

	if (id < 0 || id > 65535)
		fatal("String ID '%d' is not between 0 and 65535", id);

	if (!bAllowOverrides) {
		tStringSet::iterator it = set.find(id);

		if (it != set.end())
			warning("String set %d already has a string with ID %d", setid, id);
	}

	expect(',');

	std::wstring msg = parse_string_expression();

	set[id] = msg;

	expect(';');
}

void parse_stringSet() {
	int setID = parse_expression();

	if (setID == 65535)
		fatal("String set ID 65535 is reserved");

	if (setID < 0 || setID > 65535)
		fatal("String set ID '%d' is not between 0 and 65535", setID);

	expect('{');
	parse_push_scope();

	tStringSet& strset = g_stringResource[setID];
	for(;;) {
		int t = lex();

		if (t < 0)
			fatal("Unterminated string set declaration");

		switch(t) {
		case '}':
			parse_pop_scope();
			return;

		case kTokenMessage:
			parse_message(strset, setID);
			break;

		default:
			parse_common(t);
		}
	}
}

void parse_dialog_common(int dlgID, tDialogScript& script, int t);

void dialog_int(tDialogScript& script, int v) {
	if (!v)
		script.push_back(kBC_Zero);
	else if (v == 1)
		script.push_back(kBC_One);
	else if (v == (signed char)v) {
		script.push_back(kBC_Int8);
		script.push_back(0xff & v);
	} else {
		script.push_back(kBC_Int32);
		script.push_back(v & 0xff);
		script.push_back((v>>8) & 0xff);
		script.push_back((v>>16) & 0xff);
		script.push_back((v>>24) & 0xff);
	}
}

void dialog_string(tDialogScript& script, int table, int id) {
	if (table == 0xffff) {
		script.push_back(kBC_StringShort);
		script.push_back(id&0xff);
		script.push_back((id>>8)&0xff);
	} else {
		script.push_back(kBC_String);
		script.push_back(table&0xff);
		script.push_back((table>>8)&0xff);
		script.push_back(id&0xff);
		script.push_back((id>>8)&0xff);
	}
}

// This is very expensive but I don't care

void dialog_string(tDialogScript& script, std::wstring str) {
	tStringSet& strset = g_stringResource[65535];
	tStringSet::iterator it = strset.begin(), itEnd = strset.end();
	int count = 0;

	for(; it!=itEnd; ++it, ++count) {
		if ((*it).second == str) {
			dialog_string(script, 0xffff, (*it).first);
			return;
		}
	}

	strset[count] = str;
	dialog_string(script, 0xffff, count);
}

void parse_dialog_string(int dialogID, tDialogScript& script) {
	int t = lex();

	if (t == kTokenString) {
		std::wstring s(lexident());

		for(;;) {
			t = lex();
			if (t != kTokenString) {
				lexpush(t);
				break;
			}
			s += lexident();
		}

		dialog_string(script, s);
	} else if (t == '[') {
		int group = parse_expression();
		int id;

		t = lex();
		if (t == ',') {
			id = parse_expression();
			expect(']');
		} else if (t == ']') {
			id = group;
			group = dialogID;
		} else
			expect(t, ']');

		dialog_string(script, group, id);
	} else
		fatal("expected string expression, found %s", lextokenname(t).c_str());
}

int get_runtime_op_mode(int op) {
	switch(op) {
	case kBCE_OpNegate:		return 0x501;
	case kBCE_OpNot:		return 0x501;
	case kBCE_OpMul:		return 0x400;
	case kBCE_OpDiv:		return 0x400;
	case kBCE_OpAdd:		return 0x300;
	case kBCE_OpSub:		return 0x300;
	case kBCE_OpEQ:			return 0x200;
	case kBCE_OpNE:			return 0x200;
	case kBCE_OpLT:			return 0x200;
	case kBCE_OpLE:			return 0x200;
	case kBCE_OpGT:			return 0x200;
	case kBCE_OpGE:			return 0x200;
	case kBCE_OpLogicalAnd:	return 0x100;
	case kBCE_OpLogicalOr:	return 0x100;
	default:
		return 0;
	}
}

typedef vdfastvector<unsigned char> tREBytecode;

void emit_runtime_int(tREBytecode& bytecode, int n) {
	if (!n)
		bytecode.push_back(kBCE_Zero);
	else if (n == 1)
		bytecode.push_back(kBCE_One);
	else if ((signed char)n == n) {
		bytecode.push_back(kBCE_Int8);
		bytecode.push_back((unsigned char)n);
	} else {
		bytecode.push_back(kBCE_Int32);
		bytecode.push_back((unsigned char)(n    ));
		bytecode.push_back((unsigned char)(n>> 8));
		bytecode.push_back((unsigned char)(n>>16));
		bytecode.push_back((unsigned char)(n>>24));
	}
}

void parse_runtime_expression(tREBytecode& bytecode) {
	std::vector<int> ops;
	std::vector<sint32> winrefs;
	bool expect_value = true;
	int parens = 0;
	int t;

	for(;;) {
		t = lex();

		if (expect_value) {
			switch(t) {
			case '(':
				++parens;
				break;
			case '@':
				{
					int id = parse_prefix_expression();

					std::vector<int>::const_iterator it(std::find(winrefs.begin(), winrefs.end(), id));
					bytecode.push_back(kBCE_GetValue);
					bytecode.push_back(it - winrefs.begin());
					if (it == winrefs.end()) {
						winrefs.push_back(id);
						if (winrefs.size() > 255)
							fatal("too many window references in runtime expression");
					}
				}
				expect_value = false;
				break;
			case kTokenInteger:
				emit_runtime_int(bytecode, lexint());
				expect_value = false;
				break;
			case '-':
				ops.push_back(kBCE_OpNegate + (parens<<16));
				break;
			case '!':
				ops.push_back(kBCE_OpNot + (parens<<16));
				break;
			default:
				unexpected(t, "value");
				break;
			}
		} else {
			if (t == ')' && parens) {
				--parens;

				while(!ops.empty() && (ops.back() >> 16) > parens) {
					bytecode.push_back(ops.back() & 0xffff);
					ops.pop_back();
				}
				continue;
			}

			int rop = 0;
			switch(t) {
			case '+':		rop = kBCE_OpAdd; break;
			case '-':		rop = kBCE_OpSub; break;
			case '*':		rop = kBCE_OpMul; break;
			case '/':		rop = kBCE_OpDiv; break;
			case kTokenEQ:	rop = kBCE_OpEQ; break;
			case kTokenNE:	rop = kBCE_OpNE; break;
			case '<':		rop = kBCE_OpLT; break;
			case kTokenLE:	rop = kBCE_OpLE; break;
			case '>':		rop = kBCE_OpGT; break;
			case kTokenGE:	rop = kBCE_OpGE; break;
			case kTokenLogicalAnd:	rop = kBCE_OpLogicalAnd; break;
			case kTokenLogicalOr:	rop = kBCE_OpLogicalOr; break;
			}

			int opprec = get_runtime_op_mode(rop) + (parens<<16);

			while(!ops.empty()) {
				int stackop = ops.back();

				if (opprec > get_runtime_op_mode(stackop&0xffff) + (stackop&~0xffff))
					break;

				bytecode.push_back(stackop & 0xffff);
				ops.pop_back();
			}

			if (!rop)
				break;

			ops.push_back(rop + (parens<<16));
			expect_value = true;
		}
	}

	if (parens)
		unexpected(t, "operator");

	lexpush(t);

	if (bytecode.size() > 65534)
		fatal("runtime expression too complex");

	// too lazy to do this correctly
	const unsigned bytes = bytecode.size();
	const uint8 sizehdr[2]={ (uint8)bytes, (uint8)(bytes >> 8) };

	bytecode.insert(bytecode.begin(), sizehdr, sizehdr+2);
	bytecode.insert(bytecode.begin(), (const unsigned char *)&winrefs[0], (const unsigned char *)&winrefs[0] + winrefs.size()*sizeof(winrefs[0]));
	bytecode.insert(bytecode.begin(), (uint8)winrefs.size());

	bytecode.push_back(0);
}

void parse_runtime_expressions(tREBytecode& bytecode) {
	int expressions = 0;

	expect('{');

	for(;;) {
		parse_runtime_expression(bytecode);
		++expressions;
		int t = lex();
		if (t != ',') {
			lexpush(t);
			break;
		}
	}

	expect('}');

	if (expressions > 255)
		fatal("too many link expressions (max 255)");

	bytecode.insert(bytecode.begin(), expressions);
}

void parse_parameterB(uint32 propid, tDialogScript& script) {
	dialog_int(script, propid);
	dialog_int(script, 1);
	script.push_back(kBC_SetParameterB);
}

void parse_parameterI(uint32 propid, tDialogScript& script) {
	expect('=');
	dialog_int(script, propid);
	dialog_int(script, parse_expression());
	script.push_back(kBC_SetParameterI);
}

void parse_parameterF(uint32 propid, tDialogScript& script) {
	expect('=');
	dialog_int(script, propid);
	union { float v; int i; } conv = {parse_expression() / 100.0f};
	script.push_back(kBC_Float32);
	script.push_back( conv.i       & 0xff);
	script.push_back((conv.i >>  8) & 0xff);
	script.push_back((conv.i >> 16) & 0xff);
	script.push_back((conv.i >> 24) & 0xff);
	script.push_back(kBC_SetParameterF);
}

void parse_parameters(tDialogScript& script) {
	for(;;) {
		int t = lex();

		switch(t) {
		case kTokenMarginL:		parse_parameterI(kUIParam_MarginL, script);	break;
		case kTokenMarginT:		parse_parameterI(kUIParam_MarginT, script);	break;
		case kTokenMarginR:		parse_parameterI(kUIParam_MarginR, script);	break;
		case kTokenMarginB:		parse_parameterI(kUIParam_MarginB, script);	break;
		case kTokenPadL:		parse_parameterI(kUIParam_PadL, script);	break;
		case kTokenPadT:		parse_parameterI(kUIParam_PadT, script);	break;
		case kTokenPadR:		parse_parameterI(kUIParam_PadR, script);	break;
		case kTokenPadB:		parse_parameterI(kUIParam_PadB, script);	break;
		case kTokenMinW:		parse_parameterI(kUIParam_MinW, script);	break;
		case kTokenMinH:		parse_parameterI(kUIParam_MinH, script);	break;
		case kTokenMaxW:		parse_parameterI(kUIParam_MaxW, script);	break;
		case kTokenMaxH:		parse_parameterI(kUIParam_MaxH, script);	break;
		case kTokenAlign:		parse_parameterI(kUIParam_Align, script);	break;
		case kTokenVAlign:		parse_parameterI(kUIParam_VAlign, script);	break;
		case kTokenSpacing:		parse_parameterI(kUIParam_Spacing, script);	break;
		case kTokenAffinity:	parse_parameterI(kUIParam_Affinity, script);	break;
		case kTokenAspect:		parse_parameterF(kUIParam_Aspect, script);	break;

		case kTokenRow:			parse_parameterI(kUIParam_Row, script);	break;
		case kTokenColumn:		parse_parameterI(kUIParam_Col, script);	break;
		case kTokenRowSpan:		parse_parameterI(kUIParam_RowSpan, script);	break;
		case kTokenColSpan:		parse_parameterI(kUIParam_ColSpan, script);	break;

		case kTokenFill:
			dialog_int(script, kUIParam_Align);
			dialog_int(script, nsVDUI::kFill);
			script.push_back(kBC_SetParameterI);
			dialog_int(script, kUIParam_VAlign);
			dialog_int(script, nsVDUI::kFill);
			script.push_back(kBC_SetParameterI);
			break;
		case kTokenVertical:	parse_parameterB(kUIParam_IsVertical, script); break;
		case kTokenRaised:		parse_parameterB(kUIParam_Raised, script); break;
		case kTokenSunken:		parse_parameterB(kUIParam_Sunken, script); break;
		case kTokenChild:		parse_parameterB(kUIParam_Child, script); break;
		case kTokenMultiline:	parse_parameterB(kUIParam_Multiline, script); break;
		case kTokenReadonly:	parse_parameterB(kUIParam_Readonly, script); break;
		case kTokenCheckable:	parse_parameterB(kUIParam_Checkable, script); break;
		case kTokenNoHeader:	parse_parameterB(kUIParam_NoHeader, script); break;
		case kTokenDefault:		parse_parameterB(kUIParam_Default, script); break;

		case kTokenEnable:
		case kTokenValue:
			{
				expect('=');

				tREBytecode bytecode;
				parse_runtime_expressions(bytecode);

				if (bytecode.size() >= 65536)
					fatal("runtime expression too complex");

				script.push_back(kBC_SetLinkExpr);
				script.push_back(bytecode.size() & 255);
				script.push_back(bytecode.size() >> 8);

				script.insert(script.end(), bytecode.begin(), bytecode.end());

				dialog_int(script, t == kTokenEnable ? kUIParam_EnableLinkExpr : kUIParam_ValueLinkExpr);
				dialog_int(script, g_enableExprs++);
				script.push_back(kBC_SetParameterI);
			}
			break;

		default:
			fatal("expected parameter name, found %s", lextokenname(t).c_str());
		}

		t = lex();
		if (t != ',') {
			lexpush(t);
			break;
		}
	}
}

bool parse_optional_parameters(tDialogScript& script, bool autoscope) {
	int t = lex();

	if (t == ':') {
		if (autoscope)
			script.push_back(kBC_PushParameters);
		parse_parameters(script);
		return true;
	}

	lexpush(t);
	return false;
}

void parse_dialog_common(int dlgID, tDialogScript& script, int t);

void parse_dialog_control(int dlgID, tDialogScript& script, unsigned char create_bc) {
	int t = lex();

	if (create_bc == kBC_CreateChildDialog) {
		lexpush(t);
		dialog_int(script, parse_expression());
		expect(',');
		dialog_int(script, parse_expression());
		bool popparms = parse_optional_parameters(script, true);
		script.push_back(create_bc);
		if (popparms)
			script.push_back(kBC_PopParameters);

		t = lex();
	} else if (create_bc == kBC_CreateCustom) {		// customWindow id, clsid [,caption] [: parms...]
		lexpush(t);
		dialog_int(script, parse_expression());
		expect(',');
		dialog_int(script, parse_expression());
		t = lex();
		if (t == ',') {
			parse_dialog_string(dlgID, script);
		} else {
			script.push_back(kBC_StringNull);
			lexpush(t);
		}
		bool popparms = parse_optional_parameters(script, true);
		script.push_back(create_bc);
		if (popparms)
			script.push_back(kBC_PopParameters);

		t = lex();
	} else if (t == '{') {
		dialog_int(script, 0);
		script.push_back(kBC_StringNull);
	} else {
		lexpush(t);
		dialog_int(script, parse_expression());
		t = lex();
		if (t == ',') {
			parse_dialog_string(dlgID, script);
		} else {
			script.push_back(kBC_StringNull);
			lexpush(t);
		}
		bool popparms = parse_optional_parameters(script, true);
		script.push_back(create_bc);
		if (popparms)
			script.push_back(kBC_PopParameters);

		t = lex();
	}

	if (t == '{') {
		script.push_back(kBC_BeginChildren);
		script.push_back(kBC_PushParameters);
		for(;;) {
			t = lex();
			if (t == '}')
				break;

			parse_dialog_common(dlgID, script, t);
		}
		script.push_back(kBC_PopParameters);
		script.push_back(kBC_EndChildren);
	} else {
		lexpush(t);
		expect(';');
	}
}

void parse_dialog_item(int dlgID, tDialogScript& script, unsigned char create_bc) {
	int t = lex();

	lexpush(t);
	dialog_int(script, parse_expression());

	bool popparms = parse_optional_parameters(script, true);
	script.push_back(create_bc);
	if (popparms)
		script.push_back(kBC_PopParameters);

	expect(';');
}

void parse_dialog_common(int dlgID, tDialogScript& script, int t) {

	switch(t) {
	case kTokenLabel:
		parse_dialog_control(dlgID, script, kBC_CreateLabel);
		break;
	case kTokenEdit:
		parse_dialog_control(dlgID, script, kBC_CreateEdit);
		break;
	case kTokenButton:
		parse_dialog_control(dlgID, script, kBC_CreateButton);
		break;
	case kTokenCheckBox:
		parse_dialog_control(dlgID, script, kBC_CreateCheckBox);
		break;
	case kTokenListBox:
		parse_dialog_control(dlgID, script, kBC_CreateListBox);
		break;
	case kTokenComboBox:
		parse_dialog_control(dlgID, script, kBC_CreateComboBox);
		break;
	case kTokenListView:
		parse_dialog_control(dlgID, script, kBC_CreateListView);
		break;
	case kTokenTrackbar:
		parse_dialog_control(dlgID, script, kBC_CreateTrackbar);
		break;
	case kTokenOption:
		parse_dialog_control(dlgID, script, kBC_CreateOption);
		break;
	case kTokenSplitter:
		parse_dialog_control(dlgID, script, kBC_CreateSplitter);
		break;
	case kTokenSet:
		parse_dialog_control(dlgID, script, kBC_CreateSet);
		break;
	case kTokenPageSet:
		parse_dialog_control(dlgID, script, kBC_CreatePageSet);
		break;
	case kTokenGrid:
		parse_dialog_control(dlgID, script, kBC_CreateGrid);
		break;
	case kTokenTextEdit:
		parse_dialog_control(dlgID, script, kBC_CreateTextEdit);
		break;
	case kTokenTextArea:
		parse_dialog_control(dlgID, script, kBC_CreateTextArea);
		break;
	case kTokenGroup:
		parse_dialog_control(dlgID, script, kBC_CreateGroup);
		break;
	case kTokenDialog:
		parse_dialog_control(dlgID, script, kBC_CreateChildDialog);
		break;
	case kTokenHotkey:
		parse_dialog_control(dlgID, script, kBC_CreateHotkey);
		break;
	case kTokenCustomWindow:
		parse_dialog_control(dlgID, script, kBC_CreateCustom);
		break;

	case kTokenListItem:
		parse_dialog_string(dlgID, script);
		script.push_back(kBC_AddListItem);
		expect(';');
		break;

	case kTokenPage:
		dialog_int(script, parse_expression());
		script.push_back(kBC_AddPage);
		expect(';');
		break;

	case kTokenNow:
		parse_parameters(script);
		expect(';');
		break;

	case kTokenUsing:
		script.push_back(kBC_PushParameters);
		parse_parameters(script);
		expect('{');
		for(;;) {
			t = lex();
			if (t < 0)
				fatal("unterminated using block");
			if (t == '}')
				break;
			parse_dialog_common(dlgID, script, t);
		}
		script.push_back(kBC_PopParameters);
		break;

	case kTokenInclude:
		t = lex();
		if (t == kTokenTemplate) {
			int v = parse_expression();

			tTemplateList::iterator it = g_templates.find(v);

			if (it == g_templates.end())
				fatal("Template '%d' not defined", v);

			script.push_back(kBC_InvokeTemplate);
			script.push_back(v & 0xff);
			script.push_back((v>>8) & 0xff);
			break;
		}
		lexpush(t);
		parse_common(kTokenInclude);
		break;

	case kTokenColumn:
		parse_dialog_item(dlgID, script, kBC_SetColumn);
		break;

	case kTokenRow:
		parse_dialog_item(dlgID, script, kBC_SetRow);
		break;

	case kTokenNextRow:
		script.push_back(kBC_NextRow);
		break;

	default:
		parse_common(t);
	}
}

void parse_template() {
	int t = lex();
	bool bAllowOverride = false;

	if (t == kTokenOverride)
		bAllowOverride = true;
	else
		lexpush(t);

	int tmplID = parse_expression();

	if (tmplID < 0 || tmplID > 65535)
		fatal("Template ID '%d' is not within 0 to 65535", tmplID);

	if (g_templates.find(tmplID) != g_templates.end()) {
		if (!bAllowOverride)
			fatal("Dialog template '%d' has already been defined (use 'override' if intentional)", tmplID);

		g_templates[tmplID].clear();
	}

	tDialogScript& script = g_templates[tmplID];

	g_enableExprs = 0;

	expect('{');
	parse_push_scope();

	for(;;) {
		t = lex();

		if (t < 0)
			fatal("Unterminated template declaration");

		switch(t) {
		case '}':
			parse_pop_scope();
			script.push_back(0);
			return;

		default:
			parse_dialog_common(tmplID, script,t);
		}
	}
}

void parse_dialog() {
	int t = lex();
	bool bAllowOverride = false;

	if (t == kTokenOverride)
		bAllowOverride = true;
	else
		lexpush(t);

	expect(kTokenInteger);
	int dialogID = lexint();

	if (dialogID < 0 || dialogID > 65535)
		fatal("Dialog ID '%d' is not between 0 and 65535", dialogID);

	if (g_dialogs.find(dialogID) != g_dialogs.end()) {
		if (!bAllowOverride)
			fatal("Dialog ID '%d' has already been defined (use 'override' if intentional)", dialogID);

		g_dialogs[dialogID].clear();
	}

	tDialogScript& script = g_dialogs[dialogID];

	g_enableExprs = 0;

	expect(',');
	dialog_int(script, dialogID);
	parse_dialog_string(dialogID, script);

	bool autoscoped = parse_optional_parameters(script, true);
	script.push_back(kBC_CreateDialog);
	if (autoscoped)
		script.push_back(kBC_PopParameters);

	script.push_back(kBC_BeginChildren);

	expect('{');
	parse_push_scope();

	for(;;) {
		t = lex();

		if (t < 0)
			fatal("Unterminated dialog declaration");

		switch(t) {
		case '}':
			parse_pop_scope();
			script.push_back(kBC_EndChildren);
			script.push_back(kBC_End);
			return;

		default:
			parse_dialog_common(dialogID, script,t);
		}
	}
}

void parse() {

	parse_push_scope();

	for(;;) {
		int t = lex();

		if (t < 0)
			break;

		switch(t) {
		case kTokenStringSet:
			parse_stringSet();
			break;

		case kTokenDialog:
			parse_dialog();
			break;

		case kTokenTemplate:
			parse_template();
			break;

		case kTokenInclude:
			{
				expect(kTokenString);
				std::string filename(ANSIify(lexident()));
				expect(';');

				lexinclude(filename);
			}
			break;

		default:
			parse_common(t);
		}
	}

	parse_pop_scope();

	if (!g_scopes.empty())
		fatal_internal(__FILE__, __LINE__);
}

///////////////////////////////////////////////////////////////////////////
//
//	writeout
//
///////////////////////////////////////////////////////////////////////////

typedef unsigned Fourcc;

#define MAKE_FCC(a,b,c,d) (((unsigned char)(d) << 24) + ((unsigned char)(c) << 16) + ((unsigned char)(b) << 8) + (unsigned char)(a))

enum {
	kFourcc_StringSets		= MAKE_FCC('S','T','R','S'),
	kFourcc_Dialogs			= MAKE_FCC('D','L','G','S'),
	kFourcc_Templates		= MAKE_FCC('D','T','P','S')
};

typedef std::list<long> tFileScopeList;

tFileScopeList g_fileScopes;
FILE *g_fileOut;

void write_begin_scope(Fourcc fcc) {
	fwrite(&fcc, 4, 1, g_fileOut);
	fwrite(&fcc, 4, 1, g_fileOut);	// reserve space for size

	g_fileScopes.push_front(ftell(g_fileOut));
}

void write_end_scope() {
	if (g_fileScopes.empty())
		fatal_internal(__FILE__, __LINE__);

	long fp = ftell(g_fileOut);
	int pad = (-fp)&3;

	fwrite("\0\0\0", pad, 1, g_fileOut);
	fp += pad;

	long fp_scope = g_fileScopes.front();
	long siz = fp - fp_scope;

	g_fileScopes.pop_front();

	fseek(g_fileOut, fp_scope-4, SEEK_SET);
	fwrite(&siz, 4, 1, g_fileOut);
	fseek(g_fileOut, fp, SEEK_SET);
}

void write_short(int i) {
	short s = (short)i;

	fwrite(&s, 2, 1, g_fileOut);
}

int writeout(FILE *f) {
	char header[64]="[01|01] VirtualDub language resource file\r\n\x1A";

	g_fileOut = f;

	fwrite(header, sizeof header, 1, f);

	// write stringsets

	write_begin_scope(kFourcc_StringSets);
	{
		tStringResource::iterator it = g_stringResource.begin(), itEnd = g_stringResource.end();

		write_short(g_stringResource.size());

		for(; it!=itEnd; ++it) {
			tStringSet& strset = (*it).second;
			tStringSet::iterator itStr = strset.begin(), itStrEnd = strset.end();

			write_short((*it).first);
			write_short(strset.size());

			for(; itStr != itStrEnd; ++itStr) {
				std::wstring& wstr = (*itStr).second;
				write_short((*itStr).first);

				std::basic_string<unsigned char> scsu_encoded = ConvertToSCSU(wstr);

				int s = scsu_encoded.size();

				if (s <= 0x7f) {
					putc(s, f);
				} else if (s <= 0x3fff) {
					putc((s>>7)|0x80, f);
					putc((s&0x7f), f);
				} else
					fatal_internal(__FILE__, __LINE__);

				fwrite(scsu_encoded.data(), scsu_encoded.size(), 1, f);
			}
		}
	}
	write_end_scope();

	// write dialogs

	write_begin_scope(kFourcc_Dialogs);
	{
		tDialogList::iterator it = g_dialogs.begin(), itEnd = g_dialogs.end();

		write_short(g_dialogs.size());
		for(; it!=itEnd; ++it) {
			tDialogScript& scr = (*it).second;
			int siz = scr.size();
			write_short((*it).first);

			if (siz >= 65535)
				fatal("Dialog %d is too big (%d > 65535 bytes)", (*it).first, siz);

			write_short(siz);

			fwrite(&scr[0], siz, 1, f);
		}
	}
	write_end_scope();

	// write templates

	write_begin_scope(kFourcc_Templates);
	{
		tTemplateList::iterator it = g_templates.begin(), itEnd = g_templates.end();

		write_short(g_templates.size());
		for(; it!=itEnd; ++it) {
			tDialogScript& scr = (*it).second;
			int siz = scr.size();
			write_short((*it).first);

			if (siz >= 65535)
				fatal("Template %d is too big (%d > 65535 bytes)", (*it).first, siz);

			write_short(siz);

			fwrite(&scr[0], siz, 1, f);
		}
	}
	write_end_scope();

	return ftell(f);
}

///////////////////////////////////////////////////////////////////////////
//
//	main
//
///////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv) {
	if (argc < 3) {
		printf(
			"Ami - Language resource compiler for VirtualDub, version 1.0\n"
			"\n"
			"Usage: ami <input-file.ami> <output-file.vlr>\n"
			);
		return 5;
	}

	// open input file

	lexopen(argv[1]);
	printf("Ami: Compiling %s (%s) -> %s\n", argv[1], lexisunicode() ? "Unicode" : "ANSI", argv[2]);

	// process

	parse();

	FILE *f = fopen(argv[2], "wb");

	if (!f)
		fatal("Cannot open output file");

	int bytes = writeout(f);

	if (ferror(f) || fclose(f))
		fatal("Write error creating output file");

	printf("Ami: Compile successful -- %d string tables, %d bytes\n", g_stringResource.size(), bytes);

	return 0;
}
