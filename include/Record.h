#pragma once

#include "BufferOperations.h"
#include "Utility.h"
#include "Logging.h"

#include <iostream>

class LoadResolver;
class ExecutionHandler;

struct Records
{
	static void CreateRecordHeaderStringHashmap(std::ostream* buffer, size_t& length, size_t& offset)
	{
		offset = 0;
		//length = sz + 8 + 4;
		Buffer::WriteSize(length, buffer, offset);
		Buffer::Write((int32_t)'STRH', buffer, offset);
		length += 8 + 4;
	}

	/// <summary>
	/// Creates a new record and returns the length in [length]
	/// </summary>
	/// <typeparam name="T"></typeparam>
	/// <param name="value"></param>
	/// <returns></returns>
	template <class T>
	static void CreateRecord(T* value, std::ostream* buffer, size_t& offset, size_t& length)
	{
		offset = 0;
		size_t sz = value->GetDynamicSize();
		// record length + uint64_t record length + int32_t record type
		length = sz + 8 + 4;
		Buffer::WriteSize(sz, buffer, offset);
		Buffer::Write(T::GetTypeStatic(), buffer, offset);
		if (value->WriteData(buffer, offset, length) == false)
		{
			logwarn("Error writing record, len: {}, off: {}", length, offset);
		}
		if (offset > length) {
			logcritical("Buffer overflow in record: Type: {}, Offset: {}, Length: {}, Flags: {}", value->GetType(), offset, length, Utility::GetHex(value->GetFlags()));
			auto size = value->GetDynamicSize();
		}
	}

	/// <summary>
	/// Creates a new record and returns the length in [length]
	/// </summary>
	/// <typeparam name="T"></typeparam>
	/// <param name="value"></param>
	/// <param name="length"></param>
	/// <returns></returns>
	template <class T>
	static void CreateRecord(Types::shared_ptr<T> value, std::ostream* buffer, size_t& offset, size_t& length)
	{
		CreateRecord<T>(value.get(), buffer, offset, length);
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
	static Types::shared_ptr<T> ReadRecord(std::istream* buffer, size_t /*offset*/, size_t& internaloffset, size_t length, LoadResolver* resolver)
	{
		T::RegisterFactories();
		internaloffset = 0;
		Types::shared_ptr<T> rec = Types::make_shared<T>();
		rec->ReadData(buffer, internaloffset, length, resolver);
		// if the offset (that was updated by ReadData) is greater than length, we have read beyond the limits of the buffer
		if (internaloffset > length) {
			rec.reset();
			return {};
		}
		return rec;
	}
};

template <>
void Records::CreateRecord(ExecutionHandler* value, std::ostream* buffer, size_t& offset, size_t& length);
