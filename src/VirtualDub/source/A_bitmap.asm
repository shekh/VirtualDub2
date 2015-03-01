;	VirtualDub - Video processing and capture application
;	Copyright (C) 1998-2001 Avery Lee
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

	extern	_MMX_enabled:byte
	extern	_FPU_enabled:byte

	segment	.rdata, align=16

bilinFPU_tbl:
	dd	256.0,  0.0,  0.0,  0.0,240.0, 16.0,  0.0,  0.0,224.0, 32.0,  0.0,  0.0,208.0, 48.0,  0.0,  0.0
	dd	192.0, 64.0,  0.0,  0.0,176.0, 80.0,  0.0,  0.0,160.0, 96.0,  0.0,  0.0,144.0,112.0,  0.0,  0.0
	dd	128.0,128.0,  0.0,  0.0,112.0,144.0,  0.0,  0.0, 96.0,160.0,  0.0,  0.0, 80.0,176.0,  0.0,  0.0
	dd	 64.0,192.0,  0.0,  0.0, 48.0,208.0,  0.0,  0.0, 32.0,224.0,  0.0,  0.0, 16.0,240.0,  0.0,  0.0

	dd	240.0,  0.0, 16.0,  0.0,225.0, 15.0, 15.0,  1.0,210.0, 30.0, 14.0,  2.0,195.0, 45.0, 13.0,  3.0
	dd	180.0, 60.0, 12.0,  4.0,165.0, 75.0, 11.0,  5.0,150.0, 90.0, 10.0,  6.0,135.0,105.0,  9.0,  7.0
	dd	120.0,120.0,  8.0,  8.0,105.0,135.0,  7.0,  9.0, 90.0,150.0,  6.0, 10.0, 75.0,165.0,  5.0, 11.0
	dd	 60.0,180.0,  4.0, 12.0, 45.0,195.0,  3.0, 13.0, 30.0,210.0,  2.0, 14.0, 15.0,225.0,  1.0, 15.0

	dd	224.0,  0.0, 32.0,  0.0,210.0, 14.0, 30.0,  2.0,196.0, 28.0, 28.0,  4.0,182.0, 42.0, 26.0,  6.0
	dd	168.0, 56.0, 24.0,  8.0,154.0, 70.0, 22.0, 10.0,140.0, 84.0, 20.0, 12.0,126.0, 98.0, 18.0, 14.0
	dd	112.0,112.0, 16.0, 16.0, 98.0,126.0, 14.0, 18.0, 84.0,140.0, 12.0, 20.0, 70.0,154.0, 10.0, 22.0
	dd	 56.0,168.0,  8.0, 24.0, 42.0,182.0,  6.0, 26.0, 28.0,196.0,  4.0, 28.0, 14.0,210.0,  2.0, 30.0

	dd	208.0,  0.0, 48.0,  0.0,195.0, 13.0, 45.0,  3.0,182.0, 26.0, 42.0,  6.0,169.0, 39.0, 39.0,  9.0
	dd	156.0, 52.0, 36.0, 12.0,143.0, 65.0, 33.0, 15.0,130.0, 78.0, 30.0, 18.0,117.0, 91.0, 27.0, 21.0
	dd	104.0,104.0, 24.0, 24.0, 91.0,117.0, 21.0, 27.0, 78.0,130.0, 18.0, 30.0, 65.0,143.0, 15.0, 33.0
	dd	 52.0,156.0, 12.0, 36.0, 39.0,169.0,  9.0, 39.0, 26.0,182.0,  6.0, 42.0, 13.0,195.0,  3.0, 45.0

	dd	192.0,  0.0, 64.0,  0.0,180.0, 12.0, 60.0,  4.0,168.0, 24.0, 56.0,  8.0,156.0, 36.0, 52.0, 12.0
	dd	144.0, 48.0, 48.0, 16.0,132.0, 60.0, 44.0, 20.0,120.0, 72.0, 40.0, 24.0,108.0, 84.0, 36.0, 28.0
	dd	 96.0, 96.0, 32.0, 32.0, 84.0,108.0, 28.0, 36.0, 72.0,120.0, 24.0, 40.0, 60.0,132.0, 20.0, 44.0
	dd	 48.0,144.0, 16.0, 48.0, 36.0,156.0, 12.0, 52.0, 24.0,168.0,  8.0, 56.0, 12.0,180.0,  4.0, 60.0

	dd	176.0,  0.0, 80.0,  0.0,165.0, 11.0, 75.0,  5.0,154.0, 22.0, 70.0, 10.0,143.0, 33.0, 65.0, 15.0
	dd	132.0, 44.0, 60.0, 20.0,121.0, 55.0, 55.0, 25.0,110.0, 66.0, 50.0, 30.0, 99.0, 77.0, 45.0, 35.0
	dd	 88.0, 88.0, 40.0, 40.0, 77.0, 99.0, 35.0, 45.0, 66.0,110.0, 30.0, 50.0, 55.0,121.0, 25.0, 55.0
	dd	 44.0,132.0, 20.0, 60.0, 33.0,143.0, 15.0, 65.0, 22.0,154.0, 10.0, 70.0, 11.0,165.0,  5.0, 75.0

	dd	160.0,  0.0, 96.0,  0.0,150.0, 10.0, 90.0,  6.0,140.0, 20.0, 84.0, 12.0,130.0, 30.0, 78.0, 18.0
	dd	120.0, 40.0, 72.0, 24.0,110.0, 50.0, 66.0, 30.0,100.0, 60.0, 60.0, 36.0, 90.0, 70.0, 54.0, 42.0
	dd	 80.0, 80.0, 48.0, 48.0, 70.0, 90.0, 42.0, 54.0, 60.0,100.0, 36.0, 60.0, 50.0,110.0, 30.0, 66.0
	dd	 40.0,120.0, 24.0, 72.0, 30.0,130.0, 18.0, 78.0, 20.0,140.0, 12.0, 84.0, 10.0,150.0,  6.0, 90.0

	dd	144.0,  0.0,112.0,  0.0,135.0,  9.0,105.0,  7.0,126.0, 18.0, 98.0, 14.0,117.0, 27.0, 91.0, 21.0
	dd	108.0, 36.0, 84.0, 28.0, 99.0, 45.0, 77.0, 35.0, 90.0, 54.0, 70.0, 42.0, 81.0, 63.0, 63.0, 49.0
	dd	 72.0, 72.0, 56.0, 56.0, 63.0, 81.0, 49.0, 63.0, 54.0, 90.0, 42.0, 70.0, 45.0, 99.0, 35.0, 77.0
	dd	 36.0,108.0, 28.0, 84.0, 27.0,117.0, 21.0, 91.0, 18.0,126.0, 14.0, 98.0,  9.0,135.0,  7.0,105.0

	dd	128.0,  0.0,128.0,  0.0,120.0,  8.0,120.0,  8.0,112.0, 16.0,112.0, 16.0,104.0, 24.0,104.0, 24.0
	dd	 96.0, 32.0, 96.0, 32.0, 88.0, 40.0, 88.0, 40.0, 80.0, 48.0, 80.0, 48.0, 72.0, 56.0, 72.0, 56.0
	dd	 64.0, 64.0, 64.0, 64.0, 56.0, 72.0, 56.0, 72.0, 48.0, 80.0, 48.0, 80.0, 40.0, 88.0, 40.0, 88.0
	dd	 32.0, 96.0, 32.0, 96.0, 24.0,104.0, 24.0,104.0, 16.0,112.0, 16.0,112.0,  8.0,120.0,  8.0,120.0

	dd	112.0,  0.0,144.0,  0.0,105.0,  7.0,135.0,  9.0, 98.0, 14.0,126.0, 18.0, 91.0, 21.0,117.0, 27.0
	dd	 84.0, 28.0,108.0, 36.0, 77.0, 35.0, 99.0, 45.0, 70.0, 42.0, 90.0, 54.0, 63.0, 49.0, 81.0, 63.0
	dd	 56.0, 56.0, 72.0, 72.0, 49.0, 63.0, 63.0, 81.0, 42.0, 70.0, 54.0, 90.0, 35.0, 77.0, 45.0, 99.0
	dd	 28.0, 84.0, 36.0,108.0, 21.0, 91.0, 27.0,117.0, 14.0, 98.0, 18.0,126.0,  7.0,105.0,  9.0,135.0

	dd	 96.0,  0.0,160.0,  0.0, 90.0,  6.0,150.0, 10.0, 84.0, 12.0,140.0, 20.0, 78.0, 18.0,130.0, 30.0
	dd	 72.0, 24.0,120.0, 40.0, 66.0, 30.0,110.0, 50.0, 60.0, 36.0,100.0, 60.0, 54.0, 42.0, 90.0, 70.0
	dd	 48.0, 48.0, 80.0, 80.0, 42.0, 54.0, 70.0, 90.0, 36.0, 60.0, 60.0,100.0, 30.0, 66.0, 50.0,110.0
	dd	 24.0, 72.0, 40.0,120.0, 18.0, 78.0, 30.0,130.0, 12.0, 84.0, 20.0,140.0,  6.0, 90.0, 10.0,150.0

	dd	 80.0,  0.0,176.0,  0.0, 75.0,  5.0,165.0, 11.0, 70.0, 10.0,154.0, 22.0, 65.0, 15.0,143.0, 33.0
	dd	 60.0, 20.0,132.0, 44.0, 55.0, 25.0,121.0, 55.0, 50.0, 30.0,110.0, 66.0, 45.0, 35.0, 99.0, 77.0
	dd	 40.0, 40.0, 88.0, 88.0, 35.0, 45.0, 77.0, 99.0, 30.0, 50.0, 66.0,110.0, 25.0, 55.0, 55.0,121.0
	dd	 20.0, 60.0, 44.0,132.0, 15.0, 65.0, 33.0,143.0, 10.0, 70.0, 22.0,154.0,  5.0, 75.0, 11.0,165.0

	dd	 64.0,  0.0,192.0,  0.0, 60.0,  4.0,180.0, 12.0, 56.0,  8.0,168.0, 24.0, 52.0, 12.0,156.0, 36.0
	dd	 48.0, 16.0,144.0, 48.0, 44.0, 20.0,132.0, 60.0, 40.0, 24.0,120.0, 72.0, 36.0, 28.0,108.0, 84.0
	dd	 32.0, 32.0, 96.0, 96.0, 28.0, 36.0, 84.0,108.0, 24.0, 40.0, 72.0,120.0, 20.0, 44.0, 60.0,132.0
	dd	 16.0, 48.0, 48.0,144.0, 12.0, 52.0, 36.0,156.0,  8.0, 56.0, 24.0,168.0,  4.0, 60.0, 12.0,180.0

	dd	 48.0,  0.0,208.0,  0.0, 45.0,  3.0,195.0, 13.0, 42.0,  6.0,182.0, 26.0, 39.0,  9.0,169.0, 39.0
	dd	 36.0, 12.0,156.0, 52.0, 33.0, 15.0,143.0, 65.0, 30.0, 18.0,130.0, 78.0, 27.0, 21.0,117.0, 91.0
	dd	 24.0, 24.0,104.0,104.0, 21.0, 27.0, 91.0,117.0, 18.0, 30.0, 78.0,130.0, 15.0, 33.0, 65.0,143.0
	dd	 12.0, 36.0, 52.0,156.0,  9.0, 39.0, 39.0,169.0,  6.0, 42.0, 26.0,182.0,  3.0, 45.0, 13.0,195.0

	dd	 32.0,  0.0,224.0,  0.0, 30.0,  2.0,210.0, 14.0, 28.0,  4.0,196.0, 28.0, 26.0,  6.0,182.0, 42.0
	dd	 24.0,  8.0,168.0, 56.0, 22.0, 10.0,154.0, 70.0, 20.0, 12.0,140.0, 84.0, 18.0, 14.0,126.0, 98.0
	dd	 16.0, 16.0,112.0,112.0, 14.0, 18.0, 98.0,126.0, 12.0, 20.0, 84.0,140.0, 10.0, 22.0, 70.0,154.0
	dd	  8.0, 24.0, 56.0,168.0,  6.0, 26.0, 42.0,182.0,  4.0, 28.0, 28.0,196.0,  2.0, 30.0, 14.0,210.0

	dd	 16.0,  0.0,240.0,  0.0, 15.0,  1.0,225.0, 15.0, 14.0,  2.0,210.0, 30.0, 13.0,  3.0,195.0, 45.0
	dd	 12.0,  4.0,180.0, 60.0, 11.0,  5.0,165.0, 75.0, 10.0,  6.0,150.0, 90.0,  9.0,  7.0,135.0,105.0
	dd	  8.0,  8.0,120.0,120.0,  7.0,  9.0,105.0,135.0,  6.0, 10.0, 90.0,150.0,  5.0, 11.0, 75.0,165.0
	dd	  4.0, 12.0, 60.0,180.0,  3.0, 13.0, 45.0,195.0,  2.0, 14.0, 30.0,210.0,  1.0, 15.0, 15.0,225.0


