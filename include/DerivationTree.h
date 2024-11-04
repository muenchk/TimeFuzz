#pragma once

#include "Form.h"

#include <string>
#include <vector>
#include <list>
class DerivationTree : public Form
{

private:
	const int32_t classversion = 0x2;

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
	};
	
	struct NonTerminalNode : public Node
	{
		std::vector<Node*> children;
		uint64_t grammarID;

		inline NodeType Type() override { return NodeType::NonTerminal; }
	};

	struct TerminalNode : public Node
	{
		uint64_t grammarID;
		std::string content;

		inline NodeType Type() override { return NodeType::Terminal; }
	};

	struct SequenceNode : public NonTerminalNode
	{
		inline NodeType Type() override { return NodeType::Sequence; }
	};

	Node* root;
	FormID grammarID;
	bool valid = false;
	bool regenerate = false;
	uint32_t seed = 0;
	int32_t targetlen = 0;
	std::list<Node*> nodes;
	int32_t sequenceNodes = 0;

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

	#pragma endregion
};
