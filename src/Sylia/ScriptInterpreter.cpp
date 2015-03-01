#include <vd2/system/vdtypes.h>
#include <vd2/system/text.h>
#include <vd2/system/VDString.h>
#include <vd2/system/vdstl.h>
#include <vector>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <malloc.h>

#include "StringHeap.h"
#include "ScriptError.h"
#include "ScriptValue.h"
#include "ScriptInterpreter.h"
#include "VariableTable.h"

///////////////////////////////////////////////////////////////////////////

//#define DEBUG_TOKEN

///////////////////////////////////////////////////////////////////////////

extern VDScriptObject obj_Sylia;

///////////////////////////////////////////////////////////////////////////

class VDScriptInterpreter : public IVDScriptInterpreter {
private:
	enum {
		MAX_FUNCTION_PARAMS = 16,
		MAX_IDENT_CHARS = 64
	};

	enum {
		TOK_IDENT		= 256,
		TOK_INTVAL,
		TOK_LONGVAL,
		TOK_DBLVAL,
		TOK_INT,
		TOK_LONG,
		TOK_DOUBLE,
		TOK_STRING,
		TOK_DECLARE,
		TOK_TRUE,
		TOK_FALSE,
		TOK_AND,
		TOK_OR,
		TOK_EQUALS,
		TOK_NOTEQ,
		TOK_LESSEQ,
		TOK_GRTREQ,
	};

	const char *tokbase;
	const char *tokstr;
	union {
		int tokival;
		sint64 toklval;
		double tokdval;
	};
	int tokhold;
	char **tokslit;
	char szIdent[MAX_IDENT_CHARS+1];
	VDScriptValue		mErrorObject;
	vdfastvector<char> mError;

	std::vector<VDScriptValue> mStack;
	vdfastvector<int> mOpStack;

	VDScriptRootHandlerPtr lpRoothandler;
	void *lpRoothandlerData;

	VDScriptStringHeap strheap;
	VariableTable	vartbl;

	const VDScriptFunctionDef *mpCurrentInvocationMethod;
	const VDScriptFunctionDef *mpCurrentInvocationMethodOverload;
	int mMethodArgumentCount;

	VDStringA mErrorExtraToken;

	////////

	void			ParseExpression();
	int				ExprOpPrecedence(int op);
	bool			ExprOpIsRightAssoc(int op);
	void			ParseValue();
	void			Reduce();

	void	ConvertToRvalue();
	void	InvokeMethod(const VDScriptObject *objdef, const char *name, int argc);
	void	InvokeMethod(const VDScriptFunctionDef *sfd, int pcount);

	bool	isExprFirstToken(int t);
	bool	isUnaryToken(int t);

	VDScriptValue	LookupRootVariable(char *szName);

	bool isIdentFirstChar(char c);
	bool isIdentNextChar(char c);
	void TokenBegin(const char *s);
	void TokenUnput(int t);
	int Token();
	void GC();

public:
	VDScriptInterpreter();
	~VDScriptInterpreter();
	void Destroy();

	void SetRootHandler(VDScriptRootHandlerPtr, void *);

	void ExecuteLine(const char *s);

	void ScriptError(int e);
	const char *TranslateScriptError(const VDScriptError& cse);
	char** AllocTempString(long l);
	VDScriptValue	DupCString(const char *s);

	VDScriptValue	LookupObjectMember(const VDScriptObject *obj, void *lpVoid, char *szIdent);

	const VDScriptFunctionDef *GetCurrentMethod() { return mpCurrentInvocationMethodOverload; }
	int GetErrorLocation() { return tokstr - tokbase; }
};

///////////////////////////////////////////////////////////////////////////

VDScriptInterpreter::VDScriptInterpreter() : vartbl(128) {
}

VDScriptInterpreter::~VDScriptInterpreter() {
	lpRoothandler = NULL;
}

void VDScriptInterpreter::Destroy() {
	delete this;
}

IVDScriptInterpreter *VDCreateScriptInterpreter() {
	return new VDScriptInterpreter();
}

///////////////////////////////////////////////////////////////////////////

