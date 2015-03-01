//	VirtualDub - Video processing and capture application
//	A/V interface library
//	Copyright (C) 1998-2007 Avery Lee
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

#ifndef f_VD2_RIZA_D3DXFX_H
#define f_VD2_RIZA_D3DXFX_H

#ifdef _MSC_VER
	#pragma once
#endif

#include <d3d9.h>
#include <d3dx9.h>

// {017C18AC-103F-4417-8C51-6BF6EF1E56BE}
DEFINE_GUID(IID_ID3DXBaseEffectVersion25, 0x17c18ac, 0x103f, 0x4417, 0x8c, 0x51, 0x6b, 0xf6, 0xef, 0x1e, 0x56, 0xbe);

#ifdef INTERFACE
	#undef INTERFACE
#endif

#define INTERFACE ID3DXBaseEffectVersion25

DECLARE_INTERFACE_(ID3DXBaseEffectVersion25, IUnknown)
{
	// IUnknown
	STDMETHOD(QueryInterface)(THIS_ REFIID iid, LPVOID *ppv) PURE;
	STDMETHOD_(ULONG, AddRef)(THIS) PURE;
	STDMETHOD_(ULONG, Release)(THIS) PURE;

	// Descs
	STDMETHOD(GetDesc)(THIS_ D3DXEFFECT_DESC* pDesc) PURE;
	STDMETHOD(GetParameterDesc)(THIS_ D3DXHANDLE hParameter, D3DXPARAMETER_DESC* pDesc) PURE;
	STDMETHOD(GetTechniqueDesc)(THIS_ D3DXHANDLE hTechnique, D3DXTECHNIQUE_DESC* pDesc) PURE;
	STDMETHOD(GetPassDesc)(THIS_ D3DXHANDLE hPass, D3DXPASS_DESC* pDesc) PURE;
	STDMETHOD(GetFunctionDesc)(THIS_ D3DXHANDLE hShader, D3DXFUNCTION_DESC* pDesc) PURE;

	// Handle operations
	STDMETHOD_(D3DXHANDLE, GetParameter)(THIS_ D3DXHANDLE hParameter, UINT Index) PURE;
	STDMETHOD_(D3DXHANDLE, GetParameterByName)(THIS_ D3DXHANDLE hParameter, LPCSTR pName) PURE;
	STDMETHOD_(D3DXHANDLE, GetParameterBySemantic)(THIS_ D3DXHANDLE hParameter, LPCSTR pSemantic) PURE;
	STDMETHOD_(D3DXHANDLE, GetParameterElement)(THIS_ D3DXHANDLE hParameter, UINT Index) PURE;
	STDMETHOD_(D3DXHANDLE, GetTechnique)(THIS_ UINT Index) PURE;
	STDMETHOD_(D3DXHANDLE, GetTechniqueByName)(THIS_ LPCSTR pName) PURE;
	STDMETHOD_(D3DXHANDLE, GetPass)(THIS_ D3DXHANDLE hTechnique, UINT Index) PURE;
	STDMETHOD_(D3DXHANDLE, GetPassByName)(THIS_ D3DXHANDLE hTechnique, LPCSTR pName) PURE;
	STDMETHOD_(D3DXHANDLE, GetFunction)(THIS_ UINT Index) PURE;
	STDMETHOD_(D3DXHANDLE, GetFunctionByName)(THIS_ LPCSTR pName) PURE;
	STDMETHOD_(D3DXHANDLE, GetAnnotation)(THIS_ D3DXHANDLE hObject, UINT Index) PURE;
	STDMETHOD_(D3DXHANDLE, GetAnnotationByName)(THIS_ D3DXHANDLE hObject, LPCSTR pName) PURE;

	// Get/Set Parameters
	STDMETHOD(SetValue)(THIS_ D3DXHANDLE hParameter, LPCVOID pData, UINT Bytes) PURE;
	STDMETHOD(GetValue)(THIS_ D3DXHANDLE hParameter, LPVOID pData, UINT Bytes) PURE;
	STDMETHOD(SetBool)(THIS_ D3DXHANDLE hParameter, BOOL b) PURE;
	STDMETHOD(GetBool)(THIS_ D3DXHANDLE hParameter, BOOL* pb) PURE;
	STDMETHOD(SetBoolArray)(THIS_ D3DXHANDLE hParameter, CONST BOOL* pb, UINT Count) PURE;
	STDMETHOD(GetBoolArray)(THIS_ D3DXHANDLE hParameter, BOOL* pb, UINT Count) PURE;
	STDMETHOD(SetInt)(THIS_ D3DXHANDLE hParameter, INT n) PURE;
	STDMETHOD(GetInt)(THIS_ D3DXHANDLE hParameter, INT* pn) PURE;
	STDMETHOD(SetIntArray)(THIS_ D3DXHANDLE hParameter, CONST INT* pn, UINT Count) PURE;
	STDMETHOD(GetIntArray)(THIS_ D3DXHANDLE hParameter, INT* pn, UINT Count) PURE;
	STDMETHOD(SetFloat)(THIS_ D3DXHANDLE hParameter, FLOAT f) PURE;
	STDMETHOD(GetFloat)(THIS_ D3DXHANDLE hParameter, FLOAT* pf) PURE;
	STDMETHOD(SetFloatArray)(THIS_ D3DXHANDLE hParameter, CONST FLOAT* pf, UINT Count) PURE;
	STDMETHOD(GetFloatArray)(THIS_ D3DXHANDLE hParameter, FLOAT* pf, UINT Count) PURE;
	STDMETHOD(SetVector)(THIS_ D3DXHANDLE hParameter, CONST D3DXVECTOR4* pVector) PURE;
	STDMETHOD(GetVector)(THIS_ D3DXHANDLE hParameter, D3DXVECTOR4* pVector) PURE;
	STDMETHOD(SetVectorArray)(THIS_ D3DXHANDLE hParameter, CONST D3DXVECTOR4* pVector, UINT Count) PURE;
	STDMETHOD(GetVectorArray)(THIS_ D3DXHANDLE hParameter, D3DXVECTOR4* pVector, UINT Count) PURE;
	STDMETHOD(SetMatrix)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX* pMatrix) PURE;
	STDMETHOD(GetMatrix)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX* pMatrix) PURE;
	STDMETHOD(SetMatrixArray)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX* pMatrix, UINT Count) PURE;
	STDMETHOD(GetMatrixArray)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX* pMatrix, UINT Count) PURE;
	STDMETHOD(SetMatrixPointerArray)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX** ppMatrix, UINT Count) PURE;
	STDMETHOD(GetMatrixPointerArray)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX** ppMatrix, UINT Count) PURE;
	STDMETHOD(SetMatrixTranspose)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX* pMatrix) PURE;
	STDMETHOD(GetMatrixTranspose)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX* pMatrix) PURE;
	STDMETHOD(SetMatrixTransposeArray)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX* pMatrix, UINT Count) PURE;
	STDMETHOD(GetMatrixTransposeArray)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX* pMatrix, UINT Count) PURE;
	STDMETHOD(SetMatrixTransposePointerArray)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX** ppMatrix, UINT Count) PURE;
	STDMETHOD(GetMatrixTransposePointerArray)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX** ppMatrix, UINT Count) PURE;
	STDMETHOD(SetString)(THIS_ D3DXHANDLE hParameter, LPCSTR pString) PURE;
	STDMETHOD(GetString)(THIS_ D3DXHANDLE hParameter, LPCSTR* ppString) PURE;
	STDMETHOD(SetTexture)(THIS_ D3DXHANDLE hParameter, LPDIRECT3DBASETEXTURE9 pTexture) PURE;
	STDMETHOD(GetTexture)(THIS_ D3DXHANDLE hParameter, LPDIRECT3DBASETEXTURE9 *ppTexture) PURE;
	STDMETHOD(GetPixelShader)(THIS_ D3DXHANDLE hParameter, LPDIRECT3DPIXELSHADER9 *ppPShader) PURE;
	STDMETHOD(GetVertexShader)(THIS_ D3DXHANDLE hParameter, LPDIRECT3DVERTEXSHADER9 *ppVShader) PURE;

	//Set Range of an Array to pass to device
	//Useful for sending only a subrange of an array down to the device
	STDMETHOD(SetArrayRange)(THIS_ D3DXHANDLE hParameter, UINT uStart, UINT uEnd) PURE; 

};

