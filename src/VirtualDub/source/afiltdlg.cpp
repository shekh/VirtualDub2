//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2003 Avery Lee
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

#include <vd2/system/thread.h>
#include <vd2/system/refcount.h>
#include <vd2/system/error.h>
#include <vd2/system/registry.h>
#include <vd2/system/fraction.h>
#include <vd2/VDLib/Dialog.h>
#include "resource.h"
#include "AudioFilterSystem.h"
#include "FilterGraph.h"
#include "plugins.h"
#include "gui.h"
#include "PositionControl.h"
#include <vd2/plugin/vdplugin.h>
#include <vd2/plugin/vdaudiofilt.h>

extern const char g_szError[];
extern const char g_szWarning[];

class AudioSource;

#ifdef VD_COMPILER_MSVC
	#pragma warning(disable: 4355) //warning C4355: 'this' : used in base member initializer list
#endif

///////////////////////////////////////////////////////////////////////////

namespace {
	class AudioFilterData : public vdrefcounted<IVDRefCount> {
	public:
		VDStringW			mName;
		VDPluginConfig		mConfigBlock;
		bool				mbHasConfigDialog;
	};
}

///////////////////////////////////////////////////////////////////////////

class IVDDialogAddAudioFilterCallbackW32 {
public:
	virtual void FilterSelected() = 0;
	virtual void FilterDialogClosed() = 0;
};

class VDDialogAddAudioFilterW32 : public VDDialogFrameW32 {
public:
	VDDialogAddAudioFilterW32(IVDDialogAddAudioFilterCallbackW32 *pParent);

	const VDPluginDescription *GetFilter() const { return mpSelectedFilter; }

protected:
	INT_PTR DlgProc(UINT message, WPARAM wParam, LPARAM lParam);

	bool OnLoaded();
	void OnDestroy();
	void UpdateDescription();

	std::vector<VDPluginDescription *> mAudioFilters;
	const VDPluginDescription *mpSelectedFilter;

	IVDDialogAddAudioFilterCallbackW32 *mpParent;

	ModelessDlgNode		mDlgNode;
};

VDDialogAddAudioFilterW32::VDDialogAddAudioFilterW32(IVDDialogAddAudioFilterCallbackW32 *pParent)
	: VDDialogFrameW32(IDD_AF_LIST)
	, mpSelectedFilter(NULL)
	, mpParent(pParent)
{
	VDEnumeratePluginDescriptions(mAudioFilters, kVDXPluginType_Audio);
}

INT_PTR VDDialogAddAudioFilterW32::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDC_ADD:
				if (mpSelectedFilter)
					mpParent->FilterSelected();
				return TRUE;
			case IDC_FILTER_LIST:
				switch(HIWORD(wParam)) {
				case LBN_SELCHANGE:
					UpdateDescription();
					break;
				case LBN_DBLCLK:
					if (mpSelectedFilter)
						mpParent->FilterSelected();
					break;
				}
			}
			break;
	}

	return VDDialogFrameW32::DlgProc(msg, wParam, lParam);
}

bool VDDialogAddAudioFilterW32::OnLoaded() {
	VDSetDialogDefaultIcons(mhdlg);

	// reposition window
	RECT r, r2;
	GetWindowRect(GetParent(mhdlg), &r);
	GetWindowRect(mhdlg, &r2);
	r2.right -= r2.left;
	r2.bottom -= r2.top;
	r2.left = r.right;
	r2.top = r.top;
	r2.right += r2.left;
	r2.bottom += r2.top;

	if (r2.right <= GetSystemMetrics(SM_CXSCREEN) && r2.bottom <= GetSystemMetrics(SM_CYSCREEN))
		SetWindowPos(mhdlg, NULL, r2.left, r2.top, r2.right-r2.left, r2.bottom-r2.top, SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);

	// fill filter list

	HWND hwndList = GetDlgItem(mhdlg, IDC_FILTER_LIST);

	INT tabs[]={ 175 };
	SendMessage(hwndList, LB_SETTABSTOPS, 1, (LPARAM)tabs);

	for(std::vector<VDPluginDescription *>::const_iterator it(mAudioFilters.begin()), itEnd(mAudioFilters.end()); it!=itEnd; ++it) {
		const VDPluginDescription& b = **it;

		if (b.mName[0] != '*') {
			char buf[1024];

			sprintf(buf, "%ls\t%ls", b.mName.c_str(), b.mAuthor.c_str());
			int idx = SendMessage(hwndList, LB_ADDSTRING, 0, (LPARAM)buf);

			if (idx != LB_ERR)
				SendMessage(hwndList, LB_SETITEMDATA, idx, (LPARAM)&b);
		}
	}

	mDlgNode.hdlg	= mhdlg;
	mDlgNode.mhAccel = NULL;

	guiAddModelessDialog(&mDlgNode);

	SetFocus(GetDlgItem(mhdlg, IDC_FILTER_LIST));

	return true;
}