void VDScriptInterpreter::SetRootHandler(VDScriptRootHandlerPtr rh, void *rh_data) {
	lpRoothandler = rh;
	lpRoothandlerData = rh_data;
}

///////////////////////////////////////////////////////////////////////////

void VDScriptInterpreter::ExecuteLine(const char *s) {
	int t;

	mErrorExtraToken.clear();

	TokenBegin(s);

	while(t = Token()) {
		if (t == ';')
			continue;

		if (isExprFirstToken(t)) {
			TokenUnput(t);
			VDASSERT(mStack.empty());
			ParseExpression();

			VDASSERT(!mStack.empty());

			VDScriptValue& val = mStack.back();

			mStack.pop_back();
			VDASSERT(mStack.empty());

			if (Token() != ';')
				SCRIPT_ERROR(SEMICOLON_EXPECTED);
		} else if (t == TOK_DECLARE) {

			do {
				t = Token();

				if (t != TOK_IDENT)
					SCRIPT_ERROR(IDENTIFIER_EXPECTED);

				VariableTableEntry *vte = vartbl.Declare(szIdent);

				t = Token();

				if (t == '=') {
					ParseExpression();

					VDASSERT(!mStack.empty());
					vte->v = mStack.back();
					mStack.pop_back();

					t = Token();
				}

			} while(t == ',');

			if (t != ';')
				SCRIPT_ERROR(SEMICOLON_EXPECTED);

		} else
			SCRIPT_ERROR(PARSE_ERROR);
	}

	VDASSERT(mStack.empty());

	GC();
}

void VDScriptInterpreter::ScriptError(int e) {
	throw VDScriptError(e);
}

namespace {
	void strveccat(vdfastvector<char>& error, const char *s) {
		error.insert(error.end(), s, s+strlen(s));
	}
}

const char *VDScriptInterpreter::TranslateScriptError(const VDScriptError& cse) {
	int i;
	char szError[1024];

	switch(cse.err) {
	case VDScriptError::OVERLOADED_FUNCTION_NOT_FOUND:
	case VDScriptError::AMBIGUOUS_CALL:
		{
			if (cse.err == VDScriptError::OVERLOADED_FUNCTION_NOT_FOUND) {
				if (mpCurrentInvocationMethod && mpCurrentInvocationMethod->name)
					sprintf(szError, "Overloaded method %s(", mpCurrentInvocationMethod->name);
				else
					strcpy(szError, "Overloaded method (");
			} else {
				if (mpCurrentInvocationMethod && mpCurrentInvocationMethod->name)
					sprintf(szError, "Ambiguous call with arguments %s(", mpCurrentInvocationMethod->name);
				else
					strcpy(szError, "Ambiguous call with arguments (");
			}

			mError.assign(szError, szError + strlen(szError));

			if (mMethodArgumentCount) {
				const VDScriptValue *const argv = &*(mStack.end() - mMethodArgumentCount);

				for(i=0; i<mMethodArgumentCount; i++) {
					if (i) {
						if (argv[i].isVoid()) break;

						mError.push_back(',');
					}

					switch(argv[i].type) {
					case VDScriptValue::T_VOID:		strveccat(mError, "void"); break;
					case VDScriptValue::T_INT:		strveccat(mError, "int"); break;
					case VDScriptValue::T_LONG:		strveccat(mError, "long"); break;
					case VDScriptValue::T_DOUBLE:		strveccat(mError, "double"); break;
					case VDScriptValue::T_STR:		strveccat(mError, "string"); break;
					case VDScriptValue::T_OBJECT:	strveccat(mError, "object"); break;
					case VDScriptValue::T_FNAME:		strveccat(mError, "method"); break;
					case VDScriptValue::T_FUNCTION:	strveccat(mError, "function"); break;
					case VDScriptValue::T_VARLV:		strveccat(mError, "var"); break;
					}
				}
			}

			strveccat(mError, ")");
			if (cse.err == VDScriptError::OVERLOADED_FUNCTION_NOT_FOUND)
				strveccat(mError, " not found");
		}
		mError.push_back(0);

		return mError.data();
	case VDScriptError::MEMBER_NOT_FOUND:
		sprintf(szError, "Class '%s' does not have a member called '%s'", mErrorObject.asObjectDef()->mpName, szIdent);
		mError.assign(szError, szError + strlen(szError) + 1);
		return mError.data();
	case VDScriptError::VAR_NOT_FOUND:
		if (!mErrorExtraToken.empty()) {
			sprintf(szError, "Variable '%s' not found", mErrorExtraToken.c_str());
			mError.assign(szError, szError + strlen(szError) + 1);
			return mError.data();
		}
		break;
	}
	return ::VDScriptTranslateError(cse);
}

