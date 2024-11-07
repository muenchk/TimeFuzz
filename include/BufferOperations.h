#pragma once
#include <utility>
#include <type_traits>
#include <chrono>
#include <list>
#include <vector>
#include <string>


namespace Buffer
{
	/// <summary>
	/// Writes an UInt32 to the buffer
	/// </summary>
	/// <param name="value">value to write</param>
	/// <param name="buffer">buffer to write to</param>
	/// <param name="offset">offset to write at, is updated with the written size</param>
	void Write(uint32_t value, unsigned char* buffer, size_t& offset);
	/// <summary>
	/// Writes an UInt64 to the buffer
	/// </summary>
	/// <param name="value">value to write</param>
	/// <param name="buffer">buffer to write to</param>
	/// <param name="offset">offset to write at, is updated with the written size</param>
	void Write(uint64_t value, unsigned char* buffer, size_t& offset);
	/// <summary>
	/// Writes a Boolean to the buffer
	/// </summary>
	/// <param name="value">value to write</param>
	/// <param name="buffer">buffer to write to</param>
	/// <param name="offset">offset to write at, is updated with the written size</param>
	void Write(bool value, unsigned char* buffer, size_t& offset);
	/// <summary>
	/// Writes an Int32 to the buffer
	/// </summary>
	/// <param name="value">value to write</param>
	/// <param name="buffer">buffer to write to</param>
	/// <param name="offset">offset to write at, is updated with the written size</param>
	void Write(int32_t value, unsigned char* buffer, size_t& offset);
	/// <summary>
	/// Wrotes an Int64 to the buffer
	/// </summary>
	/// <param name="value">value to write</param>
	/// <param name="buffer">buffer to write to</param>
	/// <param name="offset">offset to write at, is updated with the written size</param>
	void Write(int64_t value, unsigned char* buffer, size_t& offset);
	/// <summary>
	/// Writes a Float to the buffer
	/// </summary>
	/// <param name="value">value to write</param>
	/// <param name="buffer">buffer to write to</param>
	/// <param name="offset">offset to write at, is updated with the written size</param>
	void Write(float value, unsigned char* buffer, size_t& offset);
	/// <summary>
	/// Writes a Double to the buffer
	/// </summary>
	/// <param name="value">value to write</param>
	/// <param name="buffer">buffer to write to</param>
	/// <param name="offset">offset to write at, is updated with the written size</param>
	void Write(double value, unsigned char* buffer, size_t& offset);
	/// <summary>
	/// Writes a String to the buffer
	/// </summary>
	/// <param name="value">value to write</param>
	/// <param name="buffer">buffer to write to</param>
	/// <param name="offset">offset to write at, is updated with the written size</param>
	void Write(std::string value, unsigned char* buffer, size_t& offset);
	/// <summary>
	/// writes nanoseconds to the buffer
	/// </summary>
	/// <param name=""></param>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	void Write(std::chrono::nanoseconds value, unsigned char* buffer, size_t& offset);
	/// <summary>
	/// writes microseconds to the buffer
	/// </summary>
	/// <param name=""></param>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	void Write(std::chrono::microseconds value, unsigned char* buffer, size_t& offset);
	/// <summary>
	/// writes milliseconds to the buffer
	/// </summary>
	/// <param name=""></param>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	void Write(std::chrono::milliseconds value, unsigned char* buffer, size_t& offset);
	/// <summary>
	/// writes seconds to the buffer
	/// </summary>
	/// <param name=""></param>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	void Write(std::chrono::seconds value, unsigned char* buffer, size_t& offset);
	/// <summary>
	/// Writes size_t to the buffer
	/// </summary>
	/// <param name="value"></param>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	void WriteSize(size_t value, unsigned char* buffer, size_t& offset);
	/// <summary>
	/// Writes a timepoint to the buffer
	/// </summary>
	/// <param name="value"></param>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	void Write(std::chrono::steady_clock::time_point value, unsigned char* buffer, size_t& offset);
	/// <summary>
	/// Writes an unsigned char to the buffer
	/// </summary>
	/// <param name=""></param>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	void Write(unsigned char value, unsigned char* buffer, size_t& offset);
	/// <summary>
	/// Writes a buffer to the buffer
	/// </summary>
	/// <param name="value"></param>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <param name="count"></param>
	void Write(unsigned char* value, unsigned char* buffer, size_t& offset, size_t count);

