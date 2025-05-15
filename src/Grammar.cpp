#include "Grammar.h"
#include "Logging.h"
#include "Utility.h"
#include "BufferOperations.h"
#include "DerivationTree.h"
#include "EarleyParser.h"
#include "Data.h"
#include "Input.h"
#include "Allocators.h"

#include <stack>
#include <random>

bool GrammarNode::IsLeaf()
{
	return _type == NodeType::Terminal;
}

bool GrammarNode::IsSequence()
{
	return _type == NodeType::Sequence;
}

bool GrammarNode::IsNonTerminal()
{
	return _type == NodeType::NonTerminal;
}

bool GrammarNode::IsValid()
{
	return _identifier.empty() == false && _derivation.empty() && (IsLeaf() && _expansions.size() == 0 || !IsLeaf() && _expansions.size() > 0);
}

GrammarNode::operator std::string()
{
	switch (_type)
	{
	case NodeType::Terminal:
		return "T_" + _identifier;
	case NodeType::NonTerminal:
		return "NT_" + _identifier;
	case NodeType::Sequence:
		return "SEQ_" + _identifier;
	}
	return "T_";
}

std::string GrammarNode::string()
{
	return this->operator std::string();
}

GrammarObject::Type GrammarNode::GetObjectType()
{
	return Type::GrammarNode;
}

std::string GrammarNode::Scala()
{
	std::string str = "\t" + _identifier + " := ";
	for (int32_t i = 0; i < _expansions.size(); i++)
	{
		if (i == _expansions.size() - 1)
			str += _expansions[i]->Scala();
		else
			str += _expansions[i]->Scala() + " | ";
	}
	str += ",";
	return str;
}

size_t GrammarNode::GetDynamicSize()
{
	size_t sz = 4                                       // version
	            + Buffer::CalcStringLength(_identifier)  // _identifier
	            + Buffer::CalcStringLength(_derivation)  // derivation
	            + 8                                     // id
	            + 8 + 8 * _expansions.size()             // list of expansions
	            + 8                                     // flags
	            + 4                                     // type
	            + 8 + 8 * _parents.size()                // list of parents
	            + 1                                     // reachable
	            + 1                                     // producing
	            + 1;                                    // remove
	return sz;
}

bool GrammarNode::WriteData(std::ostream* buffer, size_t& offset)
{
	Buffer::Write(classversion, buffer, offset);
	Buffer::Write(_identifier, buffer, offset);
	Buffer::Write(_derivation, buffer, offset);
	Buffer::Write(_id, buffer, offset);
	// expansions
	Buffer::WriteSize(_expansions.size(), buffer, offset);
	for (int32_t i = 0; i < _expansions.size(); i++)
		Buffer::Write(_expansions[i]->_id, buffer, offset);
	Buffer::Write(_flags, buffer, offset);
	Buffer::Write((uint32_t)_type, buffer, offset);
	// parents
	Buffer::WriteSize(_parents.size(), buffer, offset);
	for (auto expansion : _parents)
		Buffer::Write(expansion->_id, buffer, offset);
	Buffer::Write(_reachable, buffer, offset);
	Buffer::Write(_producing, buffer, offset);
	Buffer::Write(_remove, buffer, offset);
	return true;
}

bool GrammarNode::ReadData(std::istream* buffer, size_t& offset, size_t /*length*/, LoadResolverGrammar* resolver)
{
	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version)
	{
	case 0x1:
		{
			_identifier = Buffer::ReadString(buffer, offset);
			_derivation = Buffer::ReadString(buffer, offset);
			_id = Buffer::ReadUInt64(buffer, offset);
			// expansions
			size_t len = Buffer::ReadSize(buffer, offset);
			std::vector<uint64_t> exp;
			for (int32_t i = 0; i < len; i++) {
				exp.push_back(Buffer::ReadUInt64(buffer, offset));
			}
			resolver->AddTask([this, exp, resolver]() {
				for (int32_t i = 0; i < exp.size(); i++) {
					_expansions.push_back(resolver->ResolveExpansionID(exp[i]));
				}
			});

			_flags = Buffer::ReadUInt64(buffer, offset);
			_type = (GrammarNode::NodeType)(Buffer::ReadUInt32(buffer, offset));
			// parents
			size_t plen = Buffer::ReadSize(buffer, offset);
			std::vector<uint64_t> par;
			for (int32_t i = 0; i < plen; i++) {
				par.push_back(Buffer::ReadUInt64(buffer, offset));
			}
			resolver->AddTask([this, par, resolver]() {
				for (int32_t i = 0; i < par.size(); i++) {
					_parents.insert(resolver->ResolveExpansionID(par[i]));
				}
			});

			_reachable = Buffer::ReadBool(buffer, offset);
			_producing = Buffer::ReadBool(buffer, offset);
			_remove = Buffer::ReadBool(buffer, offset);
			return true;
		}
	default:
		return false;
	}
}

GrammarExpansion::operator std::string()
{
	std::string ret;
	for (int32_t i = 0; i < _nodes.size(); i++)
	{
		if (i == _nodes.size() - 1)
			ret += _nodes[i]->string();
		else
			ret += _nodes[i]->string() + " ~ ";
	}
	return ret;
}

GrammarObject::Type GrammarExpansion::GetObjectType()
{
	return Type::GrammarExpansion;
}

std::string GrammarExpansion::Scala()
{
	std::string ret;
	for (int32_t i = 0; i < _nodes.size(); i++) {
		switch (_nodes[i]->_type) {
		case GrammarNode::NodeType::Terminal:
			ret += "\"" + _nodes[i]->_identifier + "\"";
			break;
		case GrammarNode::NodeType::NonTerminal:
		case GrammarNode::NodeType::Sequence:
			ret += _nodes[i]->_identifier;
			break;
		}
		if (i != _nodes.size() - 1)
			ret += " ~ ";
	}
	return ret;
}

size_t GrammarExpansion::GetDynamicSize(int32_t version)
{
	size_t size0x1 = 4                        // version
	                 + 8 + 8 * _nodes.size()  // nodes
	                 + 4                      // weight
	                 + 8                      // id
	                 + 1                      // producing
	                 + 8                      // flags
	                 + 1                      // remove
	                 + 8;                     // parent
	size_t size0x2 = size0x1                 // prior version
	                 + 4                     // nonterminals
	                 + 4                     // seqnonterminals
	                 + 4;                    // terminals
	switch (version) {
	case 0x1:
		return size0x1;
	case 0x2:
		return size0x2;
	default:
		return 0;
	}
}

bool GrammarExpansion::WriteData(std::ostream* buffer, size_t& offset)
{
	Buffer::Write(classversion, buffer, offset);
	Buffer::WriteSize(_nodes.size(), buffer, offset);
	for (int32_t i = 0; i < _nodes.size(); i++)
		Buffer::Write(_nodes[i]->_id, buffer, offset);
	Buffer::Write(_weight, buffer, offset);
	Buffer::Write(_id, buffer, offset);
	Buffer::Write(_producing, buffer, offset);
	Buffer::Write(_flags, buffer, offset);
	Buffer::Write(_remove, buffer, offset);
	Buffer::Write(_parent->_id, buffer, offset);
	Buffer::Write(_nonterminals, buffer, offset);
	Buffer::Write(_seqnonterminals, buffer, offset);
	Buffer::Write(_terminals, buffer, offset);
	return true;
}

bool GrammarExpansion::ReadData(std::istream* buffer, size_t& offset, size_t /*length*/, LoadResolverGrammar* resolver)
{
	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version) {
	case 0x1:
		{
			size_t len = Buffer::ReadSize(buffer, offset);
			std::vector<uint64_t> nds;
			for (int i = 0; i < len; i++)
				nds.push_back(Buffer::ReadUInt64(buffer, offset));
			resolver->AddTask([this, resolver, nds]() {
				for (int32_t i = 0; i < nds.size(); i++)
					this->_nodes.push_back(resolver->ResolveNodeID(nds[i]));
			});
			_weight = Buffer::ReadFloat(buffer, offset);
			_id = Buffer::ReadUInt64(buffer, offset);
			_producing = Buffer::ReadBool(buffer, offset);
			_flags = Buffer::ReadUInt64(buffer, offset);
			_remove = Buffer::ReadBool(buffer, offset);
			uint64_t pid = Buffer::ReadUInt64(buffer, offset);
			resolver->AddTask([this, resolver, pid]() {
				this->_parent = resolver->ResolveNodeID(pid);
			});
			return true;
		}
	case 0x2:
		{
			size_t len = Buffer::ReadSize(buffer, offset);
			std::vector<uint64_t> nds;
			for (int i = 0; i < len; i++)
				nds.push_back(Buffer::ReadUInt64(buffer, offset));
			resolver->AddTask([this, resolver, nds]() {
				for (int32_t i = 0; i < nds.size(); i++)
					this->_nodes.push_back(resolver->ResolveNodeID(nds[i]));
			});
			_weight = Buffer::ReadFloat(buffer, offset);
			_id = Buffer::ReadUInt64(buffer, offset);
			_producing = Buffer::ReadBool(buffer, offset);
			_flags = Buffer::ReadUInt64(buffer, offset);
			_remove = Buffer::ReadBool(buffer, offset);
			uint64_t pid = Buffer::ReadUInt64(buffer, offset);
			resolver->AddTask([this, resolver, pid]() {
				this->_parent = resolver->ResolveNodeID(pid);
			});
			_nonterminals = Buffer::ReadInt32(buffer, offset);
			_seqnonterminals = Buffer::ReadInt32(buffer, offset);
			_terminals = Buffer::ReadInt32(buffer, offset);
			return true;
		}
	default:
		return false;
	}
}

GrammarObject::Type GrammarExpansionRegex::GetObjectType()
{
	return Type::GrammarExpansionRegex;
}

GrammarExpansionRegex::operator std::string()
{
	std::string ret;
	if (_node) {
		if (_min == 0) {
			return "(" + _node->string() + ")" + "*";
		} else {
			return "(" + _node->string() + ")" + "+";
		}
	}
	return ret;
}

std::string GrammarExpansionRegex::Scala()
{
	std::string ret;
	if (_node){
		switch (_node->_type) {
		case GrammarNode::NodeType::Terminal:
			ret += "\"" + _node->_identifier + "\"";
			break;
		case GrammarNode::NodeType::NonTerminal:
		case GrammarNode::NodeType::Sequence:
			ret += _node->_identifier;
			break;
		}
	}
	if (_min == 0)
		ret += "*";
	else
		ret += "+";
	return ret;
}

size_t GrammarExpansionRegex::GetDynamicSize(int32_t version)
{
	size_t size0x1 = 4     // version
	                 + 8   // node
	                 + 4   // min
	                 + 8   // id
	                 + 1   // producing
	                 + 8   // flags
	                 + 1   // remove
	                 + 8   // parent
	                 + 4   // nonterminals
	                 + 4   // seqnonterminals
	                 + 4;  // terminals
	switch (version) {
	case 0x1:
		return size0x1;
	default:
		return 0;
	}
}

bool GrammarExpansionRegex::WriteData(std::ostream* buffer, size_t& offset)
{
	Buffer::Write(classversion, buffer, offset);
	Buffer::Write(_node->_id, buffer, offset);
	Buffer::Write(_min, buffer, offset);
	Buffer::Write(_id, buffer, offset);
	Buffer::Write(_producing, buffer, offset);
	Buffer::Write(_flags, buffer, offset);
	Buffer::Write(_remove, buffer, offset);
	Buffer::Write(_parent->_id, buffer, offset);
	Buffer::Write(_nonterminals, buffer, offset);
	Buffer::Write(_seqnonterminals, buffer, offset);
	Buffer::Write(_terminals, buffer, offset);
	return true;
}

bool GrammarExpansionRegex::ReadData(std::istream* buffer, size_t& offset, size_t /*length*/, LoadResolverGrammar* resolver)
{
	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version) {
	case 0x1:
		{
			uint64_t id = Buffer::ReadUInt64(buffer, offset);
			resolver->AddTask([this, resolver, id]() {
				_node = resolver->ResolveNodeID(id);
			});
			_min = Buffer::ReadInt32(buffer, offset);
			_id = Buffer::ReadUInt64(buffer, offset);
			_producing = Buffer::ReadBool(buffer, offset);
			_flags = Buffer::ReadUInt64(buffer, offset);
			_remove = Buffer::ReadBool(buffer, offset);
			uint64_t pid = Buffer::ReadUInt64(buffer, offset);
			resolver->AddTask([this, resolver, pid]() {
				this->_parent = resolver->ResolveNodeID(pid);
			});
			_nonterminals = Buffer::ReadInt32(buffer, offset);
			_seqnonterminals = Buffer::ReadInt32(buffer, offset);
			_terminals = Buffer::ReadInt32(buffer, offset);
			return true;
		}
	default:
		return false;
	}
}


GrammarTree::GrammarTree()
{

}

GrammarTree::~GrammarTree()
{
	Clear();
}

void GrammarTree::Clear()
{
	// why not wait for automatic destruction?
	//    because there may be multiple references (even though there shouldn't) and we want to destroy all pointers as soon as we can
	//	  to avoid lingering shared objects
	//	  this shouldn't happen to this class, but for consistency within the project its here
	_valid = false;
	_nonterminals.clear();
	_terminals.clear();
	for (auto [key, node] : _hashmap) {
		if (node) {
			node->_parents.clear();
			node->_expansions.clear();
		}
	}
	_hashmap.clear();
	for (auto [key, expansion] : _hashmap_expansions) {
		if (expansion) {
			expansion->_parent.reset();
			expansion->_nodes.clear();
		}
		if (expansion->IsRegex())
			dynamic_pointer_cast<GrammarExpansionRegex>(expansion)->_node.reset();
	}
	_hashmap_expansions.clear();
	_root.reset();
}

bool GrammarTree::IsValid()
{
	return _valid;
}

void GrammarTree::SetRoot(std::string symbol, std::string derivation)
{
	// erase old root if there was one
	_nonterminals.erase(_root);
	_root.reset();

	// remove whitespaces from symbol and identifiers, since they are superfluous
	Utility::RemoveWhiteSpaces(symbol, '\"', true);
	Utility::RemoveWhiteSpaces(derivation, '\"', true);

	// create new root node
	_root = std::make_shared<GrammarNode>();
	_root->_derivation = derivation;
	_root->_identifier = symbol;
	_root->_type = GrammarNode::NodeType::Terminal;
	_root->_id = GetNextID();
	_nonterminals.insert(_root);
	_ruleorder.push_back(_root->_id);
}

