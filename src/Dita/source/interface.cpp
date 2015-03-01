//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2004 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include <stdafx.h>
#include <vd2/system/vdtypes.h>
#include <vd2/Dita/interface.h>
#include <vd2/Dita/basetypes.h>
#include <vd2/Dita/bytecode.h>
#include <vd2/Dita/resources.h>
#include <vd2/Dita/controls.h>

#include <vector>

using namespace nsVDDitaBytecode;

namespace {
	class VDUICreator {
	public:
		VDUICreator(IVDUIWindow *pParent);

		IVDUIWindow *Execute(const unsigned char *resdata);

	protected:
		void AddLastWindow();

		union Value {
			sint32 i;
			float f;
			const wchar_t *s;

			Value(sint32 _i) : i(_i) {}
			Value(float _f) : f(_f) {}
			Value(const wchar_t *_s) : s(_s) {}
		};

		std::vector<IVDUIWindow *>	mWinStack;
		std::vector<Value>			mValueStack;
		std::list<VDUIParameters>	mParameters;

		typedef std::list<std::vector<unsigned char> > tLinkExprs;
		tLinkExprs	mLinkExprs;

		IVDUIWindow *mpFirstWindow;
		IVDUIWindow *mpLastWindow;
	};
}

VDUICreator::VDUICreator(IVDUIWindow *pParent)
	: mpFirstWindow(NULL)
{
	mParameters.push_back(VDUIParameters());

	if (pParent)
		mWinStack.push_back(pParent);
}