bilinMMX_tab1	dq	0000000000000000h
		dq	0001000100010001h
		dq	0002000200020002h
		dq	0003000300030003h
		dq	0004000400040004h
		dq	0005000500050005h
		dq	0006000600060006h
		dq	0007000700070007h
		dq	0008000800080008h
		dq	0009000900090009h
		dq	000a000a000a000ah
		dq	000b000b000b000bh
		dq	000c000c000c000ch
		dq	000d000d000d000dh
		dq	000e000e000e000eh
		dq	000f000f000f000fh

bilinMMX_tab2	dq	0010001000100010h
		dq	000f000f000f000fh
		dq	000e000e000e000eh
		dq	000d000d000d000dh
		dq	000c000c000c000ch
		dq	000b000b000b000bh
		dq	000a000a000a000ah
		dq	0009000900090009h
		dq	0008000800080008h
		dq	0007000700070007h
		dq	0006000600060006h
		dq	0005000500050005h
		dq	0004000400040004h
		dq	0003000300030003h
		dq	0002000200020002h
		dq	0001000100010001h

zero	dq	0000000000000000h
sixteen	dq	0010001000100010h


	segment	.text

;**************************************************************************
;
;asm_resize_nearest(
;	[esp+ 4] Pixel32 *dst + width,
;	[esp+ 8] Pixel32 *src,
;	[esp+12] ulong -width*4,
;	[esp+16] ulong height,
;	[esp+20] ulong srcpitch,
;	[esp+24] ulong dstpitch,
;	[esp+28] ulong xaccum,
;	[esp+32] ulong yaccum,
;	[esp+36] ulong xfrac,
;	[esp+40] ulong yfrac,
;	[esp+44] ulong xistep,
;	[esp+48] ulong yistep,
;	[esp+52] Pixel32 *precopysrc,
;	[esp+56] ulong precopy,
;	[esp+60] Pixel32 *postcopysrc,
;	[esp+64] ulong postcopy,
;	);
;
;**************************************************************************

	global	_asm_resize_nearest	

