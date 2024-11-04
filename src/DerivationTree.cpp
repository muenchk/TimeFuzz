#include "DerivationTree.h"
#include "BufferOperations.h"

#include <stack>

size_t DerivationTree::GetStaticSize(int32_t version)
{
	size_t size0x1 = Form::GetDynamicSize()  // form size
	                 + 4;                   // version
	size_t size0x2 = size0x1                 // bisherige größe
	                 + 8                     // grammarID
	                 + 1                     // regenerate
	                 + 4                     // seed
	                 + 4;                    // targetlen
		

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
	return GetStaticSize(classversion);
}

bool DerivationTree::WriteData(unsigned char* buffer, size_t& offset)
{
	Buffer::Write(classversion, buffer, offset);
	Form::WriteData(buffer, offset);
	Buffer::Write(grammarID, buffer, offset);
	Buffer::Write(regenerate, buffer, offset);
	Buffer::Write(seed, buffer, offset);
	Buffer::Write(targetlen, buffer, offset);
	return true;
}

bool DerivationTree::ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver)
{
	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version)
	{
	case 0x1:
		Form::ReadData(buffer, offset, length, resolver);
		return true;
	case 0x2:
		Form::ReadData(buffer, offset, length, resolver);
		grammarID = Buffer::ReadUInt64(buffer, offset);
		regenerate = Buffer::ReadBool(buffer, offset);
		seed = Buffer::ReadUInt32(buffer, offset);
		targetlen = Buffer::ReadInt32(buffer, offset);
		return true;
	default:
		return false;
	}
}

void DerivationTree::Delete(Data*)
{
	Clear();
}

void DerivationTree::Clear()
{
	std::stack<Node*> stack;
	if (root == nullptr)
		return;
	for (auto node : nodes) {
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
				delete n;
			}
			break;
		case NodeType::Sequence:
			{
				SequenceNode* n = (SequenceNode*)node;
				delete n;
			}
			break;
		}
	}
	root = nullptr;
}

void DerivationTree::RegisterFactories()
{
	if (!_registeredFactories) {
		_registeredFactories = !_registeredFactories;
	}
}