void VDDialogAddAudioFilterW32::OnDestroy() {
	mDlgNode.Remove();
	mpParent->FilterDialogClosed();
}

void VDDialogAddAudioFilterW32::UpdateDescription() {
	HWND hwndList = GetDlgItem(mhdlg, IDC_FILTER_LIST);

	int sel = SendMessage(hwndList, LB_GETCURSEL, 0, 0);

	mpSelectedFilter = NULL;
	if (sel >= 0)
		if (const VDPluginDescription *pb = (const VDPluginDescription *)SendMessage(hwndList, LB_GETITEMDATA, sel, 0)) {
			SetDlgItemText(mhdlg, IDC_FILTER_INFO, VDTextWToA(pb->mDescription).c_str());
			mpSelectedFilter = pb;
		}
}

///////////////////////////////////////////////////////////////////////////

class VDAudioFilterPreviewThread : public VDThread {
public:
	VDAudioFilterPreviewThread();
	~VDAudioFilterPreviewThread();

	bool Start(const VDAudioFilterGraph& graph, sint64 us);
	void Stop();
	void Seek(sint64 us);

	sint64 GetPosition() const { return mPosition; }
	sint64 GetLength() const { return mLength; }

protected:
	void ThreadRun();

	VDScheduler					mFilterScheduler;
	VDAudioFilterSystem			mFilterSys;
	VDSignal					msigIdle;
	VDSignal					msigResumed;
	volatile bool				mbRequestExit;
	volatile bool				mbSuspend;
	volatile bool				mbResume;

	volatile sint64		mPosition;
	sint64				mLength;
};

VDAudioFilterPreviewThread::VDAudioFilterPreviewThread()
	: mbRequestExit(false)
	, mbSuspend(false)
	, mbResume(false)
{
	mFilterScheduler.setSignal(&msigIdle);
	mFilterSys.SetScheduler(&mFilterScheduler);
}

VDAudioFilterPreviewThread::~VDAudioFilterPreviewThread() {
}

bool VDAudioFilterPreviewThread::Start(const VDAudioFilterGraph& graph, sint64 us) {
	Stop();

	// copy the graph and replace Output nodes with Preview nodes

	VDAudioFilterGraph graph2(graph);

	VDAudioFilterGraph::FilterList::iterator it(graph2.mFilters.begin()), itEnd(graph2.mFilters.end());

	for(; it!=itEnd; ++it) {
		VDAudioFilterGraph::FilterEntry& filt = *it;

		if (filt.mFilterName == L"output")
			filt.mFilterName = L"*playback";
	}

	{
		std::vector<IVDAudioFilterInstance *> filterPtrs;
		mFilterSys.LoadFromGraph(graph2, filterPtrs);
	}
	mFilterSys.Start();
	mFilterSys.Seek(us);

	IVDAudioFilterInstance *pClock = mFilterSys.GetClock();
	mPosition = us;
	mLength = 0;
	if (pClock)
		mLength = pClock->GetLength();

	mbRequestExit=false;
	return ThreadStart();
}

void VDAudioFilterPreviewThread::Stop() {
	mbRequestExit = true;
	msigIdle.signal();
	ThreadWait();
}

void VDAudioFilterPreviewThread::Seek(sint64 us) {
	mbSuspend = true;
	mFilterSys.Seek(us);
	mbResume = true;
	msigResumed.wait();
}