_asm_resize_nearest:
	push	ebp
	push	edi
	push	esi
	push	ebx

	mov	edi,[esp+ 4+16]
rowloop_nearest:
	mov	ecx,[esp+56+16]		;ecx = precopy count
	mov	esi,[esp+52+16]		;esi = precopy ptr
	or	ecx,ecx
	jz	rowloop_no_precopy
	add	edi,[esp+12+16]
	mov	eax,[esi]		;load precopy pixel
	std
	sub	edi,4
	rep	stosd			;do precopy
	cld
rowloop_no_precopy:

	;EAX
	;EBX	accumulator
	;ECX	fractional increment
	;EDX	integer increment
	;ESI	source
	;EDI	destination
	;EBP	loop counter

	mov	esi,[esp+ 8+16]
	mov	ebp,[esp+12+16]
	shr	esi,2
	mov	edx,[esp+44+16]
	mov	ecx,[esp+36+16]
	mov	ebx,[esp+28+16]
	mov	edi,[esp+ 4+16]
colloop_nearest:
	mov	eax,[esi*4]		;1u
	add	ebx,ecx			;1v
	adc	esi,edx			;2u
	mov	[edi+ebp],eax		;2v
	add	ebp,4			;3u
	jne	colloop_nearest		;3v

	mov	ecx,[esp+64+16]		;ecx = postcopy count
	mov	esi,[esp+60+16]		;esi = postcopy ptr
	or	ecx,ecx
	jz	rowloop_no_postcopy
	mov	eax,[esi]		;load precopy pixel
	rep	stosd			;do precopy
rowloop_no_postcopy:

	mov	edi,[esp+ 4+16]

	mov	esi,[esp+ 8+16]		;esi = source pointer
	mov	ecx,[esp+52+16]		;ecx = precopy pointer

	mov	edx,[esp+60+16]		;edx = postcopy pointer
	mov	eax,[esp+32+16]		;get y accumulator

	add	esi,[esp+48+16]		;add integer source bump
	add	ecx,[esp+48+16]		;add integer source bump

	add	edx,[esp+48+16]		;add integer source bump
	add	eax,[esp+40+16]		;add y fraction

	sbb	ebx,ebx			;ebx = -1 if need fractional step
	mov	[esp+32+16],eax		;store new y accumulator

	and	ebx,[esp+24+16]		;ebx = fractional step
	add	edi,[esp+20+16]		;advance dest to next row

	add	esi,ebx			;add y fractional step
	add	ecx,ebx			;add y fractional step

	add	edx,ebx			;add y fractional step
	mov	ebp,[esp+16+16]		;get y counter

	mov	[esp+ 8+16],esi		;store new source ptr
	mov	[esp+52+16],ecx		;store new source ptr

	mov	[esp+60+16],edx		;store new source ptr
	dec	ebp			;decrement y counter

	mov	[esp+16+16],ebp		;store y counter
	mov	[esp+ 4+16],edi		;store dest pointer

	jne	rowloop_nearest		;continue until all rows done

	pop	ebx
	pop	esi
	pop	edi
	pop	ebp
	ret

	global	_asm_resize_bilinear	

