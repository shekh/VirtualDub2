#include <windows.h>
#include <vd2/system/VDString.h>

#include "ScriptInterpreter.h"
#include "ScriptValue.h"
#include "ScriptError.h"

namespace {
#define FUNC(name) void name(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc)

	FUNC(dprint) {
		char lbuf[12];

		while(argc--) {
			if (argv->isInt()) {
				wsprintf(lbuf, "%ld", argv->asInt());
				OutputDebugString(lbuf);
			} else if (argv->isString()) {
				OutputDebugString(*argv->asString());
			} else
				SCRIPT_ERROR(TYPE_INT_REQUIRED);

			++argv;
		}
	}

	FUNC(messagebox) {
		MessageBox(NULL, *argv[0].asString(), *argv[1].asString(), MB_OK);
	}

	FUNC(IntToString) {
		char buf[32];

		if ((unsigned)_snprintf(buf, sizeof buf, "%d", argv[0].asInt()) > 31)
			buf[0] = 0;

		argv[0] = isi->DupCString(buf);
	}

	FUNC(LongToString) {
		char buf[32];

		if ((unsigned)_snprintf(buf, sizeof buf, "%I64d", argv[0].asLong()) > 31)
			buf[0] = 0;

		argv[0] = isi->DupCString(buf);
	}

	FUNC(DoubleToString) {
		char buf[256];

		if ((unsigned)_snprintf(buf, sizeof buf, "%g", argv[0].asDouble()) > 255)
			buf[0] = 0;

		argv[0] = isi->DupCString(buf);
	}

	FUNC(StringToString) {
		// don't need to do anything
	}

	FUNC(Atoi) {
		char *s = *argv[0].asString();

		while(*s == ' ')
			++s;

		errno = 0;
		int v = (int)strtol(s, &s, 0);

		if (errno && v != 0)
			SCRIPT_ERROR(NUMERIC_OVERFLOW);

		while(*s == ' ')
			++s;

		if (*s)
			SCRIPT_ERROR(STRING_NOT_AN_INTEGER_VALUE);

		argv[0] = (int)v;
	}

	FUNC(Atol) {
		const char *s = *argv[0].asString();
		char dummy;
		sint64 val;

		int result = sscanf(s, " %I64d %c", &val, &dummy);

		if (result != 1)
			SCRIPT_ERROR(STRING_NOT_AN_INTEGER_VALUE);

		argv[0] = val;
	}

	FUNC(Atod) {
		char *s = *argv[0].asString();

		while(*s == ' ')
			++s;

		errno = 0;
		double v = strtod(s, &s);

		if (errno && v != 0)
			SCRIPT_ERROR(NUMERIC_OVERFLOW);

		while(*s == ' ')
			++s;

		if (*s)
			SCRIPT_ERROR(STRING_NOT_A_REAL_VALUE);

		argv[0] = v;
	}

	FUNC(TypeName) {
		switch(argv[0].type) {
			case VDScriptValue::T_VOID:		argv[0] = isi->DupCString("void"); break;
			case VDScriptValue::T_INT:		argv[0] = isi->DupCString("int"); break;
			case VDScriptValue::T_LONG:		argv[0] = isi->DupCString("long"); break;
			case VDScriptValue::T_DOUBLE:	argv[0] = isi->DupCString("double"); break;
			case VDScriptValue::T_STR:		argv[0] = isi->DupCString("string"); break;
			case VDScriptValue::T_OBJECT:	argv[0] = isi->DupCString("object"); break;
			case VDScriptValue::T_FNAME:	argv[0] = isi->DupCString("method"); break;
			case VDScriptValue::T_FUNCTION:	argv[0] = isi->DupCString("function"); break;
			case VDScriptValue::T_VARLV:	argv[0] = isi->DupCString("var"); break;
			default:						argv[0] = isi->DupCString("unknown"); break;
		}
	}

	FUNC(Assert) {
		if (!argv[0].asInt())
			SCRIPT_ERROR(ASSERTION_FAILED);
	}

	FUNC(AssertEqual_int) {
		int x = argv[0].asInt();
		int y = argv[1].asInt();

		if (x != y)
			SCRIPT_ERROR(ASSERTION_FAILED);
	}

	FUNC(AssertEqual_long) {
		sint64 x = argv[0].asLong();
		sint64 y = argv[1].asLong();

		if (x != y)
			SCRIPT_ERROR(ASSERTION_FAILED);
	}

