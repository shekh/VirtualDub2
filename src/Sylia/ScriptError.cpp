#include "ScriptError.h"

namespace {
	static struct ErrorEnt {
		int e;
		char *s;
	} error_list[]={
		{ VDScriptError::PARSE_ERROR,					"parse error" },
		{ VDScriptError::SEMICOLON_EXPECTED,			"expected ';'" },
		{ VDScriptError::IDENTIFIER_EXPECTED,			"identifier expected" },
		{ VDScriptError::TYPE_INT_REQUIRED,				"integer type required" },
		{ VDScriptError::TYPE_ARRAY_REQUIRED,			"array type required" },
		{ VDScriptError::TYPE_FUNCTION_REQUIRED,		"function type required" },
		{ VDScriptError::TYPE_OBJECT_REQUIRED,			"object type required" },
		{ VDScriptError::OBJECT_MEMBER_NAME_REQUIRED,	"object member name expected" },
		{ VDScriptError::FUNCCALLEND_EXPECTED,			"expected ')'" },
		{ VDScriptError::TOO_MANY_PARAMS,				"function call parameter limit exceeded" },
		{ VDScriptError::DIVIDE_BY_ZERO,				"divide by zero" },
		{ VDScriptError::VAR_NOT_FOUND,					"variable not found" },
		{ VDScriptError::MEMBER_NOT_FOUND,				"member not found" },
		{ VDScriptError::OVERLOADED_FUNCTION_NOT_FOUND,	"overloaded function not found" },
		{ VDScriptError::IDENT_TOO_LONG,				"identifier length limit exceeded" },
		{ VDScriptError::OPERATOR_EXPECTED,				"expression operator expected" },
		{ VDScriptError::CLOSEPARENS_EXPECTED,			"expected ')'" },
		{ VDScriptError::CLOSEBRACKET_EXPECTED,			"expected ']'" },
		{ VDScriptError::OUT_OF_STRING_SPACE,			"out of string space" },
		{ VDScriptError::OUT_OF_MEMORY,					"out of memory" },
		{ VDScriptError::INTERNAL_ERROR,				"internal error" },
		{ VDScriptError::EXTERNAL_ERROR,				"error in external Sylia linkages" },
		{ VDScriptError::VAR_UNDEFINED,					"variable's value is undefined" },
		{ VDScriptError::FCALL_OUT_OF_RANGE,			"argument out of range" },
		{ VDScriptError::FCALL_INVALID_PTYPE,			"argument has wrong type" },
		{ VDScriptError::FCALL_UNKNOWN_STR,				"string argument not recognized" },
		{ VDScriptError::ARRAY_INDEX_OUT_OF_BOUNDS,		"array index out of bounds" },
		{ VDScriptError::NUMERIC_OVERFLOW,				"numeric overflow" },
		{ VDScriptError::STRING_NOT_AN_INTEGER_VALUE,	"string is not an integer number" },
		{ VDScriptError::STRING_NOT_A_REAL_VALUE,		"string is not an real number" },
		{ VDScriptError::ASSERTION_FAILED,				"assertion failed" },
		{ VDScriptError::AMBIGUOUS_CALL,				"ambiguous call to overloaded method" },
		{ VDScriptError::CANNOT_CAST,					"cannot cast to specified type" },
		{ 0, 0 },
	};
}

const char *VDScriptTranslateError(int e) {
	struct ErrorEnt *eeptr = error_list;

	while(eeptr->s) {
		if (eeptr->e == e)
			return eeptr->s;

		++eeptr;
	}

	return "unknown error";
}

