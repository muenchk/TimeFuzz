#include "Grammar.h"
#include "Logging.h"
#include "Utility.h"

#include <stack>
bool GrammarNode::IsLeaf()
{
	return type == NodeType::Terminal;
}

bool GrammarNode::IsSequence()
{
	return type == NodeType::Sequence;
}

bool GrammarNode::IsValid()
{
	return identifier.empty() == false && derivation.empty() && (IsLeaf() && expansions.size() == 0 || !IsLeaf() && expansions.size() > 0);
}

GrammarNode::operator std::string()
{
	switch (type)
	{
	case NodeType::Terminal:
		return "T_" + identifier;
	case NodeType::NonTerminal:
		return "NT_" + identifier;
	case NodeType::Sequence:
		return "SEQ_" + identifier;
	}
}

std::string GrammarNode::string()
{
	return this->operator std::string();
}

GrammarObject::Type GrammarNode::GetObjectType()
{
	return Type::GrammarNode;
}

GrammarExpansion::operator std::string()
{
	std::string ret;
	for (int i = 0; i < nodes.size(); i++)
	{
		if (i == nodes.size() - 1)
			ret += nodes[i]->string();
		else
			ret += nodes[i]->string() + " ~ ";
	}
	return ret;
}

GrammarObject::Type GrammarExpansion::GetObjectType()
{
	return Type::GrammarExpansion;
}



GrammarTree::GrammarTree()
{

}

void GrammarTree::SetRoot(std::string symbol, std::string derivation)
{
	// erase old root if there was one
	nonterminals.erase(root);
	root.reset();

	// remove whitespaces from symbol and identifiers, since they are superfluous
	Utility::RemoveWhiteSpaces(symbol, '\"', true);
	Utility::RemoveWhiteSpaces(derivation, '\"', true);

	// create new root node
	root = std::make_shared<GrammarNode>();
	root->derivation = derivation;
	root->identifier = symbol;
	root->type = GrammarNode::NodeType::Terminal;
	nonterminals.insert(root);
}

void GrammarTree::AddSymbol(std::string symbol, std::string derivation)
{
	// remove whitespaces from symbol and identifiers, since they are superfluous
	Utility::RemoveWhiteSpaces(symbol, '\"', true);
	Utility::RemoveWhiteSpaces(derivation, '\"', true);

	// create node
	std::shared_ptr<GrammarNode> node = std::make_shared<GrammarNode>();
	node->derivation = derivation;
	node->identifier = symbol;
	node->type = GrammarNode::NodeType::NonTerminal;
	nonterminals.insert(node);
}

void GrammarTree::AddSequenceSymbol(std::string symbol, std::string derivation)
{
	// remove whitespaces from symbol and identifiers, since they are superfluous
	Utility::RemoveWhiteSpaces(symbol, '\"', true);
	Utility::RemoveWhiteSpaces(derivation, '\"', true);

	// create node
	std::shared_ptr<GrammarNode> node = std::make_shared<GrammarNode>();
	node->derivation = derivation;
	node->identifier = symbol;
	node->type = GrammarNode::NodeType::Sequence;
	nonterminals.insert(node);
}

std::shared_ptr<GrammarNode> GrammarTree::FindNode(std::string identifier)
{
	for (auto& node : nonterminals) {
		if (node->identifier == identifier)
			return node;
	}
	return {};
}