void GrammarTree::SetRoot(std::shared_ptr<GrammarNode> node)
{
	_root.reset();
	_root = node;
}

void GrammarTree::AddSymbol(std::string symbol, std::string derivation)
{
	logdebug("Adding new symbol: {} -> {}", symbol, derivation);
	// remove whitespaces from symbol and identifiers, since they are superfluous
	Utility::RemoveWhiteSpaces(symbol, '\"', true);
	Utility::RemoveWhiteSpaces(derivation, '\"', true);

	// create node
	std::shared_ptr<GrammarNode> node = std::make_shared<GrammarNode>();
	node->_derivation = derivation;
	node->_identifier = symbol;
	node->_type = GrammarNode::NodeType::NonTerminal;
	node->_id = GetNextID();
	_nonterminals.insert(node);
	_ruleorder.push_back(node->_id);
}

void GrammarTree::AddSequenceSymbol(std::string symbol, std::string derivation)
{
	logdebug("Adding new seqsymbol: {} -> {}", symbol, derivation);
	// remove whitespaces from symbol and identifiers, since they are superfluous
	Utility::RemoveWhiteSpaces(symbol, '\"', true);
	Utility::RemoveWhiteSpaces(derivation, '\"', true);

	// create node
	std::shared_ptr<GrammarNode> node = std::make_shared<GrammarNode>();
	node->_derivation = derivation;
	node->_identifier = symbol;
	node->_type = GrammarNode::NodeType::Sequence;
	node->_id = GetNextID();
	_nonterminals.insert(node);
	_ruleorder.push_back(node->_id);
}

std::shared_ptr<GrammarNode> GrammarTree::FindNode(std::string identifier)
{
	std::string symbols;
	for (auto& node : _nonterminals) {
		symbols += "|" + node->_identifier;
		if (node->_identifier == identifier) {
			return node;
		}
	}
	symbols += "|";
	logdebug("Nonterminals: {}, Symbols found: {}", _nonterminals.size(), symbols);
	return {};
}

void GrammarTree::Construct()
{
	loginfo("enter");
	// This function actually contructs the grammar tree itself.
	// before this is called, the root symbol and (optionally) a
	// number of other non-terminal nodes have been added.
	// Now all those nodes are processed, and the derivations, given in string form,
	// are replaced by the actual GrammarNodes that they represent.
	// Non-Terminals will be taken from [nonterminals] and terminals are
	// going to be constructed as new objects.
	// Most importantly, this function needs all used non-terminals to be present
	// if this is not the case the function will fail.

	// checks if a derivation is "empty", thus it produces the empty word
	auto isEmpty = [](std::string derive) {
		if (derive == "")
			return true;
		// if the only symbols are whitespace or tab this might be the empty word
		auto symbols = Utility::GetSymbols(derive);
		for (auto [c, _] : symbols)
			if (c != ' ' && c != '\t')
				return false;
		return true;
	};

	// iterate over all non-terminals and initialize them
	for (auto node : _nonterminals) {
		if (node->IsValid() == false) {
			_hashmap.insert({ node->_id, node });
			// extract the alternative derivations
			std::vector<std::string> alternatives = Utility::SplitString(node->_derivation, '|', false, true, '\"');
			for (std::string alter : alternatives) {
				// go over the derivations and get all concated productions
				std::vector<std::string> productions = Utility::SplitString(alter, '~', true, true, '\"', true);
				std::shared_ptr<GrammarExpansion> expansion = std::make_shared<GrammarExpansion>();
				float weight = 0.0f;
				for (int32_t i = 0; i < productions.size(); i++) {
					// if the rule is a weight, get the weight
					if (float wgt = GetWeight(productions[i]); wgt != -1.0f)
						weight = wgt;
					// check for non-terminal symbol
					else if (size_t pos = productions[i].find("\'"); pos != std::string::npos && Utility::IsEscaped(productions[i], (int32_t)pos, '\"') == false) {
						auto newnode = FindNode(productions[i]);
						if (newnode) {
							expansion->_nodes.push_back(newnode);
							newnode->_parents.insert(expansion);
						} else
							logcritical("Cannot find unknown Symbol: {}", productions[i]);
					}
					else if (isEmpty(productions[i]))
					{
						// do nothing, this may result in an empty rule
					}
					// if the rule is a terminal get the terminal and create a node
					else {
						std::string iden = productions[i];
						if (iden.substr(0, 2).find("\\x") != std::string::npos)
						{
							int ascii = 0;
							try {
								ascii = std::stoi(iden.substr(2, iden.size()), nullptr, 16);
								iden = std::string(1, char(ascii));
							}
							catch (std::exception& )
							{
								Utility::RemoveSymbols(iden, '\"', '\\');
							}
						} else {
							Utility::RemoveSymbols(iden, '\"', '\\');
						}
						auto tnode = std::make_shared<GrammarNode>();
						tnode->_derivation = "";
						tnode->_identifier = iden;
						tnode->_type = GrammarNode::NodeType::Terminal;
						tnode->_flags = GrammarNode::NodeFlags::ProduceTerminals;
						tnode->_id = GetNextID();
						tnode->_producing = true;
						tnode->_parents.insert(expansion);
						_terminals.insert(tnode);
						expansion->_nodes.push_back(tnode);
						_hashmap.insert({ tnode->_id, tnode });
					}
				}
				expansion->_weight = weight;
				expansion->_id = GetNextID();
				expansion->_parent = node;
				node->_expansions.push_back(expansion);
				_hashmap_expansions.insert({ expansion->_id, expansion });
			}
			node->_derivation = "";
			if (node->IsValid() == false)
				logcritical("Cannot fully initialize grammar node: {}. Expansions: {}, Flags: {}, Type: {}", node->_identifier, node->_expansions.size(), Utility::GetHex(node->_flags), Utility::GetHex((EnumType)node->_type));
		}
	}

	// gather all flags for all tree nodes
	GatherFlags(_root, {}, true);

	// prune tree to valid expansion and nodes
	Prune();

	// check whether there still is a grammar that we can work with
	if (_root) {
		_valid = true;
		loginfo("Successfully pruned the grammar");
	} else {
		_valid = false;
		logcritical("Grammar is not valid");
	}

	loginfo("exit");
}


uint64_t GrammarTree::GetNextID()
{
	return ++_nextid;
}

float GrammarTree::GetWeight(std::string production)
{
	// the weight is specified insight a special variable of form 'WGT_0.5
	if (size_t pos = production.find("\'WGT_"); pos != std::string::npos) {
		std::string wgt = production.substr(pos + 5, production.length() - (pos + 5));
		try {
			return std::stof(wgt);
		} catch (std::exception&) {
			logwarn("Cannot extract weight form symbol: {}", production);
		}
	}
	return -1.0f;
}

// list of nodes that have been fully explored
std::set<uint64_t> finished_ids;

void GrammarTree::GatherFlags(std::shared_ptr<GrammarNode> node, std::set<uint64_t> path, bool reset)
{
	// define terminal patterns
	static std::regex class_pattern("[:.*:]");
	static std::regex class_pattern_ascii("[:ascii:]");
	static std::regex class_pattern_alpha("[:alpha:]");
	static std::regex class_pattern_alnum("[:alnum:]");
	static std::regex class_pattern_digit("[:digit:]");

	// if we are starting of from a new non-recursive function call, delete prior finished ids
	StartProfiling;
	if (reset) {
		loginfo("enter");
		finished_ids.clear();
		for (auto [id, nd] : _hashmap) {
			nd->_reachable = false;
			nd->_producing = false;
			nd->_remove = false;
			nd->_flags = 0;
		}
		for (auto [id, exp] : _hashmap_expansions)
		{
			exp->_flags = 0;
			exp->_producing = false;
			exp->_remove = false;
		}
		TimeProfiling = std::chrono::steady_clock::now();
	}
	// this node is reachable
	node->_reachable = true;
	// add node to path
	path.insert(node->_id);
	// if this is a terminal, it was reached and it is producing
	if (node->IsLeaf()) {
		node->_producing = true;
		finished_ids.insert(node->_id);
		if (std::regex_search(node->_identifier, class_pattern)) {
			node->_flags |= GrammarNode::NodeFlags::TerminalCharClass;
			if (std::regex_search(node->_identifier, class_pattern_ascii)) {
				node->_flags |= GrammarNode::NodeFlags::TerminalCharClassAscii;
			} else if (std::regex_search(node->_identifier, class_pattern_alpha)) {
				node->_flags |= GrammarNode::NodeFlags::TerminalCharClassAlpha;
			} else if (std::regex_search(node->_identifier, class_pattern_alnum)) {
				node->_flags |= GrammarNode::NodeFlags::TerminalCharClassAlphaNumeric;
			} else if (std::regex_search(node->_identifier, class_pattern_digit)) {
				node->_flags |= GrammarNode::NodeFlags::TerminalCharClassDigit;
			}
		}
		return;
	}
	std::set<std::shared_ptr<GrammarExpansion>> skip;
	// otherwise this is a
	for (auto expansion : node->_expansions) {
		if (finished_ids.contains(expansion->_id) == false) {
			if (path.contains(expansion->_id)) {
				// if the expansion is not finished, but it is already on our path
				// we have encountered a cycle. This cycle can be resolved quite easily
				// by knowing that an expansion is only producing if all of its nodes are
				// producing. If its our only expansion than we cannot be producing ourselves
				// so just skip this expansion when collecting information later
				skip.insert(expansion);
			} else
				GatherFlags(expansion, path);
		}
	}
	// collect information from expansions;
	// the node is producing if at least one of its expansions is producing
	node->_producing = false;
	node->_flags = [&node]() {
		switch (node->_type) {
		case GrammarNode::NodeType::NonTerminal:
			return GrammarNode::NodeFlags::ProduceNonTerminals;
			break;
		case GrammarNode::NodeType::Sequence:
			return GrammarNode::NodeFlags::ProduceSequence;
			break;
		case GrammarNode::NodeType::Terminal:
			return GrammarNode::NodeFlags::ProduceTerminals;
			break;
		}
		return GrammarNode::NodeFlags::ProduceTerminals;
	}();
	for (int32_t i = 0; i < (int)(node->_expansions.size()); i++) {
		if (skip.contains(node->_expansions[i]))  // skip cycling expansions
			continue;
		node->_producing |= node->_expansions[i]->_producing;
		node->_flags |= node->_expansions[i]->_flags;
	}
	finished_ids.insert(node->_id);

	if (reset)
	{
		loginfo("exit");
		profile(TimeProfiling, "function execution time");
	}
}

void GrammarTree::GatherFlags(std::shared_ptr<GrammarExpansion> expansion, std::set<uint64_t> path)
{
	// add expansion to path
	path.insert(expansion->_id);
	// the expansion is only producing if and only if ALL its nodes are also producing
	expansion->_producing = true;
	for (int32_t i = 0; i < expansion->_nodes.size(); i++) {
		if (finished_ids.contains(expansion->_nodes[i]->_id) == false) {
			GatherFlags(expansion->_nodes[i], path);
		}
		expansion->_producing &= expansion->_nodes[i]->_producing;
		expansion->_flags |= expansion->_nodes[i]->_flags;
		switch (expansion->_nodes[i]->_type)
		{
		case GrammarNode::NodeType::NonTerminal:
			expansion->_flags |= GrammarNode::NodeFlags::ProduceNonTerminals;
			expansion->_nonterminals++;
			break;
		case GrammarNode::NodeType::Sequence:
			expansion->_flags |= GrammarNode::NodeFlags::ProduceSequence;
			break;
		case GrammarNode::NodeType::Terminal:
			expansion->_flags |= GrammarNode::NodeFlags::ProduceTerminals;
			expansion->_terminals++;
			break;
		}
		if (expansion->_nodes[i]->_flags & GrammarNode::NodeFlags::ProduceSequence)
			expansion->_seqnonterminals++;
	}
	if (expansion->IsRegex()) {
		auto regex = dynamic_pointer_cast<GrammarExpansionRegex>(expansion);
		if (finished_ids.contains(regex->_node->_id) == false)
			GatherFlags(regex->_node, path);
		expansion->_producing &= regex->_node->_producing;
		expansion->_flags |= regex->_node->_flags;
		switch (regex->_node->_type) {
		case GrammarNode::NodeType::NonTerminal:
			expansion->_flags |= GrammarNode::NodeFlags::ProduceNonTerminals;
			expansion->_nonterminals++;
			break;
		case GrammarNode::NodeType::Sequence:
			expansion->_flags |= GrammarNode::NodeFlags::ProduceSequence;
			break;
		case GrammarNode::NodeType::Terminal:
			expansion->_flags |= GrammarNode::NodeFlags::ProduceTerminals;
			expansion->_terminals++;
			break;
		}
		if (regex->_node->_flags & GrammarNode::NodeFlags::ProduceSequence)
			expansion->_seqnonterminals++;
	}
	if (expansion->IsRegex() == false && expansion->_nodes.size() == 0)
		expansion->_flags |= GrammarNode::NodeFlags::ProduceEmptyWord;
	if (expansion->IsRegex() && dynamic_pointer_cast<GrammarExpansionRegex>(expansion)->_min == 0)
		expansion->_flags |= GrammarNode::NodeFlags::ProduceEmptyWord;
	finished_ids.insert(expansion->_id);
}

