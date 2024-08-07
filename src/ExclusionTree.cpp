#include "ExclusionTree.h"
#include "Input.h"

ExclusionTree::TreeNode* ExclusionTree::TreeNode::HasChild(std::string str)
{
	for (int32_t i = 0; i < children.size(); i++) {
		if (children[i] && children[i]->identifier == str)
			return children[i];
	}
	return nullptr;
}

ExclusionTree::~ExclusionTree()
{
	DeleteChildren(&root);
	root = TreeNode();
}

void ExclusionTree::AddInput(std::shared_ptr<Input> input)
{
	if (input.get() == nullptr)
		return;
	TreeNode* node = &root;
	auto itr = input->begin();
	while (itr != input->end()) {
		if (auto child = node->HasChild(*itr); child != nullptr) {
			// if the current node already has the required child,
			// choose it as our node and go to the next input entry
			if (child->isLeaf)
				return;  // the input is already excluded
			node = child;
		} else {
			// if there is no matching child, create one.
			TreeNode* newnode = new TreeNode;
			newnode->identifier = *itr;
			node->children.push_back(newnode);
			node = newnode;
		}
		itr++;
	}
	// The result of the inputs saved to this tree is fixed and any input that contains
	// it as a prefix yields the same result.
	// Thus we can remove all children from our current node, as they would yield the same result.
	node->isLeaf = true;
	DeleteChildren(node);
}

bool ExclusionTree::HasPrefix(std::shared_ptr<Input> input)
{
	if (input.get() == nullptr)
		return false;
	TreeNode* node = &root;
	auto itr = input->begin();
	while (itr != input->end()) {
		if (auto child = node->HasChild(*itr); child != nullptr) {
			if (child->isLeaf)
				return true;  // if the child is a leaf, then we have found an excluded prefix
			node = child;
		} else {
			// there is no matching child, so the input does not have an excluded prefix
			return false;
		}
		itr++;
	}
	return false;
}

void ExclusionTree::DeleteChildren(TreeNode* node)
{
	// delete all children
	for (int32_t i = 0; i < node->children.size(); i++) {
		DeleteChildren(node->children[i]);
	}
	node->children.clear();
}

void ExclusionTree::DeleteNode(TreeNode* node)
{
	// delete all children of this node
	for (int32_t i = 0; i < node->children.size(); i++) {
		DeleteNode(node->children[i]);
	}
	node->children.clear();
	// delete the node
	delete node;
}