void VDAudioFilterPreviewThread::ThreadRun() {
	IVDAudioFilterInstance *pClock = mFilterSys.GetClock();

	while(!mbRequestExit) {
		if (mbResume) {
			mbResume = false;
			mbSuspend = false;
			msigResumed.signal();
		}

		if (!mFilterScheduler.Run()) {
			if (mbSuspend) {
				msigIdle.wait();
			} else {
				VDDEBUG("AudioFilterPreview: Audio filter graph has halted.\n");
				mFilterScheduler.DumpStatus();
//				break;
				Sleep(1000);
			}
		}

		if (pClock)
			mPosition = pClock->GetPosition();
	}
	mFilterSys.Stop();
}

///////////////////////////////////////////////////////////////////////////

class VDDialogAudioFiltersW32 : public VDDialogFrameW32, public IVDDialogAddAudioFilterCallbackW32, public IVDFilterGraphControlCallback {
public:
	VDDialogAudioFiltersW32(VDAudioFilterGraph& afg, AudioSource *pAS);

protected:
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
	bool OnLoaded();
	void OnSize();

	void SaveDialogSettings();
	void FilterSelected() {
		if (const VDPluginDescription *pDesc = mAddDialog.GetFilter()) {
			const VDAudioFilterDefinition *pDef = reinterpret_cast<const VDAudioFilterDefinition *>(pDesc->mpInfo->mpTypeSpecificInfo);
			vdrefptr<AudioFilterData> afd(new_nothrow AudioFilterData);

			if (afd) {
				afd->mName				= pDesc->mName;
				afd->mbHasConfigDialog	= 0 != (pDef->mFlags & kVFAF_HasConfig);

				mpGraphControl->AddFilter(pDesc->mName.c_str(), pDef->mInputPins, pDef->mOutputPins, false, afd);
			}
		}
	}

	void FilterDialogClosed() {
		EnableWindow(GetDlgItem(mhdlg, IDC_ADD), TRUE);
	}

	void SelectionChanged(IVDRefCount *pInstance);
	bool RequeryFormats();

	bool Configure(VDGUIHandle hParent, IVDRefCount *pInstance) {
		try {
			AudioFilterData *pd = static_cast<AudioFilterData *>(pInstance);
			VDScheduler sched;
			VDAudioFilterSystem afs;

			afs.SetScheduler(&sched);

			VDPluginDescription *pDesc = VDGetPluginDescription(pd->mName.c_str(), kVDXPluginType_Audio);

			if (!pDesc)
				throw MyError("Audio filter \"%s\" is not loaded.", VDTextWToA(pd->mName).c_str());

			IVDAudioFilterInstance *pInst = afs.Create(pDesc);

			if (pInst) {
				pInst->DeserializeConfig(pd->mConfigBlock);
				pInst->Configure(hParent);
				pInst->SerializeConfig(pd->mConfigBlock);
			}

		} catch(const MyError& e) {
			e.post((HWND)hParent, g_szError);
		}
		return true;
	}

	void Preview();
	void LoadGraph(IVDFilterGraphControl *pSrcGraph, const VDAudioFilterGraph& graph);
	void SaveGraph(VDAudioFilterGraph& graph, IVDFilterGraphControl *pSrcGraph);

	IVDFilterGraphControl *mpGraphControl;

	VDAudioFilterPreviewThread	mPreview;
	VDDialogAddAudioFilterW32	mAddDialog;
	VDAudioFilterGraph&			mGraph;
	AudioSource					*mpAudio;
	IVDPositionControl			*mpPosition;
	bool						mbPreviewActive;
	bool						mbPreviewWasActive;

	VDDialogResizerW32			mResizer;
};

VDDialogAudioFiltersW32::VDDialogAudioFiltersW32(VDAudioFilterGraph& afg, AudioSource *pAS)
	: VDDialogFrameW32(IDD_AF_SETUP)
	, mpGraphControl(NULL)
	, mAddDialog(this)
	, mGraph(afg)
	, mpAudio(pAS)
	, mpPosition(NULL)
	, mbPreviewActive(false)
	, mbPreviewWasActive(false)
{
}