void GrammarTree::Prune(bool pruneall)
{
	StartProfiling;
	loginfo("enter");
	std::set<uint64_t> toremove;
	// we can use these shenanigans, since all nodes and expansions are present in hashmap and hashmap_expansions, so they won't be deleted
	std::set<uint64_t> touched;
	std::stack<GrammarObject*> stack;
	// begin with root
	stack.push(_root.get());

	// now add all non-reachable node into the stack
	for (auto node : _nonterminals)
		if (node->_reachable == false)
			stack.push(node.get());
	for (auto node : _terminals)
		if (node->_reachable == false)
			stack.push(node.get());
	// iterate over expansions and add orpahned to the stack
	for (auto [_, exp] : _hashmap_expansions)
		if (!exp->_parent)
			stack.push(exp.get());

	while (stack.size() > 0) {
		auto ptr = stack.top();
		stack.pop();
		if (ptr->GetObjectType() == GrammarObject::Type::GrammarNode) {
			GrammarNode* node = (GrammarNode*)ptr;
			if (node->_producing == false || node->_remove || node->_reachable == false || pruneall == true) {
				for (auto parent : node->_parents) {
					parent->_remove = true;
					stack.push(parent.get());
				}
				node->_parents.clear();
				node->_remove = true;
				toremove.insert(node->_id);
				for (auto expansion : node->_expansions) {
					expansion->_remove = true;
					stack.push(expansion.get());
				}
				node->_expansions.clear();
				if (node->_producing == false) {
					logwarn("Node is not producing and has been removed: {}", node->string());
				} else if (node->_reachable == false) {
					logwarn("Node is not reachable and has been removed: {}", node->string());
				} else
					logwarn("Node has been marked for removal: {}", node->string());
			} else {
				// enter all nodes into touched, so that we do not run into cycles
				for (auto exp : node->_expansions) {
					if (touched.contains(exp->_id) == false) {
						stack.push(exp.get());
						touched.insert(exp->_id);
						logdebug("Enter expansion to stack: {}", exp->operator std::string());
					}
				}
			}
		} else if (ptr->GetObjectType() == GrammarObject::Type::GrammarExpansion ||
				   ptr->GetObjectType() == GrammarObject::Type::GrammarExpansionRegex) {
			GrammarExpansion* exp = (GrammarExpansion*)ptr;

			auto erasefromparent = [exp]() {
				if (exp->_parent) {
					auto itr = exp->_parent->_expansions.begin();
					while (itr != exp->_parent->_expansions.end()) {
						if ((*itr).get() == exp) {
							exp->_parent->_expansions.erase(itr);
							break;
						}
						itr++;
					}
				}
			};
			auto eraseasparent = [exp](std::shared_ptr<GrammarNode> node) {
				if (node) {
					auto itr = node->_parents.begin();
					while (itr != node->_parents.end()) {
						if ((*itr).get() == exp) {
							node->_parents.erase(itr);
							break;
						}
						itr++;
					}
				}
			};

			if (exp->_remove || exp->_producing == false || pruneall == true) {
				toremove.insert(exp->_id);
				// we have been set to be removed
				erasefromparent();
				for (auto node : exp->_nodes) {
					// if node is already flagged remove, its either in the stack or already processed
					if (node && node->_remove == false) {
						// remove expansion are parent from the node
						eraseasparent(node);
						// if node doesn't have any more parent, remove it
						if (node->_parents.size() == 0 && node->_id != _root->_id) {
							node->_remove = true;
							stack.push(node.get());
						}
					}
				}
				if (exp->IsRegex()) {
					auto node = dynamic_cast<GrammarExpansionRegex*>(exp)->_node;
					if (node && node->_remove == false) {
						eraseasparent(node);
						if (node->_parents.size() == 0 && node->_id != _root->_id) {
							node->_remove = true;
							stack.push(node.get());
						}
					}
				}
				if (exp->_producing == false) {
					logwarn("Expansion is not producing and has been removed: {}", exp->operator std::string());
				} else if (exp->_remove) {
					logwarn("Expansion has been marked for removal: {}", exp->operator std::string());
				}
			} else {
				// enter all nodes onto stack
				for (auto node : exp->_nodes) {
					if (touched.contains(node->_id) == false) {
						stack.push(node.get());
						logdebug("Enter node to stack: {}", node->operator std::string());
					}
				}
				if (exp->IsRegex())
					if (auto regex = dynamic_cast<GrammarExpansionRegex*>(exp); regex->_node) {
						stack.push(regex->_node.get());
						logdebug("Enter node to stack: {}", regex->_node->operator std::string());
					}
			}
		}
	}

	// now that the tree itself has been pruned, remove all nodes and expansions to be removed
	auto itr = _hashmap.begin();
	while (itr != _hashmap.end()) {
		auto [id, node] = *itr;
		if (toremove.contains(id)) {
			itr = _hashmap.erase(itr);
			_terminals.erase(node);
			_nonterminals.erase(node);
			if (node.use_count() != 1)
				logcritical("MEMORY LEAK: Use_Count of Node {} is not 1", node->string());
		} else
			itr++;
	}
	auto expitr = _hashmap_expansions.begin();
	while (expitr != _hashmap_expansions.end()) {
		auto [id, exp] = *expitr;
		if (toremove.contains(id)) {
			expitr = _hashmap_expansions.erase(expitr);
			if (exp.use_count() != 1)
				logcritical("MEMORY LEAK: Use_Count of Expansion {} is not 1", exp->operator std::string());
		} else
			expitr++;
	}

	loginfo("exit");
	profile(TimeProfiling, "function execution time");
}

std::string GrammarTree::Scala()
{
	std::string str = "Grammar(\n";
	for (auto id : _ruleorder)
	{
		if (auto itr = _hashmap.find(id); itr != _hashmap.end())
		{
			str += itr->second->Scala() + "\n";
		}
	}
	str += ")\n";
	return str;
}

void GrammarTree::DeepCopy(std::shared_ptr<GrammarTree> other)
{
	// this function is structured into multiple sections
	// ----- lambda copy functions -----
	auto copyGrammarNode = [](std::shared_ptr<GrammarNode> node) {
		auto newnode = std::make_shared<GrammarNode>();
		newnode->_identifier = node->_identifier;
		newnode->_derivation = node->_derivation;
		newnode->_id = node->_id;
		newnode->_expansions = node->_expansions;
		newnode->_flags = node->_flags;
		newnode->_type = node->_type;
		newnode->_parents = node->_parents;
		newnode->_reachable = node->_reachable;
		newnode->_producing = node->_producing;
		newnode->_remove = node->_remove;
		return newnode;
	};
	auto copyGrammarExpansion = [](std::shared_ptr<GrammarExpansion> expansion) {
		std::shared_ptr<GrammarExpansion> newexpansion;
		if (expansion->IsRegex())
		{
			auto regex = std::make_shared<GrammarExpansionRegex>();
			newexpansion = regex;
			regex->_min = dynamic_pointer_cast<GrammarExpansionRegex>(expansion)->_min;
			regex->_node = dynamic_pointer_cast<GrammarExpansionRegex>(expansion)->_node;
		} else {
			newexpansion = std::make_shared<GrammarExpansion>();
		}
		newexpansion->_nodes = expansion->_nodes;
		newexpansion->_weight = expansion->_weight;
		newexpansion->_id = expansion->_id;
		newexpansion->_producing = expansion->_producing;
		newexpansion->_flags = expansion->_flags;
		newexpansion->_remove = expansion->_remove;
		newexpansion->_nonterminals = expansion->_nonterminals;
		newexpansion->_seqnonterminals = expansion->_seqnonterminals;
		newexpansion->_terminals = expansion->_terminals;
		newexpansion->_parent = expansion->_parent;
		return newexpansion;
	};

	// ----- Copy all objects in the tree to the new tree -----
	other->_nextid = _nextid;
	other->_numcycles = _numcycles;
	other->_valid = _valid;
	// not available in linux (barf)
	//other->_ruleorder.append_range(_ruleorder);
	other->_ruleorder.resize(_ruleorder.size());
	for (size_t i = 0; i < _ruleorder.size(); i++)
		other->_ruleorder[i] = _ruleorder[i];

	// copy hashmap
	for (auto [id, node] : _hashmap)
		other->_hashmap.insert_or_assign(id, copyGrammarNode(node));
	for (auto [id, expansion] : _hashmap_expansions)
		other->_hashmap_expansions.insert_or_assign(id, copyGrammarExpansion(expansion));
	// apply new data from hashmap to root and sets
	other->_root = other->_hashmap.at(_root->_id);
	for (auto node : _nonterminals)
		other->_nonterminals.insert(other->_hashmap.at(node->_id));
	for (auto tnode : _terminals)
		other->_terminals.insert(other->_hashmap.at(tnode->_id));

	// ----- Change all the references in the copied objects
	// to references from the new tree -----
	for (auto [id, node] : other->_hashmap)
	{
		std::set<std::shared_ptr<GrammarExpansion>> parents;
		std::vector<std::shared_ptr<GrammarExpansion>> expansions;
		for (auto pnode : node->_parents)
			parents.insert(other->_hashmap_expansions.at(pnode->_id));
		for (auto cnode : node->_expansions)
			expansions.push_back(other->_hashmap_expansions.at(cnode->_id));
		node->_parents.clear();
		// not available in linux (barf)
		//node->_parents.insert_range(parents);
		for (auto x : parents)
			node->_parents.insert(x);
		node->_expansions.clear();
		// not available in linux (barf)
		//node->_expansions.append_range(expansions);
		for (auto x : expansions)
			node->_expansions.push_back(x);
	}

	for (auto [id, node] : other->_hashmap_expansions)
	{
		std::vector<std::shared_ptr<GrammarNode>> nodes;
		node->_parent = other->_hashmap.at(node->_parent->_id);
		for (auto cnode : node->_nodes)
			nodes.push_back(other->_hashmap.at(cnode->_id));
		node->_nodes.clear();
		// not available in linux (barf)
		//node->_nodes.append_range(nodes);
		for (auto x : nodes)
			node->_nodes.push_back(x);
		if (node->IsRegex())
		{
			dynamic_pointer_cast<GrammarExpansionRegex>(node)->_node = other->_hashmap.at(dynamic_pointer_cast<GrammarExpansionRegex>(node)->_node->_id);
		}
	}

	// all references have been resolved so we are done
}

void GrammarTree::InsertParseNodes()
{
	// simple grammars cannot work with EarlyParser, so no need
	if (_simpleGrammar)
		return;
	// this function will insert a new production PN -> S for every
	// sequence node S and change all productions P -> S to P -> PN

	// gather sequence nodes
	std::set<std::shared_ptr<GrammarNode>> seqnodes;
	for (auto node : _nonterminals)
		if (node->IsSequence())
			seqnodes.insert(node);
	// create new rules for all seqnodes
	for (auto node : seqnodes)
	{
		// create new node
		auto nnode = std::make_shared<GrammarNode>();
		nnode->_derivation = "parse";
		nnode->_type = GrammarNode::NodeType::NonTerminal;
		nnode->_id = GetNextID();
		nnode->_identifier = std::string("'ParseNode") + std::to_string(nnode->_id);
		_nonterminals.insert(nnode);
		_hashmap.insert_or_assign(nnode->_id, nnode);
		_hashmap_parsenodes.insert(nnode->_id);
		_ruleorder.push_back(nnode->_id);
		for (auto expansion : node->_parents)
		{
			nnode->_parents.insert(expansion);
			if (expansion->IsRegex()) {
				if (dynamic_pointer_cast<GrammarExpansionRegex>(expansion)->_node->_id == node->_id)
					dynamic_pointer_cast<GrammarExpansionRegex>(expansion)->_node = nnode;
			} else {
				for (int64_t i = 0; i < (int64_t)expansion->_nodes.size(); i++) {
					if (expansion->_nodes[i]->_id == node->_id)
						expansion->_nodes[i] = nnode;
				}
			}
		}
		// create new expansion
		auto nexp = std::make_shared<GrammarExpansion>();
		nexp->_weight = 0;
		nexp->_nonterminals++;
		nexp->_parent = nnode;
		nexp->_nodes.push_back(node);
		nexp->_id = GetNextID();
		_hashmap_expansions.insert_or_assign(nexp->_id, nexp);
		nnode->_expansions.push_back(nexp);
		// update parents
		node->_parents.clear();
		node->_parents.insert(nexp);
	}
	// check that root isn't a seq-node
	if (_root->IsSequence()) // if it is (which should not be the case) assigns its parent as new root
		_root = (*(_root->_parents.begin()))->_parent;

	// as we have inserted a lot of stuff, call gather flags so the tree is consistent
	GatherFlags(_root, {}, true);
}

void GrammarTree::FixRoot()
{
	// for simple grammars this isn't necessary, it's unwanted even, as we don't need to earley parser
	if (_simpleGrammar)
		return;
	if (_root->_expansions.size() > 1 ||
		(_root->_expansions.size() == 1 &&
			(_root->_expansions[0]->_nodes.size() > 1 ||
				_root->_expansions[0]->_nodes.size() == 1 &&
					_root->_expansions[0]->_nodes[0]->IsLeaf()))) {
		auto node = std::make_shared<GrammarNode>();
		node->_derivation = _root->_identifier;
		node->_type = GrammarNode::NodeType::NonTerminal;
		node->_id = GetNextID();
		node->_identifier = std::string("'FixedStart");
		_hashmap.insert_or_assign(node->_id, node);
		_nonterminals.insert(node);
		_ruleorder.push_back(node->_id);
		
		auto exp = std::make_shared<GrammarExpansion>();
		exp->_weight = 0;
		exp->_nonterminals++;
		exp->_nodes.push_back(_root);
		_root->_parents.insert(exp);
		exp->_id = GetNextID();
		_hashmap_expansions.insert_or_assign(exp->_id, exp);
		node->_expansions.push_back(exp);
		exp->_parent = node;

		_root = node;

		GatherFlags(_root, {}, true);
	}
}


void GrammarTree::SimplifyRecursionHelper()
{
	for (auto [id, expansion] : _hashmap_expansions)
	{
		auto node = expansion->_parent;
		// if the expansion expands into a single other symbol, we can transfer the rules to the parent
		if (expansion->_nodes.size() == 1 && node != expansion->_nodes[0])
		{
			// DO WE REALLY WANT TO INVEST TIME HERE
		}
	}
}


