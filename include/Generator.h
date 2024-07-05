#pragma once

#include <memory>

#include "Input.h"

class Generator
{
public:
	virtual void Generate();
	/// <summary>
	/// clears all internal variables
	/// </summary>
	virtual void Clear();
	/// <summary>
	/// resets all progress made
	/// </summary>
	virtual void Clean();

	virtual void AddExclusion(std::shared_ptr<Input> input);
	virtual void AddExclusionPrefix(std::shared_ptr<Input> input);
};


class SimpleGenerator : Generator
{
	void Generate();
	/// <summary>
	/// clears all internal variables
	/// </summary>
	void Clear();
	/// <summary>
	/// resets all progress made
	/// </summary>
	void Clean();
	void AddExclusion(std::shared_ptr<Input> input);
	void AddExclusionPrefix(std::shared_ptr<Input> input);

private:
	
	class ExclusionNode
	{

	};
	class ExclusionTree
	{
		std::shared_ptr<ExclusionNode> head;
		size_t max_depth = 0;
		void Exclude(std::shared_ptr<Input> input);
		void ExcludePrefix(std::shared_ptr<Input> input);
		bool CheckExcluded(std::shared_ptr<Input> input);
	};
};
