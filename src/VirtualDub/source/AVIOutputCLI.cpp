//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2009 Avery Lee
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

#include "stdafx.h"

#include <windows.h>

#include <vd2/system/error.h>
#include <vd2/system/filesys.h>
#include <vd2/system/fraction.h>
#include <vd2/system/log.h>
#include <vd2/system/strutil.h>
#include <vd2/system/thread.h>
#include <vd2/system/w32assist.h>
#include <vd2/Riza/audiocodec.h>
#include <vd2/Dita/resources.h>

#include "AVIOutputWAV.h"
#include "AVIOutputRawAudio.h"
#include "AVIOutputRawVideo.h"
#include "AVIOutputCLI.h"

#ifdef _M_AMD64
	#define VD_LAUNCH_HELPER_NAMEA	"vdlaunch64.exe"
	#define VD_LAUNCH_HELPER_NAMEW	L"vdlaunch64.exe"
#else
	#define VD_LAUNCH_HELPER_NAMEA	"vdlaunch.exe"
	#define VD_LAUNCH_HELPER_NAMEW	L"vdlaunch.exe"
#endif

namespace {
	enum { kVDST_Dub = 1 };
};

//////////////////////////////////////////////////////////////////////

class VDCLIOutputSinkW32 : public VDThread {
	VDCLIOutputSinkW32(const VDCLIOutputSinkW32&);
public:
	VDCLIOutputSinkW32();
	~VDCLIOutputSinkW32();

	void SetLogPrefix(const wchar_t *prefix) { mLogPrefix = prefix; }

	void Init(HANDLE h);
	void Shutdown();
	void Read();

protected:
	void ThreadRun();
	void ClosePipe();
	void FlushBuffer();

	HANDLE mHandle;

	typedef vdfastvector<uint8> Buffer;
	Buffer mBuffer;
	VDAtomicInt mActive;
	VDStringW mLogPrefix;
	VDStringW mTempLine;
};

VDCLIOutputSinkW32::VDCLIOutputSinkW32()
	: mHandle(INVALID_HANDLE_VALUE)
	, mActive(false)
{
}

VDCLIOutputSinkW32::~VDCLIOutputSinkW32() {
	Shutdown();
}

void VDCLIOutputSinkW32::Init(HANDLE h) {
	mHandle = h;
	mActive = true;

	if (!ThreadStart())
		ClosePipe();
}

void VDCLIOutputSinkW32::Shutdown() {
	mActive = false;
	ThreadWait();

	if (!mBuffer.empty()) {
		if (mBuffer.back() != '\n')
			mBuffer.push_back('\n');

		FlushBuffer();

		Buffer tmp;
		tmp.swap(mBuffer);
	}
}

void VDCLIOutputSinkW32::Read() {
	DWORD avail;
	if (!PeekNamedPipe(mHandle, NULL, 0, NULL, &avail, NULL)) {
		ClosePipe();
		return;
	}

	if (!avail) {
		::Sleep(100);
		return;
	}

	uint8 buf[256];
	bool flushOK = false;

	while(avail) {
		DWORD tc = avail > sizeof buf ? sizeof buf : avail;
		avail -= tc;

		DWORD actual;
		if (!ReadFile(mHandle, buf, tc, &actual, NULL))
			break;

		if (!actual) {
			ClosePipe();
			return;
		}

		if (!flushOK) {
			for(uint32 i=0; i<actual; ++i) {
				if (buf[i] == '\r' || buf[i] == '\n') {
					flushOK = true;
					break;
				}
			}
		}

		mBuffer.insert(mBuffer.end(), buf, buf + actual);

		// Force a flush anyway if the buffer is growing too large.
		if (mBuffer.size() >= 1024)
			flushOK = true;
	}

	if (flushOK)
		FlushBuffer();
}

void VDCLIOutputSinkW32::ThreadRun() {
	while(mHandle != INVALID_HANDLE_VALUE) {
		if (!mActive) {
			Read();
			ClosePipe();
			return;
		}

		Read();
	}
}

void VDCLIOutputSinkW32::ClosePipe() {
	CloseHandle(mHandle);
	mHandle = INVALID_HANDLE_VALUE;
}

void VDCLIOutputSinkW32::FlushBuffer() {
	int idx = 0;

	mTempLine.clear();
	mTempLine = mLogPrefix;

	Buffer::iterator it(mBuffer.begin()), itEnd(mBuffer.end());
	Buffer::iterator itMark(mBuffer.begin());
	bool lineContainsNonSpaces = false;
	for(; it != itEnd; ++it) {
		char c = *it;

		if (c == '\r')
			c = '\n';

		if (c != ' ' && c != '\t')
			lineContainsNonSpaces = true;

		if (c == '\n' || idx >= 1024) {
			if (idx > 1 && lineContainsNonSpaces) {
				VDLog(kVDLogInfo, mTempLine.c_str());
				mTempLine = mLogPrefix;
			}

			idx = 0;
			lineContainsNonSpaces = false;

			itMark = it + 1;
		} else {
			mTempLine += c;
			++idx;
		}
	}

	if (itMark != mBuffer.begin())
		mBuffer.erase(mBuffer.begin(), itMark);
}

//////////////////////////////////////////////////////////////////////

class VDAutoHandleW32 {
public:
	VDAutoHandleW32();
	explicit VDAutoHandleW32(HANDLE h);
	VDAutoHandleW32(const VDAutoHandleW32&, bool inheritable = false);
	~VDAutoHandleW32();

	VDAutoHandleW32& operator=(const VDAutoHandleW32&);

	operator bool() const { return mHandle != INVALID_HANDLE_VALUE; }
	HANDLE *operator~() { Reset(); return &mHandle; }

	HANDLE GetHandle() const { return mHandle; }

	void Attach(HANDLE h) {
		VDASSERT(mHandle != h || !h || h == INVALID_HANDLE_VALUE);
		Reset();
		mHandle = h ? h : INVALID_HANDLE_VALUE;
	}

	HANDLE Detach() {
		HANDLE h = mHandle;
		mHandle = INVALID_HANDLE_VALUE;
		return h;
	}

