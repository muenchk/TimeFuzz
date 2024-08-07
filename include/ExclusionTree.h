#pragma once

#include <memory>
#include <vector>
#include <string>
#include <shared_mutex>
#include <unordered_map>

#include "Input.h"


/// <summary>
/// this class holds information about prefixes that are excluded
/// </summary>
class ExclusionTree
{
	struct TreeNode
	{
		std::string identifier;
		uint64_t id;
		std::vector<TreeNode*> children;
		std::vector<uint64_t> childrenids;
		bool isLeaf = false;

		TreeNode* HasChild(std::string str);
	};

public:
	ExclusionTree();

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

	const int32_t classversion = 0x1;

	static int32_t GetType()
	{
		return 'EXCL';
	}
	/// <summary>
	/// returns the total size of the fields with static size
	/// </summary>
	/// <returns></returns>
	size_t GetStaticSize(int32_t version = 0x1);
	/// <summary>
	/// returns the total size of all fields of this instance
	/// </summary>
	/// <returns></returns>
	size_t GetDynamicSize();
	/// <summary>
	/// returns the class version
	/// </summary>
	/// <returns></returns>
	int32_t GetClassVersion();
	/// <summary>
	/// saves all relevant information of this instance to the given buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <returns></returns>
	bool WriteData(unsigned char* buffer, size_t offset);
	/// <summary>
	/// reads all relevant information of this instance from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="length"></param>
	bool ReadData(unsigned char* buffer, size_t offset, size_t length);

	bool AqcuireLock();

	void ReleaseLock();

private:
	TreeNode root;

	std::shared_mutex mutex;

	uint64_t nextid = 1;

	std::unordered_map<uint64_t, TreeNode*> hashmap;

	void DeleteChildren(TreeNode* node);

	void DeleteChildrenIntern(TreeNode* node);

	void DeleteNodeIntern(TreeNode* node);
};
