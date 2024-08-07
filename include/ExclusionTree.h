#pragma once

#include <memory>
#include <vector>
#include <string>

#include "Input.h"

/// <summary>
/// this class holds information about prefixes that are excluded
/// </summary>
class ExclusionTree
{
	struct TreeNode
	{
		std::string identifier;
		std::vector<TreeNode*> children;
		bool isLeaf = false;

		TreeNode* HasChild(std::string str);
	};

public:
	/// <summary>
	/// adds a new input to the tree
	/// </summary>
	/// <param name="input"></param>
	void AddInput(std::shared_ptr<Input> input);

	/// <summary>
	/// checks if the input has a prefix that is in the tree
	/// </summary>
	/// <param name="input"></param>
	/// <returns></returns>
	bool HasPrefix(std::shared_ptr<Input> input);

	~ExclusionTree();

	
	static int32_t GetType()
	{
		return 'EXCL';
	}

private:
	TreeNode root;

	void DeleteChildren(TreeNode* node);

	void DeleteNode(TreeNode* node);
};
