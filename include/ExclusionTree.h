#pragma once

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

#include "Input.h"
#include "Form.h"
#include "Utility.h"

class SessionData;

enum OracleResult : EnumType;

struct ExclusionTreeNode;

namespace Hashing
{
	size_t hash(ExclusionTreeNode const& node);
}

struct ExclusionTreeNode : public Form
{
	/// <summary>
	/// identifier of the node
	/// </summary>
	FormID _stringID;
	/// <summary>
	/// tree-id of the node
	/// </summary>
	uint64_t _id;
	/// <summary>
	/// (rough) number of visits to node, exclusion wise, may be used to prune the tree if necessary [race condition]
	/// </summary>
	//uint64_t _visitcount;
	/// <summary>
	/// children of the node
	/// </summary>
	std::vector<std::shared_ptr<ExclusionTreeNode>> _children;
	/// <summary>
	/// childrens ids
	/// </summary>
	//std::vector<uint64_t> _childrenids;
	/// <summary>
	/// whether this node is a lead
	/// </summary>
	bool _isLeaf = false;

	OracleResult _result;
	FormID _InputID = 0;

	/// <summary>
	/// Returns the child with the identifier [str] if there is one
	/// </summary>
	/// <param name="str"></param>
	/// <returns></returns>
	std::shared_ptr<ExclusionTreeNode> HasChild(FormID stringID);

	friend size_t Hashing::hash(ExclusionTreeNode const& node);

private:
	const int32_t classversion = 0x1;

	struct LoadData
	{
		std::vector<FormID> _nodes;
	};
	LoadData* _loadData = nullptr;

public:

#pragma region InheritedForm


	size_t GetStaticSize(int32_t version) override;
	size_t GetDynamicSize() override;
	bool WriteData(std::ostream* buffer, size_t& offset, size_t length) override;
	bool ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver) override;
	/// <summary>
	/// Early initialiazation pass [is executed once all forms have been loaded
	/// </summary>
	/// <param name="resolver"></param>
	void InitializeEarly(LoadResolver* resolver) override;
	/// <summary>
	/// late initialization pass [is executed once all Initialize Early has been called for all forms]
	/// </summary>
	/// <param name="resolver"></param>
	void InitializeLate(LoadResolver* resolver) override;
	int32_t GetType() override
	{
		return FormType::ExclTreeNode;
	}
	static int32_t GetTypeStatic()
	{
		return FormType::ExclTreeNode;
	}
	void Delete(Data* data) override;
	void Clear() override;
	inline static bool _registeredFactories = false;
	static void RegisterFactories();
	size_t MemorySize() override;

#pragma endregion
};

/// <summary>
/// this class holds information about prefixes that are excluded
/// </summary>
class ExclusionTree : public Form
{
	struct LoadData
	{
		FormID rootid;
	};
	LoadData* _loadData = nullptr;

public:
	ExclusionTree();

	/// <summary>
	/// adds a new input to the tree
	/// </summary>
	/// <param name="input"></param>
	void AddInput(std::shared_ptr<Input> input, OracleResult result);

	/// <summary>
	/// checks if the input has a prefix that is in the tree
	/// </summary>
	/// <param name="input"></param>
	/// <returns></returns>
	bool HasPrefix(std::shared_ptr<Input> input);

	/// <summary>
	/// checks if the input has a prefix that is in the tree, and return the formID of the corresponding test
	/// </summary>
	/// <param name="input"></param>
	/// <returns></returns>
	bool HasPrefix(std::shared_ptr<Input> input, FormID& prefixID);

	/// <summary>
	/// checks if the input has a prefix that is in the tree, it also checks whether there is an extension of the input and returns the shortest one.
	/// </summary>
	/// <param name="input"></param>
	/// <returns></returns>
	std::tuple<bool, FormID, bool, FormID> HasPrefixAndShortestExtension(std::shared_ptr<Input> input);

	void Init(std::shared_ptr<SessionData> sessiondata);

	~ExclusionTree();

	const int32_t classversion = 0x3;

	#pragma region InheritedForm

	size_t GetStaticSize(int32_t version) override;
	size_t GetDynamicSize() override;
	bool WriteData(std::ostream* buffer, size_t& offset, size_t length) override;
	bool ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver) override;
	/// <summary>
	/// Early initialiazation pass [is executed once all forms have been loaded
	/// </summary>
	/// <param name="resolver"></param>
	void InitializeEarly(LoadResolver* resolver) override;
	/// <summary>
	/// late initialization pass [is executed once all Initialize Early has been called for all forms]
	/// </summary>
	/// <param name="resolver"></param>
	void InitializeLate(LoadResolver* resolver) override;
	int32_t GetType() override
	{
		return FormType::ExclTree;
	}
	static int32_t GetTypeStatic()
	{
		return FormType::ExclTree;
	}
	void Delete(Data* data) override;
	void Clear() override;
	inline static bool _registeredFactories = false;
	static void RegisterFactories();
	size_t MemorySize() override;

#pragma endregion

	int64_t GetDepth();
	uint64_t GetNodeCount();
	uint64_t GetLeafCount();

	/// <summary>
	/// This function iterates over the complete tree and reports whether the tree is complete or if there still alternative routes that can be produced
	/// </summary>
	/// <param name="alternativesPerNode"></param>
	/// <returns></returns>
	double CheckForAlternatives(int32_t alternativesPerNode);

private:
	std::shared_ptr<ExclusionTreeNode> root;

	std::shared_mutex _lock;

	uint64_t nextid = 1;

	int64_t depth = 0;

	uint64_t leafcount = 0;

	uint64_t nodecount = 0;

	//std::unordered_map<uint64_t, ExclusionTreeNode*> hashmap;

	std::shared_ptr<SessionData> _sessiondata;

	void DeleteChildren(std::shared_ptr<ExclusionTreeNode> node);

	void DeleteChildrenIntern(std::shared_ptr<ExclusionTreeNode> node);

	void DeleteNodeIntern(std::shared_ptr<ExclusionTreeNode> node);
};
