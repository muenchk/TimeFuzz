#pragma once

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

#include "Input.h"
#include "Form.h"

/// <summary>
/// this class holds information about prefixes that are excluded
/// </summary>
class ExclusionTree : public Form
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

	#pragma region InheritedForm

	size_t GetStaticSize(int32_t version = 0x1) override;
	size_t GetDynamicSize() override;
	bool WriteData(unsigned char* buffer, size_t& offset) override;
	bool ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver) override;
	int32_t GetType() override
	{
		return FormType::ExclTree;
	}
	static int32_t GetTypeStatic()
	{
		return FormType::ExclTree;
	}
	void Delete(Data* data);


	#pragma endregion

private:
	TreeNode root;

	uint64_t nextid = 1;

	std::unordered_map<uint64_t, TreeNode*> hashmap;

	void DeleteChildren(TreeNode* node);

	void DeleteChildrenIntern(TreeNode* node);

	void DeleteNodeIntern(TreeNode* node);
};