IVDUIWindow *VDUICreator::Execute(const unsigned char *resdata) {
	while(const unsigned char c = *resdata++) {
		switch(c) {
		case kBC_Zero:
			mValueStack.push_back(0);
			break;
		case kBC_One:
			mValueStack.push_back(1);
			break;
		case kBC_Int8:
			mValueStack.push_back((sint8)*resdata++);
			break;
		case kBC_Int32:
		case kBC_Float32:
			mValueStack.push_back((sint32)(resdata[0] + (resdata[1]<<8) + (resdata[2]<<16) + (resdata[3]<<24)));
			resdata += 4;
			break;
		case kBC_String:
			mValueStack.push_back(VDLoadString(0, resdata[0] + (resdata[1]<<8), resdata[2] + (resdata[3]<<8)));
			resdata += 4;
			break;

		case kBC_StringShort:
			mValueStack.push_back(VDLoadString(0, 0xFFFF, resdata[0] + (resdata[1]<<8)));
			resdata += 2;
			break;

		case kBC_StringNull:
			mValueStack.push_back(L"");
			break;

		case kBC_InvokeTemplate:
			if (const unsigned char *tmp = VDLoadTemplate(0, resdata[0] + (resdata[1]<<8)))
				Execute(tmp);
			resdata += 2;
			break;

		case kBC_BeginChildren:
			mWinStack.push_back(mpLastWindow);
			break;

		case kBC_EndChildren:
			mWinStack.pop_back();
			break;

		case kBC_SetParameterB:
			{
				sint32 val = mValueStack.back().i;
				mValueStack.pop_back();
				uint32 id = mValueStack.back().i;
				mValueStack.pop_back();
				mParameters.back().SetB(id, val != 0);
			}
			break;

		case kBC_SetParameterI:
			{
				sint32 val = mValueStack.back().i;
				mValueStack.pop_back();
				uint32 id = mValueStack.back().i;
				mValueStack.pop_back();
				mParameters.back().SetI(id, val);
			}
			break;

		case kBC_SetParameterF:
			{
				float val = mValueStack.back().f;
				mValueStack.pop_back();
				uint32 id = mValueStack.back().i;
				mValueStack.pop_back();
				mParameters.back().SetF(id, val);
			}
			break;

		case kBC_PushParameters:
			mParameters.push_back(mParameters.back());
			break;

		case kBC_PopParameters:
			mParameters.pop_back();
			break;

		case kBC_SetLinkExpr:
			{
				const int len = resdata[0] + (resdata[1] << 8);
				resdata += 2;
				mLinkExprs.push_back(std::vector<unsigned char>());
				mLinkExprs.back().assign(resdata, resdata+len);
				resdata += len;
			}
			break;

		case kBC_AddListItem:
			vdpoly_cast<IVDUIList *>(mWinStack.back())->AddItem(mValueStack.back().s);
			mValueStack.pop_back();
			break;

		case kBC_AddPage:
			vdpoly_cast<IVDUIPageSet *>(mWinStack.back())->AddPage(mValueStack.back().i);
			mValueStack.pop_back();
			break;

		case kBC_SetColumn:{
			IVDUIGrid *const pGrid = vdpoly_cast<IVDUIGrid *>(mWinStack.back());
			if(VDINLINEASSERT(pGrid)){
				VDUIParameters& parms = mParameters.back();
				const int col = mValueStack.back().i;
				mValueStack.pop_back();

				const int minsize = parms.GetI(nsVDUI::kUIParam_MinW, -1);
				const int maxsize = parms.GetI(nsVDUI::kUIParam_MaxW, -1);
				const int affinity = parms.GetI(nsVDUI::kUIParam_Affinity, -1);
				pGrid->SetColumn(col, minsize, maxsize, affinity);
			}
			break;
		}

		case kBC_SetRow:{
			IVDUIGrid *const pGrid = vdpoly_cast<IVDUIGrid *>(mWinStack.back());
			if(VDINLINEASSERT(pGrid)){
				VDUIParameters& parms = mParameters.back();
				const int row = mValueStack.back().i;
				mValueStack.pop_back();

				const int minsize = parms.GetI(nsVDUI::kUIParam_MinH, -1);
				const int maxsize = parms.GetI(nsVDUI::kUIParam_MaxH, -1);
				const int affinity = parms.GetI(nsVDUI::kUIParam_Affinity, -1);
				pGrid->SetRow(row, minsize, maxsize, affinity);
			}
			break;
		}

		case kBC_NextRow:
			vdpoly_cast<IVDUIGrid *>(mWinStack.back())->NextRow();
			break;

		case kBC_CreateChildDialog:
			mpLastWindow = VDCreateDialogFromResource(mValueStack.back().i, mWinStack.empty() ? NULL : mWinStack.back());
			mValueStack.pop_back();
			mpLastWindow->SetID(mValueStack.back().i);
			mValueStack.pop_back();
			if (!mpFirstWindow)
				mpFirstWindow = mpLastWindow;
			break;

		case kBC_CreateCustom:
			{
				const wchar_t *caption = mValueStack.back().s;
				mValueStack.pop_back();
				const uint32 clsid = mValueStack.back().i;
				mValueStack.pop_back();
				const uint32 id = mValueStack.back().i;
				mValueStack.pop_back();

				IVDUIWindow *pWin = VDUICreateWindowClass(clsid);
				if (pWin) {
					mpLastWindow = pWin;
					mpLastWindow->SetCaption(caption);
					mpLastWindow->SetID(id);

					AddLastWindow();
				}
			}
			break;

		default:
			if (c >= 0x80) {
				switch(c) {
				case kBC_CreateLabel:		mpLastWindow = VDCreateUILabel(); break;
				case kBC_CreateButton:		mpLastWindow = VDCreateUIButton(); break;
				case kBC_CreateCheckBox:	mpLastWindow = VDCreateUICheckbox(); break;
				case kBC_CreateDialog:		mpLastWindow = VDCreateUIBaseWindow(); break;
				case kBC_CreateSplitter:	mpLastWindow = VDCreateUISplitter(); break;
				case kBC_CreateSet:			mpLastWindow = VDCreateUISet(); break;
				case kBC_CreatePageSet:		mpLastWindow = VDCreateUIPageSet(); break;
				case kBC_CreateGrid:		mpLastWindow = VDCreateUIGrid(); break;
				case kBC_CreateComboBox:	mpLastWindow = VDCreateUIComboBox(); break;
				case kBC_CreateListBox:		mpLastWindow = VDCreateUIListBox(); break;
				case kBC_CreateListView:	mpLastWindow = VDCreateUIListView(); break;
				case kBC_CreateTextEdit:	mpLastWindow = VDCreateUITextEdit(); break;
				case kBC_CreateTextArea:	mpLastWindow = VDCreateUITextArea(); break;
				case kBC_CreateGroup:		mpLastWindow = VDCreateUIGroup(); break;
				case kBC_CreateOption:		mpLastWindow = VDCreateUIOption(); break;
				case kBC_CreateTrackbar:	mpLastWindow = VDCreateUITrackbar(); break;
				case kBC_CreateHotkey:		mpLastWindow = VDCreateUIHotkey(); break;
				default:	VDNEVERHERE;
				}

				mpLastWindow->SetCaption(mValueStack.back().s);
				mValueStack.pop_back();
				const uint32 id = mValueStack.back().i;
				mpLastWindow->SetID(id);
				mValueStack.pop_back();

				AddLastWindow();
			} else {
				VDNEVERHERE;
			}
			break;
		}
	}

	vdpoly_cast<IVDUIBase *>(mpFirstWindow)->FinalizeDialog();

	return mpFirstWindow;
}

