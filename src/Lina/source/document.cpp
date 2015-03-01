#pragma warning(disable: 4786)
#include <set>
#include "document.h"

std::set<std::string>	g_tagSetSupportingCDATA;

///////////////////////////////////////////////////////////////////////////////

TreeDocument::TreeDocument()
	: mpRoot(NULL)
{
}

TreeNode *TreeDocument::AllocNode() {
	mNodeHeap.push_back(TreeNode());
	TreeNode *t = &mNodeHeap.back();

	t->mpDocument = this;

	return t;
}

///////////////////////////////////////////////////////////////////////////////

TreeNode *TreeNode::ShallowClone() {
	TreeNode *newNode = mpDocument->AllocNode();

	newNode->mpLocation		= mpLocation;
	newNode->mLineno		= mLineno;
	newNode->mName			= mName;
	newNode->mAttribs		= mAttribs;
	newNode->mbIsText		= mbIsText;
	newNode->mbIsControl	= mbIsControl;

	return newNode;
}

const TreeAttribute *TreeNode::Attrib(const std::string& s) const {
	Attributes::const_iterator it(mAttribs.begin()), itEnd(mAttribs.end());

	for(; it!=itEnd; ++it) {
		if ((*it).mName == s)
			return &*it;
	}

	return NULL;
}

const TreeNode *TreeNode::ResolvePath(const std::string& path, std::string& name) const {
	std::string::size_type p = path.find('/');

	if (!p) {
		return mpDocument->mpRoot->ResolvePath(path.substr(1), name);
	} else if (p != std::string::npos) {
		const TreeNode *t = Child(path.substr(0, p));

		if (!t)
			return NULL;

		return t->ResolvePath(path.substr(p+1), name);
	} else {
		name = path;
		return this;
	}
}

const TreeNode *TreeNode::Child(const std::string& s) const {
	std::string name;
	const TreeNode *parent = ResolvePath(s, name);

	Children::const_iterator it(mChildren.begin()), itEnd(mChildren.end());

	for(; it!=itEnd; ++it) {
		TreeNode *child = *it;
		if (!child->mbIsText) {
			if (child->mName == "lina:data") {
				const TreeNode *t = child->Child(name);
				if (t)
					return t;
			}
			if (child->mName == name)
				return *it;
		}
	}
	return NULL;
}

bool TreeNode::SupportsCDATA() const {
	return g_tagSetSupportingCDATA.find(mName) != g_tagSetSupportingCDATA.end();
}

void TreeNode::SetSupportsCDATA(const std::string& tagname, bool supports_cdata) {
	if (supports_cdata)
		g_tagSetSupportingCDATA.insert(tagname);
	else
		g_tagSetSupportingCDATA.erase(tagname);
}
