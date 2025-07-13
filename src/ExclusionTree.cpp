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


namespace Hashing
{
	size_t hash(ExclusionTreeNode const& node)
	{
		std::size_t hs = 0;
		hs = std::hash<FormID>{}(node._stringID);
		hs = hs ^ (std::hash<FormID>{}(node._id) << 1);
		hs = hs ^ (std::hash<bool>{}(node._isLeaf) << 1);
		hs = hs ^ (std::hash<OracleResult>{}(node._result) << 1);
		hs = hs ^ (std::hash<FormID>{}(node._InputID) << 1);
		auto itr = node._children.begin();
		while (itr != node._children.end()) {
			hs = hs ^ (std::hash<FormID>{}((*itr)->_id) << 1);
			itr++;
		}
		return hs;
	}
}

std::shared_ptr<ExclusionTreeNode> ExclusionTreeNode::HasChild(FormID stringID)
{
	for (int64_t i = 0; i < (int64_t)_children.size(); i++) {
		if (_children[i] && _children[i]->_stringID == stringID)
			return _children[i];
	}
	return nullptr;
}

size_t ExclusionTreeNode::GetStaticSize(int32_t version)
{
	static size_t size0x1 = 4     // version
	                 + 8   // _stringID
	                 + 8   // _id
	                 + 1   // _isLeaf
	                 + 8   // _result
	                 + 8;  // _InputID

	switch (version) {
	case 0x1:
		return size0x1;
	default:
		return 0;
	}
}
size_t ExclusionTreeNode::GetDynamicSize()
{
	size_t sz = Form::GetDynamicSize()  // form stuff
	            + GetStaticSize(classversion);
	sz += 8 + _children.size() * 8;
	return sz;
}
#define CHECK(x)                                                                          \
	if (x == false) {                                                                     \
		logcritical("Buffer overflow error, len: {}, off: {}", abuf.Size(), abuf.Free()); \
		throw std::runtime_error("");                                                     \
	}
bool ExclusionTreeNode::WriteData(std::ostream* buffer, size_t& offset, size_t length)
{
	Buffer::Write(classversion, buffer, offset);
	Form::WriteData(buffer, offset, length);

	Buffer::ArrayBuffer abuf(length - offset);
	try {
		CHECK(abuf.Write<FormID>(_stringID));
		CHECK(abuf.Write<FormID>(_id));
		CHECK(abuf.Write<size_t>(_children.size()));
		for (int64_t i = 0; i < (int64_t)_children.size(); i++) {
			if (_children[i]) {
				CHECK(abuf.Write<FormID>(_children[i]->GetFormID()));
			} else {
				CHECK(abuf.Write<FormID>(0));
			}
		}
		CHECK(abuf.Write<bool>(_isLeaf));
		CHECK(abuf.Write<OracleResult>(_result));
		CHECK(abuf.Write<FormID>(_InputID));
	} catch (std::exception&) {
		auto [data, sz] = abuf.GetBuffer();
		buffer->write((char*)data, sz);
		offset += sz;
		return false;
	}

	auto [data, sz] = abuf.GetBuffer();
	buffer->write((char*)data, sz);
	offset += sz;
	return true;
}
bool ExclusionTreeNode::ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver)
{
	if (_loadData)
		delete _loadData;
	_loadData = new LoadData();

	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version) {
	case 0x1:
		{
			Form::ReadData(buffer, offset, length, resolver);
			try {
				Buffer::ArrayBuffer data(buffer, length - offset);
				_stringID = data.Read<FormID>();
				_id = data.Read<FormID>();
				size_t size = data.Read<size_t>();
				for (int64_t i = 0; i < (int64_t)size; i++)
					_loadData->_nodes.push_back(data.Read<FormID>());
				_isLeaf = data.Read<bool>();
				_result = data.Read<OracleResult>();
				_InputID = data.Read<FormID>();
			} catch (std::exception& e) {
				logcritical("Exception in read method: {}", e.what());
				return false;
			}
		}
		return true;
	default:
		return false;
	}
}
void ExclusionTreeNode::InitializeEarly(LoadResolver* resolver)
{
	if (_loadData) {
		for (int64_t x = 0; x < (int64_t)_loadData->_nodes.size(); x++)
		{
			auto ptr = resolver->ResolveFormID<ExclusionTreeNode>(_loadData->_nodes[x]);
			if (ptr)
				_children.push_back(ptr);
			else
				logwarn("Exclusion Tree Node couldn't be found");
		}

		delete _loadData;
		_loadData = nullptr;
	}
}
void ExclusionTreeNode::InitializeLate(LoadResolver* /*resolver*/)
{
}
void ExclusionTreeNode::Delete(Data*)
{
	Clear();
}
void ExclusionTreeNode::Clear()
{
	_stringID = 0;
	_id = 0;
	_children.clear();
	_children.shrink_to_fit();
	_result = OracleResult::None;
	_InputID = 0;
}
void ExclusionTreeNode::RegisterFactories()
{
}
size_t ExclusionTreeNode::MemorySize()
{
	return sizeof(ExclusionTreeNode) + _children.size()*16;
}




