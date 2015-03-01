//	VirtualDub - Video processing and capture application
//	Video capture library
//	Copyright (C) 1998-2012 Avery Lee
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
#include <objbase.h>
#include <vd2/system/refcount.h>
#include <vd2/VDCapture/AudioGrabberWASAPI.h>
#include <vd2/VDCapture/win32/api_wasapi.h>
#include <vd2/Priss/convert.h>

VDAudioGrabberWASAPI::VDAudioGrabberWASAPI()
	: mCurrentState(kStateNone)
	, mRequestedState(kStateNone)
	, mpCB(NULL)
	, mpMMDevice(NULL)
	, mpAudioClientCapture(NULL)
	, mpAudioClientRender(NULL)
	, mpAudioCaptureClient(NULL)
	, mpAudioRenderClient(NULL)
	, mhEventRender(NULL)
{
}

VDAudioGrabberWASAPI::~VDAudioGrabberWASAPI() {
}

bool VDAudioGrabberWASAPI::GetFormat(vdstructex<WAVEFORMATEX>& format) {
	bool local = !isThreadAttached();

	if (local) {
		if (!InitEndpoint()) {
			ShutdownEndpoint();
			return false;
		}
	}

	format.resize(sizeof(WAVEFORMATEX));
	format->wFormatTag = WAVE_FORMAT_PCM;
	format->nChannels = mChannels;
	format->nSamplesPerSec = mMixFormat->nSamplesPerSec;
	format->nBlockAlign = sizeof(sint16) * mChannels;
	format->nAvgBytesPerSec = format->nBlockAlign * format->nSamplesPerSec;
	format->wBitsPerSample = 16;
	format->cbSize = 0;

	if (local)
		ShutdownEndpoint();

	return true;
}

bool VDAudioGrabberWASAPI::Init(IVDAudioGrabberCallbackWASAPI *cb) {
	mCurrentState = kStateNone;
	mRequestedState = kStateInited;
	mpCB = cb;

	if (!ThreadStart())
		return false;

	for(;;) {
		State curState = (State)(int)mCurrentState;

		if (curState == kStateInited)
			break;

		if (curState == kStateInitFailed) {
			ThreadWait();
			return false;
		}

		mStateChangeCompleted.wait();
	}

	return true;
}

void VDAudioGrabberWASAPI::Shutdown() {
	mRequestedState = kStateNone;
	mStateChangeRequested.signal();

	ThreadWait();
}

bool VDAudioGrabberWASAPI::Start() {
	while(mCurrentState == kStateStartFailed) {
		mRequestedState = kStateInited;
		mStateChangeCompleted.wait();
	}

	mRequestedState = kStateRunning;
	mStateChangeRequested.signal();

	for(;;) {
		State curState = (State)(int)mCurrentState;

		if (curState == kStateStartFailed)
			return false;

		if (curState == kStateRunning)
			return true;

		mStateChangeCompleted.wait();
	}
}

void VDAudioGrabberWASAPI::Stop() {
	mRequestedState = kStateInited;
	mStateChangeRequested.signal();

	for(;;) {
		State curState = (State)(int)mCurrentState;

		if (curState == kStateInited)
			break;

		mStateChangeCompleted.wait();
	}
}

uint32 VDAudioGrabberWASAPI::ReadData(void *dst, size_t bytes) {
	return mTransferBuffer.Read((sint16 *)dst, bytes / sizeof(sint16)) * sizeof(sint16);
}

void VDAudioGrabberWASAPI::ThreadRun() {
	if (InitLocal()) {
		mCurrentState = kStateInited;
		mStateChangeCompleted.signal();

		RunLocal();
	} else {
		mCurrentState = kStateInitFailed;
		mStateChangeCompleted.signal();
	}

	ShutdownLocal();

	mCurrentState = kStateNone;
	mStateChangeCompleted.signal();
}