	void Reset();
	void Assign(const VDAutoHandleW32& src, bool inheritable);
	void Duplicate(HANDLE h, bool inheritable);
	void Swap(VDAutoHandleW32& src);

protected:
	HANDLE mHandle;
};

VDAutoHandleW32::VDAutoHandleW32()
	: mHandle(INVALID_HANDLE_VALUE)
{
}

VDAutoHandleW32::VDAutoHandleW32(HANDLE h)
	: mHandle(h)
{
}

VDAutoHandleW32::VDAutoHandleW32(const VDAutoHandleW32& src, bool inheritable)
	: mHandle(INVALID_HANDLE_VALUE)
{
	Assign(src, inheritable);
}

VDAutoHandleW32::~VDAutoHandleW32() {
	Reset();
}

VDAutoHandleW32& VDAutoHandleW32::operator=(const VDAutoHandleW32& src) {
	Assign(src, false);
	return *this;
}

void VDAutoHandleW32::Reset() {
	if (mHandle != INVALID_HANDLE_VALUE) {
		CloseHandle(mHandle);
		mHandle = INVALID_HANDLE_VALUE;
	}
}

void VDAutoHandleW32::Assign(const VDAutoHandleW32& src, bool inheritable) {
	if (mHandle != src.mHandle)
		Duplicate(src.mHandle, inheritable);
}

void VDAutoHandleW32::Duplicate(HANDLE h, bool inheritable) {
	if (h != INVALID_HANDLE_VALUE) {
		HANDLE me = GetCurrentProcess();
		if (!::DuplicateHandle(me, h, me, &h, 0, inheritable, DUPLICATE_SAME_ACCESS))
			throw MyError("Unable to duplicate handle: %%s", GetLastError());
	}

	if (mHandle != INVALID_HANDLE_VALUE)
		CloseHandle(mHandle);

	mHandle = h;
}

void VDAutoHandleW32::Swap(VDAutoHandleW32& src) {
	std::swap(mHandle, src.mHandle);
}

//////////////////////////////////////////////////////////////////////

class VDCLIPipeW32 {
	VDCLIPipeW32(const VDCLIPipeW32&);
	VDCLIPipeW32& operator=(const VDCLIPipeW32&);
public:
	VDCLIPipeW32();
	VDCLIPipeW32(uint32 size, bool inputInheritable, bool outputInheritable);

	void Init(uint32 size, bool inputInheritable, bool outputInheritable);
	void Flush();
	void Close();
	void CloseOutput();

	HANDLE GetInput() const { return mInput.GetHandle(); }
	HANDLE GetOutput() const { return mOutput.GetHandle(); }
	HANDLE DetachInput() { return mInput.Detach(); }
	HANDLE DetachOutput() { return mOutput.Detach(); }

protected:
	VDAutoHandleW32	mInput;
	VDAutoHandleW32	mOutput;
};

VDCLIPipeW32::VDCLIPipeW32() {
}

VDCLIPipeW32::VDCLIPipeW32(uint32 size, bool inputInheritable, bool outputInheritable) {
	Init(size, inputInheritable, outputInheritable);
}

void VDCLIPipeW32::Init(uint32 size, bool inputInheritable, bool outputInheritable) {
	if (!CreatePipe(~mOutput, ~mInput, NULL, size))
		throw MyError("Unable to create pipe: %%s", GetLastError());

	if (inputInheritable) {
		VDAutoHandleW32 t(mInput, true);
		mInput.Swap(t);
	}

	if (outputInheritable) {
		VDAutoHandleW32 t(mOutput, true);
		mOutput.Swap(t);
	}
}

void VDCLIPipeW32::Flush() {
	if (mInput)
		::FlushFileBuffers(mInput.GetHandle());
}

void VDCLIPipeW32::Close() {
	mInput.Reset();
	mOutput.Reset();
}

void VDCLIPipeW32::CloseOutput() {
	mOutput.Reset();
}

//////////////////////////////////////////////////////////////////////

struct VDLaunchIpcRawDataW32 {
	int version;
	int programOffsetA;
	int programOffsetW;
	int commandLineOffsetA;
	int commandLineOffsetW;
	int _pad;
	uint64 hStdInput;
	uint64 hStdOutput;
	uint64 hStdError;
	uint64 hCurrentProcess;
	uint64 hLaunchEvent;
};

class VDLaunchIpcDataW32 {
public:
	void Init(uint32 pid, const wchar_t *programName, const wchar_t *commandLine, HANDLE hStdInput, HANDLE hStdOutput, HANDLE hStdError, HANDLE hCurrentProcessClone, HANDLE hLaunchEvent);
	void Shutdown();

protected:
	VDAutoHandleW32 mHandle;
};

void VDLaunchIpcDataW32::Init(uint32 pid, const wchar_t *programName, const wchar_t *commandLine, HANDLE hStdInput, HANDLE hStdOutput, HANDLE hStdError, HANDLE hCurrentProcessClone, HANDLE hLaunchEvent) {
	size_t programNameLen = programName ? wcslen(programName) : 0;
	size_t commandLineLen = commandLine ? wcslen(commandLine) : 0;
	VDStringA programNameA(VDTextWToA(programName ? programName : L""));
	VDStringA cmdLineA(VDTextWToA(commandLine ? commandLine : L""));

	size_t size = sizeof(VDLaunchIpcRawDataW32) + programNameA.size() + cmdLineA.size() + 2 + (programNameLen + commandLineLen + 2) * sizeof(wchar_t);

	VDStringA mappingName;
	mappingName.sprintf("vdlaunch-data-%08x", pid);
	HANDLE h = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, size, mappingName.c_str());

	if (!h)
		throw MyWin32Error("Unable to launch external program: %%s", GetLastError());

	mHandle.Attach(h);

	void *p = MapViewOfFile(h, FILE_MAP_WRITE, 0, 0, size);
	if (!p)
		throw MyWin32Error("Unable to launch external program: %%s", GetLastError());

	VDLaunchIpcRawDataW32 *data = (VDLaunchIpcRawDataW32 *)p;

	data->version = 0x100;
	data->programOffsetA = 0;
	data->programOffsetW = 0;
	data->commandLineOffsetA = 0;
	data->commandLineOffsetW = 0;
	data->hStdInput = (uint64)hStdInput;
	data->hStdOutput = (uint64)hStdOutput;
	data->hStdError = (uint64)hStdError;
	data->hCurrentProcess = (uint64)hCurrentProcessClone;
	data->hLaunchEvent = (uint64)hLaunchEvent;

	wchar_t *dstw = (wchar_t *)(data + 1);

	if (programName) {
		data->programOffsetW = (char *)dstw - (char *)data;
		memcpy(dstw, programName, sizeof(wchar_t) * (programNameLen + 1));
		dstw += programNameLen + 1;
	}

	if (commandLine) {
		data->commandLineOffsetW = (char *)dstw - (char *)data;
		memcpy(dstw, commandLine, sizeof(wchar_t) * (commandLineLen + 1));
		dstw += commandLineLen + 1;
	}

	char *dsta = (char *)dstw;

	if (programName) {
		data->programOffsetA = (char *)dsta - (char *)data;
		memcpy(dsta, programNameA.c_str(), programNameA.size() + 1);
		programName += programNameA.size() + 1;
	}

	if (commandLine) {
		data->commandLineOffsetA = (char *)dsta - (char *)data;
		memcpy(dsta, cmdLineA.c_str(), cmdLineA.size() + 1);
		commandLine += cmdLineA.size() + 1;
	}

	UnmapViewOfFile(p);
}

