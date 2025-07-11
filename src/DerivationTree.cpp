#include "DerivationTree.h"
#include "BufferOperations.h"
#include "Data.h"
#include "Allocators.h"

#include <stack>


void DerivationTree::NonTerminalNode::__Clear(Allocators* alloc)
{
	for (int i = 0; i < _children.size(); i++) {
		_children[i]->__Delete(alloc);
	}
	_children.clear();
}

void DerivationTree::NonTerminalNode::__Delete(Allocators* alloc)
{
	alloc->DerivationTree_NonTerminalNode()->Delete((DerivationTree::NonTerminalNode*)this);
}

void DerivationTree::TerminalNode::__Delete(Allocators* alloc)
{
	alloc->DerivationTree_TerminalNode()->Delete((DerivationTree::TerminalNode*)this);
}

void DerivationTree::SequenceNode::__Delete(Allocators* alloc)
{
	alloc->DerivationTree_SequenceNode()->Delete((DerivationTree::SequenceNode*)this);
}

std::pair<DerivationTree::Node*, std::vector<DerivationTree::Node*>> DerivationTree::TerminalNode::CopyRecursiveAlloc(Allocators* alloc)
{
	auto tmp = alloc->DerivationTree_TerminalNode()->New();
	tmp->_grammarID = _grammarID;
	tmp->_content = _content;
	return { tmp, { tmp } };
}

std::pair<DerivationTree::Node*, std::vector<DerivationTree::Node*>> DerivationTree::NonTerminalNode::CopyRecursiveAlloc(Allocators* alloc)
{
	std::vector<Node*> nodes;
	auto tmp = alloc->DerivationTree_NonTerminalNode()->New();
	nodes.push_back(tmp);
	tmp->_grammarID = _grammarID;
	tmp->_children.resize(_children.size());
	for (int64_t i = 0; i < (int64_t)_children.size(); i++) {
		auto [node, cnodes] = _children[i]->CopyRecursiveAlloc(alloc);
		tmp->_children[i] = node;
		// not available in linux (barf)
		//nodes.append_range(cnodes);
		auto begin = nodes.size();
		nodes.resize(begin + cnodes.size());
		for (size_t x = 0; x < cnodes.size(); x++)
			nodes[begin + x] = cnodes[x];
	}
	nodes.push_back(tmp);
	return { tmp, nodes };
}

std::pair<DerivationTree::Node*, std::vector<DerivationTree::Node*>> DerivationTree::SequenceNode::CopyRecursiveAlloc(Allocators* alloc)
{
	std::vector<Node*> nodes;
	auto tmp = alloc->DerivationTree_SequenceNode()->New();
	tmp->_grammarID = _grammarID;
	tmp->_children.resize(_children.size());
	for (int64_t i = 0; i < (int64_t)_children.size(); i++) {
		auto [node, cnodes] = _children[i]->CopyRecursiveAlloc(alloc);
		tmp->_children[i] = node;
		// not available in linux (barf)
		//nodes.append_range(cnodes);
		auto begin = nodes.size();
		nodes.resize(begin + cnodes.size());
		for (size_t x = 0; x < cnodes.size(); x++)
			nodes[begin + x] = cnodes[x];
	}
	nodes.push_back(tmp);
	return { tmp, nodes };
}

size_t DerivationTree::GetStaticSize(int32_t version)
{
	size_t size0x1 = 4                      // version
	                 + 8                     // grammarID
	                 + 1                     // regenerate
	                 + 4                     // seed
	                 + 4                     // targetlen
	                 + 8                     // _parent._parentID
	                 + 8                     // _parent._posbegin
	                 + 8                     // _parent._length
	                 + 8                     // _parent._stop
	                 + 1                     // _parent._complement
	                 + 8;                    // inputID
	size_t size0x2 = 4     // version
	                 + 8   // grammarID
	                 + 1   // regenerate
	                 + 4   // seed
	                 + 4   // targetlen
	                 + 8   // _parent._parentID
	                 + 8   // _parent._stop
	                 + 8   // _parent._length
	                 + 1   // _parent._complement
	                 + 8;  // inputID

	switch (version) {
	case 0x1:
		return size0x1;
	case 0x2:
		return size0x2;
	default:
		return 0;
	}
}