INT_PTR VDDialogAudioFiltersW32::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDC_ADD:
				if (!mAddDialog.IsCreated()) {
					if (mAddDialog.Create((VDGUIHandle)mhdlg))
						EnableWindow((HWND)lParam, FALSE);
				}

				return TRUE;
			case IDC_DELETE:
				mpGraphControl->DeleteSelection();
				return TRUE;
			case IDC_CONFIGURE:
				mpGraphControl->ConfigureSelection();
				return TRUE;
			case IDC_CLEAR:
				if (MessageBox(mhdlg, "Clear filter graph?", g_szWarning, MB_ICONEXCLAMATION|MB_OKCANCEL)==IDOK){
					mpGraphControl->SetFilterGraph(std::vector<VDFilterGraphNode>(), std::vector<VDFilterGraphConnection>());
				}
				return TRUE;
			case IDC_TEST:
				Preview();
				return TRUE;
			case IDC_ARRANGE:
				mpGraphControl->Arrange();
				return TRUE;

			case IDC_AUTOARRANGE:
				if (HIWORD(wParam) == BN_CLICKED)
					mpGraphControl->EnableAutoArrange(BST_CHECKED == (3&SendMessage((HWND)lParam, BM_GETSTATE, 0, 0)));
				return TRUE;

			case IDC_AUTOCONNECT:
				if (HIWORD(wParam) == BN_CLICKED)
					mpGraphControl->EnableAutoConnect(BST_CHECKED == (3&SendMessage((HWND)lParam, BM_GETSTATE, 0, 0)));
				return TRUE;

			case IDOK:
				mPreview.Stop();
				SaveGraph(mGraph, mpGraphControl);
				SaveDialogSettings();
				End(TRUE);
				return TRUE;
			case IDCANCEL:
				mPreview.Stop();
				End(FALSE);
				return TRUE;
			}
			break;

		case WM_TIMER:
			if (mPreview.isThreadActive())
				mpPosition->SetPosition(mPreview.GetPosition() / 1000);
			else
				mbPreviewActive = false;
			break;

		case WM_NOTIFY:
			{
				const NMHDR& hdr = *(const NMHDR *)lParam;

				if (GetWindowLong(hdr.hwndFrom, GWL_ID) == IDC_POSITION) {
					switch(hdr.code) {
					case PCN_BEGINTRACK:
						mbPreviewWasActive = false;
						if (mPreview.isThreadActive()) {
							mbPreviewWasActive = true;
							mPreview.Stop();
						}
						break;
					case PCN_ENDTRACK:
						if (mbPreviewWasActive) {
							VDAudioFilterGraph graph;
							SaveGraph(graph, mpGraphControl);
							mPreview.Start(graph, mpPosition->GetPosition() * 1000);
							mbPreviewActive = true;
						}
						break;
					case PCN_THUMBPOSITION:
					case PCN_THUMBPOSITIONPREV:
					case PCN_THUMBPOSITIONNEXT:
						if (mbPreviewActive)
							mPreview.Seek(mpPosition->GetPosition() * 1000);
						break;
					}
				}
			}
			break;
	}

	return VDDialogFrameW32::DlgProc(msg, wParam, lParam);
}

void VDDialogAudioFiltersW32::Preview() {
	try {
		if (mbPreviewActive) {
			mPreview.Stop();
			mbPreviewActive = false;
		} else {
			VDAudioFilterGraph graph;
			SaveGraph(graph, mpGraphControl);
			mPreview.Start(graph, mpPosition->GetPosition() * (sint64)1000);
			mpPosition->SetRange(0, mPreview.GetLength() / 1000);
			mbPreviewActive = true;
		}
	} catch(const MyError& e) {
		e.post(mhdlg, g_szError);
	}
}