void GrammarTree::Construct()
{
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
	for (auto node : nonterminals) {
		if (node->IsValid() == false) {
			node->id = GetNextID();
			hashmap.insert({ node->id, node });
			// extract the alternative derivations
			std::vector<std::string> alternatives = Utility::SplitString(node->derivation, '|', true, true, '\"');
			for (std::string alter : alternatives) {
				// go over the derivations and get all concated productions
				std::vector<std::string> productions = Utility::SplitString(alter, '~', true, true, '\"');
				std::shared_ptr<GrammarExpansion> expansion = std::make_shared<GrammarExpansion>();
				float weight = 0.0f;
				for (int i = 0; i < productions.size(); i++) {
					// if the rule is a weight, get the weight
					if (float wgt = GetWeight(productions[i]); wgt != -1.0f)
						weight = wgt;
					// check for non-terminal symbol
					else if (size_t pos = productions[i].find("\'"); pos != std::string::npos) {
						auto newnode = FindNode(productions[i]);
						if (newnode) {
							expansion->nodes.push_back(newnode);
							newnode->parents.insert(expansion);
						}
						else
							logcritical("Cannot find unknown Symbol: {}", newnode->identifier);
					}
					// if the rule is a terminal get the terminal and create a node
					else {
						std::string iden = productions[i];
						Utility::RemoveSymbols(iden, '\"');
						auto tnode = std::make_shared<GrammarNode>();
						tnode->derivation = "";
						tnode->identifier = iden;
						tnode->type = GrammarNode::NodeType::Terminal;
						tnode->flags = GrammarNode::NodeFlags::ProduceTerminals;
						tnode->id = GetNextID();
						tnode->producing = true;
						tnode->parents.insert(expansion);
						terminals.insert(tnode);
						expansion->nodes.push_back(tnode);
						hashmap.insert({ tnode->id, tnode });
					}
				}
				expansion->weight = weight;
				expansion->id = GetNextID();
				expansion->parent = node;
				node->expansions.push_back(expansion);
				hashmap_expansions.insert({ expansion->id, expansion });
			}
			if (node->IsValid() == false)
				logcritical("Cannot fully initialize grammar node: {}. Expansions: {}, Flags: {}, Type: {}", node->identifier, node->expansions.size(), Utility::GetHex(node->flags), Utility::GetHex((EnumType)node->type));
		}
	}

	// gather all flags for all tree nodes
	GatherFlags(root, {}, true);

	// prune tree to valid expansion and nodes
	Prune();




	// check whether there still is a grammar that we can work with





}


uint64_t GrammarTree::GetNextID()
{
	return ++nextid;
}