size_t DerivationTree::GetDynamicSize()
{
	return Form::GetDynamicSize()               // form stuff
	       + GetStaticSize(classversion)        // static size
	       + 8 + 16 * _parent.segments.size();  // sizzeof(), content of _parent.segments
}

#define CHECK(x)                                                                          \
	if (x == false) {                                                                     \
		logcritical("Buffer overflow error, len: {}, off: {}", abuf.Size(), abuf.Free()); \
		throw std::runtime_error("");                                                     \
	}

bool DerivationTree::WriteData(std::ostream* buffer, size_t& offset, size_t length)
{
	Buffer::Write(classversion, buffer, offset);
	Form::WriteData(buffer, offset, length);

	Buffer::ArrayBuffer abuf(length - offset);
	try {
		CHECK(abuf.Write<FormID>(_grammarID));
		CHECK(abuf.Write<bool>(_regenerate));
		CHECK(abuf.Write<uint32_t>(_seed));
		CHECK(abuf.Write<int32_t>(_targetlen));
		CHECK(abuf.Write<FormID>(_parent._parentID));
		CHECK(abuf.Write<size_t>(_parent.segments.size()));
		for (size_t i = 0; i < _parent.segments.size(); i++) {
			CHECK(abuf.Write<int64_t>(_parent.segments[i].first));
			CHECK(abuf.Write<int64_t>(_parent.segments[i].second));
		}
		CHECK(abuf.Write<int64_t>(_parent._length));
		CHECK(abuf.Write<int64_t>(_parent._stop));
		CHECK(abuf.Write<bool>(_parent._complement));
		CHECK(abuf.Write<FormID>(_inputID));
	}
	catch (std::exception&) {
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

bool DerivationTree::ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver)
{
	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version) {
	case 0x1:
		{
			Form::ReadData(buffer, offset, length, resolver);
			_grammarID = Buffer::ReadUInt64(buffer, offset);
			_regenerate = Buffer::ReadBool(buffer, offset);
			_seed = Buffer::ReadUInt32(buffer, offset);
			_targetlen = Buffer::ReadInt32(buffer, offset);
			_parent._parentID = Buffer::ReadUInt64(buffer, offset);
			_parent.segments.clear();
			auto _posbegin = Buffer::ReadInt64(buffer, offset);
			_parent.segments.push_back({ _posbegin, Buffer::ReadInt64(buffer, offset) });
			_parent._stop = Buffer::ReadInt64(buffer, offset);
			_parent._complement = Buffer::ReadBool(buffer, offset);
			_inputID = Buffer::ReadUInt64(buffer, offset);
			return true;
		}
	case 0x2:
		{
			Form::ReadData(buffer, offset, length, resolver);
			try {
				Buffer::ArrayBuffer data(buffer, length - offset);
				offset += length - offset;

				_grammarID = data.Read<FormID>();
				_regenerate = data.Read<bool>();
				_seed = data.Read<uint32_t>();
				_targetlen = data.Read<int32_t>();
				_parent._parentID = data.Read<FormID>();
				_parent.segments.clear();
				size_t len = data.Read<size_t>();
				for (size_t i = 0; i < len; i++) {
					auto _posbegin = data.Read<int64_t>();
					_parent.segments.push_back({ _posbegin, data.Read<int64_t>() });
				}
				_parent._length = data.Read<FormID>();
				_parent._stop = data.Read<int64_t>();
				_parent._complement = data.Read<bool>();
				_inputID = data.Read<FormID>();
			} catch (std::exception& e) {
				logcritical("Exception in read method: {}", e.what());
			}
		}
		return true;
	default:
		return false;
	}
}

void DerivationTree::InitializeEarly(LoadResolver* /*resolver*/)
{
}

void DerivationTree::InitializeLate(LoadResolver* /*resolver*/)
{
}

bool DerivationTree::CanDelete(Data* data)
{
	if (_inputID != 0)
	{
		if (auto input = data->LookupFormID<Input>(_inputID); input && input->GetDerivedInputs() > 0)
			return false;
	}
	return true;
}

