;	VirtualDub - Video processing and capture application
;	A/V interface library	
;	Copyright (C) 1998-2008 Avery Lee
;
;	This program is free software; you can redistribute it and/or modify
;	it under the terms of the GNU General Public License as published by
;	the Free Software Foundation; either version 2 of the License, or
;	(at your option) any later version.
;
;	This program is distributed in the hope that it will be useful,
;	but WITHOUT ANY WARRANTY; without even the implied warranty of
;	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;	GNU General Public License for more details.
;
;	You should have received a copy of the GNU General Public License
;	along with this program; if not, write to the Free Software
;	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

		extern		_gVDCaptureAudioResamplingKernel : near

	segment	.rdata, align=16

rounder	dd			00002000h

	segment	.text

	global	_vdasm_capture_resample16_MMX
_vdasm_capture_resample16_MMX:

		push		ebp
		push		edi
		push		esi
		push		ebx

		mov			edx, [esp+4+16]		;edx = destination
		mov			ebp, [esp+8+16]		;ebp = stride
		mov			ecx, [esp+12+16]	;ecx = source
		mov			eax, [esp+16+16]	;eax = counter
		shr			ecx, 1
		add			ebp, ebp			;convert sample stride to pointer stride
		mov			esi, [esp+20+16]	;esi = fractional accumulator
		add			ecx, [esp+24+16]	;add integer accumulator
		mov			ebx, [esp+32+16]	;ebx = integer increment

		push		0
		push		dword [fs:0]
		mov			dword [fs:0], esp

		mov			esp, [esp+28+24]	;esp = fractional increment

		movd		mm6, [rounder]

		;eax		loop counter
		;ebx		integer increment
		;ecx		source
		;edx		destination
		;esi		fractional accumulator
		;edi		current filter
		;esp		fractional increment
		;ebp		destination stride

		mov			edi, esi
		shr			edi, 23
		and			edi, 1f0h
		add			edi, _gVDCaptureAudioResamplingKernel

xloop:
		movq		mm0, [ecx+ecx]
		pmaddwd		mm0, [edi]
		movq		mm1, [ecx+ecx+8]
		pmaddwd		mm1, [edi+8]
		paddd		mm0, mm1
		movq		mm1, mm0
		psrlq		mm0, 32
		paddd		mm0, mm1
		paddd		mm0, mm6
		psrad		mm0, 14
		packssdw	mm0, mm0
		movd		edi, mm0
		mov			[edx], di

		add			esi, esp
		adc			ecx, ebx
		mov			edi, esi
		shr			edi, 23
		add			edx, ebp
		and			edi, 1f0h
		add			edi, _gVDCaptureAudioResamplingKernel

		sub			eax, 1
		jne			xloop

		mov			esp, dword [fs:0]
		pop			dword [fs:0]
		pop			eax

		emms
		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		ret


		end
