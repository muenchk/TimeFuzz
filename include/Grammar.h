#pragma once

#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <unordered_map>

#include "Utility.h"

class GrammarExpansion;

class GrammarObject
{
public:
	enum class Type
	{
		GrammarNode,
		GrammarExpansion,
	};

	virtual Type GetObjectType() = 0;
	virtual std::string Scala() = 0;
};

class GrammarNode : public GrammarObject
{
public:
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
	std::string derivation;
	/// <summary>
	/// global treeid associated with this object
	/// </summary>
	uint64_t id;
	std::vector<std::shared_ptr<GrammarExpansion>> expansions;
	EnumType flags;
	NodeType type = NodeType::NonTerminal;
	std::set<std::shared_ptr<GrammarExpansion>> parents;

	bool reachable = false;
	bool producing = false;

	/// <summary>
	/// used in pruning. When set to true, the node needs to be deleted
	/// </summary>
	bool remove = false;

	/// <summary>
	/// whether this node is a leaf
	/// </summary>
	/// <returns></returns>
	bool IsLeaf();
	/// <summary>
	/// whether this node is a sequence non-terminal
	/// </summary>
	/// <returns></returns>
	bool IsSequence();

	/// <summary>
	/// whether this node has been evaluated
	/// </summary>
	/// <returns></returns>
	bool IsValid();

	virtual Type GetObjectType();

	virtual std::string Scala();


	operator std::string();
	std::string string();
};

class GrammarExpansion : public GrammarObject
{
public:
	/// <summary>
	/// The nodes that are produced by this expansion
	/// </summary>
	std::vector<std::shared_ptr<GrammarNode>> nodes;
	/// <summary>
	/// the relative weight, that this expansion is chosen during input generation
	/// </summary>
	float weight;
	/// <summary>
	/// global treeid associated with this object
	/// </summary>
	uint64_t id;
	/// <summary>
	/// whether at least one subtree created by this expansion is producing
	/// </summary>
	bool producing = false;
	/// <summary>
	/// flags collected from nodes
	/// </summary>
	EnumType flags;
	/// <summary>
	/// used in pruning. When set to true the expansion needs to be deleted
	/// </summary>
	bool remove = false;

	/// <summary>
	/// parent of this expansion
	/// </summary>
	std::shared_ptr<GrammarNode> parent;

	virtual Type GetObjectType();

	virtual std::string Scala();

	operator std::string();
};

class GrammarTree
{
public:
	/// <summary>
	/// Checks the grammar for validity
	/// </summary>
	/// <returns></returns>
	bool IsValid();

	/// <summary>
	/// Constructs the grammar from the present nodes
	/// </summary>
	void Construct();

	GrammarTree();

	~GrammarTree();

	/// <summary>
	/// Sets the root symbol of the grammar
	/// </summary>
	/// <param name="symbol"></param>
	/// <param name="derivation"></param>
	void SetRoot(std::string symbol, std::string derivation);
	void SetRoot(std::shared_ptr<GrammarNode> node);
	/// <summary>
	/// Adds a new non-terminal to the grammar
	/// </summary>
	/// <param name="symbol"></param>
	/// <param name="derivation"></param>
	void AddSymbol(std::string symbol, std::string derivation);
	/// <summary>
	/// Adds a new sequence non-terminal to the grammar
	/// </summary>
	/// <param name="symbol"></param>
	/// <param name="derivation"></param>
	void AddSequenceSymbol(std::string symbol, std::string derivation);

	/// <summary>
	/// Returns the node with the corresponding identifier
	/// </summary>
	/// <param name="identifier"></param>
	/// <returns></returns>
	std::shared_ptr<GrammarNode> FindNode(std::string identifier);

	/// <summary>
	/// Returns a valid representation of the grammar in scala
	/// </summary>
	/// <returns></returns>
	std::string Scala();

private:
	std::set<std::shared_ptr<GrammarNode>> nonterminals;
	std::set<std::shared_ptr<GrammarNode>> terminals;
	std::unordered_map<uint64_t, std::shared_ptr<GrammarNode>> hashmap;
	std::unordered_map<uint64_t, std::shared_ptr<GrammarExpansion>> hashmap_expansions;
	std::vector<uint64_t> ruleorder;

	std::shared_ptr<GrammarNode> root;

	bool valid = false;

	int _numcycles = 0;
	uint64_t nextid = 0;

	uint64_t GetNextID();

	/// <summary>
	/// Finds cycles in the grammar
	/// </summary>
	void FindCycles();

	/// <summary>
	/// Returns the weight extracted from [production]
	/// </summary>
	/// <param name="production"></param>
	/// <returns>the weight specified by [production] if there is one</returns>
	float GetWeight(std::string production);

	/// <summary>
	/// Recursively gathers all node flags
	/// </summary>
	/// <param name="node"></param>
	void GatherFlags(std::shared_ptr<GrammarNode> node, std::set<uint64_t> path, bool reset = false);
	/// <summary>
	/// Recusively gathers all node flags
	/// </summary>
	/// <param name="expansion"></param>
	void GatherFlags(std::shared_ptr<GrammarExpansion> expansion, std::set<uint64_t> path);

	/// <summary>
	/// Prunes the grammar tree and removes all non-reachable and non-producing subtrees
	/// </summary>
	void Prune(bool pruneall = false);
};

class Grammar
{
public:
	/// <summary>
	/// Parses the grammar from the given file
	/// </summary>
	/// <param name=""></param>
	void ParseScala(std::filesystem::path path);

	/// <summary>
	/// Transforms the grammar into a valid scala representation
	/// </summary>
	/// <returns></returns>
	std::string Scala();

	/// <summary>
	/// Transforms the grammar into a valid python representation
	/// </summary>
	/// <returns></returns>
	std::string Python();

	bool IsValid();

private:
	std::shared_ptr<GrammarTree> tree;
};