	FUNC(AssertEqual_double) {
		double x = argv[0].asDouble();
		double y = argv[1].asDouble();

		if (x != y)
			SCRIPT_ERROR(ASSERTION_FAILED);
	}

	FUNC(TestOverload1) {argv[0] = 1;}
	FUNC(TestOverload2) {argv[0] = 2;}
	FUNC(TestOverload3) {argv[0] = 3;}
	FUNC(TestOverload4) {argv[0] = 4;}
	FUNC(TestOverload5) {argv[0] = 5;}
	FUNC(TestOverload6) {argv[0] = 6;}
	FUNC(TestOverload7) {argv[0] = 7;}
	FUNC(TestOverload8) {argv[0] = 8;}
	FUNC(TestOverload9) {argv[0] = 9;}
	FUNC(TestOverload10) {argv[0] = 10;}
	FUNC(TestOverload11) {argv[0] = 11;}
	FUNC(TestOverload12) {argv[0] = 12;}
	FUNC(TestOverload13) {argv[0] = 13;}
	FUNC(TestOverload14) {argv[0] = 14;}

	FUNC(add_int) {	argv[0] = argv[0].asInt() + argv[1].asInt(); }
	FUNC(add_long) { argv[0] = argv[0].asLong() + argv[1].asLong(); }
	FUNC(add_double) { argv[0] = argv[0].asDouble() + argv[1].asDouble(); }

	FUNC(add_string) {
		int l1 = strlen(*argv[0].asString());
		int l2 = strlen(*argv[1].asString());

		char **pp = isi->AllocTempString(l1+l2);

		memcpy(*pp, *argv[0].asString(), l1);
		memcpy(*pp + l1, *argv[1].asString(), l2);

		argv[0] = VDScriptValue(pp);
	}

	FUNC(add_string_int) {
		char buf[32];
		sprintf(buf, "%d", argv[1].asInt());
		int l1 = strlen(*argv[0].asString());
		int l2 = strlen(buf);

		char **pp = isi->AllocTempString(l1+l2);

		memcpy(*pp, *argv[0].asString(), l1);
		memcpy(*pp + l1, buf, l2);

		argv[0] = VDScriptValue(pp);
	}

	FUNC(add_string_long) {
		char buf[32];
		sprintf(buf, "%I64d", argv[1].asLong());
		int l1 = strlen(*argv[0].asString());
		int l2 = strlen(buf);

		char **pp = isi->AllocTempString(l1+l2);

		memcpy(*pp, *argv[0].asString(), l1);
		memcpy(*pp + l1, buf, l2);

		argv[0] = VDScriptValue(pp);
	}

	FUNC(add_string_double) {
		VDStringA tmp;
		tmp.sprintf("%s%g", *argv[0].asString(), argv[1].asDouble());

		argv[0] = isi->DupCString(tmp.c_str());
	}

	FUNC(upos_int) {}
	FUNC(upos_long) {}
	FUNC(upos_double) {}

	FUNC(sub_int) { argv[0] = argv[0].asInt() - argv[1].asInt(); }
	FUNC(sub_long) { argv[0] = argv[0].asLong() - argv[1].asLong(); }
	FUNC(sub_double) { argv[0] = argv[0].asDouble() - argv[1].asDouble(); }

	FUNC(uneg_int) { argv[0] = -argv[0].asInt(); }
	FUNC(uneg_long) { argv[0] = -argv[0].asLong(); }
	FUNC(uneg_double) { argv[0] = -argv[0].asDouble(); }

	FUNC(mul_int) { argv[0] = argv[0].asInt() * argv[1].asInt(); }
	FUNC(mul_long) { argv[0] = argv[0].asLong() * argv[1].asLong(); }
	FUNC(mul_double) { argv[0] = argv[0].asDouble() * argv[1].asDouble(); }

	FUNC(div_int) {
		if (!argv[1].asInt())
			SCRIPT_ERROR(DIVIDE_BY_ZERO);
		argv[0] = argv[0].asInt() / argv[1].asInt();
	}

	FUNC(div_long) {
		if (!argv[1].asLong())
			SCRIPT_ERROR(DIVIDE_BY_ZERO);
		argv[0] = argv[0].asLong() / argv[1].asLong();
	}

