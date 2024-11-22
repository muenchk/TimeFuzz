#include "ExclusionTree.h"
#include "Input.h"
#include "BufferOperations.h"
#include "Logging.h"
#include "Oracle.h"
#include "Data.h"
#include "SessionData.h"

#include <stack>

#define exclrlock std::shared_lock<std::shared_mutex> guard(_lock);  //((void)0); 
#define exclwlock std::unique_lock<std::shared_mutex> guard(_lock);

ExclusionTree::TreeNode* ExclusionTree::TreeNode::HasChild(FormID stringID)
{
	for (int64_t i = 0; i < (int64_t)_children.size(); i++) {
		if (_children[i] && _children[i]->_stringID == stringID)
			return _children[i];
	}
	return nullptr;
}

ExclusionTree::ExclusionTree()
{
	root = new TreeNode();
	root->_result = OracleResult::Undefined;
	root->_id = 0;
}

ExclusionTree::~ExclusionTree()
{
	Clear();
}

void ExclusionTree::Init(std::shared_ptr<SessionData> sessiondata)
{
	_sessiondata = sessiondata;
}

void ExclusionTree::AddInput(std::shared_ptr<Input> input, OracleResult result)
{
	if (input.get() == nullptr)
		return;
	if (input->begin() == input->end())
		return;
	if (_sessiondata->_settings->optimization.disableExclusionTree)
		return;

	exclwlock;
	TreeNode* node = root;
	int dep = 0;
	auto itr = input->begin();
	while (itr != input->end()) {
		FormID stringID = _sessiondata->data->GetIDFromString(*itr);
		if (auto child = node->HasChild(stringID); child != nullptr) {
			// if the current node already has the required child,
			// choose it as our node and go to the next _input entry
			if (child->_isLeaf) {
				// check the result to determine whether the _input is excluded or if its just unfinished
				if (child->_result == OracleResult::Passing || child->_result == OracleResult::Failing)
					return;  // the _input is already excluded
			}
			node = child;
		} else {
			// if there is no matching child, create one.
			TreeNode* newnode = new TreeNode;
			newnode->_result = OracleResult::Undefined;
			newnode->_id = nextid++;
			newnode->_stringID = stringID;
			node->_children.push_back(newnode);
			hashmap.insert({ newnode->_id, newnode });
			node = newnode;
		}
		dep++;
		itr++;
	}
	// The result of the inputs saved to this tree is fixed and any _input that contains
	// it as a prefix yields the same result.
	// Thus we can remove all children from our current node, as they would yield the same result.
	if (result == OracleResult::Passing || result == OracleResult::Failing) {
		node->_result = result;
		node->_isLeaf = true;
		node->_InputID = input->GetFormID();
		leafcount++;
		DeleteChildrenIntern(node);
	} else {
		node->_result = result;
		node->_InputID = input->GetFormID();
	}
	// adjust depth
	if (depth < dep)
		depth = dep;
}

bool ExclusionTree::HasPrefix(std::shared_ptr<Input> input)
{
	static FormID prefixID;
	return HasPrefix(input, prefixID);
}

bool ExclusionTree::HasPrefix(std::shared_ptr<Input> input, FormID& prefixID)
{
	if (input.get() == nullptr)
		return false;

	//std::shared_lock<std::shared_mutex> guard(_lock);

	prefixID = 0;

	TreeNode* node = root;
	auto itr = input->begin();
	size_t count = 0;
	while (itr != input->end()) {
		FormID stringID = _sessiondata->data->GetIDFromString(*itr);
		if (auto child = node->HasChild(stringID); child != nullptr) {
			if (child->_isLeaf) {
				child->_visitcount++; // this will lead to races, but as we do not need a precise number, a few races don't matter
				prefixID = child->_InputID;
				return true;  // if the child is a leaf, then we have found an excluded prefix
			}
			node = child;
		} else {
			// there is no matching child, so the _input does not have an excluded prefix
			return false;
		}
		itr++;
		count++;
		// do this here because the loop will exit
		if (count == input->Length()) {
			// if the current depth is identical to our length and the result is unfinished
			// we are trying to run a test a second time
			if (node->_result == OracleResult::Unfinished) {
				prefixID = node->_InputID;
				return true;
			}
		}
	}
	return false;
}