void VDLaunchIpcDataW32::Shutdown() {
	mHandle.Reset();
}

//////////////////////////////////////////////////////////////////////

class VDCLIProcessW32 {
	VDCLIProcessW32(const VDCLIProcessW32&);
	VDCLIProcessW32& operator=(const VDCLIProcessW32&);
public:
	VDCLIProcessW32();
	~VDCLIProcessW32();

	bool IsRunning() const;
	uint32 GetExitCode() const;

	HANDLE GetProcessHandle() const { return mProcessHandle.GetHandle(); }

	void Attach(HANDLE h, uint32 processId);
	void Run(const char *name, const wchar_t *cmdLine, HANDLE hStdInput, HANDLE hStdOutput, HANDLE hStdError);
	void Wait();
	void Close();

	void SendCtrlBreak();

protected:
	VDAutoHandleW32	mProcessHandle;
	uint32 mProcessId;

	VDLaunchIpcDataW32 mLaunchData;
};

VDCLIProcessW32::VDCLIProcessW32()
	: mProcessId(0)
{
}

VDCLIProcessW32::~VDCLIProcessW32() {
}

bool VDCLIProcessW32::IsRunning() const {
	HANDLE h = mProcessHandle.GetHandle();

	if (h == INVALID_HANDLE_VALUE)
		return false;

	return WaitForSingleObject(h, 0) == WAIT_TIMEOUT;
}

uint32 VDCLIProcessW32::GetExitCode() const {
	if (!mProcessHandle)
		return 0;

	DWORD errCode;
	if (!GetExitCodeProcess(mProcessHandle.GetHandle(), &errCode))
		return 0;
	
	return errCode;
}

void VDCLIProcessW32::Attach(HANDLE h, uint32 processId) {
	mProcessHandle.Attach(h);
	mProcessId = processId;
}