	FUNC(div_double) {
		if (argv[1].asDouble() == 0.0)
			SCRIPT_ERROR(DIVIDE_BY_ZERO);
		argv[0] = argv[0].asDouble() / argv[1].asDouble();
	}

	FUNC(mod_int) {
		if (!argv[1].asInt())
			SCRIPT_ERROR(DIVIDE_BY_ZERO);
		argv[0] = argv[0].asInt() % argv[1].asInt();
	}

	FUNC(mod_long) {
		if (!argv[1].asLong())
			SCRIPT_ERROR(DIVIDE_BY_ZERO);
		argv[0] = argv[0].asLong() % argv[1].asLong();
	}

	FUNC(and_int) { argv[0] = argv[0].asInt() & argv[1].asInt(); }
	FUNC(and_long) { argv[0] = argv[0].asLong() & argv[1].asLong(); }

	FUNC(or_int) { argv[0] = argv[0].asInt() | argv[1].asInt(); }
	FUNC(or_long) { argv[0] = argv[0].asLong() | argv[1].asLong(); }

	FUNC(xor_int) { argv[0] = argv[0].asInt() ^ argv[1].asInt(); }
	FUNC(xor_long) { argv[0] = argv[0].asLong() ^ argv[1].asLong(); }

	FUNC(lt_int) { argv[0] = argv[0].asInt() < argv[1].asInt(); }
	FUNC(lt_long) { argv[0] = argv[0].asLong() < argv[1].asLong(); }
	FUNC(lt_double) { argv[0] = argv[0].asDouble() < argv[1].asDouble(); }

	FUNC(gt_int) { argv[0] = argv[0].asInt() > argv[1].asInt(); }
	FUNC(gt_long) { argv[0] = argv[0].asLong() > argv[1].asLong(); }
	FUNC(gt_double) { argv[0] = argv[0].asDouble() > argv[1].asDouble(); }

	FUNC(le_int) { argv[0] = argv[0].asInt() <= argv[1].asInt(); }
	FUNC(le_long) { argv[0] = argv[0].asLong() <= argv[1].asLong(); }
	FUNC(le_double) { argv[0] = argv[0].asDouble() <= argv[1].asDouble(); }

	FUNC(ge_int) { argv[0] = argv[0].asInt() >= argv[1].asInt(); }
	FUNC(ge_long) { argv[0] = argv[0].asLong() >= argv[1].asLong(); }
	FUNC(ge_double) { argv[0] = argv[0].asDouble() >= argv[1].asDouble(); }

	FUNC(eq_int) { argv[0] = argv[0].asInt() == argv[1].asInt(); }
	FUNC(eq_long) { argv[0] = argv[0].asLong() == argv[1].asLong(); }
	FUNC(eq_double) { argv[0] = argv[0].asDouble() == argv[1].asDouble(); }
	FUNC(eq_string) { argv[0] = !strcmp(*argv[0].asString(), *argv[1].asString()); }

	FUNC(ne_int) { argv[0] = argv[0].asInt() != argv[1].asInt(); }
	FUNC(ne_long) { argv[0] = argv[0].asLong() != argv[1].asLong(); }
	FUNC(ne_double) { argv[0] = argv[0].asDouble() != argv[1].asDouble(); }
	FUNC(ne_string) { argv[0] = !!strcmp(*argv[0].asString(), *argv[1].asString()); }

	FUNC(land_int) { argv[0] = argv[0].asInt() && argv[1].asInt(); }
	FUNC(land_long) { argv[0] = argv[0].asLong() && argv[1].asLong(); }
	FUNC(land_double) { argv[0] = argv[0].asDouble() && argv[1].asDouble(); }

	FUNC(lor_int) { argv[0] = argv[0].asInt() || argv[1].asInt(); }
	FUNC(lor_long) { argv[0] = argv[0].asLong() || argv[1].asLong(); }
	FUNC(lor_double) { argv[0] = argv[0].asDouble() || argv[1].asDouble(); }

	FUNC(unot_int) { argv[0] = !argv[0].asInt(); }
	FUNC(unot_long) { argv[0] = !argv[0].asLong(); }
	FUNC(unot_double) { argv[0] = !argv[0].asDouble(); }

	FUNC(uinv_int) { argv[0] = ~argv[0].asInt(); }
	FUNC(uinv_long) { argv[0] = ~argv[0].asLong(); }
}

