#pragma once

#include "Form.h"

#include <string>
#include <vector>
#include <list>
#include <set>

class DerivationTree : public Form
{

private:
	const int32_t classversion = 0x1;

	void ClearInternal();

public:
	enum class NodeType
	{
		Terminal = 0,
		NonTerminal = 1,
		Sequence = 2,
	};
	struct Node
	{
		virtual NodeType Type() = 0;
		virtual std::pair<Node*, std::vector<Node*>> CopyRecursive() = 0;
	};
	
	struct NonTerminalNode : public Node
	{
		std::vector<Node*> _children;
		uint64_t _grammarID;

		NodeType Type() override { return NodeType::NonTerminal; }

		std::pair<Node*, std::vector<Node*>> CopyRecursive() override
		{
			std::vector<Node*> nodes;
			auto tmp = new NonTerminalNode();
			nodes.push_back(tmp);
			tmp->_grammarID = _grammarID;
			tmp->_children.resize(_children.size());
			for (int64_t i = 0; i < (int64_t)_children.size(); i++) {
				auto [node, cnodes] = _children[i]->CopyRecursive();
				tmp->_children[i] = node;
				// not available in linux (barf)
				//nodes.append_range(cnodes);
				auto begin = nodes.size();
				nodes.resize(begin + cnodes.size());
				for (size_t x = 0; x < cnodes.size(); x++)
					nodes[begin + x] = cnodes[x];
			}
			return { tmp, nodes };
		}
	};

	struct TerminalNode : public Node
	{
		uint64_t _grammarID;
		std::string _content;

		NodeType Type() override { return NodeType::Terminal; }

		std::pair<Node*, std::vector<Node*>> CopyRecursive() override
		{
			auto tmp = new TerminalNode();
			tmp->_grammarID = _grammarID;
			tmp->_content = _content;
			return { tmp, { tmp } };
		}
	};

	struct SequenceNode : public NonTerminalNode
	{
		NodeType Type() override { return NodeType::Sequence; }

		std::pair<Node*, std::vector<Node*>> CopyRecursive() override
		{
			std::vector<Node*> nodes;
			auto tmp = new SequenceNode();
			tmp->_grammarID = _grammarID;
			tmp->_children.resize(_children.size());
			for (int64_t i = 0; i < (int64_t)_children.size(); i++) {
				auto [node, cnodes] = _children[i]->CopyRecursive();
				tmp->_children[i] = node;
				// not available in linux (barf)
				//nodes.append_range(cnodes);
				auto begin = nodes.size();
				nodes.resize(begin + cnodes.size());
				for (size_t x = 0; x < cnodes.size(); x++)
					nodes[begin + x] = cnodes[x];
			}
			return { tmp, nodes };
		}
	};

	struct ParentTree
	{
		enum Method
		{
			/// <summary>
			/// created as a subset of a parent tree
			/// </summary>
			DD,
			/// <summary>
			/// created by extending an existing parent tree
			/// </summary>
			Extension,
		};

		Method method;

		/// <summary>
		/// formid of the parent derivation tree
		/// </summary>
		FormID _parentID = 0;
		/// <summary>
		/// begin in the sequence
		/// </summary>
		int64_t _posbegin = 0;
		/// <summary>
		/// length of the subsequence
		/// </summary>
		int64_t _length = 0;
		/// <summary>
		/// the maximum length of the sequence with regards to the sourcetree
		/// this is used in case the source sequence has been trimmed, and not the whole derivation tree
		/// is considered an input
		/// </summary>
		int64_t _stop = 0;
		/// <summary>
		/// whether to invert
		/// </summary>
		bool _complement = false;
	};

	Node* _root;
	FormID _grammarID;
	bool _valid = false;
	bool _regenerate = false;
	uint32_t _seed = 0;
	int32_t _targetlen = 0;
	std::set<Node*> _nodes;
	int64_t _sequenceNodes = 0;
	ParentTree _parent;
	FormID _inputID = 0;

	void Parse(std::string);

	#pragma region InheritedForm

	size_t GetStaticSize(int32_t version = 0x1) override;
	size_t GetDynamicSize() override;
	bool WriteData(unsigned char* buffer, size_t& offset) override;
	bool ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver) override;
	int32_t GetType() override {
		return FormType::DevTree;
	}
	static int32_t GetTypeStatic()
	{
		return FormType::DevTree;
	}
	void Delete(Data* data) override;
	/// <summary>
	/// resets all internal variables
	/// </summary>
	void Clear() override;
	inline static bool _registeredFactories = false;
	static void RegisterFactories();

	void FreeMemory() override;

	#pragma endregion
};
