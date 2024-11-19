#pragma once

#include "BufferOperations.h"
#include "Utility.h"

#include <iostream>

class LoadResolver;
class ExecutionHandler;

class Records
{
public:
	static unsigned char* CreateRecordHeaderStringHashmap(size_t& length, size_t& offset)
	{
		unsigned char* buf = 0;
		offset = 0;
		buf = new unsigned char[length + 8 + 4];
		//length = sz + 8 + 4;
		Buffer::WriteSize(length, buf, offset);
		Buffer::Write((int32_t)'STRH', buf, offset);
		length += 8 + 4;
		return buf;
	}

	/// <summary>
	/// Creates a new record and returns the length in [length]
	/// </summary>
	/// <typeparam name="T"></typeparam>
	/// <param name="value"></param>
	/// <returns></returns>
	template <class T>
	static unsigned char* CreateRecord(T* value, size_t& offset, size_t& length)
	{
		offset = 0;
		size_t sz = value->GetDynamicSize();
		// record length + uint64_t record length + int32_t record type
		length = sz + 8 + 4;
		unsigned char* buffer = new unsigned char[length];
		Buffer::WriteSize(sz, buffer, offset);
		Buffer::Write(T::GetTypeStatic(), buffer, offset);
		value->WriteData(buffer, offset);
		return buffer;
	}
	template <>
	unsigned char* CreateRecord(ExecutionHandler* value, size_t& offset, size_t& length);

	/// <summary>
	/// Creates a new record and returns the length in [length]
	/// </summary>
	/// <typeparam name="T"></typeparam>
	/// <param name="value"></param>
	/// <param name="length"></param>
	/// <returns></returns>
	template <class T>
	static unsigned char* CreateRecord(std::shared_ptr<T> value, size_t& offset, size_t& length)
	{
		return CreateRecord<T>(value.get(), offset, length);
	}

	/// <summary>
	/// Reads a record from the buffer
	/// </summary>
	/// <typeparam name="T"></typeparam>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <param name="length"></param>
	/// <returns></returns>
	template <class T>
	static std::shared_ptr<T> ReadRecord(unsigned char* buffer, size_t offset, size_t& internaloffset, size_t length, LoadResolver* resolver)
	{
		T::RegisterFactories();
		internaloffset = 0;
		T* rec = new T();
		rec->ReadData(buffer + offset, internaloffset, length, resolver);
		// if the offset (that was updated by ReadData) is greater than length, we have read beyond the limits of the buffer
		if (internaloffset > length) {
			delete rec;
			return {};
		}
		return std::shared_ptr<T>(rec);
	}
};