char **VDScriptInterpreter::AllocTempString(long l) {
	char **handle = strheap.Allocate(l);

	(*handle)[l]=0;

	return handle;
}

VDScriptValue VDScriptInterpreter::DupCString(const char *s) {
	const size_t l = strlen(s);
	char **pp = AllocTempString(l);

	memcpy(*pp, s, l);
	return VDScriptValue(pp);
}

///////////////////////////////////////////////////////////////////////////
//
//	Expression parsing
//
///////////////////////////////////////////////////////////////////////////

int VDScriptInterpreter::ExprOpPrecedence(int op) {
	// All of these need to be EVEN.
	switch(op) {
	case '=':			return 2;
	case TOK_OR:		return 4;
	case TOK_AND:		return 6;
	case '|':			return 8;
	case '^':			return 10;
	case '&':			return 12;
	case TOK_EQUALS:
	case TOK_NOTEQ:		return 14;
	case '<':
	case '>':
	case TOK_LESSEQ:
	case TOK_GRTREQ:	return 16;
	case '+':
	case '-':			return 18;
	case '*':
	case '/':
	case '%':			return 20;
	case '.':			return 22;
	}

	return 0;
}

bool VDScriptInterpreter::ExprOpIsRightAssoc(int op) {
	return op == '=';
}

void VDScriptInterpreter::ParseExpression() {
	int depth = 0;
	int t;
	bool need_value = true;

	for(;;) {
		if (need_value) {
			ParseValue();
			need_value = false;
		}

		t = Token();

		if (!t || t==')' || t==']' || t==',' || t==';') {
			TokenUnput(t);
			break;
		}

		VDScriptValue& v = mStack.back();

		if (t=='.') {			// object indirection operator (object -> member)
			ConvertToRvalue();

			if (!v.isObject()) {
				SCRIPT_ERROR(TYPE_OBJECT_REQUIRED);
			}

			if (Token() != TOK_IDENT)
				SCRIPT_ERROR(OBJECT_MEMBER_NAME_REQUIRED);

			try {
				VDScriptValue v2 = LookupObjectMember(v.asObjectDef(), v.asObjectPtr(), szIdent);

				if (v2.isVoid()) {
					mErrorObject = v;
					SCRIPT_ERROR(MEMBER_NOT_FOUND);
				}

				v = v2;
			} catch(const VDScriptError&) {
				mErrorObject = v;
				throw;
			}

		} else if (t == '[') {	// array indexing operator (object, value -> value)
			// Reduce lvalues to rvalues

			ConvertToRvalue();

			if (!v.isObject())
				SCRIPT_ERROR(TYPE_OBJECT_REQUIRED);

			ParseExpression();
			InvokeMethod((mStack.end() - 2)->asObjectDef(), "[]", 1);

			VDASSERT(mStack.size() >= 2);
			mStack.erase(mStack.end() - 2);

			if (Token() != ']')
				SCRIPT_ERROR(CLOSEBRACKET_EXPECTED);
		} else if (t == '(') {	// function indirection operator (method -> value)
			ConvertToRvalue();	// can happen if a method is assigned

			const VDScriptValue fcall(mStack.back());
			mStack.pop_back();

			mStack.push_back(VDScriptValue(fcall.u.method.p, fcall.thisPtr));

			int pcount = 0;

			t = Token();
			if (t != ')') {
				TokenUnput(t);

				for(;;) {
					ParseExpression();
					++pcount;
					
					t = Token();

					if (t==')')
						break;
					else if (t!=',')
						SCRIPT_ERROR(FUNCCALLEND_EXPECTED);
				}
			}

			InvokeMethod(fcall.u.method.pfn, pcount);

			VDASSERT(mStack.size() >= 2);
			mStack.erase(mStack.end() - 2);
		} else {
			int prec = ExprOpPrecedence(t) + ExprOpIsRightAssoc(t);

			while(depth > 0 && ExprOpPrecedence(mOpStack.back()) >= prec) {
				--depth;
				Reduce();
			}

			mOpStack.push_back(t);
			++depth;

			need_value = true;
		}
	}

	// reduce until no ops are left
	while(depth-- > 0)
		Reduce();
}