float GrammarTree::GetWeight(std::string production)
{
	// the weight is specified insight a special variable of form 'WGT_0.5
	if (size_t pos = production.find("\'WGT_"); pos != std::string::npos) {
		std::string wgt = production.substr(pos + 5, production.length() - (pos + 5));
		try {
			return std::stof(wgt);
		} catch (std::exception& e) {
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
		finished_ids.clear();
		begin = std::chrono::steady_clock::now();
	}
	// this node is reachable
	node->reachable = true;
	// add node to path
	path.insert(node->id);
	// if this is a terminal, it was reached and it is producing
	if (node->IsLeaf()) {
		node->producing = true;
		finished_ids.insert(node->id);
		return;
	}
	std::set<std::shared_ptr<GrammarExpansion>> skip;
	// otherwise this is a
	for (auto expansion : node->expansions) {
		if (finished_ids.contains(expansion->id) == false) {
			if (path.contains(expansion->id)) {
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
	for (int i = 0; i < node->expansions.size(); i++) {
		if (skip.contains(node->expansions[i]))  // skip cycling expansions
			continue;
		node->producing |= node->expansions[i]->producing;
		node->flags |= node->expansions[i]->flags;
	}
	finished_ids.insert(node->id);

	if (reset)
	{
		profile(TimeProfiling, "function execution time");
	}
}

void GrammarTree::GatherFlags(std::shared_ptr<GrammarExpansion> expansion, std::set<uint64_t> path)
{
	// add expansion to path
	path.insert(expansion->id);
	// the expansion is only producing if and only if ALL its nodes are also producing
	expansion->producing = true;
	for (int i = 0; i < expansion->nodes.size(); i++) {
		if (finished_ids.contains(expansion->nodes[i]->id) == false) {
			GatherFlags(expansion->nodes[i], path);
		}
		expansion->producing &= expansion->nodes[i]->producing;
		expansion->flags |= expansion->nodes[i]->flags;
		switch (expansion->nodes[i]->type)
		{
		case GrammarNode::NodeType::NonTerminal:
			expansion->flags |= GrammarNode::NodeFlags::ProduceNonTerminals;
			break;
		case GrammarNode::NodeType::Sequence:
			expansion->flags |= GrammarNode::NodeFlags::ProduceSequence;
			break;
		case GrammarNode::NodeType::Terminal:
			expansion->flags |= GrammarNode::NodeFlags::ProduceTerminals;
			break;
		}
	}
	finished_ids.insert(expansion->id);
}

void GrammarTree::Prune()
{
	StartProfiling;
	std::set<uint64_t> toremove;
	// we can use these shenanigans, since all nodes and expansions are present in hashmap and hashmap_expansions, so they won't be deleted
	std::set<uint64_t> touched;
	std::stack<GrammarObject*> stack;
	// begin with root
	stack.push(root.get());

	// now add all non-reachable node into the stack
	for (auto node : nonterminals)
		if (node->reachable == false)
			stack.push(node.get());
	for (auto node : terminals)
		if (node->reachable == false)
			stack.push(node.get());

	while (stack.size() > 0)
	{
		auto ptr = stack.top();
		stack.pop();
		if (ptr->GetObjectType() == GrammarObject::Type::GrammarNode) {
			GrammarNode* node = (GrammarNode*)ptr;
			if (node->producing == false || node->remove || node->reachable == false)
			{
				for (auto parent : node->parents)
				{
					parent->remove = true;
					stack.push(parent.get());
				}
				node->parents.clear();
				node->remove = true;
				toremove.insert(node->id);
				for (auto expansion : node->expansions)
				{
					expansion->remove = true;
					stack.push(expansion.get());
				}
				node->expansions.clear();
				if (node->producing == false) {
					logwarn("Node is not producing and has been removed: {}", node->string());
				} else if (node->reachable == false) {
					logwarn("Node is not reachable and has been removed: {}", node->string());
				}	else
					logwarn("Node has been marked for removal: {}", node->string());
			}
			else
			{
				// enter all nodes into touched, so that we do not run into cycles
				for (auto exp : node->expansions)
				{
					if (touched.contains(exp->id) == false)
					{
						stack.push(exp.get());
						touched.insert(exp->id);
						logdebug("Enter expansion to stack: {}", exp->operator std::string());
					}
				}
			}
		}
		else if (ptr->GetObjectType() == GrammarObject::Type::GrammarExpansion)
		{
			GrammarExpansion* exp = (GrammarExpansion*)ptr;

			auto erasefromparent = [exp]() {
				if (exp->parent) {
					auto itr = exp->parent->expansions.begin();
					while (itr != exp->parent->expansions.end()) {
						if ((*itr).get() == exp) {
							exp->parent->expansions.erase(itr);
							break;
						}
						itr++;
					}
				}
			};
			auto eraseasparent = [exp](std::shared_ptr<GrammarNode> node) {
				if (node) {
					auto itr = node->parents.begin();
					while (itr != node->parents.end()) {
						if ((*itr).get() == exp) {
							node->parents.erase(itr);
							break;
						}
						itr++;
					}
				}
			};

			if (exp->remove || exp->producing == false) {
				toremove.insert(exp->id);
				// we have been set to be removed
				erasefromparent();
				for (auto node : exp->nodes)
				{
					// if node is already flagged remove, its either in the stack or already processed
					if (node && node->remove == false)
					{
						// remove expansion are parent from the node
						eraseasparent(node);
						// if node doesn't have any more parent, remove it
						if (node->parents.size() == 0)
						{
							node->remove = true;
							stack.push(node.get());
						}
					}
				}
				if (exp->producing == false) {
					logwarn("Expansion is not producing and has been removed: {}", exp->operator std::string());
				} else if (exp->remove) {
					logwarn("Expansion has been marked for removal: {}", exp->operator std::string());
				}
			}
			else
			{
				// enter all nodes onto stack
				for (auto node : exp->nodes) {
					if (touched.contains(node->id) == false) {
						stack.push(node.get());
						logdebug("Enter node to stack: {}", node->operator std::string());
					}
				}
			}
		}
	}

	// now that the tree itself has been pruned, remove all nodes and expansions to be removed
	for (auto [id, node] : hashmap)
	{
		if (toremove.contains(id))
		{
			hashmap.erase(id);
			terminals.erase(node);
			nonterminals.erase(node);
			if (node.use_count() != 1)
				logcritical("MEMORY LEAK: Use_Count of Node {} is not 1", node->string());
		}
	}
	for (auto [id, exp] : hashmap_expansions) {
		if (toremove.contains(id)) {
			hashmap.erase(id);
			if (exp.use_count() != 1)
				logcritical("MEMORY LEAK: Use_Count of Expansion {} is not 1", exp->operator std::string());
		}
	}

	profile(TimeProfiling, "function execution time");
}