ExclusionTree::ExclusionTree()
{
}

ExclusionTree::~ExclusionTree()
{
	Clear();
	if (_loadData)
		delete _loadData;
}

void ExclusionTree::Init(std::shared_ptr<SessionData> sessiondata)
{
	_sessiondata = sessiondata;
	if (!root) {
		root = sessiondata->data->CreateForm<ExclusionTreeNode>();
		root->_result = OracleResult::Undefined;
		root->_id = 0;
	}
}

void ExclusionTree::AddInput(std::shared_ptr<Input> input, OracleResult result)
{
	if (input.get() == nullptr)
		return;
	if (input->begin() == input->end())
		return;
	if (_sessiondata->_settings->optimization.disableExclusionTree || !_sessiondata->_settings->runtime.enableExclusionTree)
		return;
	if (input->GetSequenceLength() > _sessiondata->_settings->optimization.exclusionTreeLengthLimit)
		return;

	exclwlock;
	std::shared_ptr<ExclusionTreeNode> node = root;
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
			SetChanged();
			// if there is no matching child, create one.
			std::shared_ptr<ExclusionTreeNode> newnode = _sessiondata->data->CreateForm<ExclusionTreeNode>();
			newnode->SetChanged();
			newnode->_result = OracleResult::Undefined;
			newnode->_id = nextid++;
			newnode->_stringID = stringID;
			node->_children.push_back(newnode);
			node->SetChanged();
			nodecount++;
			//hashmap.insert({ newnode->_id, newnode });
			node = newnode;
		}
		dep++;
		itr++;
	}
	// The result of the inputs saved to this tree is fixed and any _input that contains
	// it as a prefix yields the same result.
	// Thus we can remove all children from our current node, as they would yield the same result.
	if (result == OracleResult::Passing || result == OracleResult::Failing) {
		node->SetChanged();
		node->_result = result;
		node->_isLeaf = true;
		node->_InputID = input->GetFormID();
		leafcount++;
		if (node->_children.size() > 0) {
				DeleteChildrenIntern(node);
			loginfo("Killing some children");
		}
	} else {
		node->SetChanged();
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
	if (_sessiondata->_settings->optimization.disableExclusionTree || !_sessiondata->_settings->runtime.enableExclusionTree)
		return false;

	//std::shared_lock<std::shared_mutex> guard(_lock);
	exclrlock

	prefixID = 0;

	std::shared_ptr<ExclusionTreeNode> node = root;
	auto itr = input->begin();
	size_t count = 0;
	while (itr != input->end()) {
		FormID stringID = _sessiondata->data->GetIDFromString(*itr);
		if (auto child = node->HasChild(stringID); child != nullptr) {
			if (child->_isLeaf) {
				//child->_visitcount++; // this will lead to races, but as we do not need a precise number, a few races don't matter
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
		if (count == input->GetSequenceLength()) {
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
	if (_sessiondata->_settings->optimization.disableExclusionTree || !_sessiondata->_settings->runtime.enableExclusionTree) {
		return { false, 0, false, 0 };
	}

	//std::shared_lock<std::shared_mutex> guard(_lock);
	exclrlock

	FormID prefixID = 0;

	std::shared_ptr<ExclusionTreeNode> node = root;
	auto itr = input->begin();
	size_t count = 0;
	while (itr != input->end()) {
		FormID stringID = _sessiondata->data->GetIDFromString(*itr);
		if (auto child = node->HasChild(stringID); child != nullptr) {
			if (child->_isLeaf) {
				//child->_visitcount++;  // this will lead to races, but as we do not need a precise number, a few races don't matter
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
		if (count == input->GetSequenceLength()) {
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

	std::queue<std::shared_ptr<ExclusionTreeNode>> nodequeue;
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

void ExclusionTree::DeleteChildren(std::shared_ptr<ExclusionTreeNode> node)
{
	SetChanged();
	exclwlock;
	DeleteChildrenIntern(node);
}

void ExclusionTree::DeleteChildrenIntern(std::shared_ptr<ExclusionTreeNode> node)
{
	// delete all children
	// recursion free
	std::stack<std::shared_ptr<ExclusionTreeNode>> stack;
	for (int64_t i = 0; i < (int64_t)node->_children.size(); i++)
		stack.push(node->_children[i]);
	node->_children.clear();
	std::shared_ptr<ExclusionTreeNode> tmp = nullptr;
	while (stack.size() > 0)
	{
		tmp = stack.top();
		stack.pop();
		for (int64_t i = 0; i < (int64_t)tmp->_children.size(); i++) {
			stack.push(tmp->_children[i]);
		}
		if (tmp->_isLeaf)
			leafcount--;
		//hashmap.erase(tmp->_id);
		nodecount--;
		_sessiondata->data->DeleteForm(tmp);
		//delete tmp;
	}

	// delete all children
	for (int64_t i = 0; i < (int64_t)node->_children.size(); i++) {
		DeleteChildrenIntern(node->_children[i]);
		if (node->_children[i]->_isLeaf)
			leafcount--;
		//hashmap.erase(node->_children[i]->_id);
		nodecount--;
		_sessiondata->data->DeleteForm(node->_children[i]);
		//delete node->_children[i];
	}
	node->_children.clear();
}

void ExclusionTree::DeleteNodeIntern(std::shared_ptr<ExclusionTreeNode> node)
{
	// delete all children of this node
	for (int64_t i = 0; i < (int64_t)node->_children.size(); i++) {
		DeleteNodeIntern(node->_children[i]);
	}
	node->_children.clear();
	// delete the node
	//hashmap.erase(node->_id);
	nodecount--;
	_sessiondata->data->DeleteForm(node);
	//delete node;
}

int64_t ExclusionTree::GetDepth()
{
	return depth;
}

uint64_t ExclusionTree::GetNodeCount()
{
	//return hashmap.size();
	return nodecount;
}

uint64_t ExclusionTree::GetLeafCount()
{
	return leafcount;
}

double ExclusionTree::CheckForAlternatives(int32_t alternativesPerNode)
{
	if (alternativesPerNode == 0)
		return 0;
	exclrlock;

	struct Path
	{
	public:
		std::vector<std::shared_ptr<ExclusionTreeNode>> nodes;
		bool finished = false;

		Path* Copy()
		{
			Path* path = new Path();
			for (auto node : nodes)
				path->nodes.push_back(node);
			return path;
		}
	};
	std::vector<Path*> done;
	int64_t open = 0;
	int64_t leaf = 0;

	std::stack<std::pair<std::shared_ptr<ExclusionTreeNode>, Path*>> stack;
	for (auto child : root->_children)
		stack.push({ child, new Path() });
	while (stack.size() > 0)
	{
		auto [node, path] = stack.top();
		stack.pop();
		path->nodes.push_back(node);
		if (node->_isLeaf) {
			path->finished = true;
			done.push_back(path);
			leaf++;
		} else {
			for (auto child : node->_children)
				stack.push({ child, path->Copy() });
			if ((int64_t)node->_children.size() < alternativesPerNode) {
				done.push_back(path);
				open++;
			} else
				delete path;
		}
	}

	logmessage("FINISHED PATHES:    {}", leaf);
	logmessage("OPEN PATHES:        {}", open);
	// print pathes
	for (auto path : done)
	{
		std::string message = "";
		if (path->finished)
			message = "LEAF:\t";
		else
			message = "OPEN:\t";
		for (auto node : path->nodes)
		{
			message += _sessiondata->data->GetStringFromID(node->_stringID).first + "|";
		}
		logmessage("{}", message);
		delete path;
	}
	return 0;
}

size_t ExclusionTree::GetStaticSize(int32_t version)
{
	static size_t size0x1 = 4     // version
	                        + 8   // nextid
	                        + 8   // root children count
	                        + 1   // root isLeaf
	                        + 8   // size of hashmap
	                        + 8   // depth
	                        + 8;  // leafcount
	static size_t size0x2 = size0x1;
	static size_t size0x3 = 4     // version
	                        + 8   // nextid
	                        + 8   // rootid
	                        + 8   // depth
	                        + 8   // leafcount
	                        + 8;  // nodecount
	switch (version) {
	case 0x1:
		return size0x1;
	case 0x2:
		return size0x2;
	case 0x3:
		return size0x3;
	default:
		return 0;
	}
}

size_t ExclusionTree::GetDynamicSize()
{
	size_t sz = Form::GetDynamicSize()  // form stuff
	            + GetStaticSize(classversion);
	// root children
	//sz += 8 * root->_children.size();
	//for (auto& [id, node] : hashmap)
	//{
		// id, stringID, visitcount, children count, children, isLeaf, result, InputID
	//	sz += 8 + 8 + /*8 +*/ 8 + 8 * node->_children.size() + 1 + 4 + 8;
	//}
	return sz;
}

bool ExclusionTree::WriteData(std::ostream* buffer, size_t& offset, size_t length)
{
	Buffer::Write(classversion, buffer, offset);
	Form::WriteData(buffer, offset, length);
	Buffer::Write(nextid, buffer, offset);
	if (root)
		Buffer::Write(root->GetFormID(), buffer, offset);
	else
		Buffer::Write((FormID)0, buffer, offset);
	// root children
	//Buffer::WriteSize(root->_children.size(), buffer, offset);
	//for (int32_t i = 0; i < (int32_t)root->_children.size(); i++)
	//{
	//	Buffer::Write(root->_children[i]->_id, buffer, offset);
	//}
	//Buffer::Write(root->_isLeaf, buffer, offset);
	// hashmap
	/*Buffer::WriteSize(hashmap.size(), buffer, offset);
	for (auto& [id, node] : hashmap)
	{
		Buffer::Write(id, buffer, offset);
		Buffer::Write(node->_stringID, buffer, offset);
		//Buffer::Write(node->_visitcount, buffer, offset);
		Buffer::WriteSize(node->_children.size(), buffer, offset);
		for (int32_t i = 0; i < (int32_t)node->_children.size(); i++)
			Buffer::Write(node->_children[i]->_id, buffer, offset);
		Buffer::Write(node->_isLeaf, buffer, offset);
		Buffer::Write((int32_t)node->_result, buffer, offset);
		Buffer::Write(node->_InputID, buffer, offset);
	}*/
	Buffer::Write(depth, buffer, offset);
	Buffer::Write(leafcount, buffer, offset);
	Buffer::Write(nodecount, buffer, offset);
	return true;
}

bool ExclusionTree::ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver)
{
	if (_loadData)
		delete _loadData;
	_loadData = new LoadData();

	// get settings
	auto settings = resolver->_data->CreateForm<Settings>();
	// clear everything
	Clear();

	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version) {
	/*case 0x1:
		{
			Form::ReadData(buffer, offset, length, resolver);
			// load data
			nextid = Buffer::ReadUInt64(buffer, offset);
			size_t rch = Buffer::ReadSize(buffer, offset);
			// root
			root->_id = 0;
			std::vector<uint64_t> rchid;
			for (int32_t i = 0; i < (int32_t)rch; i++)
				if (settings->runtime.enableExclusionTree) {
					rchid.push_back(Buffer::ReadUInt64(buffer, offset));
				} else {
					Buffer::ReadUInt64(buffer, offset);
				}
			root->_isLeaf = Buffer::ReadBool(buffer, offset);
			// hashmap
			//hashmap.clear();
			size_t smap = Buffer::ReadSize(buffer, offset);
			for (int64_t i = 0; i < (int64_t)smap; i++) {
				if (offset > length)
					return false;
				ExclusionTreeNode* node = new ExclusionTreeNode();
				node->_id = Buffer::ReadUInt64(buffer, offset);
				node->_stringID = Buffer::ReadUInt64(buffer, offset);
				//node->_visitcount = 
				Buffer::ReadUInt64(buffer, offset);
				uint64_t sch = Buffer::ReadSize(buffer, offset);
				for (int32_t c = 0; c < (int32_t)sch; c++)
					node->_children.push_back((ExclusionTreeNode*)Buffer::ReadUInt64(buffer, offset));
				//node->_childrenids.push_back(Buffer::ReadUInt64(buffer, offset));
				node->_isLeaf = Buffer::ReadBool(buffer, offset);
				node->_result = (OracleResult)Buffer::ReadInt32(buffer, offset);
				node->_InputID = Buffer::ReadUInt64(buffer, offset);
				if (settings->runtime.enableExclusionTree) {
					hashmap.insert({ node->_id, node });
				} else
					delete node;
			}
			// rest
			depth = Buffer::ReadInt64(buffer, offset);
			leafcount = Buffer::ReadUInt64(buffer, offset);
			// hashmap complete init all the links
			for (auto& [id, node] : hashmap) {
				//for (int32_t i = 0; i < node->_childrenids.size(); i++) {
				for (int32_t i = 0; i < node->_children.size(); i++) {
					//ExclusionTreeNode* nnode = hashmap.at(node->_childrenids[i]);
					ExclusionTreeNode* nnode = hashmap.at((uint64_t)node->_children[i]);
					if (nnode) {
						node->_children[i] = nnode;
						//node->_children.push_back(nnode);
					} else
						logcritical("cannot resolve nodeid {}", (uint64_t)node->_children[i]);
				}
				//node->_childrenids.clear();
			}
			for (int32_t i = 0; i < rchid.size(); i++) {
				ExclusionTreeNode* node = hashmap.at(rchid[i]);
				if (node) {
					root->_children.push_back(node);
				} else
					logcritical("cannot resolve nodeid {}", rchid[i]);
			}
			return true;
		}
		break;
	case 0x2:
		{
			Form::ReadData(buffer, offset, length, resolver);
			// load data
			nextid = Buffer::ReadUInt64(buffer, offset);
			size_t rch = Buffer::ReadSize(buffer, offset);
			// root
			root->_id = 0;
			std::vector<uint64_t> rchid;
			for (int32_t i = 0; i < (int32_t)rch; i++)
				if (settings->runtime.enableExclusionTree) {
					rchid.push_back(Buffer::ReadUInt64(buffer, offset));
				} else {
					Buffer::ReadUInt64(buffer, offset);
				}
			root->_isLeaf = Buffer::ReadBool(buffer, offset);
			// hashmap
			hashmap.clear();
			size_t smap = Buffer::ReadSize(buffer, offset);
			for (int64_t i = 0; i < (int64_t)smap; i++) {
				if (offset > length)
					return false;
				ExclusionTreeNode* node = new ExclusionTreeNode();
				node->_id = Buffer::ReadUInt64(buffer, offset);
				node->_stringID = Buffer::ReadUInt64(buffer, offset);
				//node->_visitcount = Buffer::ReadUInt64(buffer, offset);
				uint64_t sch = Buffer::ReadSize(buffer, offset);
				for (int32_t c = 0; c < (int32_t)sch; c++)
					node->_children.push_back((ExclusionTreeNode*)Buffer::ReadUInt64(buffer, offset));
				//node->_childrenids.push_back(Buffer::ReadUInt64(buffer, offset));
				node->_isLeaf = Buffer::ReadBool(buffer, offset);
				node->_result = (OracleResult)Buffer::ReadInt32(buffer, offset);
				node->_InputID = Buffer::ReadUInt64(buffer, offset);
				if (settings->runtime.enableExclusionTree) {
					hashmap.insert({ node->_id, node });
				} else
					delete node;
			}
			// rest
			depth = Buffer::ReadInt64(buffer, offset);
			leafcount = Buffer::ReadUInt64(buffer, offset);
			// hashmap complete init all the links
			for (auto& [id, node] : hashmap) {
				//for (int32_t i = 0; i < node->_childrenids.size(); i++) {
				for (int32_t i = 0; i < node->_children.size(); i++) {
					//TreeNode* nnode = hashmap.at(node->_childrenids[i]);
					ExclusionTreeNode* nnode = hashmap.at((uint64_t)node->_children[i]);
					if (nnode) {
						node->_children[i] = nnode;
						//node->_children.push_back(nnode);
					} else
						logcritical("cannot resolve nodeid {}", (uint64_t)node->_children[i]);
				}
				//node->_childrenids.clear();
			}
			for (int32_t i = 0; i < rchid.size(); i++) {
				ExclusionTreeNode* node = hashmap.at(rchid[i]);
				if (node) {
					root->_children.push_back(node);
				} else
					logcritical("cannot resolve nodeid {}", rchid[i]);
			}
			return true;
		}
		break;*/
	case 0x3:
		{
			Form::ReadData(buffer, offset, length, resolver);
			// load data
			nextid = Buffer::ReadUInt64(buffer, offset);
			_loadData->rootid = Buffer::ReadUInt64(buffer, offset);
			depth = Buffer::ReadInt64(buffer, offset);
			leafcount = Buffer::ReadUInt64(buffer, offset);
			nodecount = Buffer::ReadUInt64(buffer, offset);
			return true;
		}
		break;
	default:
		return false;
	}
}

void ExclusionTree::InitializeEarly(LoadResolver* resolver)
{
	if (_loadData)
	{
		root = resolver->ResolveFormID<ExclusionTreeNode>(_loadData->rootid);
		delete _loadData;
		_loadData = nullptr;
	}
}

void ExclusionTree::InitializeLate(LoadResolver* resolver)
{
	_sessiondata = resolver->_data->CreateForm<SessionData>();
}

void ExclusionTree::Delete(Data*)
{
	Clear();
}


void ExclusionTree::Clear()
{
	Form::ClearForm();
	/* exclwlock;
	// fast delete, delete everything thats in the hashmap
	for (auto [id, node] : hashmap) {
		node->_children.clear();
		//node->_childrenids.clear();
		delete node;
	}*/
	//hashmap.clear();
	//DeleteChildren(root);
	//delete root;
	root.reset();
	_sessiondata.reset();
}

void ExclusionTree::RegisterFactories()
{
	if (!_registeredFactories) {
		_registeredFactories = !_registeredFactories;
	}
}

size_t ExclusionTree::MemorySize()
{
	//return sizeof(ExclusionTree) + hashmap.size() * (sizeof(std::pair<uint64_t, ExclusionTreeNode*>) + sizeof(ExclusionTreeNode) + 8);
	return sizeof(ExclusionTree);
}