bool VDAudioGrabberWASAPI::InitEndpoint() {
	HRESULT hr;

	// get the default audio endpoint for multimedia rendering
	vdrefptr<IMMDeviceEnumerator> pDevEnum;
	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void **)~pDevEnum);

	if (FAILED(hr))
		return false;

	hr = pDevEnum->GetDefaultAudioEndpoint(eRender, eMultimedia, &mpMMDevice);
	pDevEnum.clear();

	if (FAILED(hr))
		return false;

	// create two audio clients for the default endpoint
	hr = mpMMDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void **)&mpAudioClientCapture);
	if (FAILED(hr)) {
		ShutdownEndpoint();
		return false;
	}

	// retrieve the mixing format
	WAVEFORMATEX *pwfex;
	hr = mpAudioClientCapture->GetMixFormat(&pwfex);
	if (FAILED(hr)) {
		ShutdownEndpoint();
		return false;
	}

	mMixFormat.assign(pwfex, sizeof(WAVEFORMATEX) + pwfex->cbSize);

	mBytesPerFrame = mMixFormat->nBlockAlign;
	mChannels = mMixFormat->nChannels;

	CoTaskMemFree(pwfex);

	return true;
}

void VDAudioGrabberWASAPI::ShutdownEndpoint() {
	vdsaferelease <<= mpAudioCaptureClient;
	vdsaferelease <<= mpMMDevice;
}

bool VDAudioGrabberWASAPI::InitLocal() {
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	if (FAILED(hr))
		return false;

	if (!InitEndpoint())
		return false;

	mhEventRender = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (!mhEventRender) {
		Shutdown();
		return false;
	}

	hr = mpMMDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void **)&mpAudioClientRender);
	if (FAILED(hr)) {
		Shutdown();
		return false;
	}

	// init the render pipeline for silence
	WAVEFORMATEX silenceFormat = {};
	silenceFormat.wFormatTag = WAVE_FORMAT_PCM;
	silenceFormat.nChannels = mMixFormat->nChannels;
	silenceFormat.nSamplesPerSec = mMixFormat->nSamplesPerSec;
	silenceFormat.nAvgBytesPerSec = silenceFormat.nSamplesPerSec * silenceFormat.nChannels;
	silenceFormat.nBlockAlign = silenceFormat.nChannels;
	silenceFormat.wBitsPerSample = 8;
	silenceFormat.cbSize = 0;

	const REFERENCE_TIME halfSecond = 5000000;

	hr = mpAudioClientRender->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, halfSecond, 0, &silenceFormat, NULL);
	if (FAILED(hr)) {
		Shutdown();
		return false;
	}

	hr = mpAudioClientCapture->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, halfSecond, 0, &*mMixFormat, NULL);
	if (FAILED(hr)) {
		Shutdown();
		return false;
	}

	UINT32 outputBufferFrames;
	hr = mpAudioClientRender->GetBufferSize(&outputBufferFrames);
	if (FAILED(hr)) {
		Shutdown();
		return false;
	}

	mOutputBufferFrames = outputBufferFrames;

	hr = mpAudioClientRender->GetService(__uuidof(IAudioRenderClient), (void **)&mpAudioRenderClient);
	if (FAILED(hr)) {
		Shutdown();
		return false;
	}

	hr = mpAudioClientCapture->GetService(__uuidof(IAudioCaptureClient), (void **)&mpAudioCaptureClient);
	if (FAILED(hr)) {
		Shutdown();
		return false;
	}

	hr = mpAudioClientRender->SetEventHandle(mhEventRender);
	if (FAILED(hr)) {
		Shutdown();
		return false;
	}

	mTransferBuffer.Init(mMixFormat->nAvgBytesPerSec);

	return true;
}

