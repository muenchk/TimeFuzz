#include "ExclusionTree.h"
#include "Input.h"
#include "BufferOperations.h"
#include "Logging.h"

#define exclrlock std::unique_lock<std::shared_mutex> guard(_lock);  //((void)0); 
#define exclwlock std::shared_lock<std::shared_mutex> guard(_lock);

ExclusionTree::TreeNode* ExclusionTree::TreeNode::HasChild(std::string str)
{
	for (int32_t i = 0; i < children.size(); i++) {
		if (children[i] && children[i]->identifier == str)
			return children[i];
	}
	return nullptr;
}

ExclusionTree::ExclusionTree()
{
	root = TreeNode();
	root.id = 0;
}

ExclusionTree::~ExclusionTree()
{
	exclwlock;
	DeleteChildren(&root);
	root = TreeNode();
	root.id = 0;
}

void ExclusionTree::AddInput(std::shared_ptr<Input> input)
{
	if (input.get() == nullptr)
		return;

	exclwlock;
	TreeNode* node = &root;
	auto itr = input->begin();
	while (itr != input->end()) {
		if (auto child = node->HasChild(*itr); child != nullptr) {
			// if the current node already has the required child,
			// choose it as our node and go to the next input entry
			if (child->isLeaf)
				return;  // the input is already excluded
			node = child;
		} else {
			// if there is no matching child, create one.
			TreeNode* newnode = new TreeNode;
			newnode->id = nextid++;
			newnode->identifier = *itr;
			node->children.push_back(newnode);
			hashmap.insert({ newnode->id, newnode });
			node = newnode;
		}
		itr++;
	}
	// The result of the inputs saved to this tree is fixed and any input that contains
	// it as a prefix yields the same result.
	// Thus we can remove all children from our current node, as they would yield the same result.
	node->isLeaf = true;
	DeleteChildren(node);
}

bool ExclusionTree::HasPrefix(std::shared_ptr<Input> input)
{
	if (input.get() == nullptr)
		return false;

	exclrlock;
	TreeNode* node = &root;
	auto itr = input->begin();
	while (itr != input->end()) {
		if (auto child = node->HasChild(*itr); child != nullptr) {
			if (child->isLeaf)
				return true;  // if the child is a leaf, then we have found an excluded prefix
			node = child;
		} else {
			// there is no matching child, so the input does not have an excluded prefix
			return false;
		}
		itr++;
	}
	return false;
}

void ExclusionTree::DeleteChildren(TreeNode* node)
{
	exclwlock;
	DeleteChildrenIntern(node);
}

void ExclusionTree::DeleteChildrenIntern(TreeNode* node)
{
	// delete all children
	for (int32_t i = 0; i < node->children.size(); i++) {
		DeleteChildrenIntern(node->children[i]);
	}
	node->children.clear();
}

void ExclusionTree::DeleteNodeIntern(TreeNode* node)
{
	// delete all children of this node
	for (int32_t i = 0; i < node->children.size(); i++) {
		DeleteNodeIntern(node->children[i]);
	}
	node->children.clear();
	// delete the node
	hashmap.erase(node->id);
	delete node;
}

size_t ExclusionTree::GetStaticSize(int32_t version)
{
	static size_t size0x1 = Form::GetDynamicSize()  // form base size
	                        + 4                     // version
	                        + 8                     // nextid
	                        + 8                     // root children count
	                        + 1;                    // root isLeaf
	switch (version)
	{
	case 0x1:
		return size0x1;
	default:
		return 0;
	}
}

size_t ExclusionTree::GetDynamicSize()
{
	size_t sz = GetStaticSize();
	for (auto& [id, node] : hashmap)
	{
		// id, identifier, children, isLeaf
		sz += 8 + Buffer::CalcStringLength(node->identifier) + 8 * node->children.size() + 1;
	}
	return sz;
}

bool ExclusionTree::WriteData(unsigned char* buffer, size_t &offset)
{
	Buffer::Write(classversion, buffer, offset);
	Form::WriteData(buffer, offset);
	Buffer::Write(nextid, buffer, offset);
	// root children
	Buffer::WriteSize(root.children.size(), buffer, offset);
	for (int32_t i = 0; i < (int32_t)root.children.size(); i++)
	{
		Buffer::Write(root.children[i]->id, buffer, offset);
	}
	Buffer::Write(root.isLeaf, buffer, offset);
	// hashmap
	Buffer::WriteSize(hashmap.size(), buffer, offset);
	for (auto& [id, node] : hashmap)
	{
		Buffer::Write(id, buffer, offset);
		Buffer::Write(node->identifier, buffer, offset);
		Buffer::WriteSize(node->children.size(), buffer, offset);
		for (int32_t i = 0; i < (int32_t)node->children.size(); i++)
			Buffer::Write(node->children[i]->id, buffer, offset);
		Buffer::Write(node->isLeaf, buffer, offset);
	}
	return true;
}

bool ExclusionTree::ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver)
{
	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version) {
	case 0x1:
		{
			Form::ReadData(buffer, offset, length, resolver);
			nextid = Buffer::ReadUInt64(buffer, offset);
			size_t rch = Buffer::ReadSize(buffer, offset);
			// root
			root.id = 0;
			std::vector<uint64_t> rchid;
			for (int32_t i = 0; i < (int32_t)rch; i++)
				rchid.push_back(Buffer::ReadUInt64(buffer, offset));
			root.isLeaf = Buffer::ReadBool(buffer, offset);
			// hashmap
			hashmap.clear();
			size_t smap = Buffer::ReadSize(buffer, offset);
			for (int64_t i = 0; i < (int64_t)smap; i++) {
				if (offset > length)
					return false;
				TreeNode* node = new TreeNode();
				node->id = Buffer::ReadUInt64(buffer, offset);
				node->identifier = Buffer::ReadString(buffer, offset);
				uint64_t sch = Buffer::ReadSize(buffer, offset);
				for (int32_t c = 0; c < (int32_t)sch; c++)
					node->childrenids.push_back(Buffer::ReadUInt64(buffer, offset));
				node->isLeaf = Buffer::ReadBool(buffer, offset);
				hashmap.insert({ node->id, node });
			}
			// hashmap complete init all the links
			for (auto& [id, node] : hashmap) {
				for (int32_t i = 0; i < node->childrenids.size(); i++)
				{
					TreeNode* nnode = hashmap.at(node->childrenids[i]);
					if (nnode) {
						node->children.push_back(nnode);
					} else
						logcritical("cannot resolve nodeid {}", node->childrenids[i]);
				}
				node->childrenids.clear();
			}
			for (int32_t i = 0; i < rchid.size(); i++)
			{
				TreeNode* node = hashmap.at(rchid[i]);
				if (node) {
					root.children.push_back(node);
				} else
					logcritical("cannot resolve nodeid {}", rchid[i]);
			}
			return true;
		}
		break;
	default:
		return false;
	}
}