	/// <summary>
	/// Reads a generic basic type from the buffer
	/// </summary>
	/// <typeparam name="T"></typeparam>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <returns></returns>
	template <class T>
	T Read(unsigned char* buffer, size_t& offset)
	{
		T value = *((T*)(buffer + offset));
		offset += sizeof(T);
		return value;
	}
	/// <summary>
	/// Reads an UInt32 from the buffer
	/// </summary>
	/// <param name="buffer">buffer to read from</param>
	/// <param name="offset">Offset to read at, is updated with the read size</param>
	/// <returns>the read value</returns>
	uint32_t ReadUInt32(unsigned char* buffer, size_t& offset);
	/// <summary>
	/// Reads an UInt64 from the buffer
	/// </summary>
	/// <param name="buffer">buffer to read from</param>
	/// <param name="offset">Offset to read at, is updated with the read size</param>
	/// <returns>the read value</returns>
	uint64_t ReadUInt64(unsigned char* buffer, size_t& offset);
	/// <summary>
	/// Reads a Boolean from the buffer
	/// </summary>
	/// <param name="buffer">buffer to read from</param>
	/// <param name="offset">Offset to read at, is updated with the read size</param>
	/// <returns>the read value</returns>
	bool ReadBool(unsigned char* buffer, size_t& offset);
	/// <summary>
	/// Reads an Int32 from the buffer
	/// </summary>
	/// <param name="buffer">buffer to read from</param>
	/// <param name="offset">Offset to read at, is updated with the read size</param>
	/// <returns>the read value</returns>
	int32_t ReadInt32(unsigned char* buffer, size_t& offset);
	/// <summary>
	/// Reads an Int64 from the buffer
	/// </summary>
	/// <param name="buffer">buffer to read from</param>
	/// <param name="offset">Offset to read at, is updated with the read size</param>
	/// <returns>the read value</returns>
	int64_t ReadInt64(unsigned char* buffer, size_t& offset);
	/// <summary>
	/// Reads a Float from the buffer
	/// </summary>
	/// <param name="buffer">buffer to read from</param>
	/// <param name="offset">Offset to read at, is updated with the read size</param>
	/// <returns>the read value</returns>
	float ReadFloat(unsigned char* buffer, size_t& offset);
	/// <summary>
	/// Reads a Double from the buffer
	/// </summary>
	/// <param name="buffer">buffer to read from</param>
	/// <param name="offset">Offset to read at, is updated with the read size</param>
	/// <returns>the read value</returns>
	double ReadDouble(unsigned char* buffer, size_t& offset);
	/// <summary>
	/// Reads a String from the buffer
	/// </summary>
	/// <param name="buffer">buffer to read from</param>
	/// <param name="offset">Offset to read at, is updated with the read size</param>
	/// <returns>the read value</returns>
	std::string ReadString(unsigned char* buffer, size_t& offset);
	/// <summary>
	/// Reads an unsigned char from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <returns></returns>
	unsigned char ReadUChar(unsigned char* buffer, size_t& offset);
	/// <summary>
	/// Reads a buffer from the buffer
	/// </summary>
	unsigned char* ReadBuffer(unsigned char* buffer, size_t& offset, size_t count);

	/// <summary>
	/// Reads nanoseconds from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <returns></returns>
	std::chrono::nanoseconds ReadNanoSeconds(unsigned char* buffer, size_t& offset);
	/// <summary>
	/// Reads milliseconds from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <returns></returns>
	std::chrono::milliseconds ReadMilliSeconds(unsigned char* buffer, size_t& offset);
	/// <summary>
	/// Reads Microseconds from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <returns></returns>
	std::chrono::microseconds ReadMicroSeconds(unsigned char* buffer, size_t& offset);
	/// <summary>
	/// Reads seconds from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <returns></returns>
	std::chrono::seconds ReadSeconds(unsigned char* buffer, size_t& offset);
	/// <summary>
	/// Reads a size from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <returns></returns>
	size_t ReadSize(unsigned char* buffer, size_t& offset);
	/// <summary>
	/// Reads a timepoint from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <returns></returns>
	std::chrono::steady_clock::time_point ReadTime(unsigned char* buffer, size_t& offset);

	/// <summary>
	/// Calculates the write length of a string
	/// </summary>
	/// <param name="value">the string to calculate for</param>
	/// <returns>The number of bytes needed to store the string</returns>
	size_t CalcStringLength(std::string value);
	/// <summary>
	/// Calcuates the read length of a string
	/// </summary>
	/// <param name="buffer">buffer to read from</param>
	/// <param name="offset">offset of the strings begin</param>
	/// <returns>The length of the string in bytes</returns>
	size_t CalcStringLength(unsigned char* buffer, size_t offset);

	namespace List
	{
		size_t GetListLength(std::list<std::string>& list);
		size_t GetListLength(unsigned char* buffer, size_t offset);
		void WriteList(std::list<std::string>& list, unsigned char* buffer, size_t& offset);
		void ReadList(std::list<std::string>& list, unsigned char* buffer, size_t& offset);

	}

	namespace ListBasic
	{
		template <class T>
		size_t GetListLength(std::list<T>& list)
		{
			return 8 + sizeof(T) * list.size();
		}
		size_t GetListLength(unsigned char* buffer, size_t offset);
		template <class T>
		void WriteList(std::list<T>& list, unsigned char* buffer, size_t& offset)
		{
			Buffer::WriteSize(list.size(), buffer, offset);
			auto itr = list.begin();
			while (itr != list.end()) {
				Buffer::Write(*itr, buffer, offset);
				itr++;
			}
		}
		template <class T>
		void ReadList(std::list<T>& list, unsigned char* buffer, size_t& offset)
		{
			size_t len = Buffer::ReadSize(buffer, offset);
			for (int64_t i = 0; i < (int64_t)len; i++)
				list.push_back(Buffer::Read<T>(buffer, offset));
		}

	}

	/// <summary>
	/// Provides buffer operations for vectors of basic types
	/// </summary>
	namespace VectorBasic
	{
		template<class T>
		size_t GetVectorSize(std::vector<T>& vector)
		{
			return 8 + (vector.size() * sizeof(T));
		}
		template<class T>
		size_t GetVectorSize(unsigned char* buffer, size_t offset)
		{
			size_t length = *((size_t*)(buffer + offset));
			return length;
		}
		template<class T>
		void WriteVector(std::vector<T>& vector, unsigned char* buffer, size_t& offset)
		{
			Buffer::WriteSize(vector.size(), buffer, offset);
			for (int64_t i = 0; i < (int64_t)vector.size(); i++)
				Buffer::Write(vector[i], buffer, offset);
		}
		template<class T>
		std::vector<T> ReadVector(unsigned char* buffer, size_t& offset)
		{
			std::vector<T> vector;
			size_t len = Buffer::ReadSize(buffer, offset);
			for (int64_t i = 0; i < (int64_t)len; i++)
				vector.push_back(Buffer::Read<T>(buffer, offset));
			return vector;
		}
	}
}
