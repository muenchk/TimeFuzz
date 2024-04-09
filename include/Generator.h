#pragma once

#include "Interfaces/IInput.h"

class Generator
{
public:
	virtual void Generate();
	virtual void Clean();

	virtual void AddExclusion(std::shared_ptr<IInput> input);
	virtual void AddExclusionPrefix(std::shared_ptr<IInput> input);
};


class SimpleGenerator : Generator
{
	void Generate();
	void Clean();
	void AddExclusion(std::shared_ptr<IInput> input);
	void AddExclusionPrefix(std::shared_ptr<IInput> input);

private:
	
	class ExclusionNode
	{

	};
	class ExclusionTree
	{
		std::shared_ptr<ExclusionNode> head;
		size_t max_depth = 0;
		void Exclude(std::shared_ptr<IInput> input);
		void ExcludePrefix(std::shared_ptr<IInput> input);
		bool CheckExcluded(std::shared_ptr<IInput> input);
	};
};