bool GrammarTree::CheckForSequenceSimplicityAndSimplify()
{
	// if this grammar doesn't produce sequences we don't want to use this optimization
	if ((_root->_flags & GrammarNode::NodeFlags::ProduceSequence) == 0)
		return false;

	// this function checks a few cases in which the grammar can be transformed into a regular 
	// expression on the sequence generation level
	// [beneath the sequences doesn't matter, as EarleyParser works on sequence nodes]

	std::vector<bool> _rules(_ruleorder.size(), false);

	// first find all rules that are sequence rules, or beneath them, we don't care about them
	// [IsSequence, or NOT ProducingSequence]
	for (size_t i = 0; i < _rules.size(); i++)
		if (auto node = _hashmap.at(_ruleorder[i]); node->IsSequence() == false && (node->_flags & GrammarNode::NodeFlags::ProduceSequence) == 0 || node->IsSequence())
			_rules[i] = true;

	std::vector<std::shared_ptr<GrammarNode>> handle;

	std::unordered_map<uint64_t, uint64_t> simplereplace;

	// first case 
	// find all expansions that can be reformed as simple regex
	// we only accept simple regex. S*, S+, 
	bool simple = true;

	// simple productions
	for (size_t i = 0; i < _rules.size(); i++) {
		if (_rules[i] != true) {
			// 'Node1 -> 'Node2
			auto node = _hashmap.at(_ruleorder[i]);
			if (node->_expansions.size() == 1 && node->_expansions[0]->_nodes.size() == 1 && node->_type != GrammarNode::NodeType::Sequence) {
				_rules[i] = true;
				simplereplace.insert_or_assign(node->_id, node->_expansions[0]->_nodes[0]->_id);
			}
		}
	}

	for (size_t i = 0; i < _rules.size(); i++) {
		if (_rules[i] != true) {
			// acceptable:
			// 'Node1 -> 'Node1 ~ 'Node1 | 'Node2		('Node2)+
			// 'Node1 -> 'Node2 | 'Node1 ~ 'Node2		('Node2)+
			// 'Node1 -> 'Node2 | 'Node2 ~ 'Node1		('Node2)+
			// 'Node1 ->  | 'Node2 ~ 'Node1				('Node2)*
			// 'Node1 ->  | 'Node1 ~ 'Node2				('Node2)*

			auto isRegex = [&simplereplace](std::shared_ptr<GrammarNode> node) {
				auto equalid = [&simplereplace](uint64_t lhs, uint64_t rhs) {
					if (lhs == rhs)
						return true;
					auto itr = simplereplace.find(lhs);
					if (itr != simplereplace.end()) {
						while (itr != simplereplace.end()) {
							if (itr->second == rhs)
								return true;
							auto itra = simplereplace.find(rhs);
							while (itra != simplereplace.end()) {
								if (itr->second == itra->second)
									return true;
								itra = simplereplace.find(itra->second);
							}
							itr = simplereplace.find(itr->second);
						}
					} else {
						auto itra = simplereplace.find(rhs);
						while (itra != simplereplace.end()) {
							if (lhs == itra->second)
								return true;
							itra = simplereplace.find(itra->second);
						}
					}
					return false;
				};

				if (node->_expansions.size() != 2)
					return false;
				if (node->_expansions.size() == 2) {
					if (node->_expansions[0]->_nodes.size() == 1 && node->_expansions[1]->_nodes.size() == 2) {
						// +
						if ((equalid(node->_expansions[0]->_nodes[0]->_id, node->_expansions[1]->_nodes[0]->_id) ||
								equalid(node->_expansions[0]->_nodes[0]->_id, node->_expansions[1]->_nodes[1]->_id)) &&
							(equalid(node->_expansions[1]->_nodes[0]->_id, node->_id) ||
								equalid(node->_expansions[1]->_nodes[1]->_id, node->_id)))
							return true;
						// 'Node1 ~ 'Node1 | 'Node2
						if (equalid(node->_expansions[1]->_nodes[0]->_id, node->_expansions[1]->_nodes[1]->_id) &&
							equalid(node->_expansions[0]->_nodes[0]->_id, node->_expansions[1]->_nodes[0]->_id) == false)
							return true;
					} else if (node->_expansions[0]->_nodes.size() == 2 && node->_expansions[1]->_nodes.size() == 1) {
						// +
						if ((equalid(node->_expansions[1]->_nodes[0]->_id, node->_expansions[0]->_nodes[0]->_id) ||
								equalid(node->_expansions[1]->_nodes[0]->_id, node->_expansions[0]->_nodes[1]->_id)) &&
							(equalid(node->_expansions[0]->_nodes[0]->_id, node->_id) ||
								equalid(node->_expansions[0]->_nodes[1]->_id, node->_id)))
							return true;
						// 'Node1 ~ 'Node1 | 'Node2
						if (equalid(node->_expansions[0]->_nodes[0]->_id, node->_expansions[0]->_nodes[1]->_id) &&
							equalid(node->_expansions[1]->_nodes[0]->_id, node->_expansions[0]->_nodes[0]->_id) == false)
							return true;
					} else if (node->_expansions[0]->_nodes.size() == 0 && node->_expansions[1]->_nodes.size() == 2) {
						// *
						if (equalid(node->_expansions[1]->_nodes[0]->_id, node->_id) ||
							equalid(node->_expansions[1]->_nodes[1]->_id, node->_id))
							return true;
					}
				}
				return false;
			};

			auto node = _hashmap.at(_ruleorder[i]);
			// regex
			if (isRegex(node)) {
				_rules[i] = true;
				handle.push_back(node);
			}
		}
		simple &= _rules[i];
	}
	if (simple) {
		// resolve simple productions
		for (size_t i = 0; i < _rules.size(); i++) {
			auto node = _hashmap.at(_ruleorder[i]);

			if (node->_expansions.size() == 1 && node->_expansions[0]->_nodes.size() == 1 && node->IsSequence() == false) {
				if (node->_parents.size() == 0) {
					// root -> make child node the new tree root
					if (node == _root) {
						_root = node->_expansions[0]->_nodes[0];
					}
					node->_expansions[0]->_parent.reset();
					node->_expansions[0]->_nodes[0]->_parents.erase(node->_expansions[0]);
					node->_expansions.clear();
				} else {
					node->_expansions[0]->_parent.reset();
					node->_expansions[0]->_nodes[0]->_parents.erase(node->_expansions[0]);
					for (auto parent : node->_parents) {
						for (size_t c = 0; c < parent->_nodes.size(); c++) {
							if (parent->_nodes[c]->_id == node->_id) {
								// replace
								parent->_nodes[c] = node->_expansions[0]->_nodes[0];
								node->_expansions[0]->_nodes[0]->_parents.insert(parent);
							}
						}
					}
					node->_expansions.clear();
					node->_parents.clear();
				}
			}
		}

		// simple grammar, try transforming
		for (auto node : handle) {
			auto getRegexInfo = [](std::shared_ptr<GrammarNode> node) {
				if (node->_expansions.size() == 2) {
					if (node->_expansions[0]->_nodes.size() == 1 && node->_expansions[1]->_nodes.size() == 2) {
						// +
						if ((node->_expansions[0]->_nodes[0]->_id == node->_expansions[1]->_nodes[0]->_id ||
								node->_expansions[0]->_nodes[0]->_id == node->_expansions[1]->_nodes[1]->_id) &&
							(node->_expansions[1]->_nodes[0]->_id == node->_id ||
								node->_expansions[1]->_nodes[1]->_id == node->_id)) {
							if (node->_expansions[1]->_nodes[0]->_id == node->_id) {
								return std::pair<std::shared_ptr<GrammarNode>, int32_t>{ node->_expansions[1]->_nodes[1], 1 };
							} else {
								return std::pair<std::shared_ptr<GrammarNode>, int32_t>{ node->_expansions[0]->_nodes[1], 1 };
							}
						}
						// 'Node1 ~ 'Node1 | 'Node2
						if (node->_expansions[1]->_nodes[0]->_id == node->_expansions[1]->_nodes[1]->_id &&
							node->_expansions[0]->_nodes[0]->_id != node->_expansions[1]->_nodes[0]->_id)
							return std::pair<std::shared_ptr<GrammarNode>, int32_t>{ node->_expansions[0]->_nodes[0], 1 };
						/* if (node->_expansions[0]->_nodes[0]->_id == node->_id) {
							if (node->_expansions[1]->_nodes[0]->_id == node->_id) {
								return std::pair<std::shared_ptr<GrammarNode>, int32_t>{ node->_expansions[1]->_nodes[0], 1 };
							} else if (node->_expansions[1]->_nodes[1]->_id == node->_id) {
								return std::pair<std::shared_ptr<GrammarNode>, int32_t>{ node->_expansions[1]->_nodes[1], 1 };
							}
						}*/
					} else if (node->_expansions[0]->_nodes.size() == 2 && node->_expansions[1]->_nodes.size() == 1) {
						// +
						if ((node->_expansions[1]->_nodes[0]->_id == node->_expansions[0]->_nodes[0]->_id ||
								node->_expansions[1]->_nodes[0]->_id == node->_expansions[0]->_nodes[1]->_id) &&
							(node->_expansions[0]->_nodes[0]->_id == node->_id ||
								node->_expansions[0]->_nodes[1]->_id == node->_id)) {
							if (node->_expansions[0]->_nodes[0]->_id == node->_id) {
								return std::pair<std::shared_ptr<GrammarNode>, int32_t>{ node->_expansions[0]->_nodes[1], 1 };
							} else {
								return std::pair<std::shared_ptr<GrammarNode>, int32_t>{ node->_expansions[1]->_nodes[1], 1 };
							}
						}

						if (node->_expansions[0]->_nodes[0]->_id == node->_expansions[0]->_nodes[1]->_id &&
							node->_expansions[1]->_nodes[0]->_id != node->_expansions[0]->_nodes[0]->_id)
							return std::pair<std::shared_ptr<GrammarNode>, int32_t>{ node->_expansions[1]->_nodes[0], 1 };
						/* if (node->_expansions[1]->_nodes[0]->_id == node->_id) {
							if (node->_expansions[0]->_nodes[0]->_id == node->_id) {
								return std::pair<std::shared_ptr<GrammarNode>, int32_t>{ node->_expansions[0]->_nodes[0], 1 };
							} else if (node->_expansions[0]->_nodes[1]->_id == node->_id) {
								return std::pair<std::shared_ptr<GrammarNode>, int32_t>{ node->_expansions[0]->_nodes[1], 1 };
							}
						}*/
					} else if (node->_expansions[0]->_nodes.size() == 0 && node->_expansions[1]->_nodes.size() == 2) {
						// *

						if (node->_expansions[1]->_nodes[0]->_id == node->_id)
							return std::pair<std::shared_ptr<GrammarNode>, int32_t>{ node->_expansions[1]->_nodes[1], 1 };
						else if (node->_expansions[1]->_nodes[1]->_id == node->_id)
							return std::pair<std::shared_ptr<GrammarNode>, int32_t>{ node->_expansions[1]->_nodes[0], 1 };

						/* if (node->_expansions[1]->_nodes[0]->_id == node->_id) {
							return std::pair<std::shared_ptr<GrammarNode>, int32_t>{ node->_expansions[1]->_nodes[0], 0 };
						} else if (node->_expansions[1]->_nodes[1]->_id == node->_id) {
							return std::pair<std::shared_ptr<GrammarNode>, int32_t>{ node->_expansions[1]->_nodes[1], 0 };
						}*/
					}
				}
				return std::pair<std::shared_ptr<GrammarNode>, int32_t>{ {}, 0 };
			};

			// resolve regex
			if (auto [prodnode, min] = getRegexInfo(node); prodnode) {
				// regex
				std::shared_ptr<GrammarExpansionRegex> regex = std::make_shared<GrammarExpansionRegex>();
				regex->_id = GetNextID();
				regex->_node = prodnode;
				prodnode->_parents.insert(regex);
				regex->_min = min;
				regex->_parent = node;

				for (auto exp : node->_expansions) {
					exp->_parent.reset();
					for (auto expnode : exp->_nodes) {
						expnode->_parents.erase(exp);
					}
					exp->_nodes.clear();
				}
				node->_expansions.clear();
				node->_expansions.push_back(regex);
				_hashmap_expansions.insert_or_assign(regex->_id, regex);
			}
		}

		// gather all flags for all tree nodes
		GatherFlags(_root, {}, true);

		// prune tree to valid expansion and nodes
		Prune();

		// set simple grammar
		_simpleGrammar = true;
		return true;
	} else
		return false;
}

size_t GrammarTree::MemorySize()
{
	return sizeof(GrammarTree) + _nonterminals.size() * sizeof(std::shared_ptr<GrammarNode>) + _terminals.size() * sizeof(std::shared_ptr<GrammarNode>) + _hashmap.size() * sizeof(std::pair<uint64_t, std::shared_ptr<GrammarNode>>) + _hashmap_expansions.size() * sizeof(std::pair<uint64_t, std::shared_ptr<GrammarExpansion>>) + _ruleorder.size() * sizeof(uint64_t) + _hashmap_parsenodes.size() * sizeof(uint64_t);
}

void Grammar::ParseScala(std::filesystem::path path)
{
	StartProfiling;
	loginfo("enter");
	// check whether the path exists and whether its actually a file
	if (!std::filesystem::exists(path) || std::filesystem::is_directory(path)) {
		logwarn("Cannot read the grammar from file: {}. Either the path does not exist or refers to a directory.", path.string());
		return;
	}
	std::ifstream _stream;
	try {
		_stream = std::ifstream(path, std::ios_base::in);
	} catch (std::exception& e) {
		logwarn("Cannot read the grammar from file: {}. The file cannot be accessed with error message: {}", path.string(), e.what());
		return;
	}
	logdebug("filestream");
	if (_stream.is_open()) {
		logdebug("filestream is opened");
		std::string line;
		bool fGrammar = false;
		std::string backup;
		std::string grammar;
		int32_t bopen = 0;
		int32_t bclosed = 0;
		while (std::getline(_stream, line)) {
			// we read another line
			// check if its empty or with a comment
			if (line.empty())
				continue;
			// remove leading spaces and tabs
			while (line.length() > 0 && (line[0] == ' ' || line[0] == '\t')) {
				line = line.substr(1, line.length() - 1);
			}
			// check again
			if (line.length() == 0)
				continue;
			backup = grammar;
			if (fGrammar == false) {
				if (auto pos = line.find("Grammar("); pos != std::string::npos) {
					fGrammar = true;
					line = line.substr(pos + 8, line.length() - (pos + 8));
					grammar += line;
					logdebug("found beginning of grammar");
				}
			} else {
				grammar += line;
			}
			bopen += Utility::CountSymbols(line, '(', '\'', '\"');
			bclosed += Utility::CountSymbols(line, ')', '\'', '\"');
			if (bclosed == bopen + 1) {
				logdebug("found end of grammar");
				break;
			}
		}
		// we have found the last occurence of the closing bracket. Remove it.
		auto lpos = grammar.rfind(")");
		grammar = grammar.substr(0, lpos);

		// initialize the grammartree
		_tree.reset();  // destroy
		_tree = std::make_shared<GrammarTree>();

		logdebug("initialized grammar tree");

		// now we have the raw grammar in one string without newlines
		// splitting them on a comma will give us the individual rules
		auto rules = Utility::SplitString(grammar, ',', true, true, '\"');
		// handle the individual rules
		for (auto rule : rules) {
			// rules are of the form	symbol := derivation
			Utility::RemoveWhiteSpaces(rule, '\"', true, true);
			Utility::RemoveSymbols(rule, '\n', true, '\"');
			Utility::RemoveSymbols(rule, '\r', true, '\"');
			auto split = Utility::SplitString(rule, ":=", true, true, '\"');
			if (split.size() == 2) {
				if (auto pos = split[0].find("'SEQ"); pos != std::string::npos)
					_tree->AddSequenceSymbol(split[0], split[1]);
				else
					_tree->AddSymbol(split[0], split[1]);
				loginfo("Found symbol: {} with derivation: {}", split[0], split[1]);
			} else
				logwarn("Rule cannot be read: {}, splitsize: {}", rule, split.size());
		}

		logdebug("parsed grammar");

		// find 'start symbol and add it to root
		auto node = _tree->FindNode("'start");
		if (node) {
			_tree->SetRoot(node);
			loginfo("Set root node for grammar");
		}
		else {
			logcritical("The grammar does not include the symbol 'start: {}", path.string());
			return;
		}

		// all symbols have been read and added to the tree
		// now construct the tree
		_tree->Construct();

		// check if it's a simple grammar and simplify it
		_tree->CheckForSequenceSimplicityAndSimplify();

		// fix root symbol if necessary
		_tree->FixRoot();
		
		logdebug("constructed grammar");

		if (_tree->IsValid()) {
			loginfo("Successfully read the grammar from file: {}", path.string());

			// only if the grammar isn't simple
			if (_tree->_simpleGrammar == false) {
				// now construct tree for parsing
				_treeParse = std::make_shared<GrammarTree>();
				_tree->DeepCopy(_treeParse);
				_treeParse->InsertParseNodes();
			}
		} else
			logcritical("The file {} does not contain a valid grammar", path.string());
	}
	loginfo("exit");
	profile(TimeProfiling, "function execution time");
}