void VDCLIProcessW32::Run(const char *name, const wchar_t *cmdLine, HANDLE hStdInput, HANDLE hStdOutput, HANDLE hStdError) {

	PROCESS_INFORMATION pi;
	BOOL success;

#if 0	// This requires at least WinSDK 6.0, which we can't switch to yet.
	OSVERSIONINFO osvi = { sizeof(OSVERSIONINFO) };
	if (GetVersionEx(&osvi) && osvi.dwMajorVersion >= 6) {
		PROC_THREAD_ATTRIBUTE_LIST attList;
		SIZE_T attListSize;
		InitializeProcThreadAttributeList(NULL, 1, 0, &attListSize);

		PROC_THREAD_ATTRIBUTE_LIST *attList = (PROC_THREAD_ATTRIBUTE_LIST *)malloc(attListSize);
		if (!attList)
			throw MyMemoryError();

		InitializeProcThreadAttributeList(attList, 1, 0, NULL);

		STARTUPINFOEXW si = { sizeof(STARTUPINFOEXW) };
		si.StartupInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
		si.StartupInfo.hStdInput = hStdInput;
		si.StartupInfo.hStdOutput = hStdOutput;
		si.StartupInfo.hStdError = hStdError;
		si.StartupInfo.wShowWindow = SW_SHOWMINNOACTIVE;
		si.lpAttributeList = attList;

		HANDLE hInheritList[3] = { hStdInput, hStdOutput, hStdError };
		UpdateProcThreadAttribute(attList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, hInheritList, sizeof(hInheritList), NULL, NULL);

		BOOL success = CreateProcessW(NULL, (LPWSTR)cmdLine, NULL, NULL, TRUE, CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP | EXTENDED_STARTUPINFO_PRESENT, NULL, NULL, &si.StartupInfo, &pi);
		DWORD err = GetLastError();

		DeleteProcThreadAttributeList(attList);

		if (!success)
			throw MyWin32Error("CLI: Unable to launch %s: %%s.", err, name);

		CloseHandle(pi.hThread);
		mProcessHandle.Attach(pi.hProcess);
		mProcessId = pi.dwProcessId;
		return;
	}
#endif

	const VDStringW programPath(VDGetProgramPath());

	VDStringW launchCmdLine(VDMakePath(programPath.c_str(), VD_LAUNCH_HELPER_NAMEW));

	if (!VDDoesPathExist(launchCmdLine.c_str()))
		throw MyError("CLI: Cannot launch external program. The program launch helper " VD_LAUNCH_HELPER_NAMEA " is missing.");

	VDSignal launchEvent;

	const DWORD flags = (VDIsWindowsNT() ? CREATE_NO_WINDOW : 0) | CREATE_NEW_PROCESS_GROUP | CREATE_SUSPENDED;

	if (VDIsWindowsNT()) {
		STARTUPINFOW siw = { sizeof(STARTUPINFOW) };
		siw.dwFlags = STARTF_USESHOWWINDOW;
		siw.wShowWindow = SW_SHOWMINNOACTIVE;

		success = CreateProcessW(launchCmdLine.c_str(), (LPWSTR)launchCmdLine.c_str(), NULL, NULL, FALSE, flags, NULL, NULL, &siw, &pi);
	} else {
		STARTUPINFOA sia = { sizeof(STARTUPINFOA) };
		sia.dwFlags = STARTF_USESHOWWINDOW;
		sia.wShowWindow = SW_SHOWMINNOACTIVE;

		success = CreateProcess(VDTextWToA(launchCmdLine).c_str(), NULL, NULL, NULL, FALSE, flags, NULL, NULL, &sia, &pi);
	}

	if (!success)
		throw MyWin32Error("CLI: Unable to launch %s: %%s.", GetLastError(), name);

	VDAutoHandleW32 threadHandle(pi.hThread);
	mProcessHandle.Attach(pi.hProcess);

	HANDLE hLaunchEvent = launchEvent.getHandle();
	HANDLE hStdInputClone = NULL;
	HANDLE hStdOutputClone = NULL;
	HANDLE hStdErrorClone = NULL;
	HANDLE hCurrentProcessClone = NULL;
	HANDLE hLaunchEventClone = NULL;

	HANDLE hCurrentProcess = GetCurrentProcess();
	if (!DuplicateHandle(hCurrentProcess, hStdInput, pi.hProcess, &hStdInputClone, 0, FALSE, DUPLICATE_SAME_ACCESS) ||
		!DuplicateHandle(hCurrentProcess, hStdOutput, pi.hProcess, &hStdOutputClone, 0, FALSE, DUPLICATE_SAME_ACCESS) ||
		!DuplicateHandle(hCurrentProcess, hStdError, pi.hProcess, &hStdErrorClone, 0, FALSE, DUPLICATE_SAME_ACCESS) ||
		!DuplicateHandle(hCurrentProcess, hCurrentProcess, pi.hProcess, &hCurrentProcessClone, 0, FALSE, DUPLICATE_SAME_ACCESS) ||
		!DuplicateHandle(hCurrentProcess, hLaunchEvent, pi.hProcess, &hLaunchEventClone, 0, FALSE, DUPLICATE_SAME_ACCESS))
	{
		DWORD err = GetLastError();
		ResumeThread(threadHandle.GetHandle());

		throw MyWin32Error("CLI: Unable to launch %s: %%s", err, name);
	}

	mLaunchData.Init(pi.dwProcessId, NULL, cmdLine, hStdInputClone, hStdOutputClone, hStdErrorClone, hCurrentProcessClone, hLaunchEventClone);

	ResumeThread(threadHandle.GetHandle());

	HANDLE hWait[2] = { hLaunchEvent, pi.hProcess };
	if (WAIT_OBJECT_0 + 1 == WaitForMultipleObjects(2, hWait, FALSE, INFINITE)) {
		DWORD errCode = 0;

		GetExitCodeProcess(pi.hProcess, &errCode);

		if (HRESULT_SEVERITY(errCode) == SEVERITY_ERROR && HRESULT_FACILITY(errCode) == FACILITY_WIN32)
			throw MyWin32Error("CLI: Unable to launch %s: %%s", HRESULT_CODE(errCode), name);
	}

	mProcessId = pi.dwProcessId;
}

void VDCLIProcessW32::Wait() {
	HANDLE h = mProcessHandle.GetHandle();
	if (h != INVALID_HANDLE_VALUE)
		WaitForSingleObject(h, INFINITE);
}

void VDCLIProcessW32::Close() {
	mProcessHandle.Reset();
}

void VDCLIProcessW32::SendCtrlBreak() {
	if (mProcessHandle.GetHandle())
		::GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, mProcessId);
}

//////////////////////////////////////////////////////////////////////

class VDMediaOutputStreamProxy : public IVDMediaOutputStream, public IVDVideoImageOutputStream {
public:
	VDMediaOutputStreamProxy(IVDMediaOutputStream *os);

	void *AsInterface(uint32 id);

	virtual void *	getFormat();
	virtual int		getFormatLen();
	virtual void	setFormat(const void *pFormat, int len);

	virtual const VDXStreamInfo& getStreamInfo();
	virtual void	setStreamInfo(const VDXStreamInfo& hdr);
	virtual void	updateStreamInfo(const VDXStreamInfo& hdr);

	virtual void	write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 samples);
	virtual void	write(const void *pBuffer, uint32 cbBuffer, IVDXOutputFile::PacketInfo& packetInfo, FilterModPixmapInfo* info);

	virtual void	partialWriteBegin(uint32 flags, uint32 bytes, uint32 samples);
	virtual void	partialWrite(const void *pBuffer, uint32 cbBuffer);
	virtual void	partialWriteEnd();

	virtual void	flush();
	virtual void	finish();

public:
	virtual void WriteVideoImage(const VDPixmap *px);

protected:
	virtual void TranslateError(const MyError& e);

	IVDMediaOutputStream *const mpStream;
	IVDVideoImageOutputStream *const mpVOStream;
};

VDMediaOutputStreamProxy::VDMediaOutputStreamProxy(IVDMediaOutputStream *os)
	: mpStream(os)
	, mpVOStream(vdpoly_cast<IVDVideoImageOutputStream *>(mpStream))
{
}

void *VDMediaOutputStreamProxy::AsInterface(uint32 id) {
	if (id == IVDMediaOutputStream::kTypeID)
		return static_cast<IVDMediaOutputStream *>(this);

	if (id == IVDVideoImageOutputStream::kTypeID)
		return mpVOStream ? static_cast<IVDVideoImageOutputStream *>(this) : NULL;

	return NULL;
}

void *VDMediaOutputStreamProxy::getFormat() {
	return mpStream->getFormat();
}

int VDMediaOutputStreamProxy::getFormatLen() {
	return mpStream->getFormatLen();
}

void VDMediaOutputStreamProxy::setFormat(const void *pFormat, int len) {
	return mpStream->setFormat(pFormat, len);
}

const VDXStreamInfo& VDMediaOutputStreamProxy::getStreamInfo() {
	return mpStream->getStreamInfo();
}

void VDMediaOutputStreamProxy::setStreamInfo(const VDXStreamInfo& hdr) {
	mpStream->setStreamInfo(hdr);
}