void VDUICreator::AddLastWindow() {
	IVDUIParameters& parms = mParameters.back();

	if (!mWinStack.empty()) {
		IVDUIWindow *pWin = mWinStack.back();

		if (IVDUIGrid *pGrid = vdpoly_cast<IVDUIGrid *>(pWin))
			pGrid->AddChild(mpLastWindow,
				parms.GetI(nsVDUI::kUIParam_Col, -1),
				parms.GetI(nsVDUI::kUIParam_Row, -1),
				parms.GetI(nsVDUI::kUIParam_ColSpan, 1),
				parms.GetI(nsVDUI::kUIParam_RowSpan, 1));
		else
			pWin->AddChild(mpLastWindow);
	}

	mpLastWindow->Create(&parms);

	IVDUIBase *pBase = mpLastWindow->GetBase();
	uint32 id = mpLastWindow->GetID();
	if (pBase && id) {
		int expr = parms.GetI(nsVDUI::kUIParam_EnableLinkExpr, -1);
		if (expr >= 0) {
			tLinkExprs::const_iterator it(mLinkExprs.begin());

			std::advance(it, expr);
			pBase->Link(id, nsVDUI::kLinkTarget_Enable, &(*it).front(), (*it).size());
		}

		expr = parms.GetI(nsVDUI::kUIParam_ValueLinkExpr, -1);
		if (expr >= 0) {
			tLinkExprs::const_iterator it(mLinkExprs.begin());

			std::advance(it, expr);
			pBase->Link(id, nsVDUI::kLinkTarget_Value, &(*it).front(), (*it).size());
		}
	}

	if (!mpFirstWindow)
		mpFirstWindow = mpLastWindow;
}


IVDUIWindow *VDCreateDialogFromResource(int dialogID, IVDUIWindow *pParent) {
	VDUICreator ctx(pParent);

	const unsigned char *p = VDLoadDialog(0, dialogID);

	if (p)
		return ctx.Execute(p);

	return NULL;
}




uint32 VDUIExecuteRuntimeExpression(const uint8 *expr, IVDUIWindow *const *pSrcWindows) {
	std::vector<sint32> stack;

	while(uint8 c = *expr++) {
		switch(c) {
		case kBCE_Zero:
			stack.push_back(0);
			break;
		case kBCE_One:
			stack.push_back(1);
			break;
		case kBCE_Int8:
			stack.push_back((sint8)*expr++);
			break;
		case kBCE_Int32:
			stack.push_back(expr[0] + ((sint32)expr[1] << 8) + ((sint32)expr[2] << 16) + ((sint32)expr[3] << 24));
			expr += 4;
			break;

		case kBCE_OpNegate:
			stack.back() = -stack.back();
			break;

		case kBCE_OpNot:
			stack.back() = !stack.back();
			break;

		case kBCE_OpMul:
			*(stack.end() - 2) *= stack.back();
			stack.pop_back();
			break;

		case kBCE_OpDiv:
			*(stack.end() - 2) /= stack.back();
			stack.pop_back();
			break;

		case kBCE_OpAdd:
			*(stack.end() - 2) += stack.back();
			stack.pop_back();
			break;

		case kBCE_OpSub:
			*(stack.end() - 2) -= stack.back();
			stack.pop_back();
			break;

		case kBCE_OpEQ:
			*(stack.end() - 2) = *(stack.end() - 2) == stack.back();
			stack.pop_back();
			break;

		case kBCE_OpNE:
			*(stack.end() - 2) = *(stack.end() - 2) != stack.back();
			stack.pop_back();
			break;

		case kBCE_OpLT:
			*(stack.end() - 2) = *(stack.end() - 2) < stack.back();
			stack.pop_back();
			break;

		case kBCE_OpLE:
			*(stack.end() - 2) = *(stack.end() - 2) <= stack.back();
			stack.pop_back();
			break;

		case kBCE_OpGT:
			*(stack.end() - 2) = *(stack.end() - 2) > stack.back();
			stack.pop_back();
			break;

		case kBCE_OpGE:
			*(stack.end() - 2) = *(stack.end() - 2) >= stack.back();
			stack.pop_back();
			break;

		case kBCE_OpLogicalAnd:
			*(stack.end() - 2) = *(stack.end() - 2) && stack.back();
			stack.pop_back();
			break;

		case kBCE_OpLogicalOr:
			*(stack.end() - 2) = *(stack.end() - 2) || stack.back();
			stack.pop_back();
			break;

		case kBCE_GetValue:
			{
				IVDUIWindow *pWin = pSrcWindows[*expr++];

				stack.push_back(pWin ? pWin->GetValue() : 0);
			}
			break;
		}
	}

	VDASSERT(stack.size() == 1);

	return stack.back();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace {
	struct VDUIWindowClassNode {
		uint32 mClassID;
		IVDUIWindow *(*mpCreator)();
	};

	std::vector<VDUIWindowClassNode> g_VDUIWindowClassList;
}

void VDUIRegisterWindowClass(uint32 classID, IVDUIWindow *(*pCreator)()) {
	VDUIWindowClassNode node;
	node.mClassID = classID;
	node.mpCreator = pCreator;
	g_VDUIWindowClassList.push_back(node);
}

IVDUIWindow *VDUICreateWindowClass(uint32 classID) {
	std::vector<VDUIWindowClassNode>::const_iterator it(g_VDUIWindowClassList.begin()), itEnd(g_VDUIWindowClassList.end());

	for(; it!=itEnd; ++it) {
		const VDUIWindowClassNode& node = *it;

		if (node.mClassID == classID)
			return node.mpCreator();
	}

	return NULL;
}
