#include "Grammar.h"
#include "Logging.h"
#include "Utility.h"
#include "BufferOperations.h"
#include "DerivationTree.h"

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

bool GrammarNode::WriteData(unsigned char* buffer, size_t& offset)
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

bool GrammarNode::ReadData(unsigned char* buffer, size_t& offset, size_t /*length*/, LoadResolverGrammar* resolver)
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
				exp.push_back(Buffer::ReadUInt64(buffer, offset));
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

bool GrammarExpansion::WriteData(unsigned char* buffer, size_t& offset)
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

bool GrammarExpansion::ReadData(unsigned char* buffer, size_t& offset, size_t /*length*/, LoadResolverGrammar* resolver)
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

	// iterate over all non-terminals and initialize them
	for (auto node : _nonterminals) {
		if (node->IsValid() == false) {
			_hashmap.insert({ node->_id, node });
			// extract the alternative derivations
			std::vector<std::string> alternatives = Utility::SplitString(node->_derivation, '|', true, true, '\"');
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
					// if the rule is a terminal get the terminal and create a node
					else {
						std::string iden = productions[i];
						Utility::RemoveSymbols(iden, '\"', '\\');
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
	// if we are starting of from a new non-recursive function call, delete prior finished ids
	std::chrono::steady_clock::time_point begin;
	if (reset) {
		loginfo("enter");
		finished_ids.clear();
		begin = std::chrono::steady_clock::now();
	}
	// this node is reachable
	node->_reachable = true;
	// add node to path
	path.insert(node->_id);
	// if this is a terminal, it was reached and it is producing
	if (node->IsLeaf()) {
		node->_producing = true;
		finished_ids.insert(node->_id);
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

	while (stack.size() > 0)
	{
		auto ptr = stack.top();
		stack.pop();
		if (ptr->GetObjectType() == GrammarObject::Type::GrammarNode) {
			GrammarNode* node = (GrammarNode*)ptr;
			if (node->_producing == false || node->_remove || node->_reachable == false || pruneall == true)
			{
				for (auto parent : node->_parents)
				{
					parent->_remove = true;
					stack.push(parent.get());
				}
				node->_parents.clear();
				node->_remove = true;
				toremove.insert(node->_id);
				for (auto expansion : node->_expansions)
				{
					expansion->_remove = true;
					stack.push(expansion.get());
				}
				node->_expansions.clear();
				if (node->_producing == false) {
					logwarn("Node is not producing and has been removed: {}", node->string());
				} else if (node->_reachable == false) {
					logwarn("Node is not reachable and has been removed: {}", node->string());
				}	else
					logwarn("Node has been marked for removal: {}", node->string());
			}
			else
			{
				// enter all nodes into touched, so that we do not run into cycles
				for (auto exp : node->_expansions)
				{
					if (touched.contains(exp->_id) == false)
					{
						stack.push(exp.get());
						touched.insert(exp->_id);
						logdebug("Enter expansion to stack: {}", exp->operator std::string());
					}
				}
			}
		}
		else if (ptr->GetObjectType() == GrammarObject::Type::GrammarExpansion)
		{
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
				for (auto node : exp->_nodes)
				{
					// if node is already flagged remove, its either in the stack or already processed
					if (node && node->_remove == false)
					{
						// remove expansion are parent from the node
						eraseasparent(node);
						// if node doesn't have any more parent, remove it
						if (node->_parents.size() == 0)
						{
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
			}
			else
			{
				// enter all nodes onto stack
				for (auto node : exp->_nodes) {
					if (touched.contains(node->_id) == false) {
						stack.push(node.get());
						logdebug("Enter node to stack: {}", node->operator std::string());
					}
				}
			}
		}
	}

	// now that the tree itself has been pruned, remove all nodes and expansions to be removed
	for (auto [id, node] : _hashmap)
	{
		if (toremove.contains(id))
		{
			_hashmap.erase(id);
			_terminals.erase(node);
			_nonterminals.erase(node);
			if (node.use_count() != 1)
				logcritical("MEMORY LEAK: Use_Count of Node {} is not 1", node->string());
		}
	}
	for (auto [id, exp] : _hashmap_expansions) {
		if (toremove.contains(id)) {
			_hashmap.erase(id);
			if (exp.use_count() != 1)
				logcritical("MEMORY LEAK: Use_Count of Expansion {} is not 1", exp->operator std::string());
		}
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
		
		logdebug("constructed grammar");

		if (_tree->IsValid()) {
			loginfo("Successfully read the grammar from file: {}", path.string());
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

std::string Grammar::Scala()
{
	if (_tree)
		return _tree->Scala();
	else
		return "Grammar()";
}

Grammar::~Grammar()
{
	Clear();
}


void Grammar::Derive(std::shared_ptr<DerivationTree> dtree, int32_t sequence, uint32_t seed, int32_t /*maxsteps*/)
{
	// this function takes an empty derivation tree and a goal of nonterminals to produce
	// this function will step-by-step expand from the start symbol to more and more nonterminals
	// until the goal is reached
	// afterwards all nonterminals are expanded until there are only terinal nodes remaining
	// at last the terminal nodes are resolved
	dtree->_grammarID = _formid;
	dtree->_seed = seed;
	dtree->_targetlen = sequence;
	DerivationTree::NonTerminalNode* nnode = nullptr;
	DerivationTree::TerminalNode* ttnode = nullptr;
	DerivationTree::NonTerminalNode* tnnode = nullptr;
	int32_t idx = 0;
	int32_t cnodes = 0;
	float cweight = 0.f;
	std::shared_ptr<GrammarNode> gnode;
	std::shared_ptr<GrammarExpansion> gexp;

	// init random stuff
	std::mt19937 randan((unsigned int)seed);
	//std::uniform_int_distribution<signed> butdist(0, 1);

	// count of generated sequence nodes
	int32_t seq = 0;
	// holds all nonterminals that were generated
	std::deque<std::pair<DerivationTree::NonTerminalNode*, std::shared_ptr<GrammarNode>>> qnonterminals;
	// holds all sequence nodes and sequence producing nodes during generation
	std::deque<std::pair<DerivationTree::NonTerminalNode*, std::shared_ptr<GrammarNode>>> qseqnonterminals;

	// begin: insert start node 
	if (_tree->_root->_flags & GrammarNode::NodeFlags::ProduceSequence) {
		nnode = new DerivationTree::NonTerminalNode;
		dtree->_nodes.insert(nnode);
		nnode->_grammarID = _tree->_root->_id;
		dtree->_root = nnode;
		qseqnonterminals.push_back({ nnode, _tree->_root });
	} else if (_tree->_root->_flags & GrammarNode::NodeFlags::ProduceNonTerminals) {
		nnode = new DerivationTree::NonTerminalNode;
		dtree->_nodes.insert(nnode);
		nnode->_grammarID = _tree->_root->_id;
		dtree->_root = nnode;
		qnonterminals.push_back({ nnode, _tree->_root });
	} else {
		ttnode = new DerivationTree::TerminalNode;
		dtree->_nodes.insert(ttnode);
		ttnode->_grammarID = _tree->_root->_id;
		dtree->_root = ttnode;
		ttnode->_content = _tree->_root->_identifier;
	}

	bool flip = false;

	// in the first loop, we will expand the non terminals and sequence non terminals such that we only expand 
	// nodes that can produce new sequence nodes and only apply expansions that produce new sequence nodes
	while (seq < sequence && qseqnonterminals.size() > 0) {
		// expand the current valid sequence nonterminals by applying rules that may produce sequences
		int32_t nonseq = (int32_t)qseqnonterminals.size(); // number of non terminals that will be handled this iteration
		for (int c = 0; c < nonseq; c++)
		{
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
				for (int32_t i = 0; i < (int32_t)gnode->_expansions.size(); i++)
				{
					// as long as we don't use weighted rules, the one with the most sequence symbols is used
					if (cnodes < gnode->_expansions[i]->_seqnonterminals && cweight == 0)
					{
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
					}
					else if (cweight < gnode->_expansions[i]->_weight)
					{
						cnodes = gnode->_expansions[i]->_seqnonterminals;
						cweight = gnode->_expansions[i]->_weight;
						idx = i;
					}
				}
			// if there is no rule that directly produces sequence nodes, choose a random expansion that produces sequence nodes
			if (idx == -1)
			{
				std::vector<int32_t> exp;
				float tweight = 0.f;
				for (int32_t i = 0; i < (int32_t)gnode->_expansions.size(); i++)
				{
					if (gnode->_expansions[i]->_flags & GrammarNode::NodeFlags::ProduceSequence) {
						exp.push_back(i);
						tweight += gnode->_expansions[i]->_weight;
					}
				}
				if (exp.size() > 0)
				{
					if (tweight == 0.f) {
						std::uniform_int_distribution<signed> dist(0, (int32_t)exp.size() - 1);
						idx = exp[dist(randan)];
					} else {
						std::uniform_int_distribution<signed> dist(0, 100000000);
						float target = (float)dist(randan) / 100000000.f;
						float current = 0.f;
						for (int32_t i = 0; i < (int32_t)exp.size(); i++)
						{
							if (current += gnode->_expansions[exp[i]]->_weight; current >= target) {
								idx = exp[i];
								break;
							}
						}
					}
				}
				else if (gnode->IsSequence())
				{
					// this node is a sequence non terminal but doesn't have any rules that expand into new sequences
					// so we just add it to the regular nonterminals we will deal with later
					qnonterminals.push_back({ nnode, gnode });
					continue;
				}else
				{
					logcritical("Error during Derivation: Produce Sequence Flag set on node, but no expansions produce sequences.");
					return;
				}
			}
			// create derivation nodes and add them to the derivation tree
			gexp = gnode->_expansions[idx];
			for (int32_t i = 0; i < (int32_t)gexp->_nodes.size(); i++)
			{
				switch (gexp->_nodes[i]->_type) {
				case GrammarNode::NodeType::Sequence:
					seq++;
					tnnode = new DerivationTree::SequenceNode;
					dtree->_nodes.insert(tnnode);
					dtree->_sequenceNodes++;
					tnnode->_grammarID = gexp->_nodes[i]->_id;
					nnode->_children.push_back(tnnode);
					if (gexp->_nodes[i]->_flags & GrammarNode::NodeFlags::ProduceSequence)
						qseqnonterminals.push_back({ tnnode, gexp->_nodes[i] });
					else
						qnonterminals.push_back({ tnnode, gexp->_nodes[i] });
					break;
				case GrammarNode::NodeType::NonTerminal:
					tnnode = new DerivationTree::NonTerminalNode;
					dtree->_nodes.insert(tnnode);
					tnnode->_grammarID = gexp->_nodes[i]->_id;
					nnode->_children.push_back(tnnode);
					if (gexp->_nodes[i]->_flags & GrammarNode::NodeFlags::ProduceSequence)
						qseqnonterminals.push_back({ tnnode, gexp->_nodes[i] });
					else
						qnonterminals.push_back({ tnnode, gexp->_nodes[i] });
					break;
				case GrammarNode::NodeType::Terminal:
					// create new terminal node
					ttnode = new DerivationTree::TerminalNode();
					dtree->_nodes.insert(ttnode);
					ttnode->_grammarID = gexp->_nodes[i]->_id;
					ttnode->_content = gexp->_nodes[i]->_identifier;
					nnode->_children.push_back(ttnode);
					break;
				}
			}
		}
		flip = true;
	}

	// in the second loop we expand all seq-nonterminals until non remain, while favoring expansions that 
	// do not produce new sequences
	while (qseqnonterminals.size() > 0)
	{
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
				float target = (float)dist(randan) / 100000000.f;
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
				float target = (float)dist(randan) / 100000000.f;
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
				tnnode = new DerivationTree::SequenceNode;
				dtree->_nodes.insert(tnnode);
				dtree->_sequenceNodes++;
				tnnode->_grammarID = gexp->_nodes[i]->_id;
				nnode->_children.push_back(tnnode);
				qnonterminals.push_back({ tnnode, gexp->_nodes[i] });
				break;
			case GrammarNode::NodeType::NonTerminal:
				tnnode = new DerivationTree::NonTerminalNode;
				dtree->_nodes.insert(tnnode);
				tnnode->_grammarID = gexp->_nodes[i]->_id;
				nnode->_children.push_back(tnnode);
				qnonterminals.push_back({ tnnode, gexp->_nodes[i] });
				break;
			case GrammarNode::NodeType::Terminal:
				// create new terminal node
				ttnode = new DerivationTree::TerminalNode();
				dtree->_nodes.insert(ttnode);
				ttnode->_grammarID = gexp->_nodes[i]->_id;
				ttnode->_content = gexp->_nodes[i]->_identifier;
				nnode->_children.push_back(ttnode);
				break;
			}
		}
	}

	// in the third loop we expand all non-terminals, favoring rules that do not increase the sequence nodes,
	// until only terminal nodes remain
	while (qnonterminals.size() > 0) 
	{
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
			float target = (float)dist(randan) / 100000000.f;
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
				tnnode = new DerivationTree::SequenceNode;
				dtree->_nodes.insert(tnnode);
				dtree->_sequenceNodes++;
				tnnode->_grammarID = gexp->_nodes[i]->_id;
				nnode->_children.push_back(tnnode);
				qnonterminals.push_back({ tnnode, gexp->_nodes[i] });
				break;
			case GrammarNode::NodeType::NonTerminal:
				tnnode = new DerivationTree::NonTerminalNode;
				dtree->_nodes.insert(tnnode);
				tnnode->_grammarID = gexp->_nodes[i]->_id;
				nnode->_children.push_back(tnnode);
				qnonterminals.push_back({ tnnode, gexp->_nodes[i] });
				break;
			case GrammarNode::NodeType::Terminal:
				// create new terminal node
				ttnode = new DerivationTree::TerminalNode();
				dtree->_nodes.insert(tnnode);
				ttnode->_grammarID = gexp->_nodes[i]->_id;
				ttnode->_content = gexp->_nodes[i]->_identifier;
				nnode->_children.push_back(ttnode);
				break;
			}
		}
	}
	dtree->_valid = true;
	dtree->_regenerate = true;
}


void Grammar::Clear()
{
	// remove tree shared_ptr so that grammar isn't valid anymore
	std::shared_ptr tr = _tree;
	_tree.reset();
	if (tr)
	{
		tr->Clear();
		tr.reset();
	}
}

void Grammar::RegisterFactories()
{
	if (!_registeredFactories) {
		_registeredFactories = !_registeredFactories;
	}
}

size_t Grammar::GetStaticSize(int32_t version)
{
	static size_t size0x1 = Form::GetDynamicSize()  // form base size
	                        + 4                     // version
	                        + 8                     // nextid
	                        + 4                     // numcycles
	                        + 1                     // valid
	                        + 8;                    // root id
	static size_t size0x2 = size0x1;
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
	size_t sz = GetStaticSize() + Buffer::VectorBasic::GetVectorSize(_tree->_ruleorder);
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
		sz += 8 + expan->GetDynamicSize(expan->classversion);
	}
	// nonterminals
	sz += 8 + _tree->_nonterminals.size() * 8;
	// terminals
	sz += 8 + _tree->_terminals.size() * 8;

	return sz;
}

bool Grammar::WriteData(unsigned char* buffer, size_t& offset)
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
		Buffer::Write(id, buffer, offset);
		expan->WriteData(buffer, offset);
	}
	Buffer::WriteSize(_tree->_nonterminals.size(), buffer, offset);
	for (auto& node : _tree->_nonterminals)
		Buffer::Write(node->_id, buffer, offset);
	Buffer::WriteSize(_tree->_terminals.size(), buffer, offset);
	for (auto& node : _tree->_terminals)
		Buffer::Write(node->_id, buffer, offset);
	return true;
}

bool Grammar::ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver)
{
	int32_t version = Buffer::ReadInt32(buffer, offset);
	_tree = std::make_shared<GrammarTree>();
	LoadResolverGrammar* lresolve = new LoadResolverGrammar();
	lresolve->_tree = _tree;
	switch (version) {
	case 0x1:
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
				for (int64_t i = 0; i < (int64_t)len; i++)
				{
					uint64_t id = Buffer::ReadUInt64(buffer, offset);
					std::shared_ptr<GrammarNode> node = std::make_shared<GrammarNode>();
					node->ReadData(buffer, offset, length, lresolve);
					_tree->_hashmap.insert({ id, node });
				}
			}
			{
				// hashmap_expansions
				size_t len = Buffer::ReadSize(buffer, offset);
				for (int64_t i = 0; i < (int64_t)len; i++)
				{
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
				for (int64_t i = 0; i < (int64_t)len; i++)
				{
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
				for (int64_t i = 0; i < (int64_t)len; i++)
				{
					uint64_t id = Buffer::ReadUInt64(buffer, offset);
					if (auto ptr = _tree->_hashmap.at(id); ptr)
						_tree->_terminals.insert(ptr);
					else
						logcritical("cannot find terminal node {}", id);
				}
			}
			lresolve->Resolve();
			delete lresolve;
			return true;
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