void DerivationTree::Delete(Data*)
{
	Clear();
}

void DerivationTree::ClearInternal()
{
	_valid = false;
	if (_root == nullptr)
		return;

	auto allocators = Allocators::GetThreadAllocators(std::this_thread::get_id());
	//delete _root;
	_root->__Delete(allocators);
	/*auto itr = _nodes.begin();
	int count = 0;
	while (itr != _nodes.end()) {
		switch ((*itr)->Type()) {
		case NodeType::Terminal:
			{
				TerminalNode* n = (TerminalNode*)*itr;
				delete n;
			}
			break;
		case NodeType::NonTerminal:
			{
				NonTerminalNode* n = (NonTerminalNode*)*itr;
				delete n;
			}
			break;
		case NodeType::Sequence:
			{
				SequenceNode* n = (SequenceNode*)*itr;
				delete n;
			}
			break;
		}
		count++;
		itr++;
	}*/
	/*std::stack<Node*> stack;
	 stack.push(_root);
	while (stack.size() > 0)
	{
		auto node = stack.top();
		stack.pop();
		switch (node->Type()) {
		case NodeType::Terminal:
			{
				TerminalNode* n = (TerminalNode*)node;
				delete n;
			}
			break;
		case NodeType::NonTerminal:
			{
				NonTerminalNode* n = (NonTerminalNode*)node;
				for (auto child : n->_children)
					stack.push(child);
				delete n;
			}
			break;
		case NodeType::Sequence:
			{
				SequenceNode* n = (SequenceNode*)node;
				for (auto child : n->_children)
					stack.push(child);
				delete n;
			}
			break;
		}
	}*/
	_nodes = 0;
	//_grammarID = 0;
	//_valid = false;
	//_seed = 0;
	//_targetlen = 0;
	//_sequenceNodes = 0;
	//_parent = {};
	//_inputID = 0;
	_root = nullptr;
}

DerivationTree::~DerivationTree()
{
	Clear();
}

void DerivationTree::Clear()
{
	Lock();
	ClearInternal();
	Form::ClearForm();
	Unlock();
}

void DerivationTree::RegisterFactories()
{
	if (!_registeredFactories) {
		_registeredFactories = !_registeredFactories;
	}
}

void DerivationTree::FreeMemory()
{
	DerivationTree::Node* tmp = nullptr;
	if (TryLock()) {
		if (!HasFlag(FormFlags::DoNotFree))
		{
			//ClearInternal();
			_valid = false;
			if (_root == nullptr) {
				Form::Unlock();
				return;
			}
			tmp = _root;
			_root = nullptr;
			_nodes = 0;
		}
		Form::Unlock();
	}
	if (tmp != nullptr) {
		auto allocators = Allocators::GetThreadAllocators(std::this_thread::get_id());
		//delete tmp;
		tmp->__Delete(allocators);
	}
}

bool DerivationTree::Freed()
{
	if (_nodes == 0)
		return true;
	return false;
}

void DerivationTree::SetRegenerate(bool value)
{
	if (value == false)
		logwarn("warn");
	// taint changed
	SetChanged();
	_regenerate = value;
}

bool DerivationTree::GetRegenerate()
{
	return _regenerate;
}

size_t DerivationTree::MemorySize()
{
	if (_nodes > 0)
		logdebug("haha");
	return sizeof(DerivationTree) + sizeof(std::pair<int64_t, int64_t>) * _parent.segments.size() + (8 + 8 + sizeof(NonTerminalNode)) * _nodes;
}

void DerivationTree::DeepCopy(std::shared_ptr<DerivationTree> other)
{
	other->_inputID = _inputID;
	other->_parent = _parent;
	other->_sequenceNodes = _sequenceNodes;
	other->_nodes = _nodes;
	other->_seed = _seed;
	other->_targetlen = _targetlen;
	other->_valid = _valid;
	other->_grammarID = _grammarID;
	if (_root != nullptr) {
		other->_root = _root->CopyRecursive().first;
	} else
		other->_root = nullptr;
	other->_regenerate = _regenerate;
}
