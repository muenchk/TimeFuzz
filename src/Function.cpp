#include "Function.h"

namespace Functions
{
	/// <summary>
	/// runtime registry for Function-classes. factory functions for the classes are registered with their respective classid
	/// </summary>
	std::unordered_map<uint64_t, std::function<BaseFunction*()>> classregistry;

	void RegisterFactory(uint64_t classid, std::function<BaseFunction*()> factory)
	{
		classregistry.insert({ classid, factory });
	}

	BaseFunction* FunctionFactory(uint64_t classid)
	{
		auto func = classregistry.at(classid);
		if (func != nullptr)
			return func();
		else
			return nullptr;
	}
}
