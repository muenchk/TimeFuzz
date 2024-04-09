#pragma once

#include <memory>
#include <vector>

typedef EnumType uint64_t;

class GrammarNode
{
	enum NodeFlags : EnumType
	{
		None = 0,
		/// <summary>
		/// possibly expands into terminals
		/// </summary>
		ProduceTerminals = 0x1,
		/// <summary>
		/// possibly expands into non-terminals
		/// </summary>
		ProduceNonTerminals = 0x2,
		/// <summary>
		/// possibly expands into a sequence node
		/// </summary>
		ProduceSequence = 0x4,
	};

	enum class NodeType : EnumType
	{
		/// <summary>
		/// 
		/// </summary>
		Terminal = 0,
		NonTerminal = 0x1,
		Sequence = 0x2,
	};

	/// <summary>
	/// identifier of this node
	/// [non-terminal] The name of the non-terminal
	/// [terminal] the terminals to be produced
	/// [sequence] the begin of a new sequence entry
	/// </summary>
	std::string identifier;
	std::vector<std::shared_ptr<GrammarExpansion>> expansions;
	EnumType flags;
	NodeType type = NodeType::NonTerminal;

	bool IsTerminal();
	bool IsSequence();
};

class GrammarNode

class GrammarExpansion
{
	std::vector<std::shared_ptr<GrammarNode>> nodes;
};

class GrammarTree
{

};

class Grammar
{

};