;**************************************************************************
;
;asm_resize_bilinear(
;	[esp+ 4] void *dst,
;	[esp+ 8] void *src,
;	[esp+12] ulong w,
;	[esp+16] ulong h,
;	[esp+20] ulong dstpitch,
;	[esp+24] ulong srcpitch,
;	[esp+28] ulong xaccum,
;	[esp+32] ulong yaccum,
;	[esp+36] ulong xfrac,
;	[esp+40] ulong yfrac,
;	[esp+44] long xint,
;	[esp+48] long yint,
;	[esp+52] void *srcprecopy,
;	[esp+56] ulong xprecopy,
;	[esp+60] void *srcpostcopy,
;	[esp+64] ulong xpostcopy);
;
;**************************************************************************


_asm_resize_bilinear:
	test	byte [_MMX_enabled], 1
	jnz	asm_resize_bilinear_MMX

	test	byte [_FPU_enabled], 1
	jnz	asm_resize_bilinear_FPU

	push	ebp
	push	edi
	push	esi
	push	ebx

	sub	esp,32

	;compute y_frac, y_frac2

	mov	eax,[esp+32+16+32]
	mov	ebx,16
	shr	eax,28
	mov	[esp+24],eax
	sub	ebx,eax
	mov	[esp+28],ebx

;
;	[esp+28] y_frac2
;	[esp+24] y_frac
;	[esp+20] dest ptr
;	[esp+16] tstore2
;	[esp+12] tstore1
;	[esp+ 8] x_accum
;	[esp+ 4] x_frac2
;	[esp+ 0] x_frac

rowloop_bilinear:
	mov	ebp,[esp+12+16+32]		;ebp = -w*4
	mov	eax,[esp+28+16+32]		;compute x_frac, x_frac2
	mov	ebx,16
	mov	[esp+8],eax
	shr	eax,28
	mov	[esp],eax			;x_frac
	sub	ebx,eax
	mov	[esp+4],ebx			;x_frac2 = 16-x_frac

	mov	esi,[esp+ 8+16+32]		;load source ptr
	mov	edx,[esp+4+16+32]		;load dest pointer

	mov	edi,[esp+24+16+32]		;load source pitch
	mov	[esp+20],edx			;store dst ptr in temporary

	shr	esi,2
	mov	ecx,[esp+56+16+32]		;load precopy value
	or	ecx,ecx
	jz	colloop_bilinear_start

	;do precopy

	push	esi
	mov	esi,[esp+52+16+32+4]
	add	edx,ebp
	mov	[esp+20+4],edx
	mov	ebp,ecx

	call	bilinear_prepostcopy
	pop	esi

	mov	ebp,[esp+12+16+32]
	mov	edx,[esp+4+16+32]
	mov	[esp+20],edx

colloop_bilinear_start:
	or	ebp,ebp
	jz	bilinear_check_postcopy
colloop_bilinear:
	mov	eax,[esi*4]
	mov	ecx,[esi*4+4]
	mov	ebx,eax
	mov	edx,ecx
	and	eax,00ff00ffh
	and	ebx,0000ff00h
	and	ecx,00ff00ffh
	and	edx,0000ff00h
	imul	eax,[esp+4]		;x_frac2
	imul	ebx,[esp+4]		;x_frac2
	imul	ecx,[esp]		;x_frac
	imul	edx,[esp]		;x_frac
	add	eax,ecx
	add	ebx,edx
	imul	eax,[esp+28]		;y_frac2
	imul	ebx,[esp+28]		;y_frac2
	mov	[esp+12],eax		;tstore1
	mov	[esp+16],ebx		;tstore2

	mov	eax,[esi*4+edi]
	mov	ecx,[esi*4+edi+4]
	mov	ebx,eax
	mov	edx,ecx
	and	eax,00ff00ffh
	and	ebx,0000ff00h
	and	ecx,00ff00ffh
	and	edx,0000ff00h
	imul	eax,[esp+4]		;x_frac2
	imul	ebx,[esp+4]		;x_frac2
	imul	ecx,[esp]		;x_frac
	imul	edx,[esp]		;x_frac
	add	eax,ecx
	add	ebx,edx
	imul	eax,[esp+24]		;y_frac
	imul	ebx,[esp+24]		;y_frac
	add	eax,[esp+12]		;tstore1
	add	ebx,[esp+16]		;tstore2

	shr	eax,8
	and	ebx,00ff0000h
	shr	ebx,8
	and	eax,00ff00ffh

	or	eax,ebx			;[data write ] u
	mov	edx,[esp+20]		;[data write ] v

	mov	ebx,[esp+8]		;[frac update] u x_accum
	mov	ecx,[esp+36+16+32]	;[frac update] v xfrac

	mov	[edx+ebp],eax		;[data write ] u
	add	ebx,ecx			;[frac update] v: update x_accum

	adc	esi,[esp+44+16+32]	;[frac update] v: update source pointer [2 cycles]
	mov	[esp+8],ebx		;[frac update] u: store x_accum

	shr	ebx,28			;[frac update] u: x_frac = x_accum>>28
	mov	eax,16			;[frac update] v:

	sub	eax,ebx			;[frac update] u: x_frac2 = 16 - x_frac
	mov	[esp],ebx		;[frac update] v: store x_frac

	mov	[esp+4],eax		;[frac update] u: store x_frac2

	add	ebp,4
	jne	colloop_bilinear