std::tuple<bool, FormID, bool, FormID> ExclusionTree::HasPrefixAndShortestExtension(std::shared_ptr<Input> input)
{
	if (input.get() == nullptr)
		return { false, 0, false, 0 };

	//std::shared_lock<std::shared_mutex> guard(_lock);

	FormID prefixID = 0;

	TreeNode* node = root;
	auto itr = input->begin();
	size_t count = 0;
	while (itr != input->end()) {
		FormID stringID = _sessiondata->data->GetIDFromString(*itr);
		if (auto child = node->HasChild(stringID); child != nullptr) {
			if (child->_isLeaf) {
				child->_visitcount++;  // this will lead to races, but as we do not need a precise number, a few races don't matter
				prefixID = child->_InputID;
				return { true, prefixID, false, 0 };  // if the child is a leaf, then we have found an excluded prefix
			}
			node = child;
		} else {
			// there is no matching child, so the _input does not have an excluded prefix
			return { false, 0, false, 0 };
		}
		itr++;
		count++;
		// do this here because the loop will exit
		if (count == input->Length()) {
			// if the current depth is identical to our length and the result is unfinished
			// we are trying to run a test a second time
			if (node->_result == OracleResult::Unfinished) {
				prefixID = node->_InputID;
				return { true, prefixID, false, 0 };
			}
		}
	}
	// if we got here we are in the middle of the tree. The current node is neither a leaf 
	// nor is it an unfinished input, so we have no information about our input readily available.
	
	// we will now commence breath-first search to find the shortest extension of our input, and
	// return that if we find one

	std::queue<TreeNode*> nodequeue;
	nodequeue.push(node);
	while (nodequeue.size() > 0)
	{
		node = nodequeue.front();
		nodequeue.pop();
		// if the node is a leaf we have found the shortest extension and can return a result
		if (node->_isLeaf)
			return { false, 0, true, node->_InputID };
		// if the node represents an unfinished result we also have found the shortest extension
		else if (node->_result == OracleResult::Unfinished)
			return { false, 0, true, node->_InputID };
		else
		{
			// the node isn' a leaf, so add it's children to the stack and continue
			for (auto treenode : node->_children)
				nodequeue.push(treenode);
		}
	}

	return { false, 0, false, 0 };
}

void ExclusionTree::DeleteChildren(TreeNode* node)
{
	exclwlock;
	DeleteChildrenIntern(node);
}

void ExclusionTree::DeleteChildrenIntern(TreeNode* node)
{
	// delete all children
	// recursion free
	std::stack<TreeNode*> stack;
	for (int64_t i = 0; i < (int64_t)node->_children.size(); i++)
		stack.push(node->_children[i]);
	node->_children.clear();
	TreeNode* tmp = nullptr;
	while (stack.size() > 0)
	{
		tmp = stack.top();
		stack.pop();
		for (int64_t i = 0; i < (int64_t)tmp->_children.size(); i++) {
			stack.push(tmp->_children[i]);
		}
		if (tmp->_isLeaf)
			leafcount--;
		delete tmp;
	}

	// delete all children
	for (int64_t i = 0; i < (int64_t)node->_children.size(); i++) {
		DeleteChildrenIntern(node->_children[i]);
		if (node->_children[i]->_isLeaf)
			leafcount--;
		delete node->_children[i];
	}
	node->_children.clear();
}

void ExclusionTree::DeleteNodeIntern(TreeNode* node)
{
	// delete all children of this node
	for (int64_t i = 0; i < (int64_t)node->_children.size(); i++) {
		DeleteNodeIntern(node->_children[i]);
	}
	node->_children.clear();
	// delete the node
	hashmap.erase(node->_id);
	delete node;
}

int64_t ExclusionTree::GetDepth()
{
	return depth;
}

uint64_t ExclusionTree::GetNodeCount()
{
	return hashmap.size();
}

uint64_t ExclusionTree::GetLeafCount()
{
	return leafcount;
}

size_t ExclusionTree::GetStaticSize(int32_t version)
{
	static size_t size0x1 = 4                       // version
	                        + 8                     // nextid
	                        + 8                     // root children count
	                        + 1                     // root isLeaf
	                        + 8                     // size of hashmap
	                        + 8                     // depth
	                        + 8;                    // leafcount
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
	size_t sz = Form::GetDynamicSize()  // form stuff
	            + GetStaticSize();
	// root children
	sz += 8 * root->_children.size();
	for (auto& [id, node] : hashmap)
	{
		// id, stringID, visitcount, children count, children, isLeaf, result, InputID
		sz += 8 + 8 + 8 + 8 + 8 * node->_children.size() + 1 + 4 + 8;
	}
	return sz;
}