bool Grammar::IsValid()
{
	if (_tree)
		return _tree->IsValid();
	return false;
}

bool Grammar::IsSimple()
{
	if (_tree)
		return _tree->_simpleGrammar;
	return false;
}

std::string Grammar::Scala(bool parsetree)
{
	if (parsetree) {
		if (_treeParse)
			return _treeParse->Scala();
		else
			return "Grammar()";
	} else {
		if (_tree)
			return _tree->Scala();
		else
			return "Grammar()";
	}
}

Grammar::~Grammar()
{
	Clear();
}

void Grammar::Extract(std::shared_ptr<DerivationTree> stree, std::shared_ptr<DerivationTree> dtree, std::vector<std::pair<int64_t, int64_t>>& segments, int64_t stop, bool complement)
{
	if (segments.empty() || !stree || !dtree) {
		logcritical("Are you stupid the segments are empty or the trees don't exist");
		return;
	}
	if (_tree->_simpleGrammar == false)
	{
		if (segments.size() == 1)
			return ExtractEarley(stree, dtree, segments[0].first, segments[0].second, stop, complement);
		else
			logcritical("Multiple segments aren't supported for complex grammars at the moment");
	}
	else
	{
		StartProfiling;

		auto allocators = Allocators::GetThreadAllocators(std::this_thread::get_id());

		// simple grammar
		// yay no earleyparser
		// just copy tree and check that it fits our regexes

		// set up dtree information
		dtree->_parent._parentID = stree->GetFormID();
		dtree->_parent.segments.clear();
		dtree->_parent.segments = segments;
		dtree->_parent._stop = stop;
		dtree->_parent._complement = complement;
		dtree->_sequenceNodes = 0;
		dtree->_parent.method = DerivationTree::ParentTree::Method::DD;
		dtree->_grammarID = stree->_grammarID;

		// gather the sequence nodes of the source tree
		std::vector<DerivationTree::SequenceNode*> seqnodes;
		seqnodes.reserve(stree->_sequenceNodes);
		std::stack<DerivationTree::NonTerminalNode*> stacks;
		stacks.push((DerivationTree::NonTerminalNode*)(stree->_root));
		DerivationTree::NonTerminalNode* tmp = nullptr;

		while (stacks.size() > 0) {
			tmp = stacks.top();
			stacks.pop();
			if (tmp == nullptr) {
				logcritical("stree has become empty");
				return;
			}
			if (tmp->Type() == DerivationTree::NodeType::Sequence)
				seqnodes.push_back((DerivationTree::SequenceNode*)tmp);
			// push children in reverse order to stack: We explore from left to right
			// skip terminal children, they cannot produce sequences
			for (int32_t i = (int32_t)tmp->_children.size() - 1; i >= 0; i--) {
				if (tmp->_children[i]->Type() != DerivationTree::NodeType::Terminal)
					stacks.push((DerivationTree::NonTerminalNode*)(tmp->_children[i]));
			}
		}
		if ((int64_t)seqnodes.size() != stree->_sequenceNodes) {
			logwarn("Danger");
			return;
		}

		profile(TimeProfiling, "{}: gathered sequence nodes", Utility::PrintForm(dtree));
		// sequence nodes are in order

		std::vector<DerivationTree::SequenceNode*> targetnodes;
		targetnodes.reserve(stree->_sequenceNodes);
		std::vector<std::vector<DerivationTree::Node*>> targetnodesvec;
		targetnodesvec.reserve(stree->_nodes);
		if (complement)
		{
			int64_t idx = 0;
			size_t segidx = 0;
			while (idx < stop && segidx < segments.size()) {
				while (idx < stop && idx < segments[segidx].first) {
					//auto [cnode, cnodes] = seqnodes[idx]->CopyRecursive();
					auto [cnode, cnodes] = seqnodes[idx]->CopyRecursiveAlloc(allocators);
					targetnodes.push_back((DerivationTree::SequenceNode*)cnode);
					targetnodesvec.push_back(cnodes);
					idx++;
				}
				idx = segments[segidx].first + segments[segidx].second;
				segidx++;
			}
			while (idx < stop) {
				//auto [cnode, cnodes] = seqnodes[idx]->CopyRecursive();
				auto [cnode, cnodes] = seqnodes[idx]->CopyRecursiveAlloc(allocators);
				targetnodes.push_back((DerivationTree::SequenceNode*)cnode);
				targetnodesvec.push_back(cnodes);
				idx++;
			}

		} else {
			for (size_t i = 0; i < segments.size(); i++)
			{
				for (int64_t c = segments[i].first; c < segments[i].first + segments[i].second; c++)
				{
					//auto [cnode, cnodes] = seqnodes[c]->CopyRecursive();
					auto [cnode, cnodes] = seqnodes[c]->CopyRecursiveAlloc(allocators);
					targetnodes.push_back((DerivationTree::SequenceNode*)cnode);
					targetnodesvec.push_back(cnodes);
				}
			}
		}

		profile(TimeProfiling, "{}: copied nodes", Utility::PrintForm(dtree));

		// got all nodes
		// we know that our grammar is simple, so our tree root is a regex that derives all the nodes
		
		if (targetnodes.size() > 0 && _tree->_root->_expansions.size() == 1 && _tree->_root->_expansions[0]->IsRegex() && dynamic_pointer_cast<GrammarExpansionRegex>(_tree->_root->_expansions[0])->_node->_id == targetnodes[0]->_grammarID)
		{
			bool same = true;
			// check whether all nodes are the same
			for (size_t i = 0; i < targetnodes.size(); i++)
				same &= (targetnodes[0]->_grammarID == targetnodes[i]->_grammarID);
			if (same)
			{
				// build derivation tree
				DerivationTree::NonTerminalNode* droot = allocators->DerivationTree_NonTerminalNode()->New();
				//DerivationTree::NonTerminalNode* droot = DerivationTree::NonTerminalNode();
				droot->_grammarID = _tree->_root->_id;
				dtree->_root = droot;
				dtree->_nodes++;
				droot->_children.resize(targetnodes.size());
				for (size_t c = 0; c < targetnodes.size(); c++) {
					//auto [cnode, cnodes] = targetnodes[c]->CopyRecursive();
					//droot->_children[c] = targetnodes[c];
					droot->SetChild(c, targetnodes[c]);
					dtree->_nodes += targetnodesvec[c].size();
					//for (auto x : targetnodesvec[c])
					//	dtree->_nodes++;
				}

				dtree->_valid = true;
				dtree->SetRegenerate(true);
				dtree->_sequenceNodes = (int64_t)targetnodes.size();
			} else
				logwarn("Something wrong in simple extraction");
		}
		profile(TimeProfiling, "{}: Time taken for extraction of length: {}, form length: {}", Utility::PrintForm(dtree), dtree->_sequenceNodes, stree->_sequenceNodes);
	}
}

void Grammar::ExtractEarley(std::shared_ptr<DerivationTree> stree, std::shared_ptr<DerivationTree> dtree, int64_t begin, int64_t length, int64_t stop, bool complement)
{
	StartProfiling;
	// set up dtree information
	dtree->_parent._parentID = stree->GetFormID();
	dtree->_parent.segments.clear();
	dtree->_parent.segments.push_back({ begin, length });
	dtree->_parent._stop = stop;
	dtree->_parent._complement = complement;
	dtree->_sequenceNodes = 0;
	dtree->_parent.method = DerivationTree::ParentTree::Method::DD;
	dtree->_grammarID = stree->_grammarID;

	// gather the sequence nodes of the source tree
	std::vector<DerivationTree::SequenceNode*> seqnodes;
	std::stack<DerivationTree::NonTerminalNode*> stacks;
	stacks.push((DerivationTree::NonTerminalNode*)(stree->_root));
	DerivationTree::NonTerminalNode* tmp = nullptr;

	while (stacks.size() > 0) {
		tmp = stacks.top();
		stacks.pop();
		if (tmp->Type() == DerivationTree::NodeType::Sequence)
			seqnodes.push_back((DerivationTree::SequenceNode*)tmp);
		// push children in reverse order to stack: We explore from left to right
		// skip terminal children, they cannot produce sequences
		for (int32_t i = (int32_t)tmp->_children.size() - 1; i >= 0; i--) {
			if (tmp->_children[i]->Type() != DerivationTree::NodeType::Terminal)
				stacks.push((DerivationTree::NonTerminalNode*)(tmp->_children[i]));
		}
	}
	// sequence nodes are in order

	EarleyParser parser;
	std::vector<DerivationTree::SequenceNode*> targetnodes;

	for (int64_t i = 0; i < stop; i++) {
		if (complement) {
			if (i < begin || i >= begin + length) {
				parser.inputVector.push_back(_treeParse->_hashmap.at(seqnodes[i]->_grammarID));
				targetnodes.push_back(seqnodes[i]);
			}
		} else {
			if (i >= begin && i < begin + length) {
				parser.inputVector.push_back(_treeParse->_hashmap.at(seqnodes[i]->_grammarID));
				targetnodes.push_back(seqnodes[i]);
			}
		}
	}
	parser.initialize(false);
	parser.createChart(_treeParse->_root);
	// we have the correct sequence we want to create
	// now find a parse tree that supports it

	parser.parse();
	auto forest = parser.returnParsingTrees(_treeParse->_root);

	// check wether there is at least one tree in the forest
	if (forest->size() < 1) {
		// there is no valid derivation, so set dest tree to invalid and return
		dtree->_valid = false;
		dtree->SetRegenerate(false);
		return;
	}

	// there is at least one valid derivation, so just take the first there is and be done with it
	// construct the derivation tree

	// traversing the parseTree from left to right will result in the corresponding derivation tree
	// we want
	int64_t targetnodesIndex = 0;

	auto allocators = Allocators::GetThreadAllocators(std::this_thread::get_id());
	auto allocNonT = allocators->DerivationTree_NonTerminalNode();
	auto allocSeq = allocators->DerivationTree_SequenceNode();

	Node* proot = forest->at(0);
	//DerivationTree::NonTerminalNode* droot = new DerivationTree::NonTerminalNode();
	DerivationTree::NonTerminalNode* droot = allocNonT->New();
	droot->_grammarID = proot->getGrammarElement()->_id;
	dtree->_root = droot;
	dtree->_nodes++;
	std::stack<std::pair<Node*, DerivationTree::NonTerminalNode*>> stack;
	DerivationTree::NonTerminalNode* ntmp = nullptr;
	stack.push({ proot, droot });
	while (stack.size() > 0)
	{
		auto [pnode, dnode] = stack.top();
		stack.pop();
		auto children = pnode->getChildren();
		// check whether pnode is a parse node, if thats the case replace it by the original node
		// and put it back onto the stack
		if (_treeParse->_hashmap_parsenodes.find(pnode->getGrammarElement()->_id) != _treeParse->_hashmap_parsenodes.end())
			stack.push({ children[0], dnode }); // there is only one child by definition
		else if (pnode->getGrammarElement()->IsSequence()) {
			// we have found the currently left most seqnode, so copy that one and all their children and childrens children, etc.
			// to the dnode
			dnode->_grammarID = targetnodes[targetnodesIndex]->_grammarID;
			for (auto child : targetnodes[targetnodesIndex]->_children) {
				//auto [cnode, cnodes] = child->CopyRecursive();
				auto [cnode, cnodes] = child->CopyRecursiveAlloc(allocators);
				dnode->AddChild(cnode);
				// not available in linux (barf)
				//dtree->_nodes.insert_range(cnodes);
				for (auto x : cnodes)
					dtree->_nodes++;
			}
			targetnodesIndex++;
		}
		else
		{
			dnode->_children.resize(children.size());
			// its just a terminal node, so expand the dnode with children and add them to the stack
			for (int64_t i = (int64_t)children.size() - 1; i >= 0; i--) {
				// if the node is a sequence node, or the node is a special parse node insert a SequenceNode instead of a NonTerminal
				if (children[i]->getGrammarElement()->IsSequence() || _treeParse->_hashmap_parsenodes.find(children[i]->getGrammarElement()->_id) != _treeParse->_hashmap_parsenodes.end()) {
					//ntmp = new DerivationTree::SequenceNode();
					ntmp = allocSeq->New();
					dtree->_sequenceNodes++;
				} else {
					//ntmp = new DerivationTree::NonTerminalNode();
					ntmp = allocNonT->New();
				}
				ntmp->_grammarID = children[i]->getGrammarElement()->_id;
				dtree->_nodes++;
				dnode->SetChild(i, ntmp);
				//dnode->_children[i] = (ntmp);
				stack.push({ children.at(i), ntmp });
			}
		}
	}
	if (targetnodesIndex == (int64_t)targetnodes.size()) {
		//dtree->_sequenceNodes = (int64_t)targetnodes.size();
		dtree->_valid = true;
		dtree->SetRegenerate(true);
	} else {
		dtree->_valid = false;
		dtree->SetRegenerate(false);
	}
	profile(TimeProfiling, "Time taken for extraction of length: {}, form length: {}", dtree->_sequenceNodes, stree->_sequenceNodes);
}