void VDMediaOutputStreamProxy::updateStreamInfo(const VDXStreamInfo& hdr) {
	try {
		mpStream->updateStreamInfo(hdr);
	} catch(const MyError& e) {
		TranslateError(e);
	}
}

void VDMediaOutputStreamProxy::write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 samples) {
	try {
		mpStream->write(flags, pBuffer, cbBuffer, samples);
	} catch(const MyError& e) {
		TranslateError(e);
	}
}

void VDMediaOutputStreamProxy::write(const void *pBuffer, uint32 cbBuffer, IVDXOutputFile::PacketInfo& packetInfo, FilterModPixmapInfo* info) {
	try {
		mpStream->write(pBuffer, cbBuffer, packetInfo, info);
	} catch(const MyError& e) {
		TranslateError(e);
	}
}

void VDMediaOutputStreamProxy::partialWriteBegin(uint32 flags, uint32 bytes, uint32 samples) {
	try {
		mpStream->partialWriteBegin(flags, bytes, samples);
	} catch(const MyError& e) {
		TranslateError(e);
	}
}

void VDMediaOutputStreamProxy::partialWrite(const void *pBuffer, uint32 cbBuffer) {
	try {
		mpStream->partialWrite(pBuffer, cbBuffer);
	} catch(const MyError& e) {
		TranslateError(e);
	}
}

void VDMediaOutputStreamProxy::partialWriteEnd() {
	try {
		mpStream->partialWriteEnd();
	} catch(const MyError& e) {
		TranslateError(e);
	}
}

void VDMediaOutputStreamProxy::flush() {
	try {
		mpStream->flush();
	} catch(const MyError& e) {
		TranslateError(e);
	}
}

void VDMediaOutputStreamProxy::finish() {
	try {
		mpStream->finish();
	} catch(const MyError& e) {
		TranslateError(e);
	}
}

void VDMediaOutputStreamProxy::WriteVideoImage(const VDPixmap *px) {
	try {
		mpVOStream->WriteVideoImage(px);
	} catch(const MyError& e) {
		TranslateError(e);
	}
}

void VDMediaOutputStreamProxy::TranslateError(const MyError& e) {
	throw e;
}

//////////////////////////////////////////////////////////////////////

class VDMediaOutputStreamCLI : public VDMediaOutputStreamProxy {
public:
	VDMediaOutputStreamCLI(IVDMediaOutputStream *child, VDCLIProcessW32 *process, const wchar_t *encName);

protected:
	void TranslateError(const MyError& e);

	VDCLIProcessW32 *mpProcess;
	VDStringW mEncName;
};

VDMediaOutputStreamCLI::VDMediaOutputStreamCLI(IVDMediaOutputStream *child, VDCLIProcessW32 *process, const wchar_t *encName)
	: VDMediaOutputStreamProxy(child)
	, mpProcess(process)
	, mEncName(encName)
{
}

void VDMediaOutputStreamCLI::TranslateError(const MyError& e) {
	if (!mpProcess->IsRunning()) {
		uint32 exitCode = mpProcess->GetExitCode();

		throw MyError("The %ls process has prematurely exited with an error code of %d (%08x). Check the log for possible error messages.", mEncName.c_str(), exitCode, exitCode);
	}

	throw e;
}

//////////////////////////////////////////////////////////////////////
//
// AVIOutputCLI
//
//////////////////////////////////////////////////////////////////////

class AVIOutputCLI : public AVIOutput, public IAVIOutputCLI {
public:
	AVIOutputCLI(const VDAVIOutputCLITemplate& templ);
	~AVIOutputCLI();

	void *AsInterface(uint32 id);

	void SetInputLayout(const VDPixmapLayout& layout);
	void SetBufferSize(sint32 nBytes) {
		mBufferSize = nBytes;
	}

	IVDMediaOutputStream *createVideoStream();
	IVDMediaOutputStream *createAudioStream();

	bool init(const wchar_t *szFile);
	void finalize();
	void CloseWithoutFinalize();

private:
	void Close(bool finalize);
	void Cleanup();
	void WaitForProcesses(bool checkForErrorCodes);
	void ClosePipeHandles();
	void ExpandTokens(VDStringW& cmdLine, const wchar_t *templateLine0, const wchar_t *programPath);

	AVIOutput *mpAudioOutput;
	vdautoptr<AVIOutputWAV> mpAudioOutputWAV;
	vdautoptr<AVIOutputRawAudio> mpAudioOutputRawAudio;
	vdautoptr<AVIOutputRawVideo> mpVideoOutput;
	sint32		mBufferSize;
	VDFraction	mFrameRate;
	uint32		mAudioSamplingRate;
	uint32		mAudioChannels;
	uint32		mAudioPrecision;

	VDStringW	mOutputFile;
	VDStringW	mTempVideoFile;
	VDStringW	mTempAudioFile;
	VDPixmapLayout	mInputLayout;

	VDCLIProcessW32		mVideoEncoderProcess;
	VDCLIProcessW32		mAudioEncoderProcess;
	VDFile				mTempOutputLock;
	VDCLIPipeW32		mVideoPipe;
	VDCLIPipeW32		mAudioPipe;

	VDAVIOutputCLITemplate	mTemplate;

	VDCLIOutputSinkW32	mVideoOutputSink;
	VDCLIOutputSinkW32	mAudioOutputSink;
};

AVIOutputCLI::AVIOutputCLI(const VDAVIOutputCLITemplate& templ)
	: mTemplate(templ)
	, mpAudioOutput(NULL)
{
	mBufferSize			= 65536;

	mVideoOutputSink.SetLogPrefix(L"VideoEnc: ");
	mAudioOutputSink.SetLogPrefix(L"AudioEnc: ");
}

AVIOutputCLI::~AVIOutputCLI() {
	mpVideoOutput.reset();
	mpAudioOutput = NULL;
	mpAudioOutputRawAudio.reset();
	mpAudioOutputWAV.reset();

	WaitForProcesses(false);

	mVideoOutputSink.Shutdown();
	mAudioOutputSink.Shutdown();
}

void *AVIOutputCLI::AsInterface(uint32 id) {
	switch(id) {
		case IVDMediaOutput::kTypeID: return static_cast<IVDMediaOutput *>(this);
		case IAVIOutputCLI::kTypeID: return static_cast<IAVIOutputCLI *>(this);
	}

	return NULL;
}

