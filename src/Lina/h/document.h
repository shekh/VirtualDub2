//	Lina - HTML compiler for VirtualDub help system
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

#ifndef f_LINA_DOCUMENT_H
#define f_LINA_DOCUMENT_H

#include <string>
#include <list>
#include <map>

class TreeNode;
class TreeDocument;

bool issafevaluechar(int c);

class TreeAttribute {
public:
	std::string mName, mValue;
	bool mbNoValue;
};

class TreeLocation {
public:
	std::string	mName;
};

class TreeNode {
public:
	TreeDocument *mpDocument;
	TreeLocation *mpLocation;
	int mLineno;
	std::string mName;

	typedef std::list<TreeAttribute> Attributes; 
	Attributes mAttribs;

	typedef std::list<TreeNode *> Children;
	Children mChildren;
	bool mbIsText;
	bool mbIsControl;

public:
	TreeNode *ShallowClone();
	const TreeAttribute *Attrib(const std::string& s) const;
	const TreeNode *ResolvePath(const std::string& path, std::string& name) const;
	const TreeNode *Child(const std::string& s) const;

	bool SupportsCDATA() const;

	static void SetSupportsCDATA(const std::string& tagname, bool supports_cdata);
};

class TreeDocument {
public:
	TreeNode *mpRoot;

	std::list<TreeNode> mNodeHeap;
	std::map<std::string, TreeNode *> mMacros;

	TreeDocument();
	TreeNode *AllocNode();
};

#endif