void Grammar::Derive(std::shared_ptr<DerivationTree> dtree, int32_t targetlength, uint32_t seed, int32_t /*maxsteps*/)
{
	StartProfiling
	// this function takes an empty derivation tree and a goal of nonterminals to produce
	// this function will step-by-step expand from the start symbol to more and more nonterminals
	// until the goal is reached
	// afterwards all nonterminals are expanded until there are only terinal nodes remaining
	// at last the terminal nodes are resolved
	dtree->_grammarID = _formid;
	dtree->_seed = seed;
	dtree->_targetlen = targetlength;
	DerivationTree::NonTerminalNode* nnode = nullptr;
	DerivationTree::TerminalNode* ttnode = nullptr;
	DerivationTree::NonTerminalNode* tnnode = nullptr;

	// count of generated sequence nodes
	int32_t seq = 0;
	// holds all nonterminals that were generated
	std::deque<std::pair<DerivationTree::NonTerminalNode*, std::shared_ptr<GrammarNode>>> qnonterminals;
	// holds all sequence nodes and sequence producing nodes during generation
	std::deque<std::pair<DerivationTree::NonTerminalNode*, std::shared_ptr<GrammarNode>>> qseqnonterminals;

	// init random stuff
	std::mt19937 randan((unsigned int)seed);

	auto allocators = Allocators::GetThreadAllocators(std::this_thread::get_id());

	// begin: insert start node 
	if (_tree->_root->IsSequence()) {
		//nnode = new DerivationTree::NonTerminalNode;
		nnode = allocators->DerivationTree_NonTerminalNode()->New();
		if (_tree->_simpleGrammar) // if we have a simple grammar most nodes will be derived from this one
			nnode->_children.reserve(targetlength);
		dtree->_nodes++;
		nnode->_grammarID = _tree->_root->_id;
		dtree->_root = nnode;
		seq++;
		qseqnonterminals.push_back({ nnode, _tree->_root });
	} else if (_tree->_root->_type == GrammarNode::NodeType::NonTerminal) {
		//nnode = new DerivationTree::NonTerminalNode;
		nnode = allocators->DerivationTree_NonTerminalNode()->New();
		if (_tree->_simpleGrammar)  // if we have a simple grammar most nodes will be derived from this one
			nnode->_children.reserve(targetlength);
		dtree->_nodes++;
		nnode->_grammarID = _tree->_root->_id;
		dtree->_root = nnode;
		qseqnonterminals.push_back({ nnode, _tree->_root });
	} else {
		//ttnode = new DerivationTree::TerminalNode;
		ttnode = allocators->DerivationTree_TerminalNode()->New();
		dtree->_nodes++;
		ttnode->_grammarID = _tree->_root->_id;
		dtree->_root = ttnode;
		ttnode->_content = _tree->_root->_identifier;
	}

	DeriveFromNode(dtree, qnonterminals, qseqnonterminals, randan, seq);
	
	dtree->_valid = true;
	dtree->SetRegenerate(true);
	dtree->_sequenceNodes = seq;

	profile(TimeProfiling, "{}: Time taken for derivation of length: {}", Utility::PrintForm(dtree), dtree->_sequenceNodes);
}

std::string GetSymbols(std::shared_ptr<GrammarNode> node, std::mt19937& randan)
{
	if ((node->_flags & GrammarNode::NodeFlags::TerminalCharClass) > 0) {
		if ((node->_flags & GrammarNode::NodeFlags::TerminalCharClassAscii) > 0) {
			std::uniform_int_distribution<signed> dist(0x1, 0x7E);
			char c = (char)dist(randan);
			return std::string(1, c);
		} else if ((node->_flags & GrammarNode::NodeFlags::TerminalCharClassAlpha) > 0) {
			std::uniform_int_distribution<signed> dist(0x0, 52);
			char c = (char)dist(randan);
			if (c < 26)
				c += 0x41;
			else
				c += 0x61;
			return std::string(1, c);
		} else if ((node->_flags & GrammarNode::NodeFlags::TerminalCharClassAlphaNumeric) > 0) {
			std::uniform_int_distribution<signed> dist(0x0, 62);
			char c = (char)dist(randan);
			if (c < 26)
				c += 0x41;
			else if (c < 52)
				c += 0x61;
			else
				c += 0x30;
			return std::string(1, c);
		} else if ((node->_flags & GrammarNode::NodeFlags::TerminalCharClassDigit) > 0) {
			std::uniform_int_distribution<signed> dist(0x30, 0x39);
			char c = (char)dist(randan);
			return std::string(1, c);
		}
	}
	return node->_identifier;
}

void Grammar::DeriveFromNode(std::shared_ptr<DerivationTree> dtree, std::deque<std::pair<DerivationTree::NonTerminalNode*, std::shared_ptr<GrammarNode>>>& qnonterminals, std::deque<std::pair<DerivationTree::NonTerminalNode*, std::shared_ptr<GrammarNode>>>& qseqnonterminals, std::mt19937& randan, int32_t& seq)
{
	DerivationTree::NonTerminalNode* nnode = nullptr;
	DerivationTree::TerminalNode* ttnode = nullptr;
	DerivationTree::NonTerminalNode* tnnode = nullptr;
	int32_t idx = 0;
	int32_t cnodes = 0;
	float cweight = 0.f;
	std::shared_ptr<GrammarNode> gnode;
	std::shared_ptr<GrammarExpansion> gexp;

	bool flip = false;

	auto allocators = Allocators::GetThreadAllocators(std::this_thread::get_id());
	auto allocNonT = allocators->DerivationTree_NonTerminalNode();
	auto allocT = allocators->DerivationTree_TerminalNode();
	auto allocSeq = allocators->DerivationTree_SequenceNode();

	// in the first loop, we will expand the non terminals and sequence non terminals such that we only expand
	// nodes that can produce new sequence nodes and only apply expansions that produce new sequence nodes
	while (seq < dtree->_targetlen && qseqnonterminals.size() > 0) {
		// expand the current valid sequence nonterminals by applying rules that may produce sequences
		int32_t nonseq = (int32_t)qseqnonterminals.size();  // number of non terminals that will be handled this iteration
		for (int c = 0; c < nonseq; c++) {
			// get node to handle
			auto pair = qseqnonterminals.front();
			qseqnonterminals.pop_front();
			nnode = std::get<0>(pair);
			gnode = std::get<1>(pair);
			cnodes = 0;
			cweight = 0;
			idx = -1;
			// choose expansion
			// we are sure there are always expansions to choose from, as we have pruned the tree and this cannot be a terminal node
			if (!flip)
				for (int32_t i = 0; i < (int32_t)gnode->_expansions.size(); i++) {
					// as long as we don't use weighted rules, the one with the most sequence symbols is used
					if (cnodes < gnode->_expansions[i]->_seqnonterminals && cweight == 0) {
						cnodes = gnode->_expansions[i]->_seqnonterminals;
						idx = i;
					} else if (cweight < gnode->_expansions[i]->_weight) {
						cnodes = gnode->_expansions[i]->_seqnonterminals;
						cweight = gnode->_expansions[i]->_weight;
						idx = i;
					}
				}
			else
				for (int32_t i = (int32_t)gnode->_expansions.size() - 1; i >= 0; i--) {
					// as long as we don't use weighted rules, the one with the most sequence symbols is used
					if (cnodes < gnode->_expansions[i]->_seqnonterminals && cweight == 0) {
						cnodes = gnode->_expansions[i]->_seqnonterminals;
						idx = i;
					} else if (cweight < gnode->_expansions[i]->_weight) {
						cnodes = gnode->_expansions[i]->_seqnonterminals;
						cweight = gnode->_expansions[i]->_weight;
						idx = i;
					}
				}
			// if there is no rule that directly produces sequence nodes, choose a random expansion that produces sequence nodes
			if (idx == -1) {
				std::vector<int32_t> exp;
				float tweight = 0.f;
				for (int32_t i = 0; i < (int32_t)gnode->_expansions.size(); i++) {
					if (gnode->_expansions[i]->_flags & GrammarNode::NodeFlags::ProduceSequence) {
						exp.push_back(i);
						tweight += gnode->_expansions[i]->_weight;
					}
				}
				if (exp.size() > 0) {
					if (tweight == 0.f) {
						std::uniform_int_distribution<signed> dist(0, (int32_t)exp.size() - 1);
						idx = exp[dist(randan)];
					} else {
						std::uniform_int_distribution<signed> dist(0, 100000000);
						// calc target weight and adjust for the total weight not being 1
						float target = ((float)dist(randan) / 100000000.f) * tweight;
						float current = 0.f;
						for (int32_t i = 0; i < (int32_t)exp.size(); i++) {
							if (current += gnode->_expansions[exp[i]]->_weight; current >= target) {
								idx = exp[i];
								break;
							}
						}
					}
				} else if (gnode->IsSequence()) {
					// this node is a sequence non terminal but doesn't have any rules that expand into new sequences
					// so we just add it to the regular nonterminals we will deal with later
					qnonterminals.push_back({ nnode, gnode });
					continue;
				} else {
					logcritical("Error during Derivation: Produce Sequence Flag set on node, but no expansions produce sequences.");
					return;
				}
			}
			// create derivation nodes and add them to the derivation tree
			gexp = gnode->_expansions[idx];
			if (gexp->IsRegex()) {
				auto regex = dynamic_pointer_cast<GrammarExpansionRegex>(gexp);
				int64_t num = dtree->_targetlen - seq; // generate as much as we can without going over our limit
				if (num == 0 && regex->_min == 1)
					num = 1;
				for (int64_t i = 0; i < num; i++)
				{
					switch (regex->_node->_type) {
					case GrammarNode::NodeType::Sequence:
						seq++;
						//tnnode = new DerivationTree::SequenceNode;
						tnnode = allocSeq->New();
						dtree->_nodes++;
						dtree->_sequenceNodes++;
						tnnode->_grammarID = regex->_node->_id;
						nnode->AddChild(tnnode);
						if (regex->_node->_flags & GrammarNode::NodeFlags::ProduceSequence)
							qseqnonterminals.push_back({ tnnode, regex->_node });
						else
							qnonterminals.push_back({ tnnode, regex->_node });
						break;
					case GrammarNode::NodeType::NonTerminal:
						//tnnode = new DerivationTree::NonTerminalNode;
						tnnode = allocNonT->New();
						dtree->_nodes++;
						tnnode->_grammarID = regex->_node->_id;
						nnode->AddChild(tnnode);
						if (regex->_node->_flags & GrammarNode::NodeFlags::ProduceSequence)
							qseqnonterminals.push_back({ tnnode, regex->_node });
						else
							qnonterminals.push_back({ tnnode, regex->_node });
						break;
					case GrammarNode::NodeType::Terminal:
						// create new terminal node
						//ttnode = new DerivationTree::TerminalNode();
						ttnode = allocT->New();
						dtree->_nodes++;
						ttnode->_grammarID = regex->_node->_id;
						ttnode->_content = GetSymbols(regex->_node, randan);
						nnode->AddChild(ttnode);
						break;
					}
				}
			} else {
				for (int32_t i = 0; i < (int32_t)gexp->_nodes.size(); i++) {
					switch (gexp->_nodes[i]->_type) {
					case GrammarNode::NodeType::Sequence:
						seq++;
						//tnnode = new DerivationTree::SequenceNode;
						tnnode = allocSeq->New();
						dtree->_nodes++;
						dtree->_sequenceNodes++;
						tnnode->_grammarID = gexp->_nodes[i]->_id;
						nnode->AddChild(tnnode);
						if (gexp->_nodes[i]->_flags & GrammarNode::NodeFlags::ProduceSequence)
							qseqnonterminals.push_back({ tnnode, gexp->_nodes[i] });
						else
							qnonterminals.push_back({ tnnode, gexp->_nodes[i] });
						break;
					case GrammarNode::NodeType::NonTerminal:
						//tnnode = new DerivationTree::NonTerminalNode;
						tnnode = allocNonT->New();
						dtree->_nodes++;
						tnnode->_grammarID = gexp->_nodes[i]->_id;
						nnode->AddChild(tnnode);
						if (gexp->_nodes[i]->_flags & GrammarNode::NodeFlags::ProduceSequence)
							qseqnonterminals.push_back({ tnnode, gexp->_nodes[i] });
						else
							qnonterminals.push_back({ tnnode, gexp->_nodes[i] });
						break;
					case GrammarNode::NodeType::Terminal:
						// create new terminal node
						//ttnode = new DerivationTree::TerminalNode();
						ttnode = allocT->New();
						dtree->_nodes++;
						ttnode->_grammarID = gexp->_nodes[i]->_id;
						ttnode->_content = GetSymbols(gexp->_nodes[i], randan);
						//ttnode->_content = gexp->_nodes[i]->_identifier;
						nnode->AddChild(ttnode);
						break;
					}
				}
			}
		}
		flip = true;
	}

	// in the second loop we expand all seq-nonterminals until non remain, while favoring expansions that
	// do not produce new sequences
	while (qseqnonterminals.size() > 0) {
		// get node to handle
		auto pair = qseqnonterminals.front();
		qseqnonterminals.pop_front();
		nnode = std::get<0>(pair);
		gnode = std::get<1>(pair);
		cnodes = 0;
		cweight = 0;
		idx = -1;
		float tweight = 0.f;
		float ttweight = 0.f;
		// sort out expansions that do not increase sequence number
		std::vector<int32_t> exp;
		for (int32_t i = 0; i < (int32_t)gnode->_expansions.size(); i++) {
			if ((gnode->_expansions[i]->_flags & GrammarNode::NodeFlags::ProduceSequence) == 0) {
				exp.push_back(i);
				tweight += gnode->_expansions[i]->_weight;
			}
			ttweight += gnode->_expansions[i]->_weight;
		}
		if (exp.size() > 0) {
			if (tweight == 0.f) {
				std::uniform_int_distribution<signed> dist(0, (int32_t)exp.size() - 1);
				idx = exp[dist(randan)];
			} else {
				std::uniform_int_distribution<signed> dist(0, 100000000);
				float target = ((float)dist(randan) / 100000000.f) * tweight; 
				float current = 0.f;
				for (int32_t i = 0; i < (int32_t)exp.size(); i++) {
					if (current += gnode->_expansions[exp[i]]->_weight; current >= target) {
						idx = exp[i];
						break;
					}
				}
			}
		} else {
			// there weren't any expansions that do not increase the sequence size, so just choose any random expansion
			if (ttweight == 0.f) {
				std::uniform_int_distribution<signed> dist(0, (int32_t)gnode->_expansions.size() - 1);
				idx = dist(randan);
			} else {
				std::uniform_int_distribution<signed> dist(0, 100000000);
				float target = ((float)dist(randan) / 100000000.f) * tweight; 
				float current = 0.f;
				for (int32_t i = 0; i < (int32_t)gnode->_expansions.size(); i++) {
					if (current += gnode->_expansions[i]->_weight; current >= target) {
						idx = i;
						break;
					}
				}
			}
		}
		exp.clear();

		// create derivation nodes and add them to the derivation tree
		gexp = gnode->_expansions[idx];
		for (int32_t i = 0; i < (int32_t)gexp->_nodes.size(); i++) {
			switch (gexp->_nodes[i]->_type) {
			case GrammarNode::NodeType::Sequence:
				seq++;
				//tnnode = new DerivationTree::SequenceNode;
				tnnode = allocSeq->New();
				dtree->_nodes++;
				dtree->_sequenceNodes++;
				tnnode->_grammarID = gexp->_nodes[i]->_id;
				nnode->AddChild(tnnode);
				qnonterminals.push_back({ tnnode, gexp->_nodes[i] });
				break;
			case GrammarNode::NodeType::NonTerminal:
				//tnnode = new DerivationTree::NonTerminalNode;
				tnnode = allocNonT->New();
				dtree->_nodes++;
				tnnode->_grammarID = gexp->_nodes[i]->_id;
				nnode->AddChild(tnnode);
				qnonterminals.push_back({ tnnode, gexp->_nodes[i] });
				break;
			case GrammarNode::NodeType::Terminal:
				// create new terminal node
				//ttnode = new DerivationTree::TerminalNode();
				ttnode = allocT->New();
				dtree->_nodes++;
				ttnode->_grammarID = gexp->_nodes[i]->_id;
				ttnode->_content = GetSymbols(gexp->_nodes[i], randan);
				nnode->AddChild(ttnode);
				break;
			}
		}
	}

	// in the third loop we expand all non-terminals, favoring rules that do not increase the sequence nodes,
	// until only terminal nodes remain
	while (qnonterminals.size() > 0) {
		// get node to handle
		auto pair = qnonterminals.front();
		qnonterminals.pop_front();
		nnode = std::get<0>(pair);
		gnode = std::get<1>(pair);
		cnodes = 0;
		cweight = 0;
		idx = -1;
		float tweight = 0.f;
		// choose random expansion
		for (int32_t i = 0; i < (int32_t)gnode->_expansions.size(); i++)
			tweight += gnode->_expansions[i]->_weight;

		if (tweight == 0.f) {
			std::uniform_int_distribution<signed> dist(0, (int32_t)gnode->_expansions.size() - 1);
			idx = dist(randan);
		} else {
			std::uniform_int_distribution<signed> dist(0, 100000000);
			float target = ((float)dist(randan) / 100000000.f) * tweight; 
			float current = 0.f;
			for (int32_t i = 0; i < (int32_t)gnode->_expansions.size(); i++) {
				if (current += gnode->_expansions[i]->_weight; current >= target) {
					idx = i;
					break;
				}
			}
		}

		// create derivation nodes and add them to the derivation tree
		gexp = gnode->_expansions[idx];
		for (int32_t i = 0; i < (int32_t)gexp->_nodes.size(); i++) {
			switch (gexp->_nodes[i]->_type) {
			case GrammarNode::NodeType::Sequence:
				seq++;
				//tnnode = new DerivationTree::SequenceNode;
				tnnode = allocSeq->New();
				dtree->_nodes++;
				dtree->_sequenceNodes++;
				tnnode->_grammarID = gexp->_nodes[i]->_id;
				nnode->AddChild(tnnode);
				qnonterminals.push_back({ tnnode, gexp->_nodes[i] });
				break;
			case GrammarNode::NodeType::NonTerminal:
				//tnnode = new DerivationTree::NonTerminalNode;
				tnnode = allocNonT->New();
				dtree->_nodes++;
				tnnode->_grammarID = gexp->_nodes[i]->_id;
				nnode->AddChild(tnnode);
				qnonterminals.push_back({ tnnode, gexp->_nodes[i] });
				break;
			case GrammarNode::NodeType::Terminal:
				// create new terminal node
				//ttnode = new DerivationTree::TerminalNode();
				ttnode = allocT->New();
				dtree->_nodes++;
				ttnode->_grammarID = gexp->_nodes[i]->_id;
				ttnode->_content = GetSymbols(gexp->_nodes[i], randan);
				//ttnode->_content = gexp->_nodes[i]->_identifier;
				nnode->AddChild(ttnode);
				break;
			}
		}
	}
}