void VDScriptInterpreter::Reduce() {
	const int op = mOpStack.back();
	mOpStack.pop_back();

	switch(op) {
	case '=':
		{
			VDScriptValue& v = mStack[mStack.size() - 2];

			if (!v.isVarLV())
				SCRIPT_ERROR(TYPE_OBJECT_REQUIRED);

			ConvertToRvalue();

			v.asVarLV()->v = mStack.back();
			mStack.pop_back();
		}
		break;
	case TOK_OR:		InvokeMethod(&obj_Sylia, "||", 2); break;
	case TOK_AND:		InvokeMethod(&obj_Sylia, "&&", 2); break;
	case '|':			InvokeMethod(&obj_Sylia, "|", 2); break;
	case '^':			InvokeMethod(&obj_Sylia, "^", 2); break;
	case '&':			InvokeMethod(&obj_Sylia, "&", 2); break;
	case TOK_EQUALS:	InvokeMethod(&obj_Sylia, "==", 2); break;
	case TOK_NOTEQ:		InvokeMethod(&obj_Sylia, "!=", 2); break;
	case '<':			InvokeMethod(&obj_Sylia, "<", 2); break;
	case '>':			InvokeMethod(&obj_Sylia, ">", 2); break;
	case TOK_LESSEQ:	InvokeMethod(&obj_Sylia, "<=", 2); break;
	case TOK_GRTREQ:	InvokeMethod(&obj_Sylia, ">=", 2); break;
	case '+':			InvokeMethod(&obj_Sylia, "+", 2); break;
	case '-':			InvokeMethod(&obj_Sylia, "-", 2); break;
	case '*':			InvokeMethod(&obj_Sylia, "*", 2); break;
	case '/':			InvokeMethod(&obj_Sylia, "/", 2); break;
	case '%':			InvokeMethod(&obj_Sylia, "%", 2); break;
	}
}

void VDScriptInterpreter::ParseValue() {
	int t = Token();

	if (t=='(') {
		t = Token();

		if (t == TOK_INT) {
			if (Token() != ')')
				SCRIPT_ERROR(CLOSEPARENS_EXPECTED);

			ParseExpression();

			VDScriptValue& v = mStack.back();

			if (v.isDouble())
				v = (int)v.asDouble();
			else if (v.isLong())
				v = (int)v.asLong();
			else if (!v.isInt())
				SCRIPT_ERROR(CANNOT_CAST);
		} else if (t == TOK_LONG) {
			if (Token() != ')')
				SCRIPT_ERROR(CLOSEPARENS_EXPECTED);

			ParseExpression();

			VDScriptValue& v = mStack.back();

			if (v.isDouble())
				v = (sint64)v.asDouble();
			else if (v.isInt())
				v = (sint64)v.asInt();
			else if (!v.isLong())
				SCRIPT_ERROR(CANNOT_CAST);
		} else if (t == TOK_DOUBLE) {
			if (Token() != ')')
				SCRIPT_ERROR(CLOSEPARENS_EXPECTED);

			ParseExpression();

			VDScriptValue& v = mStack.back();

			if (v.isInt())
				v = (double)v.asInt();
			else if (v.isLong())
				v = (double)v.asLong();
			else if (!v.isDouble())
				SCRIPT_ERROR(CANNOT_CAST);
		} else {
			TokenUnput(t);

			ParseExpression();

			if (Token() != ')')
				SCRIPT_ERROR(CLOSEPARENS_EXPECTED);
		}
	} else if (t==TOK_IDENT) {
		mStack.push_back(LookupRootVariable(szIdent));
	} else if (t == TOK_INTVAL)
		mStack.push_back(VDScriptValue(tokival));
	else if (t == TOK_LONGVAL)
		mStack.push_back(VDScriptValue(toklval));
	else if (t == TOK_DBLVAL)
		mStack.push_back(VDScriptValue(tokdval));
	else if (t == TOK_STRING)
		mStack.push_back(VDScriptValue(tokslit));
	else if (t=='!' || t=='~' || t=='-' || t=='+') {
		ParseValue();

		switch(t) {
		case '!':		InvokeMethod(&obj_Sylia, "!", 1); break;
		case '~':		InvokeMethod(&obj_Sylia, "~", 1); break;
		case '+':		InvokeMethod(&obj_Sylia, "+", 1); break;
		case '-':		InvokeMethod(&obj_Sylia, "-", 1); break;
			break;
		default:
			SCRIPT_ERROR(PARSE_ERROR);
		}
	} else if (t == TOK_TRUE)
		mStack.push_back(VDScriptValue(1));
	else if (t == TOK_FALSE)
		mStack.push_back(VDScriptValue(0));
	else
		SCRIPT_ERROR(PARSE_ERROR);
}


