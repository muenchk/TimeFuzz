#pragma once
#include <utility>
#include <type_traits>
#include <chrono>


namespace Buffer
{
	/// <summary>
	/// Writes an UInt32 to the buffer
	/// </summary>
	/// <param name="value">value to write</param>
	/// <param name="buffer">buffer to write to</param>
	/// <param name="offset">offset to write at, is updated with the written size</param>
	void Write(uint32_t value, unsigned char* buffer, int& offset);
	/// <summary>
	/// Writes an UInt64 to the buffer
	/// </summary>
	/// <param name="value">value to write</param>
	/// <param name="buffer">buffer to write to</param>
	/// <param name="offset">offset to write at, is updated with the written size</param>
	void Write(uint64_t value, unsigned char* buffer, int& offset);
	/// <summary>
	/// Writes a Boolean to the buffer
	/// </summary>
	/// <param name="value">value to write</param>
	/// <param name="buffer">buffer to write to</param>
	/// <param name="offset">offset to write at, is updated with the written size</param>
	void Write(bool value, unsigned char* buffer, int& offset);
	/// <summary>
	/// Writes an Int32 to the buffer
	/// </summary>
	/// <param name="value">value to write</param>
	/// <param name="buffer">buffer to write to</param>
	/// <param name="offset">offset to write at, is updated with the written size</param>
	void Write(int32_t value, unsigned char* buffer, int& offset);
	/// <summary>
	/// Wrotes an Int64 to the buffer
	/// </summary>
	/// <param name="value">value to write</param>
	/// <param name="buffer">buffer to write to</param>
	/// <param name="offset">offset to write at, is updated with the written size</param>
	void Write(int64_t value, unsigned char* buffer, int& offset);
	/// <summary>
	/// Writes a Float to the buffer
	/// </summary>
	/// <param name="value">value to write</param>
	/// <param name="buffer">buffer to write to</param>
	/// <param name="offset">offset to write at, is updated with the written size</param>
	void Write(float value, unsigned char* buffer, int& offset);
	/// <summary>
	/// Writes a String to the buffer
	/// </summary>
	/// <param name="value">value to write</param>
	/// <param name="buffer">buffer to write to</param>
	/// <param name="offset">offset to write at, is updated with the written size</param>
	void Write(std::string value, unsigned char* buffer, int& offset);
	/// <summary>
	/// writes nanoseconds to the buffer
	/// </summary>
	/// <param name=""></param>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	void Write(std::chrono::nanoseconds value, unsigned char* buffer, int& offset);
	/// <summary>
	/// writes microseconds to the buffer
	/// </summary>
	/// <param name=""></param>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	void Write(std::chrono::microseconds value, unsigned char* buffer, int& offset);
	/// <summary>
	/// writes milliseconds to the buffer
	/// </summary>
	/// <param name=""></param>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	void Write(std::chrono::milliseconds value, unsigned char* buffer, int& offset);
	/// <summary>
	/// writes seconds to the buffer
	/// </summary>
	/// <param name=""></param>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	void Write(std::chrono::seconds value, unsigned char* buffer, int& offset);
	/// <summary>
	/// Writes size_t to the buffer
	/// </summary>
	/// <param name="value"></param>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	void Write(size_t value, unsigned char* buffer, int& offset);
	/// <summary>
	/// Writes a timepoint to the buffer
	/// </summary>
	/// <param name="value"></param>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	void Write(std::chrono::steady_clock::time_point value, unsigned char* buffer, int& offset);