bool ExclusionTree::WriteData(unsigned char* buffer, size_t &offset)
{
	Buffer::Write(classversion, buffer, offset);
	Form::WriteData(buffer, offset);
	Buffer::Write(nextid, buffer, offset);
	// root children
	Buffer::WriteSize(root->_children.size(), buffer, offset);
	for (int32_t i = 0; i < (int32_t)root->_children.size(); i++)
	{
		Buffer::Write(root->_children[i]->_id, buffer, offset);
	}
	Buffer::Write(root->_isLeaf, buffer, offset);
	// hashmap
	Buffer::WriteSize(hashmap.size(), buffer, offset);
	for (auto& [id, node] : hashmap)
	{
		Buffer::Write(id, buffer, offset);
		Buffer::Write(node->_stringID, buffer, offset);
		Buffer::Write(node->_visitcount, buffer, offset);
		Buffer::WriteSize(node->_children.size(), buffer, offset);
		for (int32_t i = 0; i < (int32_t)node->_children.size(); i++)
			Buffer::Write(node->_children[i]->_id, buffer, offset);
		Buffer::Write(node->_isLeaf, buffer, offset);
		Buffer::Write((int32_t)node->_result, buffer, offset);
		Buffer::Write(node->_InputID, buffer, offset);
	}
	Buffer::Write(depth, buffer, offset);
	Buffer::Write(leafcount, buffer, offset);
	return true;
}

bool ExclusionTree::ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver)
{
	return ReadData(buffer, offset, length, resolver, false);
}

bool ExclusionTree::ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver, bool skipData)
{
	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version) {
	case 0x1:
		{
			Form::ReadData(buffer, offset, length, resolver);
			if (skipData)
			{
				// reset form to factory settings
				root = nullptr;
				nextid = 1;
				depth = 0;
				leafcount = 0;
				hashmap.clear();
				resolver->AddTask([this, resolver]() {
					_sessiondata = resolver->_data->CreateForm<SessionData>();
				});

			} else {
				// load data
				nextid = Buffer::ReadUInt64(buffer, offset);
				size_t rch = Buffer::ReadSize(buffer, offset);
				// root
				root->_id = 0;
				std::vector<uint64_t> rchid;
				for (int32_t i = 0; i < (int32_t)rch; i++)
					rchid.push_back(Buffer::ReadUInt64(buffer, offset));
				root->_isLeaf = Buffer::ReadBool(buffer, offset);
				// hashmap
				hashmap.clear();
				size_t smap = Buffer::ReadSize(buffer, offset);
				for (int64_t i = 0; i < (int64_t)smap; i++) {
					if (offset > length)
						return false;
					TreeNode* node = new TreeNode();
					node->_id = Buffer::ReadUInt64(buffer, offset);
					node->_stringID = Buffer::ReadUInt64(buffer, offset);
					node->_visitcount = Buffer::ReadUInt64(buffer, offset);
					uint64_t sch = Buffer::ReadSize(buffer, offset);
					for (int32_t c = 0; c < (int32_t)sch; c++)
						node->_childrenids.push_back(Buffer::ReadUInt64(buffer, offset));
					node->_isLeaf = Buffer::ReadBool(buffer, offset);
					node->_result = (OracleResult)Buffer::ReadInt32(buffer, offset);
					node->_InputID = Buffer::ReadUInt64(buffer, offset);
					hashmap.insert({ node->_id, node });
				}
				// rest
				depth = Buffer::ReadInt64(buffer, offset);
				leafcount = Buffer::ReadUInt64(buffer, offset);
				// hashmap complete init all the links
				for (auto& [id, node] : hashmap) {
					for (int32_t i = 0; i < node->_childrenids.size(); i++) {
						TreeNode* nnode = hashmap.at(node->_childrenids[i]);
						if (nnode) {
							node->_children.push_back(nnode);
						} else
							logcritical("cannot resolve nodeid {}", node->_childrenids[i]);
					}
					node->_childrenids.clear();
				}
				for (int32_t i = 0; i < rchid.size(); i++) {
					TreeNode* node = hashmap.at(rchid[i]);
					if (node) {
						root->_children.push_back(node);
					} else
						logcritical("cannot resolve nodeid {}", rchid[i]);
				}
				resolver->AddTask([this, resolver]() {
					_sessiondata = resolver->_data->CreateForm<SessionData>();
				});
			}
			return true;
		}
		break;
	default:
		return false;
	}
}

void ExclusionTree::Delete(Data*)
{
	Clear();
}


void ExclusionTree::Clear()
{
	DeleteChildren(root);
	_sessiondata.reset();
}

void ExclusionTree::RegisterFactories()
{
	if (!_registeredFactories) {
		_registeredFactories = !_registeredFactories;
	}
}