///////////////////////////////////////////////////////////////////////////
//
//	Variables...
//
///////////////////////////////////////////////////////////////////////////

void VDScriptInterpreter::InvokeMethod(const VDScriptObject *obj, const char *name, int argc) {
	if (obj->func_list) {
		const VDScriptFunctionDef *sfd = obj->func_list;

		while(sfd->arg_list) {
			if (sfd->name && !strcmp(sfd->name, name)) {
				InvokeMethod(sfd, argc);
				return;
			}

			++sfd;
		}
	}

	SCRIPT_ERROR(OVERLOADED_FUNCTION_NOT_FOUND);
}

void VDScriptInterpreter::InvokeMethod(const VDScriptFunctionDef *sfd, int pcount) {
	VDScriptValue *params = NULL;
	
	if (pcount)
		params = &mStack[mStack.size() - pcount];

	// convert params to rvalues
	for(int j=0; j<pcount; ++j) {
		VDScriptValue& parm = params[j];

		if (parm.isVarLV())
			parm = parm.asVarLV()->v;
	}

	mpCurrentInvocationMethod = sfd;
	mMethodArgumentCount = pcount;

	// If we were passed a function name, attempt to match our parameter
	// list to one of the overloaded function templates.
	//
	// 0 = void, v = value, i = int, . = varargs
	//
	// <return value><param1><param2>...
	const char *name = sfd->name;

	// Yes, goto's are usually considered gross... but I prefer
	// cleanly labeled goto's to excessive boolean variable usage.
	const VDScriptFunctionDef *sfd_best = NULL;

	int *best_scores = (int *)_alloca(sizeof(int) * (pcount + 1));
	int *current_scores = (int *)_alloca(sizeof(int) * (pcount + 1));
	int best_promotions = 0;
	bool ambiguous = false;

	while(sfd->arg_list && (sfd->name == name || !sfd->name)) {
		const char *s = sfd->arg_list+1;
		VDScriptValue *csv = params;
		bool better_conversion;
		int current_promotions = 0;

		// reset current scores to zero (default)
		memset(current_scores, 0, sizeof(int) * (pcount + 1));

		enum {
			kConversion = -2,
			kEllipsis = -3
		};

		for(int i=0; i<pcount; ++i) {
			const char c = *s++;

			if (!c)
				goto arglist_nomatch;

			switch(c) {
			case 'v':
				break;
			case 'i':
				if (csv->isLong() || csv->isDouble())
					current_scores[i] = kConversion;
				else if (!csv->isInt())
					goto arglist_nomatch;
				break;
			case 'l':
				if (csv->isDouble())
					current_scores[i] = kConversion;
				else if (csv->isInt())
					++current_promotions;
				else if (!csv->isLong())
					goto arglist_nomatch;
				break;
			case 'd':
				if (csv->isInt() || csv->isLong())
					++current_promotions;
				else if (!csv->isDouble())
					goto arglist_nomatch;
				break;
			case 's':
				if (!csv->isString()) goto arglist_nomatch;
				break;
			case '.':
				--s;			// repeat this character
				break;
			default:
				SCRIPT_ERROR(EXTERNAL_ERROR);
			}
			++csv;
		}

		// check if we have no parms left, or only an ellipsis
		if (*s == '.' && !s[1]) {
			current_scores[pcount] = kEllipsis;
		} else if (*s)
			goto arglist_nomatch;

		// do check for better match
		better_conversion = true;

		if (sfd_best) {
			better_conversion = false;
			for(int i=0; i<=pcount; ++i) {		// we check one additional parm, the ellipsis parm
				// reject if there is a worse conversion than the best match so far
				if (current_scores[i] < best_scores[i]) {
					goto arglist_nomatch;
				}

				// add +1 if there is a better match
				if (current_scores[i] > best_scores[i])
					better_conversion = true;
			}

			// all things being equal, choose the method with fewer promotions
			if (!better_conversion) {
				if (current_promotions < best_promotions)
					better_conversion = true;
				else if (current_promotions == best_promotions)
					ambiguous = true;
			}
		}

		if (better_conversion) {
			std::swap(current_scores, best_scores);
			sfd_best = sfd;
			best_promotions = current_promotions;
			ambiguous = false;
		}

arglist_nomatch:
		++sfd;
	}

	if (!sfd_best)
		SCRIPT_ERROR(OVERLOADED_FUNCTION_NOT_FOUND);
	else if (ambiguous)
		SCRIPT_ERROR(AMBIGUOUS_CALL);

	// Make sure there is room for the return value.
	int stackcount = pcount;

	if (!stackcount) {
		++stackcount;
		mStack.push_back(VDScriptValue());
	}

	// coerce arguments
	VDScriptValue *const argv = &*(mStack.end() - stackcount);
	const char *const argdesc = sfd_best->arg_list + 1;

	for(int i=0; i<pcount; ++i) {
		VDScriptValue& a = argv[i];

		if (argdesc[i] == '.')
			break;

		switch(argdesc[i]) {
		case 'i':
			if (a.isLong())
				a = VDScriptValue((int)a.asLong());
			else if (a.isDouble())
				a = VDScriptValue((int)a.asDouble());
			break;
		case 'l':
			if (a.isInt())
				a = VDScriptValue((sint64)a.asInt());
			else if (a.isDouble())
				a = VDScriptValue((sint64)a.asDouble());
			break;
		case 'd':
			if (a.isInt())
				a = VDScriptValue((double)a.asInt());
			else if (a.isLong())
				a = VDScriptValue((double)a.asLong());
			break;
		}
	}

	// invoke
	mpCurrentInvocationMethodOverload = sfd_best;
	sfd_best->func_ptr(this, argv, pcount);
	mStack.resize(mStack.size() + 1 - stackcount);
	if (sfd_best->arg_list[0] == '0')
		mStack.back() = VDScriptValue();
}

