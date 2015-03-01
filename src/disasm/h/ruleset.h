#ifndef f_VD2_DISASM_RULESET_H
#define f_VD2_DISASM_RULESET_H

#include <vector>
#include <string>
#include <list>

#include <vd2/system/vdtypes.h>

struct rule {
	std::vector<std::pair<uint8, uint8> > match_stream;
	std::string result;
	std::string	rule_line;
	int argcount;
	bool is_return : 1;
	bool is_call : 1;
	bool is_jump : 1;
	bool is_jcc : 1;
	bool is_invalid : 1;
	bool is_66 : 1;
	bool is_67 : 1;
	bool is_f2 : 1;
	bool is_f3 : 1;
	bool is_imm8 : 1;
	bool is_imm16 : 1;
	bool is_imm32 : 1;

	rule()
		: argcount(0)
		, is_return(false)
		, is_call(false)
		, is_jump(false)
		, is_jcc(false)
		, is_invalid(false)
		, is_66(false)
		, is_67(false)
		, is_f2(false)
		, is_f3(false)
		, is_imm8(false)
		, is_imm16(false)
		, is_imm32(false)
	{
	}
};

struct ruleset {
	std::list<rule>		rules;
	std::string			name;
};

typedef std::list<ruleset>				tRuleSystem;

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
static const char kTarget_ext_r163264rexB = 3;

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

#endif