static const VDScriptFunctionDef objFL_Sylia[]={
	{ dprint,		"dprint", "0." },
	{ messagebox,	"MessageBox", "0ss" },
	{ IntToString,		"ToString", "si" },
	{ LongToString,		NULL, "sl" },
	{ DoubleToString,	NULL, "sd" },
	{ StringToString,	NULL, "ss" },
	{ Atoi,			"Atoi", "is" },
	{ Atol,			"Atol", "ls" },
	{ Atod,			"Atod", "ds" },
	{ add_int,		"+", "iii" },
	{ add_long,		NULL, "lll" },
	{ add_double,	NULL, "ddd" },
	{ upos_int,		NULL, "ii" },
	{ upos_long,	NULL, "ll" },
	{ upos_double,	NULL, "dd" },
	{ add_string,	NULL, "sss" },
	{ add_string_int,	NULL, "ssi" },
	{ add_string_long,	NULL, "ssl" },
	{ add_string_double, NULL, "ssd" },
	{ sub_int,		"-", "iii" },
	{ sub_long,		NULL, "lll" },
	{ sub_double,	NULL, "ddd" },
	{ uneg_int,		NULL, "ii" },
	{ uneg_long,	NULL, "ll" },
	{ uneg_double,	NULL, "dd" },
	{ mul_int,		"*", "iii" },
	{ mul_long,		NULL, "lll" },
	{ mul_double,	NULL, "ddd" },
	{ div_int,		"/", "iii" },
	{ div_long,		NULL, "lll" },
	{ div_double,	NULL, "ddd" },
	{ mod_int,		"%", "iii" },
	{ mod_long,		NULL, "lll" },
	{ and_int,		"&", "iii" },
	{ and_long,		NULL, "lll" },
	{ or_int,		"|", "iii" },
	{ or_long,		NULL, "lll" },
	{ xor_int,		"^", "iii" },
	{ xor_long,		NULL, "lll" },
	{ lt_int,		"<", "iii" },
	{ lt_long,		NULL, "ill" },
	{ lt_double,	NULL, "idd" },
	{ gt_int,		">", "iii" },
	{ gt_long,		NULL, "ill" },
	{ gt_double,	NULL, "idd" },
	{ le_int,		"<=", "iii" },
	{ le_long,		NULL, "ill" },
	{ le_double,	NULL, "idd" },
	{ ge_int,		">=", "iii" },
	{ ge_long,		NULL, "ill" },
	{ ge_double,	NULL, "idd" },
	{ eq_int,		"==", "iii" },
	{ eq_long,		NULL, "ill" },
	{ eq_double,	NULL, "idd" },
	{ eq_string,	NULL, "iss" },
	{ ne_int,		"!=", "iii" },
	{ ne_long,		NULL, "ill" },
	{ ne_double,	NULL, "idd" },
	{ ne_string,	NULL, "iss" },
	{ land_int,		"&&", "iii" },
	{ land_long,	NULL, "ill" },
	{ land_double,	NULL, "idd" },
	{ lor_int,		"||", "iii" },
	{ lor_long,		NULL, "ill" },
	{ lor_double,	NULL, "idd" },
	{ unot_int,		"!",  "ii" },
	{ unot_long,	NULL,  "ll" },
	{ uinv_int,		"~", "ii" },
	{ uinv_long,	NULL, "ll" },
	{ TypeName,		"TypeName", "s." },
	{ Assert,		"Assert", "0i" },
	{ AssertEqual_int,		"AssertEqual", "0ii" },
	{ AssertEqual_long,		"AssertEqual", "0ll" },
	{ AssertEqual_double,	"AssertEqual", "0dd" },
	{ TestOverload1, "TestOverloading", "i" },
	{ TestOverload2, NULL, "ii" },
	{ TestOverload3, NULL, "il" },
	{ TestOverload4, NULL, "id" },
	{ TestOverload5, NULL, "iii" },
	{ TestOverload6, NULL, "iil" },
	{ TestOverload7, NULL, "iid" },
	{ TestOverload8, NULL, "ili" },
	{ TestOverload9, NULL, "ill" },
	{ TestOverload10, NULL, "ild" },
	{ TestOverload11, NULL, "idi" },
	{ TestOverload12, NULL, "idl" },
	{ TestOverload13, NULL, "idd" },
	{ TestOverload14, NULL, "iii." },
	{ NULL }
};

VDScriptObject obj_Sylia={
	"VDScriptEngine",
	NULL,
	objFL_Sylia
};