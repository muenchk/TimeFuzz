#include "Function.h"
#include "BufferOperations.h"
#include "Logging.h"
#include "Utility.h"

namespace Functions
{
	/// <summary>
	/// runtime registry for Function-classes. factory functions for the classes are registered with their respective classid
	/// </summary>
	std::unordered_map<uint64_t, std::function<Types::shared_ptr<BaseFunction>()>> classregistry;

	void RegisterFactory(uint64_t classid, std::function<Types::shared_ptr<BaseFunction>()> factory)
	{
		loginfo("Registered callback factory: {}", Utility::GetHex(classid))
			classregistry.insert({ classid, factory });
	}

	Types::shared_ptr<BaseFunction> FunctionFactory(uint64_t classid)
	{
		//loginfo("Looking for Class ID: {}", classid);
		if (classregistry.contains(classid) == false)
			logcritical("Missing Class ID: {}", classid);
		auto func = classregistry.at(classid);
		if (func != nullptr)
			return func();
		else
			return nullptr;
	}
	Types::shared_ptr<BaseFunction> BaseFunction::Create(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver)
	{
		uint64_t type = Buffer::ReadUInt64(buffer, offset);
		auto ptr = FunctionFactory(type);
		ptr->ReadData(buffer, offset, length, resolver);
		return ptr;
	}

	Types::shared_ptr<BaseFunction> BaseFunction::Create(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver)
	{
		uint64_t type = Buffer::ReadUInt64(buffer, offset);
		auto ptr = FunctionFactory(type);
		ptr->ReadData(buffer, offset, length, resolver);
		return ptr;
	}

	size_t BaseFunction::GetLength()
	{
		return 8;  // type
	}

	bool BaseFunction::WriteData(std::ostream* buffer, size_t& offset)
	{
		Buffer::Write(GetType(), buffer, offset);
		return true;
	}
	unsigned char* BaseFunction::GetData(size_t& size) {
		unsigned char* buffer = new unsigned char[GetLength()];
		size_t off = 0;
		Buffer::Write(GetType(), buffer, off);
		size = off;
		return buffer;
	}
}