void Grammar::Extend(std::shared_ptr<Input> sinput, std::shared_ptr<DerivationTree> dtree, bool backtrack, int32_t targetlength, uint32_t seed, int32_t& backtrackingdone, int32_t /*maxsteps*/)
{
	StartProfiling;
	// we need to whole source input, because it may have been trimmed, in that case we cannot just copy the source
	// but we must extract a subtree (hopefully valid) and extend from there
	auto stree = sinput->derive;
	// init dtree
	dtree->_grammarID = _formid;
	dtree->_seed = seed;
	dtree->_targetlen = targetlength;
	dtree->_parent.method = DerivationTree::ParentTree::Method::Extension;
	dtree->_parent._parentID = stree->GetFormID();
	// set this so we can use it for length calculations in inputs that are memory freed
	dtree->_parent._length = stree->_sequenceNodes;
	dtree->_valid = false;

	// tmp
	DerivationTree::NonTerminalNode* nnode = nullptr;
	DerivationTree::TerminalNode* ttnode = nullptr;
	DerivationTree::NonTerminalNode* tnnode = nullptr;
	std::shared_ptr<GrammarNode> gnode;
	std::shared_ptr<GrammarExpansion> gexp;
	// counter
	int32_t additionalLength = 0;

	// init random stuff
	std::mt19937 randan((unsigned int)seed);
	std::uniform_int_distribution<signed> dist;
	if (backtrack) {
		dist = std::uniform_int_distribution<signed>(_backtrack_min, _backtrack_max);
	} else {
		dist = std::uniform_int_distribution<signed>(_extension_min, _extension_max);
	}

	int32_t trackback = dist(randan);

	auto allocators = Allocators::GetThreadAllocators(std::this_thread::get_id());
	auto allocNonT = allocators->DerivationTree_NonTerminalNode();
	auto allocT = allocators->DerivationTree_TerminalNode();
	auto allocSeq = allocators->DerivationTree_SequenceNode();

	// copy source derivation tree to dest tree
	// // uhhh stack overflow uhh I like complaingin uhhh
	//auto [node, nodevec] = stree->_root->CopyRecursive();
	//for (auto nd : nodevec)
	//	dtree->_nodes.insert(nd);
	//dtree->_sequenceNodes = stree->_sequenceNodes;


	if (sinput->IsTrimmed())
	{
		// nothing to extract and then extend
		if (sinput->GetTrimmedLength() - trackback < 1)
			return;
		std::vector<std::pair<int64_t, int64_t>> segments = { { 0, sinput->GetTrimmedLength() - trackback } };
		Extract(sinput->derive, dtree, segments, sinput->GetSequenceLength(), false);
		if (dtree->_valid == false)
			return;
	} else if (trackback > 0) {
		// nothing to extract and then extend
		if (sinput->GetSequenceLength() - trackback < 1)
			return;
		std::vector<std::pair<int64_t, int64_t>> segments = { { 0, sinput->GetSequenceLength() - trackback } };
		Extract(sinput->derive, dtree, segments, sinput->GetSequenceLength(), false);
		if (dtree->_valid == false)
			return;
	}
	else if (trackback == 0)
	{
		// ----- COPY THE SOURCE TREE -----
		dtree->_sequenceNodes = 0;
		std::stack<std::pair<DerivationTree::NonTerminalNode*, DerivationTree::NonTerminalNode*>> nodestack;
		if (stree->_root->Type() == DerivationTree::NodeType::Sequence) {
			//nnode = new DerivationTree::SequenceNode;
			nnode = allocSeq->New();
			dtree->_nodes++;
			nnode->_grammarID = ((DerivationTree::SequenceNode*)stree->_root)->_grammarID;
			dtree->_root = nnode;
			nodestack.push({ nnode, (DerivationTree::NonTerminalNode*)stree->_root });
		} else if (stree->_root->Type() == DerivationTree::NodeType::NonTerminal) {
			//nnode = new DerivationTree::NonTerminalNode;
			nnode = allocNonT->New();
			dtree->_nodes++;
			nnode->_grammarID = ((DerivationTree::NonTerminalNode*)stree->_root)->_grammarID;
			dtree->_root = nnode;
			nodestack.push({ nnode, (DerivationTree::NonTerminalNode*)stree->_root });
		} else {
			//ttnode = new DerivationTree::TerminalNode;
			ttnode = allocT->New();
			dtree->_nodes++;
			ttnode->_grammarID = ((DerivationTree::NonTerminalNode*)stree->_root)->_grammarID;
			dtree->_root = ttnode;
			ttnode->_content = _tree->_root->_identifier;
		}
		while (nodestack.size() > 0) {
			auto [dnode, snode] = nodestack.top();
			nodestack.pop();
			for (int32_t i = 0; i < (int32_t)snode->_children.size(); i++) {
				switch (snode->_children[i]->Type()) {
				case DerivationTree::NodeType::NonTerminal:
					//tnnode = new DerivationTree::NonTerminalNode;
					tnnode = allocNonT->New();
					tnnode->_grammarID = ((DerivationTree::NonTerminalNode*)snode->_children[i])->_grammarID;
					dnode->AddChild(tnnode);
					dtree->_nodes++;
					nodestack.push({ tnnode, (DerivationTree::NonTerminalNode*)snode->_children[i] });
					break;
				case DerivationTree::NodeType::Sequence:
					//tnnode = new DerivationTree::SequenceNode;
					tnnode = allocSeq->New();
					tnnode->_grammarID = ((DerivationTree::SequenceNode*)snode->_children[i])->_grammarID;
					dnode->AddChild(tnnode);
					dtree->_nodes++;
					dtree->_sequenceNodes++;
					nodestack.push({ tnnode, (DerivationTree::SequenceNode*)snode->_children[i] });
					break;
				case DerivationTree::NodeType::Terminal:
					//ttnode = new DerivationTree::TerminalNode;
					ttnode = allocT->New();
					ttnode->_grammarID = ((DerivationTree::TerminalNode*)snode->_children[i])->_grammarID;
					ttnode->_content = ((DerivationTree::TerminalNode*)snode->_children[i])->_content;
					dnode->AddChild(ttnode);
					dtree->_nodes++;
					break;
				}
			}
		}
	}

	// set backtracking done
	backtrackingdone = (int32_t)(sinput->derive->_sequenceNodes - dtree->_sequenceNodes);

	DerivationTree::NonTerminalNode* root;

	if (_tree->_simpleGrammar == false) {
		std::stack<DerivationTree::NonTerminalNode*> path;

		// find right-most path in the tree, that the one where we will be deleting stuff
		auto findRightPath = [&path](DerivationTree::NonTerminalNode* root) {
			DerivationTree::NonTerminalNode* tmp = root;
			while (tmp != nullptr) {
				if (tmp->_children.size() > 0) {
					for (int32_t i = (int32_t)tmp->_children.size() - 1; i >= 0; i--) {
						if (tmp->_children[i]->Type() == DerivationTree::NodeType::Sequence) {
							path.push((DerivationTree::SequenceNode*)tmp->_children[i]);
							tmp = nullptr;
							break;
						} else if (tmp->_children[i]->Type() == DerivationTree::NodeType::NonTerminal) {
							tmp = (DerivationTree::NonTerminalNode*)tmp->_children[i];
							path.push(tmp);
							break;
						} else {  // terminal
							continue;
						}
					}
				} else
					tmp = nullptr;
			}
		};

		// get the number of node children that are sequence nodes
		auto getSeqChildren = [](DerivationTree::NonTerminalNode* root) {
			int32_t i = 0;
			std::stack<DerivationTree::NonTerminalNode*> stack;
			DerivationTree::NonTerminalNode* tmp;
			stack.push(root);
			while (stack.size() > 0) {
				tmp = stack.top();
				stack.pop();
				if (tmp->Type() == DerivationTree::NodeType::Sequence)
					i++;
				else {
					for (auto node : tmp->_children) {
						if (node->Type() == DerivationTree::NodeType::Sequence)
							i++;
						else if (node->Type() == DerivationTree::NodeType::NonTerminal)
							stack.push((DerivationTree::NonTerminalNode*)node);
					}
				}
			}
			return i;
		};
		// delete this node and all its children
		auto deleteNode = [](DerivationTree::NonTerminalNode* root, std::shared_ptr<DerivationTree> tree) {
			std::stack<DerivationTree::NonTerminalNode*> stack;
			DerivationTree::NonTerminalNode* tmp;
			stack.push(root);
			while (stack.size() > 0) {
				tmp = stack.top();
				stack.pop();
				for (DerivationTree::Node* node : tmp->_children) {
					if (node->Type() == DerivationTree::NodeType::NonTerminal || node->Type() == DerivationTree::NodeType::Sequence)
						stack.push((DerivationTree::NonTerminalNode*)node);
					else {
						tree->_nodes--;
						delete (DerivationTree::TerminalNode*)node;
					}
				}
				if (tmp->Type() == DerivationTree::NodeType::Sequence) {
					tree->_nodes--;
					delete (DerivationTree::SequenceNode*)tmp;
				} else {
					tree->_nodes--;
					delete (DerivationTree::NonTerminalNode*)tmp;
				}
			}
		};

		// now that we have deleted what we had to delete, if there are still nodes in path, then we go from there
		// if that isn't the case we have to get the right-most path, cut the lowest sequence node, and backtrack until we find
		// a node that expands into multiple sequence nodes, and can get started from there

		// we cannot start on a sequence node
		while (path.size() > 0 && (path.top()->Type() == DerivationTree::NodeType::Sequence || path.top()->Type() == DerivationTree::NodeType::Terminal))
			path.pop();

		while (path.size() > 0)
			path.pop();
		bool found = false;
		while (found == false) {
			findRightPath((DerivationTree::NonTerminalNode*)dtree->_root);
			while (path.size() > 0 && found == false) {
				if (path.top()->Type() == DerivationTree::NodeType::NonTerminal) {
					auto itr = _tree->_hashmap.find(path.top()->_grammarID);
					if (itr != _tree->_hashmap.end()) {
						gnode = itr->second;
						// we are in business
						if (gnode->_expansions.size() > 0) {
							for (auto exp : gnode->_expansions) {
								// find one that has multiple nodes with ProduceSequence flag
								int32_t c = 60;
								for (auto nd : exp->_nodes)
									if (nd->_flags & GrammarNode::NodeFlags::ProduceSequence)
										c++;
								if (c > 1) {
									found = true;
									break;
								}
							}
						}
					}
				}
				if (found == false)
					path.pop();
			}
		}

		root = path.top();
		// delete path it takes up ram
		while (path.size() > 0)
			path.pop();
		//}
	}
	else
	{
		// well its a simple regex, so this stuff is simple
		root = (DerivationTree::NonTerminalNode*)dtree->_root;
		gnode = _tree->_root;
	}

	// count of generated sequence nodes
	int32_t seq = 0;
	// holds all nonterminals that were generated
	std::deque<std::pair<DerivationTree::NonTerminalNode*, std::shared_ptr<GrammarNode>>> qnonterminals;
	// holds all sequence nodes and sequence producing nodes during generation
	std::deque<std::pair<DerivationTree::NonTerminalNode*, std::shared_ptr<GrammarNode>>> qseqnonterminals;

	// begin: insert start node
	if (_tree->_simpleGrammar)
		root->_children.reserve(root->_children.size() + targetlength);
	if (gnode->_flags & GrammarNode::NodeFlags::ProduceSequence) {
		qseqnonterminals.push_back({ root, gnode });
	} else if (gnode->_flags & GrammarNode::NodeFlags::ProduceNonTerminals) {
		qnonterminals.push_back({ root, gnode });
	}

	// generated sequence nodes
	seq = 0;

	DeriveFromNode(dtree, qnonterminals, qseqnonterminals, randan, seq);

	dtree->_valid = true;
	dtree->SetRegenerate(true);
	dtree->_parent.method = DerivationTree::ParentTree::Method::Extension;

	profile(TimeProfiling, "Time taken for extension of length: {}", dtree->_sequenceNodes - sinput->Length());
}