bilinear_check_postcopy:
	mov	ebp,[esp+64+16+32]		;check for postcopy
	or	ebp,ebp
	jz	bilinear_no_postcopy

	push	esi
	mov	esi,[esp+60+16+36]
	sub	edx,ebp
	mov	[esp+20+4],edx

	call	bilinear_prepostcopy
	pop	esi

bilinear_no_postcopy:
	mov	eax,[esp+32+16+32]		;load yaccum
	mov	edx,[esp+ 4+16+32]		;load destination ptr

	add	edx,[esp+20+16+32]		;next destination line
	add	eax,[esp+40+16+32]		;add yfrac increment to yaccum

	sbb	ebx,ebx				;ebx=-1 if need fractional src increment
	mov	esi,[esp+ 8+16+32]		;reload source ptr

	mov	ecx,[esp+52+16+32]		;load precopy source ptr
	mov	ebp,[esp+60+16+32]		;load postcopy source ptr

	add	esi,[esp+48+16+32]		;add integral source increment
	add	ecx,[esp+48+16+32]		;add integral source increment
	add	ebp,[esp+48+16+32]		;add integral source increment
	and	ebx,[esp+24+16+32]		;ebx = fractional src delta

	add	esi,ebx				;add fractional source increment
	add	ecx,ebx				;add fractional source increment
	add	ebp,ebx				;add fractional source increment
	mov	[esp+ 4+16+32],edx		;store destination ptr
	mov	[esp+ 8+16+32],esi		;store source ptr
	mov	[esp+52+16+32],ecx		;store source ptr
	mov	[esp+60+16+32],ebp		;store source ptr
	mov	[esp+32+16+32],eax		;store yaccum
	shr	eax,28
	mov	ebx,16
	mov	[esp+24],eax
	sub	ebx,eax
	mov	[esp+28],ebx

	dec	dword [esp+16+16+32]	;next line!!
	jne	rowloop_bilinear

	add	esp,32

	pop	ebx
	pop	esi
	pop	edi
	pop	ebp
	ret

bilinear_prepostcopy:
	mov	eax,[esi]
	mov	ecx,[esi+edi]
	mov	ebx,eax
	mov	edx,ecx
	and	eax,00ff00ffh
	and	ebx,0000ff00h
	and	ecx,00ff00ffh
	and	edx,0000ff00h
	imul	eax,[esp+28+8]		;y_frac2
	imul	ebx,[esp+28+8]		;y_frac2
	imul	ecx,[esp+24+8]		;y_frac
	imul	edx,[esp+24+8]		;y_frac
	add	eax,ecx
	add	ebx,edx

	shr	eax,4
	and	ebx,00ff0000h
	shr	ebx,4
	and	eax,00ff00ffh

	or	eax,ebx			;[data write ] u
	mov	edx,[esp+20+8]		;[data write ] v

	mov	[edx+ebp],eax		;[data write ] u

	add	ebp,4
	jne	bilinear_prepostcopy

	ret

;
;
;******* FPU optimized version.
;
;

real80_adjust	dt	2361183241434822606848.0	;shift right by 8
real80_adjust16	dt	147573952589676412928.0

asm_resize_bilinear_FPU:
	push	ebp
	push	edi
	push	esi
	push	ebx

	mov	eax,esp
	and	esp,-32

.LOCALS	equ 96

	sub	esp,.LOCALS
	mov	[esp+.LOCALS-4],eax

        ;copy down parameters.

	lea	esi,[eax+20]
	lea	edi,[esp+24]
	mov	ecx,16
	rep	movsd

	;flip the FPU into 80-bit, round-down mode.

	fstcw	[esp+.LOCALS-8]
	mov	eax,[esp+.LOCALS-8]
	and	eax,0fffff0ffh
	or	eax,000000700h
	mov	[esp],eax
	fldcw	[esp]

	;prime FPU stack.

	fld		tword [real80_adjust]

;******************************************************
;
;	[esp+ 92] original esp
;	[esp+ 88] old FP control word
;	[esp+ 84] xpostcopy
;	[esp+ 80] srcpostcopy
;	[esp+ 76] xprecopy
;	[esp+ 72] srcprecopy
;	[esp+ 68] yint
;	[esp+ 64] xint
;	[esp+ 60] yfrac
;	[esp+ 56] xfrac
;	[esp+ 52] yaccum
;	[esp+ 48] xaccum
;	[esp+ 44] srcstride
;	[esp+ 40] dststride
;	[esp+ 36] height
;	[esp+ 32] width
;	[esp+ 28] src
;	[esp+ 24] dst
;	[esp+ 20] xaccum'
;	[esp+ 16] FPU coefficient pointer
;	[esp+ 12]
;	[esp+  8]
;	[esp+  4]
;	[esp+  0]

.rowloop_bilinear_FPU:
	mov	ebp,[esp+32]		;load width count
	mov	eax,[esp+48]		;copy xaccum
	mov	[esp+20],eax		;xaccum' = xaccum

	mov	edi,[esp+44]		;load source stride

	mov	eax,[esp+52]		;eax = yaccum
	and	eax,0f0000000h
	shr	eax,20
	add	eax, bilinFPU_tbl
	mov	[esp+16],eax

	;check for precopy

	mov	ecx,[esp+76]
	or	ecx,ecx
	jz	.colloop_bilinear_start_FPU

	;do precopy

	mov	edx,[esp+24]
	add	edx,ecx
	mov	ebp,ecx
	mov	[esp+24],edx
	mov	esi,[esp+72]

	call	.bilinear_prepostcopy_FPU

	mov	ebp,[esp+76]
	sub	edx,ebp
	mov	[esp+24],edx
	mov	ebp,[esp+32]

.colloop_bilinear_start_FPU:
	or	ebp,ebp
	jz	.bilinear_check_postcopy_FPU

	mov	esi,[esp+28]		;load source pointer
	mov	bl,[esp+23]		;cl = xaccum>>24
	shr	esi,2			;divide source ptr by 4 (!)
	mov	edx,[esp+16]		;edx = rowbase
	and	ebx,000000f0h		;ecx = x-offset in rowbase
	add	edx,ebx

