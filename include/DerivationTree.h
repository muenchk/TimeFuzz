#pragma once

#include "Form.h"
#include "Utility.h"
#include "Logging.h"
#include "Allocatable.h"

#include <string>
#include <vector>
#include <list>
#include <set>

class DerivationTree : public Form
{

private:
	const int32_t classversion = 0x2;

	bool _regenerate = false;

	void ClearInternal();

public:
	enum class NodeType
	{
		Terminal = 0,
		NonTerminal = 1,
		Sequence = 2,
	};
	struct Node : public Allocatable
	{
		virtual NodeType Type() = 0;
		virtual std::pair<Node*, std::vector<Node*>> CopyRecursive() = 0;
		virtual std::pair<Node*, std::vector<Node*>> CopyRecursiveAlloc(Allocators* alloc) = 0;

		virtual ~Node() {

		}
	};
	
	struct NonTerminalNode : public Node
	{
		std::vector<Node*> _children;
		uint64_t _grammarID;

		void __Clear(Allocators* alloc) override;
		void __Delete(Allocators* alloc) override;

		~NonTerminalNode()
		{
			for (int i = 0; i < _children.size(); i++)
				delete _children[i];
			_children.clear();
		}

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
			nodes.push_back(tmp);
			return { tmp, nodes };
		}

		std::pair<Node*, std::vector<Node*>> CopyRecursiveAlloc(Allocators* alloc) override;

		bool AddChild(Node* node)
		{
			if (node == nullptr) {
				logcritical("Trying to add nullptr to DevTree::NonTerminalNode");
				return false;
			} else {
				_children.push_back(node);
				return true;
			}
		}

		bool SetChild(size_t pos, Node* node)
		{
			if (node == nullptr) {
				logcritical("Trying to add nullptr to DevTree::NonTerminalNode");
				return false;
			} else {
				_children[pos] = node;
				return true;
			}
		}
	};

	struct TerminalNode : public Node
	{
		uint64_t _grammarID;
		std::string _content;

		void __Clear(Allocators*) override
		{
			_grammarID = 0;
			_content = "";
			_content.shrink_to_fit();
		}
		void __Delete(Allocators* alloc) override;

		NodeType Type() override { return NodeType::Terminal; }

		std::pair<Node*, std::vector<Node*>> CopyRecursive() override
		{
			auto tmp = new TerminalNode();
			tmp->_grammarID = _grammarID;
			tmp->_content = _content;
			return { tmp, { tmp } };
		}
		std::pair<Node*, std::vector<Node*>> CopyRecursiveAlloc(Allocators* alloc) override;
	};

	struct SequenceNode : public NonTerminalNode
	{
		NodeType Type() override { return NodeType::Sequence; }

		~SequenceNode()
		{
		}

		void __Delete(Allocators* alloc) override;

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
			nodes.push_back(tmp);
			return { tmp, nodes };
		}

		std::pair<Node*, std::vector<Node*>> CopyRecursiveAlloc(Allocators* alloc) override;
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
		/// segments in the parent input { positionBegin, length }
		/// </summary>
		std::vector<std::pair<int64_t, int64_t>> segments;
		/// <summary>
		/// the maximum length of the sequence with regards to the sourcetree
		/// this is used in case the source sequence has been trimmed, and not the whole derivation tree
		/// is considered an input
		/// </summary>
		int64_t _stop = 0;
		/// <summary>
		/// length of parent for length calculations
		/// </summary>
		int64_t _length = 0;
		/// <summary>
		/// whether to invert
		/// </summary>
		bool _complement = false;
	};

	~DerivationTree();

	Node* _root;
	int64_t _nodes;
	int64_t _sequenceNodes = 0;
	bool _valid = false;

private:
	FormID _grammarID;
	uint32_t _seed = 0;
	int32_t _targetlen = 0;
	ParentTree _parent;
	FormID _inputID = 0;

public:
	FormID GetGrammarID()
	{
		return _grammarID;
	}
	void SetGrammarID(FormID id)
	{
		CheckChanged(_grammarID, id);
	}
	uint32_t GetSeed()
	{
		return _seed;
	}
	void SetSeed(uint32_t seed)
	{
		CheckChanged(_seed, seed);
	}
	int32_t GetTargetLength() {
		return _targetlen;
	}
	void SetTargetLength(int32_t len) {
		CheckChanged(_targetlen, len);
	}
	ParentTree& GetParent() {
		return _parent;
	}
	void SetParentID(FormID id)
	{
		CheckChanged(_parent._parentID, id);
	}
	void SetParentLength(int64_t length)
	{
		CheckChanged(_parent._length, length);
	}
	void SetParentMethod(ParentTree::Method method)
	{
		CheckChanged(_parent.method, method);
	}
	void SetParentInformation(FormID parentid, std::vector<std::pair<int64_t, int64_t>> segments, int64_t stop, bool complement, ParentTree::Method method)
	{
		CheckChanged(_parent._parentID, parentid);
		CheckChanged(_parent.segments, segments);
		CheckChanged(_parent._stop, stop);
		CheckChanged(_parent._complement, complement);
		CheckChanged(_parent.method, method);
	}
	FormID GetInputID() {
		return _inputID;
	}
	void SetInputID(FormID id)
	{
		CheckChanged(_inputID, id);
	}


public:

	void SetRegenerate(bool vaue);
	bool GetRegenerate();

	void Parse(std::string);

	void DeepCopy(std::shared_ptr<DerivationTree> other);

	#pragma region InheritedForm

	static std::string PrintForm(std::shared_ptr<DerivationTree> form)
	{
		if (!form)
			return "None";
		return std::string("[") + typeid(DerivationTree).name() + "<FormID:" + Utility::GetHex(form->GetFormID()) + "><ParentID:" + Utility::GetHex(form->_parent._parentID) + "><Length:" + std::to_string(form->_sequenceNodes) + "><Valid:" + std::to_string(form->_valid) + "><Regenerate:" + std::to_string(form->_regenerate) + ">]";
	}

	size_t GetStaticSize(int32_t version) override;
	size_t GetDynamicSize() override;
	bool WriteData(std::ostream* buffer, size_t& offset, size_t length) override;
	bool ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver) override;
	/// <summary>
	/// Early initialiazation pass [is executed once all forms have been loaded
	/// </summary>
	/// <param name="resolver"></param>
	void InitializeEarly(LoadResolver* resolver) override;
	/// <summary>
	/// late initialization pass [is executed once all Initialize Early has been called for all forms]
	/// </summary>
	/// <param name="resolver"></param>
	void InitializeLate(LoadResolver* resolver) override;
	int32_t GetType() override {
		return FormType::DevTree;
	}
	static int32_t GetTypeStatic()
	{
		return FormType::DevTree;
	}
	void Delete(Data* data) override;
	bool CanDelete(Data* data) override;
	/// <summary>
	/// resets all internal variables
	/// </summary>
	void Clear() override;
	inline static bool _registeredFactories = false;
	static void RegisterFactories();

	void FreeMemory() override;
	/// <summary>
	/// returns whether the memmory of this form has been freed
	/// </summary>
	/// <returns></returns>
	bool Freed() override;
	size_t MemorySize() override;

	#pragma endregion
};