	/// <summary>
	/// Reads an UInt32 from the buffer
	/// </summary>
	/// <param name="buffer">buffer to read from</param>
	/// <param name="offset">Offset to read at, is updated with the read size</param>
	/// <returns>the read value</returns>
	uint32_t ReadUInt32(unsigned char* buffer, int& offset);
	/// <summary>
	/// Reads an UInt64 from the buffer
	/// </summary>
	/// <param name="buffer">buffer to read from</param>
	/// <param name="offset">Offset to read at, is updated with the read size</param>
	/// <returns>the read value</returns>
	uint64_t ReadUInt64(unsigned char* buffer, int& offset);
	/// <summary>
	/// Reads a Boolean from the buffer
	/// </summary>
	/// <param name="buffer">buffer to read from</param>
	/// <param name="offset">Offset to read at, is updated with the read size</param>
	/// <returns>the read value</returns>
	bool ReadBool(unsigned char* buffer, int& offset);
	/// <summary>
	/// Reads an Int32 from the buffer
	/// </summary>
	/// <param name="buffer">buffer to read from</param>
	/// <param name="offset">Offset to read at, is updated with the read size</param>
	/// <returns>the read value</returns>
	int32_t ReadInt32(unsigned char* buffer, int& offset);
	/// <summary>
	/// Reads an Int64 from the buffer
	/// </summary>
	/// <param name="buffer">buffer to read from</param>
	/// <param name="offset">Offset to read at, is updated with the read size</param>
	/// <returns>the read value</returns>
	int64_t ReadInt64(unsigned char* buffer, int& offset);
	/// <summary>
	/// Reads a Float from the buffer
	/// </summary>
	/// <param name="buffer">buffer to read from</param>
	/// <param name="offset">Offset to read at, is updated with the read size</param>
	/// <returns>the read value</returns>
	float ReadFloat(unsigned char* buffer, int& offset);
	/// <summary>
	/// Reads a String from the buffer
	/// </summary>
	/// <param name="buffer">buffer to read from</param>
	/// <param name="offset">Offset to read at, is updated with the read size</param>
	/// <returns>the read value</returns>
	std::string ReadString(unsigned char* buffer, int& offset);

	/// <summary>
	/// Reads nanoseconds from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <returns></returns>
	std::chrono::nanoseconds ReadNanoSeconds(unsigned char* buffer, int& offset);
	/// <summary>
	/// Reads milliseconds from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <returns></returns>
	std::chrono::milliseconds ReadMilliSeconds(unsigned char* buffer, int& offset);
	/// <summary>
	/// Reads Microseconds from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <returns></returns>
	std::chrono::microseconds ReadMicroSeconds(unsigned char* buffer, int& offset);
	/// <summary>
	/// Reads seconds from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <returns></returns>
	std::chrono::seconds ReadSeconds(unsigned char* buffer, int& offset);
	/// <summary>
	/// Reads a size from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <returns></returns>
	size_t ReadSize(unsigned char* buffer, int& offset);
	/// <summary>
	/// Reads a timepoint from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <returns></returns>
	std::chrono::steady_clock::time_point ReadTime(unsigned char* buffer, int& offset);

	/// <summary>
	/// Calculates the write length of a string
	/// </summary>
	/// <param name="value">the string to calculate for</param>
	/// <returns>The number of bytes needed to store the string</returns>
	int CalcStringLength(std::string value);
	/// <summary>
	/// Calcuates the read length of a string
	/// </summary>
	/// <param name="buffer">buffer to read from</param>
	/// <param name="offset">offset of the strings begin</param>
	/// <returns>The length of the string in bytes</returns>
	size_t CalcStringLength(unsigned char* buffer, int offset);

	namespace List
	{
		size_t GetListLength(std::list<std::string>& list);
		size_t GetListLength(unsigned char* buffer, int offset);
		void WriteList(std::list<std::string>& list, unsigned char* buffer, int& offset);
		void ReadList(std::list<string::string>& list, unsigned char* buffer, int& offset);

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
		size_t GetVectorSize(unsigned char* buffer, int offset)
		{
			size_t length = *((size_t*)(buffer + offset));
			return length;
		}
		template<class T>
		void WriteVector(std::vector<T>& vector, unsigned char* buffer, int& offset);
		template<class T>
		std::vector<T> ReadVector(unsigned char* buffer, int& offset);
	}
}