void VDAudioGrabberWASAPI::RunLocal() {
	HRESULT hr;

	HANDLE h[2] = { mhEventRender, mStateChangeRequested.getHandle() };

	for(;;) {
		const DWORD waitResult = WaitForMultipleObjects(2, h, FALSE, mCurrentState == kStateRunning ? 100 : INFINITE);

		if (waitResult == WAIT_OBJECT_0) {
			for(;;) {
				UINT32 paddingFrames;
				hr = mpAudioClientRender->GetCurrentPadding(&paddingFrames);
				if (FAILED(hr))
					break;

				UINT32 avail = mOutputBufferFrames - paddingFrames;
				if (!avail)
					break;

				BYTE *pData;
				hr = mpAudioRenderClient->GetBuffer(avail, &pData);
				if (SUCCEEDED(hr))
					mpAudioRenderClient->ReleaseBuffer(avail, AUDCLNT_BUFFERFLAGS_SILENT);
			}
		} else if (waitResult == WAIT_OBJECT_0 + 1) {
			while(mCurrentState != mRequestedState) {
				switch(mCurrentState) {
					case kStateInited:
					case kStateStartFailed:
						if (mRequestedState == kStateNone) {
							mCurrentState = kStateNone;
							mStateChangeCompleted.signal();
							goto xit;
						}

						if (mRequestedState == kStateRunning) {
							// start streams
							hr = mpAudioClientCapture->Start();
							if (SUCCEEDED(hr))
								hr = mpAudioClientRender->Start();

							if (SUCCEEDED(hr))
								mCurrentState = kStateRunning;
							else
								mCurrentState = kStateStartFailed;

							mStateChangeCompleted.signal();
						}

						if (mRequestedState == kStateInited) {
							mCurrentState = kStateInited;
							mStateChangeCompleted.signal();
						}

						break;

					case kStateRunning:
						if (mRequestedState != kStateRunning) {
							mpAudioClientRender->Stop();
							mpAudioClientCapture->Stop();

							mCurrentState = kStateInited;
							mStateChangeCompleted.signal();
						}
						break;
				}
			}
		}

		if (mCurrentState == kStateRunning) {
			for(;;) {
				UINT32 frames;
				hr = mpAudioCaptureClient->GetNextPacketSize(&frames);

				if (FAILED(hr))
					break;

				if (!frames)
					break;

				BYTE *pData;
				UINT32 framesToRead;
				DWORD flags;
				UINT64 devpos;
				UINT64 qpc;
				hr = mpAudioCaptureClient->GetBuffer(&pData, &framesToRead, &flags, &devpos, &qpc);
				if (SUCCEEDED(hr)) {
					const tpVDConvertPCM convert = VDGetPCMConversionVtable()[kVDAudioSampleType32F][kVDAudioSampleType16S];
					uint32 samples = framesToRead * mChannels;

					const float *src = (const float *)pData;
					while(samples) {
						int tc;

						void *dst = mTransferBuffer.LockWrite(samples, tc);
						if (!tc)
							break;

						convert(dst, src, tc);

						mTransferBuffer.UnlockWrite(tc);

						samples -= tc;
						src += tc;
					}

					mpCB->ReceiveAudioDataWASAPI();

					mpAudioCaptureClient->ReleaseBuffer(framesToRead);
				}
			}
		}
	}

xit:
	;
}

void VDAudioGrabberWASAPI::ShutdownLocal() {
	vdsaferelease <<= mpAudioRenderClient;

	if (mpAudioClientRender) {
		mpAudioClientRender->Stop();
		mpAudioClientRender->Reset();
		mpAudioClientRender->Release();
		mpAudioClientRender = NULL;
	}

	if (mpAudioClientCapture) {
		mpAudioClientCapture->Stop();
		mpAudioClientCapture->Reset();
		mpAudioClientCapture->Release();
		mpAudioClientCapture = NULL;
	}

	if (mhEventRender) {
		CloseHandle(mhEventRender);
		mhEventRender = NULL;
	}

	ShutdownEndpoint();

	CoUninitialize();
}