void AVIOutputCLI::SetInputLayout(const VDPixmapLayout& layout) {
	mInputLayout = layout;
}

void InitOutputFormat(VDAVIOutputRawVideoFormat& format, const VDExtEncProfile* vp) {
	format.mOutputFormat = nsVDPixmap::kPixFormat_YUV420_Planar;
	format.mScanlineAlignment = 1;
	format.mbSwapChromaPlanes = false;
	format.mbBottomUp = false;

	if (vp->mPixelFormat==L"yuv420p")
		format.mOutputFormat = nsVDPixmap::kPixFormat_YUV420_Planar;

	if (vp->mPixelFormat==L"yuv422p")
		format.mOutputFormat = nsVDPixmap::kPixFormat_YUV422_Planar;

	if (vp->mPixelFormat==L"yuv444p")
		format.mOutputFormat = nsVDPixmap::kPixFormat_YUV444_Planar;

	if (vp->mPixelFormat==L"yuv420p16le")
		format.mOutputFormat = nsVDPixmap::kPixFormat_YUV420_Planar16;

	if (vp->mPixelFormat==L"yuv422p16le")
		format.mOutputFormat = nsVDPixmap::kPixFormat_YUV422_Planar16;

	if (vp->mPixelFormat==L"yuv444p16le")
		format.mOutputFormat = nsVDPixmap::kPixFormat_YUV444_Planar16;

	if (vp->mPixelFormat==L"bgr24")
		format.mOutputFormat = nsVDPixmap::kPixFormat_RGB888;

	if (vp->mPixelFormat==L"bgra")
		format.mOutputFormat = nsVDPixmap::kPixFormat_XRGB8888;

	if (vp->mPixelFormat==L"bgra64le")
		format.mOutputFormat = nsVDPixmap::kPixFormat_XRGB64;
}

IVDMediaOutputStream *AVIOutputCLI::createVideoStream() {
	if (mpVideoOutput)
		throw MyError("CLI: Only one video output is supported.");

	VDAVIOutputRawVideoFormat rawFormat = {};
	InitOutputFormat(rawFormat,mTemplate.mpVideoEncoderProfile);

	mpVideoOutput = new AVIOutputRawVideo(rawFormat);
	mpVideoOutput->SetInputLayout(mInputLayout);

	const char *s = VDPixmapGetInfo(rawFormat.mOutputFormat).name;
	VDLogAppMessage(kVDLogInfo, kVDST_Dub, 14, 1, &s);

	vdautoptr<IVDMediaOutputStream> os(mpVideoOutput->createVideoStream());

	vdautoptr<VDMediaOutputStreamCLI> proxy(new VDMediaOutputStreamCLI(os, &mVideoEncoderProcess, L"video encoding"));
	os.release();
	videoOut = proxy.release();

	return videoOut;
}

IVDMediaOutputStream *AVIOutputCLI::createAudioStream() {
	if (mpAudioOutput)
		throw MyError("CLI: Only one audio output is supported.");

	if (!mTemplate.mpAudioEncoderProfile)
		throw MyError("CLI: The encoding template does not support audio encoding.");

	if (mTemplate.mpAudioEncoderProfile->mInputFormat == kVDExtEncInputFormat_WAV) {
		mpAudioOutputWAV = new AVIOutputWAV;
		mpAudioOutput = mpAudioOutputWAV;
	} else {
		mpAudioOutputRawAudio = new AVIOutputRawAudio;
		mpAudioOutput = mpAudioOutputRawAudio;
	}

	vdautoptr<IVDMediaOutputStream> os(mpAudioOutput->createAudioStream());

	vdautoptr<VDMediaOutputStreamCLI> proxy(new VDMediaOutputStreamCLI(os, &mAudioEncoderProcess, L"audio encoding"));
	os.release();
	audioOut = proxy.release();

	return audioOut;
}