.colloop_bilinear_FPU:
	mov	eax,[esi*4]
	mov	ecx,[esi*4+4]

	mov	ebx,eax
	and	eax,00ff00ffh

	and	ebx,0000ff00h
	mov	[esp+0],eax

	mov	[esp+4],ebx
	mov	ebx,ecx

	fild	qword [esp+0]	;stack: x1 cv

	and	ecx,00ff00ffh
	and	ebx,0000ff00h

	mov	[esp+8],ecx
	mov	[esp+12],ebx

	fmul	dword [edx+0]	;stack: y1 cv
	fild	qword [esp+8]	;stack: x2 y1 cv

	mov	eax,[esi*4+edi]
	mov	ecx,[esi*4+edi+4]

	mov	ebx,eax
	and	eax,00ff00ffh

	fmul	dword [edx+4]	;stack: y2 y1 cv
	fxch	st1			;stack: y1 y2 cv
	fadd	st0,st2		;stack: (y1+cv) y2 cv

	and	ebx,0000ff00h
	mov	[esp+0],eax

	mov	[esp+4],ebx
	mov	ebx,ecx

	fild	qword [esp+0]	;stack: x3 (y1+cv) y2 cv
	fxch	st1			;stack: (y1+cv) x3 y2 cv
	faddp	st2,st0		;stack: x3 (y1+y2+cv) cv

	and	ecx,00ff00ffh
	and	ebx,0000ff00h

	fmul	dword [edx+8]	;stack: y3 (y1+y2+cv) cv

	mov	[esp+8],ecx
	mov	[esp+12],ebx

	fild	qword [esp+8]	;stack: x4 y3 (y1+y2+cv) cv
	fxch	st1				;stack: y3 x4 (y1+y2+cv) cv
	faddp	st2,st0			;stack: x4 (y1+y2+y3+cv) cv

	mov	eax,[esp+20]		;[frac update] u x_accum
	mov	ecx,[esp+56]		;[frac update] v x_inc

	fmul	dword [edx+12]	;stack: y4 (y1+y2+y3+cv) cv

	mov	ebx,[esp+64]		;[frac update] u xint
	add	eax,ecx				;[frac update] v

	adc	esi,ebx				;[frac update] u: update source pointer
	mov	[esp+20],eax		;[frac update] v new x_accum

	faddp					;stack: (y1+y2+y3+y4+cv) cv

	shr	eax,24				;[frac update] al = xaccum>>24
	mov	edx,[esp+16]		;[frac update] edx = rowbase

	and	eax,000000f0h		;[frac update] eax = x-offset in rowbase
	mov	ebx,[esp+24]		;[data write] u

	add	edx,eax			;[frac update] edx = new FPU coefficient pointer
	;<<v-stall>>

	fstp	tword [esp+0]	;stack: cv

	mov	eax,[esp+0]		;[data merge ] u
	mov	ecx,[esp+4]		;[data merge ] v

	and	eax,00ff00ffh
	and	ecx,0000ff00h

	or	eax,ecx			;[data write ] u

	mov	[ebx+ebp],eax		;[data write ] u

	add	ebp,4
	jne	.colloop_bilinear_FPU

.bilinear_check_postcopy_FPU:

	mov	ebp,[esp+84]
	or	ebp,ebp
	jz	.bilinear_no_postcopy_FPU

	mov	eax,[esp+24]
	sub	eax,ebp
	mov	esi,[esp+80]
	mov	[esp+24],eax

	call	.bilinear_prepostcopy_FPU

	add	edx,[esp+84]
	mov	[esp+24],edx

	;******************

.bilinear_no_postcopy_FPU:
	mov	eax,[esp+52]		;load yaccum
	mov	edx,[esp+24]		;load dest. pointer

	add	edx,[esp+40]		;next destination line
	add	eax,[esp+60]		;add fractional y increment

	sbb	ebx,ebx			;ebx = -1 if fraction overflowed
	mov	esi,[esp+28]		;reload source ptr

	mov	ecx,[esp+72]		;reload source ptr
	mov	ebp,[esp+80]		;reload source ptr

	add	esi,[esp+68]		;add integer increment to source ptr
	add	ecx,[esp+68]		;add integer increment to source ptr

	add	ebp,[esp+68]		;add integer increment to source ptr
	and	ebx,[esp+44]		;ebx = fractional y bump

	add	esi,ebx			;bump source ptr if fraction overflowed
	add	ecx,ebx			;bump source ptr if fraction overflowed
	add	ebp,ebx			;bump source ptr if fraction overflowed
	mov	[esp+52],eax		;store yaccum

	mov	[esp+28],esi		;store source ptr
	mov	[esp+72],ecx		;store source ptr

	mov	[esp+80],ebp		;store source ptr
	mov	[esp+24],edx

	dec	dword [esp+36]
	jne	.rowloop_bilinear_FPU

	;ditch fp value

	fstp	st0

	;restore FPU rounding and precision

	fldcw	[esp+.LOCALS-8]

	mov	esp,[esp+.LOCALS-4]

	pop	ebx
	pop	esi
	pop	edi
	pop	ebp
	ret

.bilinear_prepostcopy_FPU:

.colloop_bilinear_prepostcopy_FPU:
	mov	edx,[esp+16+4]		;edx = rowbase
	mov	eax,[esi]
	mov	ecx,[esi+edi]

	mov	ebx,eax
	and	eax,00ff00ffh

	and	ebx,0000ff00h
	mov	[esp+0+4],eax

	mov	[esp+4+4],ebx
	mov	ebx,ecx

	fild	qword [esp+0+4]	;stack: x1 cv

	and	ecx,00ff00ffh
	and	ebx,0000ff00h

	mov	[esp+8+4],ecx
	mov	[esp+12+4],ebx

	fmul	dword [edx+0]	;stack: y1 cv
	fild	qword [esp+8+4]	;stack: x2 y1 cv

	fmul	dword [edx+8]	;stack: y2 y1 cv
	fxch	st1				;stack: y1 y2 cv
	fadd	st0,st2			;stack: (y1+cv) y2 cv

	mov	edx,[esp+24+4]		;[data write] u
	;<<v-stall>>

	faddp					;stack: (y1+y2+cv) cv

	;<<u-stall>>
	;<<v-stall>>

	;<<u-stall>>
	;<<v-stall>>

	fstp	tword [esp+0+4]	;stack: cv

	mov	eax,[esp+0+4]		;[data merge ] u
	mov	ecx,[esp+4+4]		;[data merge ] v

	and	eax,00ff00ffh
	and	ecx,0000ff00h

	or	eax,ecx			;[data write ] u

	mov	[edx+ebp],eax		;[data write ] u

	add	ebp,4
	jne	.colloop_bilinear_prepostcopy_FPU

	ret