// {D165CCB1-62B0-4a33-B3FA-A92300305A11}
DEFINE_GUID(IID_ID3DXEffectVersion25, 
0xd165ccb1, 0x62b0, 0x4a33, 0xb3, 0xfa, 0xa9, 0x23, 0x0, 0x30, 0x5a, 0x11);

#undef INTERFACE
#define INTERFACE ID3DXEffectVersion25

DECLARE_INTERFACE_(ID3DXEffectVersion25, ID3DXBaseEffectVersion25)
{
	// ID3DXBaseEffect
	STDMETHOD(QueryInterface)(THIS_ REFIID iid, LPVOID *ppv) PURE;
	STDMETHOD_(ULONG, AddRef)(THIS) PURE;
	STDMETHOD_(ULONG, Release)(THIS) PURE;

	// Descs
	STDMETHOD(GetDesc)(THIS_ D3DXEFFECT_DESC* pDesc) PURE;
	STDMETHOD(GetParameterDesc)(THIS_ D3DXHANDLE hParameter, D3DXPARAMETER_DESC* pDesc) PURE;
	STDMETHOD(GetTechniqueDesc)(THIS_ D3DXHANDLE hTechnique, D3DXTECHNIQUE_DESC* pDesc) PURE;
	STDMETHOD(GetPassDesc)(THIS_ D3DXHANDLE hPass, D3DXPASS_DESC* pDesc) PURE;
	STDMETHOD(GetFunctionDesc)(THIS_ D3DXHANDLE hShader, D3DXFUNCTION_DESC* pDesc) PURE;

	// Handle operations
	STDMETHOD_(D3DXHANDLE, GetParameter)(THIS_ D3DXHANDLE hParameter, UINT Index) PURE;
	STDMETHOD_(D3DXHANDLE, GetParameterByName)(THIS_ D3DXHANDLE hParameter, LPCSTR pName) PURE;
	STDMETHOD_(D3DXHANDLE, GetParameterBySemantic)(THIS_ D3DXHANDLE hParameter, LPCSTR pSemantic) PURE;
	STDMETHOD_(D3DXHANDLE, GetParameterElement)(THIS_ D3DXHANDLE hParameter, UINT Index) PURE;
	STDMETHOD_(D3DXHANDLE, GetTechnique)(THIS_ UINT Index) PURE;
	STDMETHOD_(D3DXHANDLE, GetTechniqueByName)(THIS_ LPCSTR pName) PURE;
	STDMETHOD_(D3DXHANDLE, GetPass)(THIS_ D3DXHANDLE hTechnique, UINT Index) PURE;
	STDMETHOD_(D3DXHANDLE, GetPassByName)(THIS_ D3DXHANDLE hTechnique, LPCSTR pName) PURE;
	STDMETHOD_(D3DXHANDLE, GetFunction)(THIS_ UINT Index) PURE;
	STDMETHOD_(D3DXHANDLE, GetFunctionByName)(THIS_ LPCSTR pName) PURE;
	STDMETHOD_(D3DXHANDLE, GetAnnotation)(THIS_ D3DXHANDLE hObject, UINT Index) PURE;
	STDMETHOD_(D3DXHANDLE, GetAnnotationByName)(THIS_ D3DXHANDLE hObject, LPCSTR pName) PURE;

	// Get/Set Parameters
	STDMETHOD(SetValue)(THIS_ D3DXHANDLE hParameter, LPCVOID pData, UINT Bytes) PURE;
	STDMETHOD(GetValue)(THIS_ D3DXHANDLE hParameter, LPVOID pData, UINT Bytes) PURE;
	STDMETHOD(SetBool)(THIS_ D3DXHANDLE hParameter, BOOL b) PURE;
	STDMETHOD(GetBool)(THIS_ D3DXHANDLE hParameter, BOOL* pb) PURE;
	STDMETHOD(SetBoolArray)(THIS_ D3DXHANDLE hParameter, CONST BOOL* pb, UINT Count) PURE;
	STDMETHOD(GetBoolArray)(THIS_ D3DXHANDLE hParameter, BOOL* pb, UINT Count) PURE;
	STDMETHOD(SetInt)(THIS_ D3DXHANDLE hParameter, INT n) PURE;
	STDMETHOD(GetInt)(THIS_ D3DXHANDLE hParameter, INT* pn) PURE;
	STDMETHOD(SetIntArray)(THIS_ D3DXHANDLE hParameter, CONST INT* pn, UINT Count) PURE;
	STDMETHOD(GetIntArray)(THIS_ D3DXHANDLE hParameter, INT* pn, UINT Count) PURE;
	STDMETHOD(SetFloat)(THIS_ D3DXHANDLE hParameter, FLOAT f) PURE;
	STDMETHOD(GetFloat)(THIS_ D3DXHANDLE hParameter, FLOAT* pf) PURE;
	STDMETHOD(SetFloatArray)(THIS_ D3DXHANDLE hParameter, CONST FLOAT* pf, UINT Count) PURE;
	STDMETHOD(GetFloatArray)(THIS_ D3DXHANDLE hParameter, FLOAT* pf, UINT Count) PURE;
	STDMETHOD(SetVector)(THIS_ D3DXHANDLE hParameter, CONST D3DXVECTOR4* pVector) PURE;
	STDMETHOD(GetVector)(THIS_ D3DXHANDLE hParameter, D3DXVECTOR4* pVector) PURE;
	STDMETHOD(SetVectorArray)(THIS_ D3DXHANDLE hParameter, CONST D3DXVECTOR4* pVector, UINT Count) PURE;
	STDMETHOD(GetVectorArray)(THIS_ D3DXHANDLE hParameter, D3DXVECTOR4* pVector, UINT Count) PURE;
	STDMETHOD(SetMatrix)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX* pMatrix) PURE;
	STDMETHOD(GetMatrix)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX* pMatrix) PURE;
	STDMETHOD(SetMatrixArray)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX* pMatrix, UINT Count) PURE;
	STDMETHOD(GetMatrixArray)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX* pMatrix, UINT Count) PURE;
	STDMETHOD(SetMatrixPointerArray)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX** ppMatrix, UINT Count) PURE;
	STDMETHOD(GetMatrixPointerArray)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX** ppMatrix, UINT Count) PURE;
	STDMETHOD(SetMatrixTranspose)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX* pMatrix) PURE;
	STDMETHOD(GetMatrixTranspose)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX* pMatrix) PURE;
	STDMETHOD(SetMatrixTransposeArray)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX* pMatrix, UINT Count) PURE;
	STDMETHOD(GetMatrixTransposeArray)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX* pMatrix, UINT Count) PURE;
	STDMETHOD(SetMatrixTransposePointerArray)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX** ppMatrix, UINT Count) PURE;
	STDMETHOD(GetMatrixTransposePointerArray)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX** ppMatrix, UINT Count) PURE;
	STDMETHOD(SetString)(THIS_ D3DXHANDLE hParameter, LPCSTR pString) PURE;
	STDMETHOD(GetString)(THIS_ D3DXHANDLE hParameter, LPCSTR* ppString) PURE;
	STDMETHOD(SetTexture)(THIS_ D3DXHANDLE hParameter, LPDIRECT3DBASETEXTURE9 pTexture) PURE;
	STDMETHOD(GetTexture)(THIS_ D3DXHANDLE hParameter, LPDIRECT3DBASETEXTURE9 *ppTexture) PURE;
	// (!) SetPixelShader was removed relative to version 22.
	STDMETHOD(GetPixelShader)(THIS_ D3DXHANDLE hParameter, LPDIRECT3DPIXELSHADER9 *ppPShader) PURE;
	// (!) SetVertexShader was removed relative to version 22.
	STDMETHOD(GetVertexShader)(THIS_ D3DXHANDLE hParameter, LPDIRECT3DVERTEXSHADER9 *ppVShader) PURE;

	//Set Range of an Array to pass to device
	//Usefull for sending only a subrange of an array down to the device
	STDMETHOD(SetArrayRange)(THIS_ D3DXHANDLE hParameter, UINT uStart, UINT uEnd) PURE; 
	// ID3DXBaseEffect


	// Pool
	STDMETHOD(GetPool)(THIS_ LPD3DXEFFECTPOOL* ppPool) PURE;

	// Selecting and setting a technique
	STDMETHOD(SetTechnique)(THIS_ D3DXHANDLE hTechnique) PURE;
	STDMETHOD_(D3DXHANDLE, GetCurrentTechnique)(THIS) PURE;
	STDMETHOD(ValidateTechnique)(THIS_ D3DXHANDLE hTechnique) PURE;
	STDMETHOD(FindNextValidTechnique)(THIS_ D3DXHANDLE hTechnique, D3DXHANDLE *pTechnique) PURE;
	STDMETHOD_(BOOL, IsParameterUsed)(THIS_ D3DXHANDLE hParameter, D3DXHANDLE hTechnique) PURE;

	// Using current technique
	// Begin           starts active technique
	// BeginPass       begins a pass
	// CommitChanges   updates changes to any set calls in the pass. This should be called before
	//                 any DrawPrimitive call to d3d
	// EndPass         ends a pass
	// End             ends active technique
	STDMETHOD(Begin)(THIS_ UINT *pPasses, DWORD Flags) PURE;
	STDMETHOD(BeginPass)(THIS_ UINT Pass) PURE;
	STDMETHOD(CommitChanges)(THIS) PURE;
	STDMETHOD(EndPass)(THIS) PURE;
	STDMETHOD(End)(THIS) PURE;

	// Managing D3D Device
	STDMETHOD(GetDevice)(THIS_ LPDIRECT3DDEVICE9* ppDevice) PURE;
	STDMETHOD(OnLostDevice)(THIS) PURE;
	STDMETHOD(OnResetDevice)(THIS) PURE;

	// Logging device calls
	STDMETHOD(SetStateManager)(THIS_ LPD3DXEFFECTSTATEMANAGER pManager) PURE;
	STDMETHOD(GetStateManager)(THIS_ LPD3DXEFFECTSTATEMANAGER *ppManager) PURE;

	// Parameter blocks
	STDMETHOD(BeginParameterBlock)(THIS) PURE;
	STDMETHOD_(D3DXHANDLE, EndParameterBlock)(THIS) PURE;
	STDMETHOD(ApplyParameterBlock)(THIS_ D3DXHANDLE hParameterBlock) PURE;

	// Cloning
	STDMETHOD(CloneEffect)(THIS_ LPDIRECT3DDEVICE9 pDevice, LPD3DXEFFECT* ppEffect) PURE;
};

