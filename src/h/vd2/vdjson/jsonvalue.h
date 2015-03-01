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

#ifndef f_VD2_VDJSON_JSONVALUE_H
#define f_VD2_VDJSON_JSONVALUE_H

#include <vd2/vdjson/jsonnametable.h>

class VDJSONNameTable;
struct VDJSONMember;
struct VDJSONElement;
struct VDJSONNameToken;
struct VDJSONString;
struct VDJSONArray;
class VDJSONDocument;

struct VDJSONValue {
	enum Type {
		kTypeNull,
		kTypeBool,
		kTypeInt,
		kTypeReal,
		kTypeString,
		kTypeObject,
		kTypeArray
	};

	void Set() { mType = kTypeNull; }
	void Set(bool v) { mType = kTypeBool; mBoolValue = v; }
	void Set(sint64 v) { mType = kTypeInt; mIntValue = v; }
	void Set(double v) { mType = kTypeReal; mRealValue = v; }
	void Set(const VDJSONString *s) { mType = kTypeString; mpString = s; }
	void Set(VDJSONArray *arr)  { mType = kTypeArray; mpArray = arr; }

	Type	mType;

	union {
		bool	mBoolValue;
		sint64	mIntValue;
		double	mRealValue;
		const VDJSONString *mpString;
		VDJSONMember *mpObject;
		VDJSONArray *mpArray;
	};

	static const VDJSONValue null;
};

struct VDJSONArray {
	size_t mLength;
	VDJSONValue *mpElements;
};

struct VDJSONString {
	size_t mLength;
	const wchar_t *mpChars;
};

struct VDJSONMember {
	VDJSONMember *mpNext;
	uint32 mNameToken;
	VDJSONValue mValue;
};

struct VDJSONElement {
	VDJSONElement *mpNext;
	VDJSONValue mValue;
};

///////////////////////////////////////////////////////////////////////

class VDJSONValuePool {
	VDJSONValuePool(const VDJSONValuePool&);
	VDJSONValuePool& operator=(const VDJSONValuePool&);
public:
	VDJSONValuePool(uint32 initialBlockSize = 256, uint32 maxBlockSize = 4096, uint32 largeBlockThreshold = 128);
	~VDJSONValuePool();

	void AddArray(VDJSONValue& dst, size_t n);
	VDJSONValue *AddObjectMember(VDJSONValue& dst, uint32 nameToken);
	const VDJSONString *AddString(const wchar_t *s);
	const VDJSONString *AddString(const wchar_t *s, size_t len);
	void AddString(VDJSONValue& dst, const wchar_t *s);
	void AddString(VDJSONValue& dst, const wchar_t *s, size_t n);

protected:
	void *Allocate(size_t n);

	union BlockNode {
		BlockNode *mpNext;
		double mAlign;
	};

	BlockNode *mpHead;
	char *mpAllocNext;
	uint32 mAllocLeft;

	uint32 mBlockSize;
	uint32 mMaxBlockSize;
	uint32 mLargeBlockThreshold;
};

struct VDJSONValueRef;
struct VDJSONMemberEnum {
	VDJSONMemberEnum(const VDJSONMember *member, const VDJSONDocument *doc)
		: mpMember(member)
		, mpDoc(doc)
	{
	}

	bool IsValid() const {
		return mpMember != NULL;
	}

	void operator++() {
		mpMember = mpMember->mpNext;
	}

	inline const wchar_t *GetName() const;
	inline const VDJSONValueRef GetValue() const;

protected:
	const VDJSONMember *mpMember;
	const VDJSONDocument *const mpDoc;
};

struct VDJSONValueRef {
	VDJSONValueRef(const VDJSONDocument *doc, const VDJSONValue *ref)
		: mpDoc(doc)
		, mpRef(ref)
	{
	}

	const VDJSONValue& operator*() const { return *mpRef; }

	bool IsValid() const { return mpRef->mType != VDJSONValue::kTypeNull; }

	bool AsBool() const { return mpRef->mType == VDJSONValue::kTypeBool ? mpRef->mBoolValue : false; }
	sint64 AsInt64() const { return mpRef->mType == VDJSONValue::kTypeInt ? mpRef->mIntValue : 0; }
	double AsDouble() const { return mpRef->mType == VDJSONValue::kTypeReal ? mpRef->mRealValue : ConvertToReal(); }
	const wchar_t *AsString() const { return mpRef->mType == VDJSONValue::kTypeString ? mpRef->mpString->mpChars : L""; }
	VDJSONMemberEnum AsObject() const { return VDJSONMemberEnum(mpRef->mType == VDJSONValue::kTypeObject ? mpRef->mpObject : NULL, mpDoc); };

	size_t GetArrayLength() const { return mpRef->mType == VDJSONValue::kTypeArray ? mpRef->mpArray->mLength : 0; }

	const VDJSONValueRef operator[](int index) const;
	const VDJSONValueRef operator[](VDJSONNameToken nameToken) const;
	const VDJSONValueRef operator[](const char *s) const;
	const VDJSONValueRef operator[](const wchar_t *s) const;

protected:
	double ConvertToReal() const;

	const VDJSONDocument *const mpDoc;
	const VDJSONValue *const mpRef;
};

class VDJSONDocument {
public:
	VDJSONValue mValue;
	VDJSONValuePool mPool;
	VDJSONNameTable mNameTable;

	VDJSONValueRef Root() { return VDJSONValueRef(this, &mValue); }
	const VDJSONValueRef Root() const { return VDJSONValueRef(this, &mValue); }
};

inline const wchar_t *VDJSONMemberEnum::GetName() const {
	return mpDoc->mNameTable.GetName(mpMember->mNameToken);
}

inline const VDJSONValueRef VDJSONMemberEnum::GetValue() const {
	return VDJSONValueRef(mpDoc, &mpMember->mValue);
}

#endif