bool AVIOutputCLI::init(const wchar_t *pwszFile) {
	mOutputFile = pwszFile;

	mTempOutputLock.open(pwszFile, nsVDFile::kWrite | nsVDFile::kDenyAll | nsVDFile::kCreateAlways);

	if (!mTemplate.mpMultiplexerProfile)
		mTempOutputLock.close();

	VDAutoHandleW32 nul;

	HANDLE h = CreateFileA("nul", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE)
		throw MyWin32Error("Unable to open null device: %%s", GetLastError());

	nul.Attach(h);

	bool useOutputPath = mTemplate.mbUseOutputPathAsTemp;
	if (mpVideoOutput) {
		VDCLIPipeW32 videoOutputPipe(1024, true, false);
		mVideoPipe.Init(mBufferSize, false, true);

		mpVideoOutput->init(mVideoPipe.GetInput());

		const VDXAVIStreamHeader& hdr = videoOut->getStreamInfo().aviHeader;

		mFrameRate = VDFraction(hdr.dwRate, hdr.dwScale);

		// launch video processor
		VDStringW cmdArguments;
		VDStringW tempVideoFilename;
		ExpandTokens(tempVideoFilename, mTemplate.mpVideoEncoderProfile->mOutputFilename.c_str(), mTemplate.mpVideoEncoderProfile->mProgram.c_str());

		if (useOutputPath)
			mTempVideoFile = mOutputFile;
		else
			mTempVideoFile = VDMakePath(VDFileSplitPathLeft(mOutputFile).c_str(), tempVideoFilename.c_str());

		ExpandTokens(cmdArguments, mTemplate.mpVideoEncoderProfile->mCommandArguments.c_str(), mTemplate.mpVideoEncoderProfile->mProgram.c_str());

		if (mTemplate.mpVideoEncoderProfile->mbPredeleteOutputFile) {
			if (VDDoesPathExist(mTempVideoFile.c_str()))
				VDRemoveFile(mTempVideoFile.c_str());
		}

		VDStringW cmdLine(L"\"");
		cmdLine += mTemplate.mpVideoEncoderProfile->mProgram;
		cmdLine += L"\" ";
		cmdLine += cmdArguments;

		VDAutoHandleW32 videoErrorSink;

		if (!mTemplate.mpVideoEncoderProfile->mbLogStdout || !mTemplate.mpVideoEncoderProfile->mbLogStderr)
			videoErrorSink.Assign(nul, true);

		mVideoEncoderProcess.Run("video encoder",
			cmdLine.c_str(),
			mVideoPipe.GetOutput(), 
			mTemplate.mpVideoEncoderProfile->mbLogStdout ? videoOutputPipe.GetInput() : videoErrorSink.GetHandle(),
			mTemplate.mpVideoEncoderProfile->mbLogStderr ? videoOutputPipe.GetInput() : videoErrorSink.GetHandle());

		mVideoOutputSink.Init(videoOutputPipe.DetachOutput());
		mVideoPipe.CloseOutput();
	}

	if (mpAudioOutput) {
		VDCLIPipeW32 audioOutputPipe(1024, true, false);
		mAudioPipe.Init(mBufferSize, false, true);

		if (mpAudioOutputWAV)
			mpAudioOutputWAV->init(mAudioPipe.GetInput(), true);
		else
			mpAudioOutputRawAudio->init(mAudioPipe.GetInput(), true);

		const VDWaveFormat& hdr = *(const VDWaveFormat *)audioOut->getFormat();

		mAudioSamplingRate = hdr.mSamplingRate;
		mAudioChannels = hdr.mChannels;
		mAudioPrecision = hdr.mSampleBits;

		// launch video processor
		VDStringW cmdArguments;
		VDStringW tempAudioFilename;
		ExpandTokens(tempAudioFilename, mTemplate.mpAudioEncoderProfile->mOutputFilename.c_str(), mTemplate.mpAudioEncoderProfile->mProgram.c_str());

		if (useOutputPath)
			mTempAudioFile = mOutputFile;
		else
			mTempAudioFile = VDMakePath(VDFileSplitPathLeft(mOutputFile).c_str(), tempAudioFilename.c_str());

		ExpandTokens(cmdArguments, mTemplate.mpAudioEncoderProfile->mCommandArguments.c_str(), mTemplate.mpAudioEncoderProfile->mProgram.c_str());

		if (mTemplate.mpAudioEncoderProfile->mbPredeleteOutputFile) {
			if (VDDoesPathExist(mTempAudioFile.c_str()))
				VDRemoveFile(mTempAudioFile.c_str());
		}

		VDStringW cmdLine(L"\"");
		cmdLine += mTemplate.mpAudioEncoderProfile->mProgram;
		cmdLine += L"\" ";
		cmdLine += cmdArguments;

		VDAutoHandleW32 audioErrorSink;

		if (!mTemplate.mpAudioEncoderProfile->mbLogStdout || !mTemplate.mpAudioEncoderProfile->mbLogStderr)
			audioErrorSink.Assign(nul, true);

		mAudioEncoderProcess.Run("audio encoder",
			cmdLine.c_str(),
			mAudioPipe.GetOutput(), 
			mTemplate.mpAudioEncoderProfile->mbLogStdout ? audioOutputPipe.GetInput() : audioErrorSink.GetHandle(),
			mTemplate.mpAudioEncoderProfile->mbLogStderr ? audioOutputPipe.GetInput() : audioErrorSink.GetHandle());

		mAudioOutputSink.Init(audioOutputPipe.DetachOutput());
		mAudioPipe.CloseOutput();
	}

	return true;
}

void AVIOutputCLI::finalize() {
	Close(true);
}

void AVIOutputCLI::CloseWithoutFinalize() {
	mVideoEncoderProcess.SendCtrlBreak();
	mAudioEncoderProcess.SendCtrlBreak();
	Close(false);
}

