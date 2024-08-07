#pragma once

#include <memory>

#include "Input.h"


class Generator
{
public:
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

	static int32_t GetType()
	{
		return 'INPU';
	}

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