VDScriptValue VDScriptInterpreter::LookupRootVariable(char *szName) {
	VariableTableEntry *vte;

	if (vte = vartbl.Lookup(szName))
		return VDScriptValue(vte);

	if (!strcmp(szName, "Sylia"))
		return VDScriptValue(NULL, &obj_Sylia);

	const char *volatile _szName = szName;		// needed to fix exception handler, for some reason

	VDScriptValue ret;

	try {
		if (!lpRoothandler)
			SCRIPT_ERROR(VAR_NOT_FOUND);

		ret = lpRoothandler(this, szName, lpRoothandlerData);
	} catch(const VDScriptError& e) {
		if (e.err == VDScriptError::VAR_NOT_FOUND) {
			mErrorExtraToken = _szName;
			throw;
		}
	}

	return ret;
}

VDScriptValue VDScriptInterpreter::LookupObjectMember(const VDScriptObject *obj, void *lpVoid, char *szIdent) {
	for(; obj; obj = obj->pNextObject) {
		if (obj->func_list) {
			const VDScriptFunctionDef *pfd = obj->func_list;

			for(; pfd->func_ptr; ++pfd) {
				if (pfd->name && !strcmp(pfd->name, szIdent))
					return VDScriptValue(lpVoid, obj, pfd);
			}
		}

		if (obj->obj_list) {
			const VDScriptObjectDef *sod = obj->obj_list;

			while(sod->name) {
				if (!strcmp(sod->name, szIdent)) {
					VDScriptValue t(lpVoid, sod->obj);
					return t;
				}

				++sod;
			}
		}

		if (obj->Lookup) {
			VDScriptValue v(obj->Lookup(this, obj, lpVoid, szIdent));
			if (!v.isVoid())
				return v;
		}
	}

	return VDScriptValue();
}