// {51B8A949-1A31-47e6-BEA0-4B30DB53F1E0}
DEFINE_GUID(IID_ID3DXEffectCompilerVersion25, 
0x51b8a949, 0x1a31, 0x47e6, 0xbe, 0xa0, 0x4b, 0x30, 0xdb, 0x53, 0xf1, 0xe0);

#undef INTERFACE
#define INTERFACE ID3DXEffectCompilerVersion25

DECLARE_INTERFACE_(ID3DXEffectCompilerVersion25, ID3DXBaseEffectVersion25)
{
	// ID3DXBaseEffect
	STDMETHOD(QueryInterface)(THIS_ REFIID iid, LPVOID *ppv) PURE;
	STDMETHOD_(ULONG, AddRef)(THIS) PURE;
	STDMETHOD_(ULONG, Release)(THIS) PURE;

	// Descs
	STDMETHOD(GetDesc)(THIS_ D3DXEFFECT_DESC* pDesc) PURE;
	STDMETHOD(GetParameterDesc)(THIS_ D3DXHANDLE hParameter, D3DXPARAMETER_DESC* pDesc) PURE;
	STDMETHOD(GetTechniqueDesc)(THIS_ D3DXHANDLE hTechnique, D3DXTECHNIQUE_DESC* pDesc) PURE;
	STDMETHOD(GetPassDesc)(THIS_ D3DXHANDLE hPass, D3DXPASS_DESC* pDesc) PURE;
	STDMETHOD(GetFunctionDesc)(THIS_ D3DXHANDLE hShader, D3DXFUNCTION_DESC* pDesc) PURE;

	// Handle operations
	STDMETHOD_(D3DXHANDLE, GetParameter)(THIS_ D3DXHANDLE hParameter, UINT Index) PURE;
	STDMETHOD_(D3DXHANDLE, GetParameterByName)(THIS_ D3DXHANDLE hParameter, LPCSTR pName) PURE;
	STDMETHOD_(D3DXHANDLE, GetParameterBySemantic)(THIS_ D3DXHANDLE hParameter, LPCSTR pSemantic) PURE;
	STDMETHOD_(D3DXHANDLE, GetParameterElement)(THIS_ D3DXHANDLE hParameter, UINT Index) PURE;
	STDMETHOD_(D3DXHANDLE, GetTechnique)(THIS_ UINT Index) PURE;
	STDMETHOD_(D3DXHANDLE, GetTechniqueByName)(THIS_ LPCSTR pName) PURE;
	STDMETHOD_(D3DXHANDLE, GetPass)(THIS_ D3DXHANDLE hTechnique, UINT Index) PURE;
	STDMETHOD_(D3DXHANDLE, GetPassByName)(THIS_ D3DXHANDLE hTechnique, LPCSTR pName) PURE;
	STDMETHOD_(D3DXHANDLE, GetFunction)(THIS_ UINT Index) PURE;
	STDMETHOD_(D3DXHANDLE, GetFunctionByName)(THIS_ LPCSTR pName) PURE;
	STDMETHOD_(D3DXHANDLE, GetAnnotation)(THIS_ D3DXHANDLE hObject, UINT Index) PURE;
	STDMETHOD_(D3DXHANDLE, GetAnnotationByName)(THIS_ D3DXHANDLE hObject, LPCSTR pName) PURE;

	// Get/Set Parameters
	STDMETHOD(SetValue)(THIS_ D3DXHANDLE hParameter, LPCVOID pData, UINT Bytes) PURE;
	STDMETHOD(GetValue)(THIS_ D3DXHANDLE hParameter, LPVOID pData, UINT Bytes) PURE;
	STDMETHOD(SetBool)(THIS_ D3DXHANDLE hParameter, BOOL b) PURE;
	STDMETHOD(GetBool)(THIS_ D3DXHANDLE hParameter, BOOL* pb) PURE;
	STDMETHOD(SetBoolArray)(THIS_ D3DXHANDLE hParameter, CONST BOOL* pb, UINT Count) PURE;
	STDMETHOD(GetBoolArray)(THIS_ D3DXHANDLE hParameter, BOOL* pb, UINT Count) PURE;
	STDMETHOD(SetInt)(THIS_ D3DXHANDLE hParameter, INT n) PURE;
	STDMETHOD(GetInt)(THIS_ D3DXHANDLE hParameter, INT* pn) PURE;
	STDMETHOD(SetIntArray)(THIS_ D3DXHANDLE hParameter, CONST INT* pn, UINT Count) PURE;
	STDMETHOD(GetIntArray)(THIS_ D3DXHANDLE hParameter, INT* pn, UINT Count) PURE;
	STDMETHOD(SetFloat)(THIS_ D3DXHANDLE hParameter, FLOAT f) PURE;
	STDMETHOD(GetFloat)(THIS_ D3DXHANDLE hParameter, FLOAT* pf) PURE;
	STDMETHOD(SetFloatArray)(THIS_ D3DXHANDLE hParameter, CONST FLOAT* pf, UINT Count) PURE;
	STDMETHOD(GetFloatArray)(THIS_ D3DXHANDLE hParameter, FLOAT* pf, UINT Count) PURE;
	STDMETHOD(SetVector)(THIS_ D3DXHANDLE hParameter, CONST D3DXVECTOR4* pVector) PURE;
	STDMETHOD(GetVector)(THIS_ D3DXHANDLE hParameter, D3DXVECTOR4* pVector) PURE;
	STDMETHOD(SetVectorArray)(THIS_ D3DXHANDLE hParameter, CONST D3DXVECTOR4* pVector, UINT Count) PURE;
	STDMETHOD(GetVectorArray)(THIS_ D3DXHANDLE hParameter, D3DXVECTOR4* pVector, UINT Count) PURE;
	STDMETHOD(SetMatrix)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX* pMatrix) PURE;
	STDMETHOD(GetMatrix)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX* pMatrix) PURE;
	STDMETHOD(SetMatrixArray)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX* pMatrix, UINT Count) PURE;
	STDMETHOD(GetMatrixArray)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX* pMatrix, UINT Count) PURE;
	STDMETHOD(SetMatrixPointerArray)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX** ppMatrix, UINT Count) PURE;
	STDMETHOD(GetMatrixPointerArray)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX** ppMatrix, UINT Count) PURE;
	STDMETHOD(SetMatrixTranspose)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX* pMatrix) PURE;
	STDMETHOD(GetMatrixTranspose)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX* pMatrix) PURE;
	STDMETHOD(SetMatrixTransposeArray)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX* pMatrix, UINT Count) PURE;
	STDMETHOD(GetMatrixTransposeArray)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX* pMatrix, UINT Count) PURE;
	STDMETHOD(SetMatrixTransposePointerArray)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX** ppMatrix, UINT Count) PURE;
	STDMETHOD(GetMatrixTransposePointerArray)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX** ppMatrix, UINT Count) PURE;
	STDMETHOD(SetString)(THIS_ D3DXHANDLE hParameter, LPCSTR pString) PURE;
	STDMETHOD(GetString)(THIS_ D3DXHANDLE hParameter, LPCSTR* ppString) PURE;
	STDMETHOD(SetTexture)(THIS_ D3DXHANDLE hParameter, LPDIRECT3DBASETEXTURE9 pTexture) PURE;
	STDMETHOD(GetTexture)(THIS_ D3DXHANDLE hParameter, LPDIRECT3DBASETEXTURE9 *ppTexture) PURE;
	STDMETHOD(GetPixelShader)(THIS_ D3DXHANDLE hParameter, LPDIRECT3DPIXELSHADER9 *ppPShader) PURE;
	STDMETHOD(GetVertexShader)(THIS_ D3DXHANDLE hParameter, LPDIRECT3DVERTEXSHADER9 *ppVShader) PURE;

	//Set Range of an Array to pass to device
	//Usefull for sending only a subrange of an array down to the device
	STDMETHOD(SetArrayRange)(THIS_ D3DXHANDLE hParameter, UINT uStart, UINT uEnd) PURE; 
	// ID3DXBaseEffect

	// Parameter sharing, specialization, and information
	STDMETHOD(SetLiteral)(THIS_ D3DXHANDLE hParameter, BOOL Literal) PURE;
	STDMETHOD(GetLiteral)(THIS_ D3DXHANDLE hParameter, BOOL *pLiteral) PURE;

	// Compilation
	STDMETHOD(CompileEffect)(THIS_ DWORD Flags,
		LPD3DXBUFFER* ppEffect, LPD3DXBUFFER* ppErrorMsgs) PURE;

	STDMETHOD(CompileShader)(THIS_ D3DXHANDLE hFunction, LPCSTR pTarget, DWORD Flags,
		LPD3DXBUFFER* ppShader, LPD3DXBUFFER* ppErrorMsgs, LPD3DXCONSTANTTABLE* ppConstantTable) PURE;
};

#undef INTERFACE

#endif
