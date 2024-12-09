#pragma once

#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <random>

#include "Utility.h"
#include "TaskController.h"
#include "Form.h"
#include "DerivationTree.h"

class GrammarExpansion;
class Grammar;
class LoadResolverGrammar;
class DerivationTree;
class Input;

class GrammarObject
{
public:
	enum class Type
	{
		GrammarNode,
		GrammarExpansion,
		GrammarExpansionRegex
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
		/// <summary>
		/// Produces the empty word
		/// </summary>
		ProduceEmptyWord = 0x8,
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
	std::string _identifier;
	std::string _derivation;
	/// <summary>
	/// global treeid associated with this object
	/// </summary>
	uint64_t _id;
	std::vector<std::shared_ptr<GrammarExpansion>> _expansions;
	EnumType _flags;
	NodeType _type = NodeType::NonTerminal;
	std::set<std::shared_ptr<GrammarExpansion>> _parents;

	bool _reachable = false;
	bool _producing = false;

	/// <summary>
	/// used in pruning. When set to true, the node needs to be deleted
	/// </summary>
	bool _remove = false;

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
	/// whether this node is a non-terminal
	/// </summary>
	/// <returns></returns>
	bool IsNonTerminal();

	/// <summary>
	/// whether this node has been evaluated
	/// </summary>
	/// <returns></returns>
	bool IsValid();

	virtual Type GetObjectType();

	virtual std::string Scala();


	operator std::string();
	std::string string();

	/// <summary>
	/// returns the size of the node in bytes
	/// </summary>
	/// <returns></returns>
	size_t GetDynamicSize();
	/// <summary>
	/// Writes the node data to the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <returns></returns>
	bool WriteData(std::ostream* buffer, size_t& offset);
	const int32_t classversion = 0x1;
	/// <summary>
	/// Reads the node data from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <param name="length"></param>
	/// <param name="resolver"></param>
	/// <returns></returns>
	bool ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolverGrammar* resolver);
};

class GrammarExpansion : public GrammarObject
{
public:
	/// <summary>
	/// The nodes that are produced by this expansion
	/// </summary>
	std::vector<std::shared_ptr<GrammarNode>> _nodes;
	/// <summary>
	/// the relative weight, that this expansion is chosen during input generation
	/// </summary>
	float _weight;
	/// <summary>
	/// global treeid associated with this object
	/// </summary>
	uint64_t _id;
	/// <summary>
	/// whether at least one subtree created by this expansion is producing
	/// </summary>
	bool _producing = false;
	/// <summary>
	/// flags collected from nodes
	/// </summary>
	EnumType _flags;
	/// <summary>
	/// used in pruning. When set to true the expansion needs to be deleted
	/// </summary>
	bool _remove = false;

	/// <summary>
	/// number of non-terminals in the expansion
	/// </summary>
	int32_t _nonterminals = 0;
	/// <summary>
	/// number of sequence non-terminals in the expansion
	/// </summary>
	int32_t _seqnonterminals = 0;
	/// <summary>
	/// number of terminals in the expansion
	/// </summary>
	int32_t _terminals = 0;

	/// <summary>
	/// parent of this expansion
	/// </summary>
	std::shared_ptr<GrammarNode> _parent;

	virtual bool IsRegex() { return false; }

	virtual Type GetObjectType();

	virtual std::string Scala();

	virtual operator std::string();

	/// <summary>
	/// returns the size of the expansion in bytes
	/// </summary>
	/// <returns></returns>
	virtual size_t GetDynamicSize(int32_t version);

	virtual int32_t GetClassVersion() { return classversion; }
	/// <summary>
	/// Writes the expansion data to the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <returns></returns>
	virtual bool WriteData(std::ostream* buffer, size_t& offset);
	const int32_t classversion = 0x2;
	/// <summary>
	/// Reads the expansion data from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <param name="length"></param>
	/// <param name="resolver"></param>
	/// <returns></returns>
	virtual bool ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolverGrammar* resolver);
};

class GrammarExpansionRegex : public GrammarExpansion
{
	public :
	/// <summary>
	/// the node produced by this regex
	/// </summary>
	std::shared_ptr<GrammarNode> _node;
	/// <summary>
	/// min number of repetitions produced
	/// </summary>
	int32_t _min = 0;

	virtual bool IsRegex() override { return true; }

	virtual Type GetObjectType() override;

	virtual std::string Scala() override;

	virtual operator std::string() override;

	/// <summary>
	/// returns the size of the expansion in bytes
	/// </summary>
	/// <returns></returns>
	size_t GetDynamicSize(int32_t version) override;