void VDScriptInterpreter::ConvertToRvalue() {
	VDASSERT(!mStack.empty());

	VDScriptValue& val = mStack.back();

	if (val.isVarLV())
		val = val.asVarLV()->v;
}

///////////////////////////////////////////////////////////////////////////
//
//	Token-level parsing
//
///////////////////////////////////////////////////////////////////////////

bool VDScriptInterpreter::isExprFirstToken(int t) {
	return t==TOK_IDENT || t==TOK_STRING || t==TOK_INTVAL || isUnaryToken(t) || t=='(';
}

bool VDScriptInterpreter::isUnaryToken(int t) {
	return t=='+' || t=='-' || t=='!' || t=='~';
}

///////////////////////////////////////////////////////////////////////////
//
//	Character-level parsing
//
///////////////////////////////////////////////////////////////////////////

bool VDScriptInterpreter::isIdentFirstChar(char c) {
	return isalpha((unsigned char)c) || c=='_';
}

bool VDScriptInterpreter::isIdentNextChar(char c) {
	return isalnum((unsigned char)c) || c=='_';
}

void VDScriptInterpreter::TokenBegin(const char *s) {
	tokbase = tokstr = s;
	tokhold = 0;
}

void VDScriptInterpreter::TokenUnput(int t) {
	tokhold = t;
}