;
;
;******* MMX optimized version.
;
;

asm_resize_bilinear_MMX:
	push	ebp
	push	edi
	push	esi
	push	ebx

	sub	esp,32

;******************************************************
;
;	[esp+28]
;	[esp+24]
;	[esp+20] y_frac2
;	[esp+16] y_frac2
;	[esp+12] y_frac
;	[esp+ 8] y_frac
;	[esp+ 4] sixteen
;	[esp+ 0] sixteen

	mov	eax,00100010h
	mov	[esp+0],eax
	mov	[esp+4],eax

.bilinear_rowloop_MMX:
	mov		esi,[esp+ 8+16+32]		;esi = source
	mov		edi,[esp+24+16+32]		;edi = source pitch

	mov		edx,[esp+4+16+32]		;edx = destination
	mov		ebx,[esp+36+16+32]		;ebx = fractional x increment
	mov		ecx,[esp+44+16+32]		;ecx = integer x increment

	mov		eax,[esp+32+16+32]
	shr		eax,28
	movd		mm7,eax
	punpcklwd	mm7,mm7
	movq		mm6,[sixteen]
	punpckldq	mm7,mm7
	psubw		mm6,mm7
	pxor		mm5,mm5

	shr		esi,2
	mov		eax,[esp+28+16+32]		;eax = x accumulator


	mov	ebp,[esp+56+16+32]		;load precopy value
	or	ebp,ebp
	jz	.colloop_bilinear_start_MMX

	;do precopy

	add	edx,[esp+12+16+32]
	mov	esi,[esp+52+16+32]

	call	.bilinear_prepostcopy_MMX

	mov	edx,[esp+4+16+32]
	mov	esi,[esp+8+16+32]

	shr	esi,2

.colloop_bilinear_start_MMX:
	mov	ebp,[esp+12+16+32]
	or	ebp,ebp
	jz	.bilinear_check_postcopy_MMX

	;<------------- begin pre-entry phase ------------->


	mov		ecx,eax
	shr		ecx,28

	movd		mm0,dword [esi*4]		;mm0 = top left pixel
	movd		mm1,dword [esi*4+4]		;mm1 = top right pixel
	movd		mm3,dword [esi*4+edi+4]	;mm3 = bottom right pixel
	movd		mm2,dword [esi*4+edi]		;mm2 = bottom left pixel
	punpcklbw	mm0,mm5
	pmullw		mm0,[bilinMMX_tab2 + ecx*8]
	punpcklbw	mm1,mm5
	pmullw		mm1,[bilinMMX_tab1 + ecx*8]
	punpcklbw	mm2,mm5
	punpcklbw	mm3,mm5
	jmp		short .bilinear_colloop_MMX_entry

	align		16

.bilinear_colloop_MMX:
	movd		mm0,dword [esi*4]		;mm0 = top left pixel
	paddw		mm4,mm2			;[last]

	movd		mm1,dword [esi*4+4]		;mm1 = top right pixel
	psrlw		mm4,8			;[last]

	movd		mm3,dword [esi*4+edi+4]	;mm3 = bottom right pixel
	packuswb	mm4,mm4			;[last]

	movd		mm2,dword [esi*4+edi]		;mm2 = bottom left pixel
	punpcklbw	mm0,mm5

	pmullw		mm0,[bilinMMX_tab2 + ecx*8]
	punpcklbw	mm1,mm5

	pmullw		mm1,[bilinMMX_tab1 + ecx*8]
	punpcklbw	mm2,mm5

	movd		dword [edx+ebp-4],mm4		;[last]
	punpcklbw	mm3,mm5

.bilinear_colloop_MMX_entry:
	pmullw		mm2,[bilinMMX_tab2 + ecx*8]
	movq		mm4,mm0

	pmullw		mm3,[bilinMMX_tab1 + ecx*8]
	paddw		mm4,mm1

	add		eax,ebx			;update x accumulator
	mov		ecx,[esp+44+16+32]

	adc		esi,ecx			;update source address
	pmullw		mm4,mm6

	paddw		mm2,mm3
	mov		ecx,eax

	shr		ecx,28
	pmullw		mm2,mm7

	add		ebp,4
	jnz		.bilinear_colloop_MMX

	;<-------------- begin exit phase -------------->

	paddw		mm4,mm2			;[last]
	psrlw		mm4,8			;[last]
	packuswb	mm4,mm4			;[last]
	movd		dword [edx+ebp-4],mm4		;[last]
	mov		ecx,[esp+44+16+32]


.bilinear_check_postcopy_MMX:
	mov	ebp,[esp+64+16+32]		;check for postcopy
	or	ebp,ebp
	jz	.bilinear_no_postcopy_MMX

	sub	edx,ebp
	mov	esi,[esp+60+16+32]

	call	.bilinear_prepostcopy_MMX

	;********************************

