#include "DerivationTree.h"
#include "BufferOperations.h"
#include "Data.h"

#include <stack>

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

bool DerivationTree::WriteData(std::ostream* buffer, size_t& offset)
{
	Buffer::Write(classversion, buffer, offset);
	Form::WriteData(buffer, offset);
	Buffer::Write(_grammarID, buffer, offset);
	Buffer::Write(_regenerate, buffer, offset);
	Buffer::Write(_seed, buffer, offset);
	Buffer::Write(_targetlen, buffer, offset);
	Buffer::Write(_parent._parentID, buffer, offset);
	Buffer::WriteSize(_parent.segments.size(), buffer, offset);
	for (size_t i = 0; i < _parent.segments.size(); i++) {
		Buffer::Write(_parent.segments[i].first, buffer, offset);
		Buffer::Write(_parent.segments[i].second, buffer, offset);
	}
	Buffer::Write(_parent._length, buffer, offset);
	Buffer::Write(_parent._stop, buffer, offset);
	Buffer::Write(_parent._complement, buffer, offset);
	Buffer::Write(_inputID, buffer, offset);
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
			_grammarID = Buffer::ReadUInt64(buffer, offset);
			_regenerate = Buffer::ReadBool(buffer, offset);
			_seed = Buffer::ReadUInt32(buffer, offset);
			_targetlen = Buffer::ReadInt32(buffer, offset);
			_parent._parentID = Buffer::ReadUInt64(buffer, offset);
			_parent.segments.clear();
			size_t len = Buffer::ReadSize(buffer, offset);
			for (size_t i = 0; i < len; i++) {
				auto _posbegin = Buffer::ReadInt64(buffer, offset);
				_parent.segments.push_back({ _posbegin, Buffer::ReadInt64(buffer, offset) });
			}
			_parent._length = Buffer::ReadUInt64(buffer, offset);
			_parent._stop = Buffer::ReadInt64(buffer, offset);
			_parent._complement = Buffer::ReadBool(buffer, offset);
			_inputID = Buffer::ReadUInt64(buffer, offset);
		}
		return true;
	default:
		return false;
	}
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
	auto itr = _nodes.begin();
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
	}
	_nodes.clear();
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
	if (TryLock()) {
		if (!HasFlag(FormFlags::DoNotFree))
		{
			ClearInternal();
		}
		Form::Unlock();
	}
}
