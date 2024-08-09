#pragma once

#include "BufferOperations.h"

class LoadResolver;
class ExecutionHandler;

namespace Records
{
	/// <summary>
	/// Creates a new record and returns the length in [length]
	/// </summary>
	/// <typeparam name="T"></typeparam>
	/// <param name="value"></param>
	/// <returns></returns>
	template<class T>
	unsigned char* CreateRecord(T* value, size_t& length)
	{
		size_t offset = 0;
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
	unsigned char* CreateRecord(ExecutionHandler* value, size_t& length);

	/// <summary>
	/// Creates a new record and returns the length in [length]
	/// </summary>
	/// <typeparam name="T"></typeparam>
	/// <param name="value"></param>
	/// <param name="length"></param>
	/// <returns></returns>
	template<class T>
	unsigned char* CreateRecord(std::shared_ptr<T> value, size_t& length)
	{
		return CreateRecord<T>(value.get(), length);
	}

	/// <summary>
	/// Reads a record from the buffer
	/// </summary>
	/// <typeparam name="T"></typeparam>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <param name="length"></param>
	/// <returns></returns>
	template<class T>
	T* ReadRecord(unsigned char* buffer, size_t offset, size_t length, LoadResolver* resolver)
	{
		size_t off = 0;
		T* rec = new T();
		rec->ReadData(buffer + offset, off, length, resolver);
		// if the offset (that was updated by ReadData) is greater than length, we have read beyond the limits of the buffer
		if (off > length)
		{
			delete rec;
			return nullptr;
		}
		return rec;
	}
}