.bilinear_no_postcopy_MMX:
	mov	eax,[esp+32+16+32]		;eax = y accumulator
	mov	edx,[esp+ 4+16+32]		;reload destination pointer

	add	edx,[esp+20+16+32]		;advance to next destination line
	add	eax,[esp+40+16+32]		;add y fraction to y accumulator

	sbb	ebx,ebx				;ebx = -1 if we have a fractional increment
	mov	esi,[esp+ 8+16+32]		;reload source pointer

	mov	ecx,[esp+52+16+32]		;reload source pointer
	mov	ebp,[esp+60+16+32]		;reload source pointer

	add	esi,[esp+48+16+32]		;add y integer increment
	add	ecx,[esp+48+16+32]		;add y integer increment

	add	ebp,[esp+48+16+32]		;add y integer increment
	and	ebx,[esp+24+16+32]		;ebx = y fractional increment

	add	esi,ebx				;add y fractional increment
	add	ecx,ebx				;add y fractional increment
	add	ebp,ebx				;add y fractional increment
	mov	[esp+ 4+16+32],edx		;store destination pointer

	mov	[esp+ 8+16+32],esi		;store source pointer
	mov	[esp+52+16+32],ecx		;store source pointer
	mov	[esp+60+16+32],ebp		;store source pointer
	mov	[esp+32+16+32],eax		;store new y accumulator

	shr	eax,28				;eax = y_frac
	mov	ebx,16

	mov	[esp+24],eax			;store y_frac
	sub	ebx,eax				;ebx = y_frac2

	mov	[esp+28],ebx			;store y_frac2

	dec	dword [esp+16+16+32]
	jne	.bilinear_rowloop_MMX

	add	esp,32

	pop	ebx
	pop	esi
	pop	edi
	pop	ebp
	emms
	ret

	align		16
.bilinear_prepostcopy_MMX:
	movd		mm0,dword [esi]		;mm0 = top left pixel

	movd		mm2,dword [esi+edi]		;mm2 = bottom left pixel
	punpcklbw	mm0,mm5

	punpcklbw	mm2,mm5
	pmullw		mm0,mm6

	pmullw		mm2,mm7			;[last]
	add		ebp,4

	paddw		mm0,mm2			;[last]

	psrlw		mm0,4			;[last]

	packuswb	mm0,mm0			;[last]

	movd		dword [edx+ebp-4],mm0		;[last]
	jne		.bilinear_prepostcopy_MMX

	ret


;**************************************************************************
;
;void asm_bitmap_xlat1(
;	[esp+ 8] Pixel32 *src,
;	[esp+ 4] Pixel32 *dst,
;	[esp+16] PixOffset spitch,
;	[esp+12] PixOffset dpitch,
;	[esp+20] PixDim w,
;	[esp+24] PixDim h,
;	[esp+28] const Pixel8 *tbl);
;
;**************************************************************************

	global	_asm_bitmap_xlat1	

_asm_bitmap_xlat1:
	push	ebp
	push	edi
	push	esi
	push	ebx

	mov	esi,[esp+ 8+16]		;esi = source
	mov	edi,[esp+ 4+16]		;edi = destination
	mov	edx,[esp+28+16]		;edx = table pointer
rowloop_xlat1:
	mov	ebp,[esp+20+16]

colloop_xlat1:
	mov	eax,[esi+ebp]		;fetch pixel
	xor	ebx,ebx

	mov	bl,al			;bl = blue
	xor	ecx,ecx

	mov	cl,ah			;cl = green
	and	eax,00ff0000h

	shr	eax,16			;al = red
	mov	bl,[edx+ebx]		;ebx = 000000BB

	mov	cl,[edx+ecx]		;cl = translated green

	shl	ecx,8			;ecx = 0000GG00
	mov	al,[edx+eax]		;al = translated red

	shl	eax,16			;eax = 00RR0000
	or	ecx,ebx			;ecx = 0000GGBB

	or	eax,ecx			;eax = 00RRGGBB

	mov	[edi+ebp],eax		;write new pixel

	add	ebp,4
	jne	colloop_xlat1

	add	esi,[esp+16+16]		;next source row
	add	edi,[esp+12+16]		;next dest row

	dec	dword [esp+24+16]
	jnz	rowloop_xlat1

	pop	ebx
	pop	esi
	pop	edi
	pop	ebp
	ret

;**************************************************************************

;void asm_bitmap_xlat3(
;	[esp+ 4] Pixel32 *dst,
;	[esp+ 8] Pixel32 *src,
;	[esp+12] PixOffset dpitch,
;	[esp+16] PixOffset spitch,
;	[esp+20] PixDim w,
;	[esp+24] PixDim h,
;	[esp+28] const Pixel32 *tbl);

	global	_asm_bitmap_xlat3	

_asm_bitmap_xlat3:
	push	ebp
	push	edi
	push	esi
	push	ebx

	mov	esi,[esp+ 8+16]		;esi = source
	mov	edi,[esp+ 4+16]		;edi = dest
	mov	edx,[esp+28+16]		;edx = table pointer
rowloop_xlat3:
	mov	ebp,[esp+20+16]

colloop_xlat3:
	mov	eax,[esi+ebp]		;fetch pixel
	xor	ebx,ebx

	mov	bl,al			;bl = blue
	xor	ecx,ecx

	mov	cl,ah			;cl = green
	and	eax,00ff0000h

	shr	eax,16			;al = red
	mov	ebx,[edx+ebx*4]		;ebx = xxxxxxBB

	mov	ecx,[edx+ecx*4]		;ecx = xxxxGGxx
	and	ebx,000000ffh		;ebx = 000000BB

	mov	eax,[edx+eax*4]		;eax = xxRRxxxx
	and	ecx,0000ff00h		;ecx = 0000GG00

	or	ecx,ebx			;ecx = 0000GGBB
	and	eax,00ff0000h		;eax = 00RR0000

	or	eax,ecx			;eax = 00RRGGBB

	mov	[edi+ebp],eax		;write new pixel

	add	ebp,4
	jne	colloop_xlat3

	add	esi,[esp+16+16]		;next source row
	add	edi,[esp+12+16]		;next dest row

	dec	dword [esp+24+16]
	jnz	rowloop_xlat3

	pop	ebx
	pop	esi
	pop	edi
	pop	ebp
	ret

	end
