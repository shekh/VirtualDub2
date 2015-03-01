//	VirtualDub - Video processing and capture application
//	JSON I/O library
//	Copyright (C) 1998-2010 Avery Lee
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
#include <vd2/vdjson/jsonvalue.h>

const VDJSONValue VDJSONValue::null = { kTypeNull };

VDJSONValuePool::VDJSONValuePool(uint32 initialBlockSize, uint32 maxBlockSize, uint32 largeBlockThreshold)
	: mpHead(NULL)
	, mpAllocNext(NULL)
	, mAllocLeft(0)
	, mBlockSize(initialBlockSize)
	, mMaxBlockSize(maxBlockSize)
	, mLargeBlockThreshold(largeBlockThreshold)
{
}

VDJSONValuePool::~VDJSONValuePool() {
	while(mpHead) {
		void *p = mpHead;
		mpHead = mpHead->mpNext;
		free(p);
	}
}

void VDJSONValuePool::AddArray(VDJSONValue& dst, size_t n) {
	VDJSONArray *arr = (VDJSONArray *)Allocate(sizeof(VDJSONArray));
	dst.Set(arr);
	arr->mLength = n;
	arr->mpElements = (VDJSONValue *)Allocate(sizeof(VDJSONValue) * n);

	for(size_t i=0; i<n; ++i)
		arr->mpElements[i].Set();
}

VDJSONValue *VDJSONValuePool::AddObjectMember(VDJSONValue& dst, uint32 nameToken) {
	VDJSONMember *el = (VDJSONMember *)Allocate(sizeof(VDJSONMember));

	if (dst.mType != VDJSONValue::kTypeObject) {
		dst.mType = VDJSONValue::kTypeObject;
		dst.mpObject = NULL;
	}

	el->mNameToken = nameToken;
	el->mValue.Set();
	el->mpNext = dst.mpObject;
	dst.mpObject = el;
	return &el->mValue;
}

const VDJSONString *VDJSONValuePool::AddString(const wchar_t *s) {
	return AddString(s, wcslen(s));
}

const VDJSONString *VDJSONValuePool::AddString(const wchar_t *s, size_t len) {
	VDJSONString *str = (VDJSONString *)Allocate(sizeof(VDJSONString));
	wchar_t *t = (wchar_t *)Allocate(sizeof(wchar_t) * (len + 1));

	memcpy(t, s, len * sizeof(wchar_t));
	t[len] = 0;
	str->mpChars = t;
	str->mLength = len;

	return str;
}

void VDJSONValuePool::AddString(VDJSONValue& dst, const wchar_t *s) {
	AddString(dst, s, wcslen(s));
}

void VDJSONValuePool::AddString(VDJSONValue& dst, const wchar_t *s, size_t len) {
	dst.Set(AddString(s, len));
}

void *VDJSONValuePool::Allocate(size_t n) {
	n = (n + 7) & ~7;

	if (mAllocLeft < n) {
		if (n >= mLargeBlockThreshold) {
			BlockNode *node = (BlockNode *)malloc(sizeof(BlockNode) + n);
			
			node->mpNext = mpHead->mpNext;
			mpHead->mpNext = node;

			return node + 1;
		}

		BlockNode *node = (BlockNode *)malloc(mBlockSize);

		node->mpNext = mpHead;
		mpHead = node;
		mAllocLeft = mBlockSize - sizeof(BlockNode);
		mpAllocNext = (char *)(node + 1);

		mBlockSize += mBlockSize;
		if (mBlockSize > mMaxBlockSize)
			mBlockSize = mMaxBlockSize;
	}

	void *p = mpAllocNext;
	mAllocLeft -= n;
	mpAllocNext += n;

	return p;
}

///////////////////////////////////////////////////////////////////////////

const VDJSONValueRef VDJSONValueRef::operator[](int index) const {
	if (mpRef->mType != VDJSONValue::kTypeArray)
		return VDJSONValueRef(mpDoc, &VDJSONValue::null);

	VDJSONArray *arr = mpRef->mpArray;
	if ((unsigned)index >= arr->mLength)
		return VDJSONValueRef(mpDoc, &VDJSONValue::null);

	return VDJSONValueRef(mpDoc, &arr->mpElements[index]);
}

const VDJSONValueRef VDJSONValueRef::operator[](VDJSONNameToken nameToken) const {
	if (mpRef->mType == VDJSONValue::kTypeObject) {
		uint32 token = nameToken.mToken;
		if (token) {
			for(VDJSONMember *p = mpRef->mpObject; p; p = p->mpNext) {
				if (p->mNameToken == token)
					return VDJSONValueRef(mpDoc, &p->mValue);
			}
		}
	}

	return VDJSONValueRef(mpDoc, &VDJSONValue::null);
}

const VDJSONValueRef VDJSONValueRef::operator[](const char *s) const {
	return operator[](mpDoc->mNameTable.GetToken(s));
}

const VDJSONValueRef VDJSONValueRef::operator[](const wchar_t *s) const {
	return operator[](mpDoc->mNameTable.GetToken(s));
}

double VDJSONValueRef::ConvertToReal() const {
	if (mpRef->mType == VDJSONValue::kTypeInt)
		return (double)mpRef->mIntValue;

	return 0;
}
