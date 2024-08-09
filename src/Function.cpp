#include "Function.h"
#include "BufferOperations.h"

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
	BaseFunction* BaseFunction::Create(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver)
	{
		uint64_t type = Buffer::ReadUInt64(buffer, offset);
		BaseFunction* ptr = FunctionFactory(type);
		ptr->ReadData(buffer, offset, length, resolver);
		return ptr;
	}

	size_t BaseFunction::GetLength()
	{
		return 8; // type
	}

	bool BaseFunction::WriteData(unsigned char* buffer, size_t& offset)
	{
		Buffer::Write(GetType(), buffer, offset);
		return true;
	}
}