void AVIOutputCLI::Close(bool finalize) {
	try {
		if (mpVideoOutput)
			mpVideoOutput->finalize();

		if (mpAudioOutput)
			mpAudioOutput->finalize();

		WaitForProcesses(true);

		mVideoOutputSink.Shutdown();
		mAudioOutputSink.Shutdown();

		if (finalize && mTemplate.mpMultiplexerProfile) {
			VDStringW cmdArguments;
			ExpandTokens(cmdArguments, mTemplate.mpMultiplexerProfile->mCommandArguments.c_str(), mTemplate.mpMultiplexerProfile->mProgram.c_str());

			VDStringW cmdLine(L"\"");
			cmdLine += mTemplate.mpMultiplexerProfile->mProgram;
			cmdLine += L"\" ";
			cmdLine += cmdArguments;

			mTempOutputLock.close();

			if (mTemplate.mpMultiplexerProfile->mbPredeleteOutputFile) {
				if (VDDoesPathExist(mOutputFile.c_str()))
					VDRemoveFile(mOutputFile.c_str());
			}

			VDCLIPipeW32 muxOutputPipe(1024, true, false);

			VDAutoHandleW32 muxErrorSink;

			if (!mTemplate.mpMultiplexerProfile->mbLogStdout || !mTemplate.mpMultiplexerProfile->mbLogStderr) {
				VDAutoHandleW32 nul;

				HANDLE h = CreateFileA("nul", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
				if (h == INVALID_HANDLE_VALUE)
					throw MyWin32Error("Unable to open null device: %%s", GetLastError());

				nul.Attach(h);
				muxErrorSink.Assign(nul, true);
			}

			VDCLIOutputSinkW32 muxOutputSink;
			muxOutputSink.SetLogPrefix(L"Mux: ");

			VDCLIProcessW32 muxProcess;
			muxProcess.Run("multiplexer",
				cmdLine.c_str(),
				muxErrorSink.GetHandle(), 
				mTemplate.mpMultiplexerProfile->mbLogStdout ? muxOutputPipe.GetInput() : muxErrorSink.GetHandle(),
				mTemplate.mpMultiplexerProfile->mbLogStderr ? muxOutputPipe.GetInput() : muxErrorSink.GetHandle());

			muxOutputSink.Init(muxOutputPipe.DetachOutput());

			muxProcess.Wait();
			muxOutputSink.Shutdown();

			DWORD errCode;
			if (mTemplate.mpMultiplexerProfile->mbCheckReturnCode && GetExitCodeProcess(muxProcess.GetProcessHandle(), &errCode) && errCode)
				throw MyError("CLI: The multiplexing process failed with error code %d (%08x). Check the log for possible error messages.", errCode, errCode);
		}
	} catch(const MyError&) {
		Cleanup();
		throw;
	}

	Cleanup();
}

void AVIOutputCLI::Cleanup() {
	if (mTemplate.mpMultiplexerProfile) {
		if (!mTempVideoFile.empty()) {
			try {
				VDRemoveFile(mTempVideoFile.c_str());
			} catch(const MyError&) {
			}
		}

		if (!mTempAudioFile.empty()) {
			try {
				VDRemoveFile(mTempAudioFile.c_str());
			} catch(const MyError&) {
			}
		}
	}
}

void AVIOutputCLI::WaitForProcesses(bool checkForErrorCodes) {
	if (mVideoEncoderProcess.IsRunning()) {
		// We must not close the pipes until the processes have exited, ensuring that they've
		// read all of the data.
		mVideoPipe.Flush();
		mVideoPipe.Close();

		mVideoEncoderProcess.Wait();

		if (checkForErrorCodes && mTemplate.mpVideoEncoderProfile->mbCheckReturnCode) {
			uint32 errCode = mVideoEncoderProcess.GetExitCode();

			if (errCode)
				throw MyError("CLI: The video encoding process failed with error code %d (%08x). Check the log for possible error messages.", errCode, errCode);
		}

		mVideoEncoderProcess.Close();
	}

	if (mAudioEncoderProcess.IsRunning()) {
		mAudioPipe.Flush();
		mAudioPipe.Close();

		mAudioEncoderProcess.Wait();

		if (checkForErrorCodes && mTemplate.mpAudioEncoderProfile->mbCheckReturnCode) {
			uint32 errCode = mAudioEncoderProcess.GetExitCode();

			if (errCode)
				throw MyError("CLI: The audio encoding process failed with error code %d (%08x). Check the log for possible error messages.", errCode, errCode);
		}

		mAudioEncoderProcess.Close();
	}
}

namespace {
	void AppendDirectoryFromPath(VDStringW& output, const wchar_t *s) {
		const wchar_t *t = VDFileSplitPath(s);

		if (s == t)
			output.append(L".");
		else if (t[-1] == L':') {
			output.append(s, t-s);
			output.append(L".");
		} else {
			if (t[-1] == L'/' || t[-1] == L'\\')
				--t;

			output.append(s, t-s);
		}
	}
}

void AVIOutputCLI::ExpandTokens(VDStringW& output, const wchar_t *templateLine0, const wchar_t *programPath) {
	const wchar_t *templateLine = templateLine0;

	output.clear();

	while(wchar_t c = *templateLine++) {
		if (c != L'%') {
			output += c;
			continue;
		}

		// okay, it's a %... check for an escaped %
		c = *templateLine++;
		if (c == L'%') {
			output += c;
			continue;
		}

		// check for '('
		if (c != L'(')
			goto invalid_template;

		const wchar_t *tokenStart = templateLine;
		const wchar_t *tokenEnd = wcschr(templateLine, L')');
		
		if (!tokenEnd)
			goto invalid_template;

		templateLine = tokenEnd + 1;

		VDStringW tokenbuf(tokenStart, tokenEnd);
		const wchar_t *token = tokenbuf.c_str();
		if (!vdwcsicmp(token, L"width"))
			output.append_sprintf(L"%u", mInputLayout.w);
		else if (!vdwcsicmp(token, L"height"))
			output.append_sprintf(L"%u", mInputLayout.h);
		else if (!vdwcsicmp(token, L"fps"))
			output.append_sprintf(L"%g", mFrameRate.asDouble());
		else if (!vdwcsicmp(token, L"fpsnum"))
			output.append_sprintf(L"%u", mFrameRate.getHi());
		else if (!vdwcsicmp(token, L"fpsden"))
			output.append_sprintf(L"%u", mFrameRate.getLo());
		else if (!vdwcsicmp(token, L"outputname"))
			output.append(VDFileSplitPath(mOutputFile.c_str()));
		else if (!vdwcsicmp(token, L"outputbasename")) {
			const wchar_t *outputName = VDFileSplitPath(mOutputFile.c_str());
			const wchar_t *outputExt = VDFileSplitExt(outputName);

			if (outputExt)
				output.append(outputName, outputExt);
			else
				output.append(outputName);
		} else if (!vdwcsicmp(token, L"outputfile"))
			output.append(mOutputFile);
		else if (!vdwcsicmp(token, L"outputdir"))
			AppendDirectoryFromPath(output, mOutputFile.c_str());
		else if (!vdwcsicmp(token, L"tempvideofile"))
			output.append(mTempVideoFile);
		else if (!vdwcsicmp(token, L"tempaudiofile"))
			output.append(mTempAudioFile);
		else if (!vdwcsicmp(token, L"pix_fmt"))
			output.append(mTemplate.mpVideoEncoderProfile->mPixelFormat);
		else if (!vdwcsicmp(token, L"samplingrate"))
			output.append_sprintf(L"%u", mAudioSamplingRate);
		else if (!vdwcsicmp(token, L"samplingratekhz"))
			output.append_sprintf(L"%u", (mAudioSamplingRate + 500) / 1000);
		else if (!vdwcsicmp(token, L"channels"))
			output.append_sprintf(L"%u", mAudioChannels);
		else if (!vdwcsicmp(token, L"audioprecision"))
			output.append_sprintf(L"%u", mAudioPrecision);
		else if (!vdwcsicmp(token, L"hostdir"))
			AppendDirectoryFromPath(output, VDGetProgramPath().c_str());
		else if (!vdwcsicmp(token, L"programdir"))
			AppendDirectoryFromPath(output, programPath);
		else if (!vdwcsicmp(token, L"systemdir"))
			AppendDirectoryFromPath(output, VDGetSystemPath().c_str());
		else
			throw MyError("Unknown token in command line template: %%(%ls)", token);
	}

	return;

invalid_template:
	throw MyError("Invalid command line template: \"%ls\"", templateLine0);
}


IAVIOutputCLI *VDCreateAVIOutputCLI(const VDAVIOutputCLITemplate& templ) {
	return new AVIOutputCLI(templ);
}