void VDDialogAudioFiltersW32::LoadGraph(IVDFilterGraphControl *pSrcGraph, const VDAudioFilterGraph& graph) {
	std::vector<VDFilterGraphNode> nodes;
	std::vector<VDFilterGraphConnection> connections;

	// convert filters

	{
		for(std::list<VDAudioFilterGraph::FilterEntry>::const_iterator it(graph.mFilters.begin()), itEnd(graph.mFilters.end()); it!=itEnd; ++it) {
			const VDAudioFilterGraph::FilterEntry& f = *it;

			nodes.push_back(VDFilterGraphNode());
			VDFilterGraphNode& node = nodes.back();

			node.name		= f.mFilterName.c_str();
			node.inputs		= f.mInputPins;
			node.outputs	= f.mOutputPins;

			AudioFilterData *pfd = new_nothrow AudioFilterData;

			if (pfd) {
				pfd->mName			= f.mFilterName;
				pfd->mConfigBlock	= f.mConfig;
			}

			node.pInstance	= pfd;
		}
	}

	// convert connections

	{
		for(std::vector<VDAudioFilterGraph::FilterConnection>::const_iterator it(graph.mConnections.begin()), itEnd(graph.mConnections.end()); it!=itEnd; ++it) {
			const VDAudioFilterGraph::FilterConnection& conn = *it;

			VDFilterGraphConnection c;

			c.srcfilt = conn.filt;
			c.srcpin = conn.pin;

			connections.push_back(c);
		}
	}
	pSrcGraph->SetFilterGraph(nodes, connections);
}

void VDDialogAudioFiltersW32::SaveGraph(VDAudioFilterGraph& graph, IVDFilterGraphControl *pSrcGraph) {
	std::vector<VDFilterGraphNode> nodes;
	std::vector<VDFilterGraphConnection> connections;

	pSrcGraph->GetFilterGraph(nodes, connections);

	graph.mConnections.clear();
	graph.mFilters.clear();

	const int nFilters = nodes.size();
	int i;

	std::vector<IVDAudioFilterInstance *> filters(nFilters);

	for(i=0; i<nFilters; ++i) {
		const VDFilterGraphNode& node = nodes[i];
		AudioFilterData *pd = static_cast<AudioFilterData *>(node.pInstance);

		graph.mFilters.push_back(VDAudioFilterGraph::FilterEntry());
		VDAudioFilterGraph::FilterEntry& e = graph.mFilters.back();

		e.mFilterName	= node.name;
		e.mInputPins	= node.inputs;
		e.mOutputPins	= node.outputs;
		e.mConfig		= pd->mConfigBlock;
	}

	const VDFilterGraphConnection *pConn = connections.empty() ? NULL : &connections[0];

	for(i=0; i<nFilters; ++i) {
		const VDFilterGraphNode& node = nodes[i];

		for(int j=0; j<node.inputs; ++j) {
			VDAudioFilterGraph::FilterConnection c;

			c.filt	= pConn->srcfilt;
			c.pin	= pConn->srcpin;

			graph.mConnections.push_back(c);
			
			++pConn;
		}
	}
}

void VDDialogAudioFiltersW32::SelectionChanged(IVDRefCount *pInstance) {
	AudioFilterData *pd = static_cast<AudioFilterData *>(pInstance);

	EnableWindow(GetDlgItem(mhdlg, IDC_DELETE), pd != 0);
	EnableWindow(GetDlgItem(mhdlg, IDC_CONFIGURE), pd && pd->mbHasConfigDialog);
}

bool VDDialogAudioFiltersW32::RequeryFormats() {
	VDAudioFilterGraph graph;
	SaveGraph(graph, mpGraphControl);

	// this is wasteful just to get the instances, but oh well
	std::vector<VDFilterGraphNode> nodes;
	std::vector<VDFilterGraphConnection> connections;
	mpGraphControl->GetFilterGraph(nodes, connections);

	VDScheduler scheduler;
	VDAudioFilterSystem filterSys;
	filterSys.SetScheduler(&scheduler);

	VDAudioFilterGraph::FilterList::iterator it(graph.mFilters.begin()), itEnd(graph.mFilters.end());
	for(; it!=itEnd; ++it) {
		VDAudioFilterGraph::FilterEntry& filt = *it;

		if (filt.mFilterName == L"output")
			filt.mFilterName = L"*playback";
	}

	std::vector<IVDAudioFilterInstance *> filterPtrs;

	try {
		filterSys.LoadFromGraph(graph, filterPtrs);
		filterSys.Prepare();

		IVDAudioFilterInstance *pClock = filterSys.GetClock();

		if (pClock)
			mpPosition->SetRange(0, pClock->GetLength() / 1000);
	} catch(const MyError&) {
		// ignore errors
	}

	int nFilters = filterPtrs.size();
	bool bFound = false;

	for(int i=0; i<nFilters; ++i) {
		IVDAudioFilterInstance *filt = filterPtrs[i];

		const int nPins = filt->GetDefinition()->mOutputPins;

		for(int j=0; j<nPins; ++j) {
			const VDXWaveFormat *fmt = filt->GetOutputPinFormat(j);
			if (!fmt)
				continue;

			const int freq = fmt->mSamplingRate / 1000;
			const char chans = fmt->mChannels > 2 ? '+' : fmt->mChannels > 1 ? 's' : 'm';
			const int bits = fmt->mSampleBits;

			VDStringW label(VDswprintf(L"%d/%d%hc", 3, &freq, &bits, &chans));

			mpGraphControl->SetConnectionLabel(nodes[i].pInstance, j, label.c_str());
			bFound = true;
		}
	}

	return bFound;
}