void Grammar::Clear()
{
	Form::ClearForm();
	// remove tree shared_ptr so that grammar isn't valid anymore
	std::shared_ptr tr = _tree;
	_tree.reset();
	if (tr)
	{
		tr->Clear();
		tr.reset();
	}
	tr = _treeParse;
	_treeParse.reset();
	if (tr) {
		tr->Clear();
		tr.reset();
	}
	_extension_min = 0;
	_extension_max = 0;
	_backtrack_min = 0;
	_backtrack_max = 0;
}

void Grammar::RegisterFactories()
{
	if (!_registeredFactories) {
		_registeredFactories = !_registeredFactories;
	}
}

size_t Grammar::GetStaticSize(int32_t version)
{
	static size_t size0x1 = 4     // version
	                        + 8   // nextid
	                        + 4   // numcycles
	                        + 1   // valid
	                        + 8   // root id
	                        + 4   // _extension_min
	                        + 4   // _extension_max
	                        + 4   // _backtrack_min
	                        + 4;  // _backtrack_max
	static size_t size0x2 = size0x1 + 1;  // simpleGrammar
	switch (version)
	{
	case 0x1:
		return size0x1;
	case 0x2:
		return size0x2;
	default:
		return 0;
	}
}

size_t Grammar::GetDynamicSize()
{
	size_t sz = Form::GetDynamicSize()  // form stuff
	            + GetStaticSize(classversion) + Buffer::VectorBasic::GetVectorSize(_tree->_ruleorder);
	// hashmap
	sz += 8;
	for (auto& [id, node] : _tree->_hashmap)
	{
		sz += 8 + node->GetDynamicSize();
	}
	// hashmap_expansions
	sz += 8;
	for (auto& [id, expan] : _tree->_hashmap_expansions)
	{
		// flag regex or not / id / class
		sz += 1 + 8 + expan->GetDynamicSize(expan->GetClassVersion());
	}
	// nonterminals
	sz += 8 + _tree->_nonterminals.size() * 8;
	// terminals
	sz += 8 + _tree->_terminals.size() * 8;

	return sz;
}

bool Grammar::WriteData(std::ostream* buffer, size_t& offset)
{
	Buffer::Write(classversion, buffer, offset);
	Form::WriteData(buffer, offset);
	Buffer::Write(_tree->_nextid, buffer, offset);
	Buffer::Write(_tree->_numcycles, buffer, offset);
	Buffer::Write(_tree->_valid, buffer, offset);
	Buffer::Write(_tree->_root->_id, buffer, offset);
	Buffer::VectorBasic::WriteVector<uint64_t>(_tree->_ruleorder, buffer, offset);
	Buffer::WriteSize(_tree->_hashmap.size(), buffer, offset);
	for (auto& [id, node] : _tree->_hashmap)
	{
		Buffer::Write(id, buffer, offset);
		node->WriteData(buffer, offset);
	}
	Buffer::WriteSize(_tree->_hashmap_expansions.size(), buffer, offset);
	for (auto& [id, expan] : _tree->_hashmap_expansions)
	{
		if (expan->IsRegex())
			Buffer::Write(true, buffer, offset);
		else
			Buffer::Write(false, buffer, offset);
		Buffer::Write(id, buffer, offset);
		expan->WriteData(buffer, offset);
	}
	Buffer::WriteSize(_tree->_nonterminals.size(), buffer, offset);
	for (auto& node : _tree->_nonterminals)
		Buffer::Write(node->_id, buffer, offset);
	Buffer::WriteSize(_tree->_terminals.size(), buffer, offset);
	for (auto& node : _tree->_terminals)
		Buffer::Write(node->_id, buffer, offset);
	Buffer::Write(_extension_min, buffer, offset);
	Buffer::Write(_extension_max, buffer, offset);
	Buffer::Write(_backtrack_min, buffer, offset);
	Buffer::Write(_backtrack_max, buffer, offset);
	Buffer::Write(_tree->_simpleGrammar, buffer, offset);
	return true;
}

bool Grammar::ReadData0x1(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver, LoadResolverGrammar* lresolve)
{
	Form::ReadData(buffer, offset, length, resolver);
	_tree->_nextid = Buffer::ReadUInt64(buffer, offset);
	_tree->_numcycles = Buffer::ReadInt32(buffer, offset);
	_tree->_valid = Buffer::ReadBool(buffer, offset);
	auto& tmptree = _tree;
	uint64_t rootid = Buffer::ReadUInt64(buffer, offset);
	lresolve->AddTask([lresolve, rootid, tmptree]() {
		tmptree->SetRoot(lresolve->ResolveNodeID(rootid));
	});
	_tree->_ruleorder = Buffer::VectorBasic::ReadVector<uint64_t>(buffer, offset);
	{
		// hashmap
		size_t len = Buffer::ReadSize(buffer, offset);
		for (int64_t i = 0; i < (int64_t)len; i++) {
			uint64_t id = Buffer::ReadUInt64(buffer, offset);
			std::shared_ptr<GrammarNode> node = std::make_shared<GrammarNode>();
			node->ReadData(buffer, offset, length, lresolve);
			_tree->_hashmap.insert({ id, node });
		}
	}
	{
		// hashmap_expansions
		size_t len = Buffer::ReadSize(buffer, offset);
		for (int64_t i = 0; i < (int64_t)len; i++) {
			uint64_t id = Buffer::ReadUInt64(buffer, offset);
			std::shared_ptr<GrammarExpansion> expan = std::make_shared<GrammarExpansion>();
			expan->ReadData(buffer, offset, length, lresolve);
			_tree->_hashmap_expansions.insert({ id, expan });
		}
	}
	// hashmaps are initialized and all nodes are known so we can just find them in the hashmap if we need them
	{
		// nonterminals
		size_t len = Buffer::ReadSize(buffer, offset);
		for (int64_t i = 0; i < (int64_t)len; i++) {
			uint64_t id = Buffer::ReadUInt64(buffer, offset);
			if (auto ptr = _tree->_hashmap.at(id); ptr)
				_tree->_nonterminals.insert(ptr);
			else
				logcritical("cannot find nonterminal node {}", id);
		}
	}
	{
		// terminals
		size_t len = Buffer::ReadSize(buffer, offset);
		for (int64_t i = 0; i < (int64_t)len; i++) {
			uint64_t id = Buffer::ReadUInt64(buffer, offset);
			if (auto ptr = _tree->_hashmap.at(id); ptr)
				_tree->_terminals.insert(ptr);
			else
				logcritical("cannot find terminal node {}", id);
		}
	}
	_extension_min = Buffer::ReadInt32(buffer, offset);
	_extension_max = Buffer::ReadInt32(buffer, offset);
	_backtrack_min = Buffer::ReadInt32(buffer, offset);
	_backtrack_max = Buffer::ReadInt32(buffer, offset);

	lresolve->Resolve();
	delete lresolve;

	// create parse tree for parsing
	resolver->AddTask([this]() {
		// now construct tree for parsing
		_treeParse = std::make_shared<GrammarTree>();
		_tree->DeepCopy(_treeParse);
		_treeParse->InsertParseNodes();
	});
	return true;
}

bool Grammar::ReadData0x2(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver, LoadResolverGrammar* lresolve)
{
	Form::ReadData(buffer, offset, length, resolver);
	_tree->_nextid = Buffer::ReadUInt64(buffer, offset);
	_tree->_numcycles = Buffer::ReadInt32(buffer, offset);
	_tree->_valid = Buffer::ReadBool(buffer, offset);
	auto& tmptree = _tree;
	uint64_t rootid = Buffer::ReadUInt64(buffer, offset);
	lresolve->AddTask([lresolve, rootid, tmptree]() {
		tmptree->SetRoot(lresolve->ResolveNodeID(rootid));
	});
	_tree->_ruleorder = Buffer::VectorBasic::ReadVector<uint64_t>(buffer, offset);
	{
		// hashmap
		size_t len = Buffer::ReadSize(buffer, offset);
		for (int64_t i = 0; i < (int64_t)len; i++) {
			uint64_t id = Buffer::ReadUInt64(buffer, offset);
			std::shared_ptr<GrammarNode> node = std::make_shared<GrammarNode>();
			node->ReadData(buffer, offset, length, lresolve);
			_tree->_hashmap.insert({ id, node });
		}
	}
	{
		// hashmap_expansions
		size_t len = Buffer::ReadSize(buffer, offset);
		for (int64_t i = 0; i < (int64_t)len; i++) {
			bool regex = Buffer::ReadBool(buffer, offset);
			uint64_t id = Buffer::ReadUInt64(buffer, offset);
			std::shared_ptr<GrammarExpansion> expan;
			if (regex)
				expan = std::make_shared<GrammarExpansionRegex>();
			else
				expan = std::make_shared<GrammarExpansion>();
			expan->ReadData(buffer, offset, length, lresolve);
			_tree->_hashmap_expansions.insert({ id, expan });
		}
	}
	// hashmaps are initialized and all nodes are known so we can just find them in the hashmap if we need them
	{
		// nonterminals
		size_t len = Buffer::ReadSize(buffer, offset);
		for (int64_t i = 0; i < (int64_t)len; i++) {
			uint64_t id = Buffer::ReadUInt64(buffer, offset);
			if (auto ptr = _tree->_hashmap.at(id); ptr)
				_tree->_nonterminals.insert(ptr);
			else
				logcritical("cannot find nonterminal node {}", id);
		}
	}
	{
		// terminals
		size_t len = Buffer::ReadSize(buffer, offset);
		for (int64_t i = 0; i < (int64_t)len; i++) {
			uint64_t id = Buffer::ReadUInt64(buffer, offset);
			if (auto ptr = _tree->_hashmap.at(id); ptr)
				_tree->_terminals.insert(ptr);
			else
				logcritical("cannot find terminal node {}", id);
		}
	}
	_extension_min = Buffer::ReadInt32(buffer, offset);
	_extension_max = Buffer::ReadInt32(buffer, offset);
	_backtrack_min = Buffer::ReadInt32(buffer, offset);
	_backtrack_max = Buffer::ReadInt32(buffer, offset);
	_tree->_simpleGrammar = Buffer::ReadBool(buffer, offset);

	lresolve->Resolve();
	delete lresolve;

	// create parse tree for parsing
	resolver->AddTask([this]() {
		// now construct tree for parsing
		_treeParse = std::make_shared<GrammarTree>();
		_tree->DeepCopy(_treeParse);
		_treeParse->InsertParseNodes();
	});
	return true;
}

bool Grammar::ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver)
{
	int32_t version = Buffer::ReadInt32(buffer, offset);
	_tree = std::make_shared<GrammarTree>();
	LoadResolverGrammar* lresolve = new LoadResolverGrammar();
	lresolve->_tree = _tree;
	switch (version) {
	case 0x1:
		{
			return ReadData0x1(buffer, offset, length, resolver, lresolve);
		}
		break;
	case 0x2:
		{
			return ReadData0x2(buffer, offset, length, resolver, lresolve);
		}
		break;
	default:
		return false;
	}
}

void Grammar::Delete(Data*)
{
	Clear();
}

size_t Grammar::MemorySize()
{
	return sizeof(Grammar) + (_tree ? _tree->MemorySize() : 0) + (_treeParse ? _treeParse->MemorySize() : 0);
}


LoadResolverGrammar::~LoadResolverGrammar()
{
	_tree.reset();
	while (!_tasks.empty()) {
		_tasks.front()->Dispose();
		_tasks.pop();
	}
}

void LoadResolverGrammar::AddTask(TaskFn a_task)
{
	AddTask(new Task(std::move(a_task)));
}

void LoadResolverGrammar::AddTask(TaskDelegate* a_task)
{
	{
		std::unique_lock<std::mutex> guard(_lock);
		_tasks.push(a_task);
	}
}

LoadResolverGrammar::Task::Task(TaskFn&& a_fn) :
	_fn(std::move(a_fn))
{}

void LoadResolverGrammar::Task::Run()
{
	_fn();
}

void LoadResolverGrammar::Task::Dispose()
{
	delete this;
}

std::shared_ptr<GrammarNode> LoadResolverGrammar::ResolveNodeID(uint64_t id)
{
	auto itr = _tree->_hashmap.find(id);
	if (itr != _tree->_hashmap.end()) {
		return std::get<1>(*itr);
	} else
		logcritical("cannot resolve nodeid {}", id);
	return {};
}

std::shared_ptr<GrammarExpansion> LoadResolverGrammar::ResolveExpansionID(uint64_t id)
{
	auto itr = _tree->_hashmap_expansions.find(id);
	if (itr != _tree->_hashmap_expansions.end()) {
		return std::get<1>(*itr);
	} else
		logcritical("cannot resolve expansionid {}", id);
	return {};
}

void LoadResolverGrammar::Resolve()
{
	while (!_tasks.empty()) {
		TaskDelegate* del;
		del = _tasks.front();
		_tasks.pop();
		del->Run();
		del->Dispose();
	}
}
