#ifndef f_VD2_DITA_BYTECODE_H
#define f_VD2_DITA_BYTECODE_H

namespace nsVDDitaBytecode {
	enum {
		kBC_End,
		kBC_Zero,				// push integer 0 onto stack
		kBC_One,				// push integer 1 onto stack
		kBC_Int8,				// push signed 8-bit integer onto stack
		kBC_Int32,				// push full 32-bit integer onto stack
		kBC_Float32,			// push full 32-bit float onto stack
		kBC_String,				// push string from string table onto stack
		kBC_StringShort,		// push string from string table -1 onto stack
		kBC_StringNull,			// push null string onto stack

		kBC_InvokeTemplate,

		kBC_BeginChildren,
		kBC_EndChildren,

		kBC_PushParameters,
		kBC_PopParameters,
		kBC_SetParameterB,
		kBC_SetParameterI,
		kBC_SetParameterF,

		kBC_SetLinkExpr,		// set link expression

		kBC_AddListItem,
		kBC_AddPage,
		kBC_SetRow,
		kBC_SetColumn,
		kBC_NextRow,

		kBC_CreateCustom = 0x7F,// create custom_window(id, clsid)

		kBC_CreateLabel	= 0x80,	// create label(id, string, maxwidth)
		kBC_CreateEdit,			// create edit(id, label, maxlen)
		kBC_CreateEditInt,		// create editInt(id, initial, min, max)
		kBC_CreateButton,		// create button(id, label)
		kBC_CreateCheckBox,		// create checkbox(id, label)
		kBC_CreateListBox,		// create listbox(id, minrows)
		kBC_CreateComboBox,		// create combobox(id, minrows)
		kBC_CreateListView,		// create listview(id, minrows)
		kBC_CreateTrackbar,		// create trackbar(id, minv, maxv)
		kBC_CreateFileControl,	// create filecontrol(id, maxlen)
		kBC_CreateOption,		// create optionset(id)
		kBC_CreateSet,			// create horizset(id)
		kBC_CreatePageSet,		// create pageset(id)
		kBC_CreateGroup,		// create groupset(id, label)
		kBC_CreateGrid,			// create grid(id, cols, rows, xpad, ypad, affinity)
		kBC_CreateDialog,		// setDialogInfo(minw, minh, aspect, title)
		kBC_CreateSplitter,
		kBC_CreateTextEdit,
		kBC_CreateTextArea,
		kBC_CreateChildDialog,
		kBC_CreateHotkey,

		kBC_Count
	};

	// expression bytecode
	enum {
		kBCE_End			= 0,
		kBCE_Zero,
		kBCE_One,
		kBCE_Int8,
		kBCE_Int32,

		kBCE_OpNegate		= 0x40,
		kBCE_OpNot,
		kBCE_OpMul,
		kBCE_OpDiv,
		kBCE_OpAdd,
		kBCE_OpSub,
		kBCE_OpEQ,
		kBCE_OpNE,
		kBCE_OpLT,
		kBCE_OpLE,
		kBCE_OpGT,
		kBCE_OpGE,
		kBCE_OpLogicalAnd,
		kBCE_OpLogicalOr,

		kBCE_GetValue		= 0x80
	};
};

#endif