	virtual int32_t GetClassVersion() { return classversion; }
	/// <summary>
	/// Writes the expansion data to the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <returns></returns>
	bool WriteData(std::ostream* buffer, size_t& offset) override;
	const int32_t classversion = 0x1;
	/// <summary>
	/// Reads the expansion data from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <param name="length"></param>
	/// <param name="resolver"></param>
	/// <returns></returns>
	bool ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolverGrammar* resolver) override;
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
	/// clears all internal variables
	/// </summary>
	void Clear();

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
	std::set<std::shared_ptr<GrammarNode>> _nonterminals;
	std::set<std::shared_ptr<GrammarNode>> _terminals;
	std::unordered_map<uint64_t, std::shared_ptr<GrammarNode>> _hashmap;
	std::unordered_map<uint64_t, std::shared_ptr<GrammarExpansion>> _hashmap_expansions;
	std::vector<uint64_t> _ruleorder;

	std::shared_ptr<GrammarNode> _root;

	std::set<uint64_t> _hashmap_parsenodes;

	bool _valid = false;

	/// <summary>
	/// whether the grammar represented by this tree is simple
	/// </summary>
	bool _simpleGrammar = false;

	int32_t _numcycles = 0;
	uint64_t _nextid = 0;

	uint64_t GetNextID();

	friend class Grammar;
	friend class LoadResolverGrammar;

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
	/// <param name="pruneall">prunes all nodes in the tree</param>
	void Prune(bool pruneall = false);

	/// <summary>
	/// copies the entire tree including all nodes and expansions by duplicating them
	/// </summary>
	/// <param name="other"></param>
	void DeepCopy(std::shared_ptr<GrammarTree> other);

	/// <summary>
	/// Inserts additional nodes used for parsing when considering sequence nodes as terminal children
	/// this is necessary for the earley parser to work correctly, then can afterwards be removed
	/// from the resulting parse tree
	/// </summary>
	void InsertParseNodes();

	/// <summary>
	/// Grammars are required to have a specfic form. This function assures that the start symbol (root) only
	/// derives into one other non-terminal symbol
	/// </summary>
	void FixRoot();

	/// <summary>
	/// Attempts to simplify the grammar, as long as it is unweighted
	/// </summary>
	void SimplifyRecursionHelper();

	/// <summary>
	/// Checks whether the rules producing sequence nodes can be simplified into a regular expression
	/// </summary>
	/// <returns></returns>
	bool CheckForSequenceSimplicityAndSimplify();

	size_t MemorySize();
};

class Grammar : public Form
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
	std::string Scala(bool parsetree = false);

	/// <summary>
	/// Transforms the grammar into a valid python representation
	/// </summary>
	/// <returns></returns>
	std::string Python();

	/// <summary>
	/// returns whether the grammar is valid
	/// </summary>
	/// <returns></returns>
	bool IsValid();

	/// <summary>
	/// returns whether the grammar is a simple grammar
	/// </summary>
	/// <returns></returns>
	bool IsSimple();

	~Grammar();

	#pragma region InheritedForm

	size_t GetStaticSize(int32_t version = 0x1) override;
	size_t GetDynamicSize() override;
	bool WriteData(std::ostream* buffer, size_t& offset) override;
	bool ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver) override;
	int32_t GetType() override
	{
		return FormType::Grammar;
	}
	static int32_t GetTypeStatic()
	{
		return FormType::Grammar;
	}
	void Delete(Data* data) override;
	void Clear() override;
	inline static bool _registeredFactories = false;
	static void RegisterFactories();
	size_t MemorySize() override;

	void SetGenerationParameters(int32_t extension_min, int32_t extension_max, int32_t backtrack_min, int32_t backtrack_max)
	{
		_extension_min = extension_min;
		_extension_max = extension_max;
		_backtrack_min = backtrack_min;
		_backtrack_max = backtrack_max;
	}

	void Derive(std::shared_ptr<DerivationTree> dtree, int32_t targetlength, uint32_t seed, int32_t maxsteps = 100000);

	void ExtractEarley(std::shared_ptr<DerivationTree> stree, std::shared_ptr<DerivationTree> dtree, int64_t begin, int64_t length, int64_t stop, bool complement);

	void Extract(std::shared_ptr<DerivationTree> stree, std::shared_ptr<DerivationTree> dtree, std::vector<std::pair<int64_t, int64_t>>& segments, int64_t stop, bool complement);

	void Extend(std::shared_ptr<Input> sinput, std::shared_ptr<DerivationTree> dtree, bool backtrack, int32_t targetlength, uint32_t seed, int32_t maxsteps = 100000);

	#pragma endregion

private:
	void DeriveFromNode(std::shared_ptr<DerivationTree> dtree, std::deque<std::pair<DerivationTree::NonTerminalNode*, std::shared_ptr<GrammarNode>>>& qnonterminals, std::deque<std::pair<DerivationTree::NonTerminalNode*, std::shared_ptr<GrammarNode>>>& qseqnonterminals, std::mt19937& randan, int32_t& seq);

	std::shared_ptr<GrammarTree> _tree;
	std::shared_ptr<GrammarTree> _treeParse;

	int32_t _extension_min = 0;
	int32_t _extension_max = 0;
	int32_t _backtrack_min = 0;
	int32_t _backtrack_max = 0;

	const int32_t classversion = 0x2;

	bool ReadData0x1(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver, LoadResolverGrammar* lresolve);
	bool ReadData0x2(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver, LoadResolverGrammar* lresolve);
};

class LoadResolverGrammar
{

public:
	using TaskFn = std::function<void()>;

	class TaskDelegate
	{
	public:
		virtual void Run() = 0;
		virtual void Dispose() = 0;
	};
	std::shared_ptr<GrammarTree> _tree;
	~LoadResolverGrammar();

	void AddTask(TaskFn a_task);
	void AddTask(TaskDelegate* a_task);

	std::shared_ptr<GrammarNode> ResolveNodeID(uint64_t id);
	std::shared_ptr<GrammarExpansion> ResolveExpansionID(uint64_t id);

	void Resolve();

private:
	std::queue<TaskDelegate*> _tasks;
	std::mutex _lock;

	class Task : public TaskDelegate
	{
	public:
		Task(TaskFn&& a_fn);

		void Run() override;
		void Dispose() override;

	private:
		TaskFn _fn;
	};
};