bool VDDialogAudioFiltersW32::OnLoaded() {
	VDSetDialogDefaultIcons(mhdlg);

	mResizer.Init(mhdlg);
	mResizer.Add(IDOK, VDDialogResizerW32::kTR);
	mResizer.Add(IDCANCEL, VDDialogResizerW32::kTR);
	mResizer.Add(IDC_POSITION, VDDialogResizerW32::kBC);
	mResizer.Add(IDC_ADD, VDDialogResizerW32::kTR);
	mResizer.Add(IDC_DELETE, VDDialogResizerW32::kTR);
	mResizer.Add(IDC_CONFIGURE, VDDialogResizerW32::kTR);
	mResizer.Add(IDC_CLEAR, VDDialogResizerW32::kTR);
	mResizer.Add(IDC_TEST, VDDialogResizerW32::kTR);
	mResizer.Add(IDC_ARRANGE, VDDialogResizerW32::kTR);
	mResizer.Add(IDC_AUTOARRANGE, VDDialogResizerW32::kTR);
	mResizer.Add(IDC_AUTOCONNECT, VDDialogResizerW32::kTR);
	mResizer.Add(IDC_GRAPH, VDDialogResizerW32::kMC);

	mpPosition = VDGetIPositionControl((VDGUIHandle)GetDlgItem(mhdlg, IDC_POSITION));
	mpPosition->SetFrameRate(VDFraction(1000,1));

	mpGraphControl = VDGetIFilterGraphControl(GetDlgItem(mhdlg, IDC_GRAPH));
	mpGraphControl->SetCallback(this);

	LoadGraph(mpGraphControl, mGraph);

	VDRegistryAppKey regkey("Audio filters");
	bool bAutoArrange = regkey.getBool("Auto arrange", true);
	bool bAutoConnect = regkey.getBool("Auto connect", true);

	mpGraphControl->EnableAutoArrange(bAutoArrange);
	mpGraphControl->EnableAutoConnect(bAutoConnect);

	CheckDlgButton(mhdlg, IDC_AUTOARRANGE, bAutoArrange);
	CheckDlgButton(mhdlg, IDC_AUTOCONNECT, bAutoConnect);

	EnableWindow(GetDlgItem(mhdlg, IDC_TEST), mpAudio != 0);

	SelectionChanged(NULL);

	SetTimer(mhdlg, 1, 250, NULL);

	return VDDialogFrameW32::OnLoaded();
}

void VDDialogAudioFiltersW32::OnSize() {
	mResizer.Relayout();
}

void VDDialogAudioFiltersW32::SaveDialogSettings() {
	VDRegistryAppKey regkey("Audio filters");

	regkey.setBool("Auto arrange", 0!=IsDlgButtonChecked(mhdlg, IDC_AUTOARRANGE));
	regkey.setBool("Auto connect", 0!=IsDlgButtonChecked(mhdlg, IDC_AUTOCONNECT));
}

void VDDisplayAudioFilterDialog(VDGUIHandle hParent, VDAudioFilterGraph& graph, AudioSource *pAS) {
	VDDialogAudioFiltersW32 dlg(graph, pAS);

	dlg.ShowDialog(hParent);
}

///////////////////////////////////////////////////////////////////////////