int VDScriptInterpreter::Token() {
	static char hexdig[]="0123456789ABCDEF";
	char *s,c;

	if (tokhold) {
		int t = tokhold;
		tokhold = 0;
		return t;
	}

	do {
		c=*tokstr++;
	} while(c && isspace((unsigned char)c));

	if (!c) {
		--tokstr;

		return 0;
	}

	// C++ style comment?

	if (c=='/')
		if (tokstr[0]=='/') {
			while(*tokstr) ++tokstr;

			return 0;		// C++ comment
		} else
			return '/';

	// string?

	if (c=='"') {
		const char *s = tokstr;
		char *t;
		long len_adjust=0;

		while((c=*tokstr++) && c!='"') {
			if (c=='\\') {
				c = *tokstr++;
				if (!c) SCRIPT_ERROR(PARSE_ERROR);
				else {
					if (c=='x') {
						if (!isxdigit((unsigned char)tokstr[0]) || !isxdigit((unsigned char)tokstr[1]))
							SCRIPT_ERROR(PARSE_ERROR);
						tokstr+=2;
						len_adjust += 2;
					}
					++len_adjust;
				}
			}
		}

		tokslit = strheap.Allocate(tokstr - s - 1 - len_adjust);
		t = *tokslit;
		while(s<tokstr-1) {
			int val;

			c = *s++;

			if (c=='\\')
				switch(c=*s++) {
				case 'a': *t++='\a'; break;
				case 'b': *t++='\b'; break;
				case 'f': *t++='\f'; break;
				case 'n': *t++='\n'; break;
				case 'r': *t++='\r'; break;
				case 't': *t++='\t'; break;
				case 'v': *t++='\v'; break;
				case 'x':
					val = strchr(hexdig,toupper(s[0]))-hexdig;
					val = (val<<4) | (strchr(hexdig,toupper(s[1]))-hexdig);
					*t++ = val;
					s += 2;
					break;
				default:
					*t++ = c;
				}
			else
				*t++ = c;
		}
		*t=0;

		if (!c) --tokstr;

		return TOK_STRING;
	}

	// unescaped string?
	if ((c=='u' || c=='U') && *tokstr == '"') {
		const char *s = ++tokstr;

		while((c=*tokstr++) && c != '"')
			;

		if (!c) {
			--tokstr;
			SCRIPT_ERROR(PARSE_ERROR);
		}

		size_t len = tokstr - s - 1;

		const VDStringA strA(VDTextWToU8(VDTextAToW(s, len)));

		len = strA.size();

		tokslit = strheap.Allocate(len);
		memcpy(*tokslit, strA.data(), len);
		(*tokslit)[len] = 0;

		return TOK_STRING;
	}

	// look for variable/keyword

	if (isIdentFirstChar(c)) {
		s = szIdent;

		*s++ = c;
		while(isIdentNextChar(c = *tokstr++)) {
			if (s>=szIdent + MAX_IDENT_CHARS)
				SCRIPT_ERROR(IDENT_TOO_LONG);

			*s++ = c;
		}

		--tokstr;
		*s=0;

		if (!strcmp(szIdent, "declare"))
			return TOK_DECLARE;
		else if (!strcmp(szIdent, "true"))
			return TOK_TRUE;
		else if (!strcmp(szIdent, "false"))
			return TOK_FALSE;
		else if (!strcmp(szIdent, "int"))
			return TOK_INT;
		else if (!strcmp(szIdent, "long"))
			return TOK_LONG;
		else if (!strcmp(szIdent, "double"))
			return TOK_DOUBLE;

		return TOK_IDENT;
	}

	// test for number: decimal (123), octal (0123), or hexadecimal (0x123)

	if (isdigit((unsigned char)c)) {
		sint64 v = 0;

		if (c=='0' && tokstr[0] == 'x') {

			// hex (base 16)
			++tokstr;

			while(isxdigit((unsigned char)(c = *tokstr++))) {
				v = v*16 + (strchr(hexdig, toupper(c))-hexdig);
			}

		} else if (c=='0' && isdigit((unsigned char)tokstr[0])) {
			// octal (base 8)
			while((c=*tokstr++)>='0' && c<='7')
				v = v*8 + (c-'0');
		} else {
			// check for float
			const char *s = tokstr;
			while(*s) {
				if (*s == '.' || *s == 'e' || *s == 'E') {
					// It's a float -- parse and return.

					--tokstr;
					tokdval = strtod(tokstr, (char **)&tokstr);
					return TOK_DBLVAL;
				}

				if (!isdigit((unsigned char)*s))
					break;
				++s;
			}

			// decimal
			v = (c-'0');
			while(isdigit((unsigned char)(c=*tokstr++)))
				v = v*10 + (c-'0');
		}
		--tokstr;

		if (v > 0x7FFFFFFF) {
			toklval = v;
			return TOK_LONGVAL;
		} else {
			tokival = (int)v;
			return TOK_INTVAL;
		}
	}

	// test for symbols:
	//
	//	charset:	+-*/<>=!&|^[]~;%(),
	//	solitary:	+-*/<>=!&|^[]~;%(),
	//	extra:		!= <= >= == && ||
	//
	//	the '/' is handled above for comment handling

	if (c=='!')
		if (tokstr[0] == '=') { ++tokstr; return TOK_NOTEQ;  } else return '!';

	if (c=='<')
		if (tokstr[0] == '=') { ++tokstr; return TOK_LESSEQ; } else return '<';

	if (c=='>')
		if (tokstr[0] == '=') { ++tokstr; return TOK_GRTREQ; } else return '>';

	if (c=='=')
		if (tokstr[0] == '=') { ++tokstr; return TOK_EQUALS; } else return '=';

	if (c=='&')
		if (tokstr[0] == '&') { ++tokstr; return TOK_AND;    } else return '&';

	if (c=='|')
		if (tokstr[0] == '|') { ++tokstr; return TOK_OR;     } else return '|';

	if (strchr("+-*^[]~;%(),.",c))
		return c;

	SCRIPT_ERROR(PARSE_ERROR);
}

void VDScriptInterpreter::GC() {
	strheap.BeginGC();
	vartbl.MarkStrings(strheap);
	strheap.EndGC();
}
