#pragma once
#include <utility>
#include <type_traits>
#include <chrono>
#include <list>
#include <vector>
#include <string>
#include <iostream>
#include <deque>
#include <iterator>
#include "Form.h"
#include "Types.h"

#include "MemoryStream.h"

namespace Buffer
{

	template <class T>
	int64_t _sizeof(T&)
	{
		return sizeof(T);
	}
	template <>
	int64_t _sizeof(std::string& value);
	template <>
	int64_t _sizeof(String& value);
	template <>
	int64_t _sizeof(size_t& value);

	class ArrayBuffer
	{
	private:
		unsigned char* _buffer = nullptr;
		int64_t _size = 0;
		int64_t _offset = 0;

		bool __copied = false;

	private:
		bool WriteIntern(unsigned char* value, int64_t count)
		{
			if (count + (int64_t)sizeof(int64_t) > Free() || count >= (int64_t)UINT32_MAX)
				return false;
			memcpy(_buffer + _offset, value, (size_t)count);
			_offset += count;
			return true;
		}

		bool WriteIntern(char* value, int64_t count)
		{
			if (count + (int64_t)sizeof(int64_t) > Free() || count >= (int64_t)UINT32_MAX)
				return false;
			memcpy(_buffer + _offset, value, (size_t)count);
			_offset += count;
			return true;
		}

		bool WriteIntern(const char* value, int64_t count)
		{
			if (count + (int64_t)sizeof(int64_t) > Free() || count >= (int64_t)UINT32_MAX)
				return false;
			memcpy(_buffer + _offset, value, (size_t)count);
			_offset += count;
			return true;
		}

	public:

		ArrayBuffer(unsigned char* data, int64_t offset, int64_t length)
		{
			_buffer = data;
			__copied = true;
			_offset = offset;
			_size = length;
			if (_offset < 0 || _offset >= _size)
				throw std::out_of_range("Offset is less than 0 or larger than size");
			if (_size <= 0)
				throw std::out_of_range("Size is less equal 0");
		}

		ArrayBuffer(std::istream* data, int64_t length)
		{
			_size = length;
			_offset = 0;
			_buffer = new unsigned char[length];
			data->read((char*)_buffer, length);
			int64_t rd = data->gcount();
			if (rd == 0)
				throw std::out_of_range("Stream is empty");
			if (rd <= length)
				_size = rd;
		}

		ArrayBuffer(int64_t length)
		{
			_offset = 0;
			_size = length;
			_buffer = new unsigned char[length];
		}

		~ArrayBuffer()
		{
			if (__copied)
				return;
			else
				delete _buffer;
		}

		int64_t Size()
		{
			return _size;
		}

		int64_t Free()
		{
			return _size - _offset;
		}

		std::pair<unsigned char*, int64_t> GetBuffer()
		{
			return { _buffer, _size };
		}

		template <class T>
		bool Write(T&& value)
		{
			if (_sizeof(value) > Free())
				return false;
			T* ptr = (T*)(_buffer + _offset);
			*ptr = value;
			_offset += _sizeof(value);
			return true;
		}

		template <class T>
		bool Write(const T& value)
		{
			if (_sizeof(value) > Free())
				return false;
			T* ptr = (T*)(_buffer + _offset);
			*ptr = value;
			_offset += _sizeof(value);
			return true;
		}

		bool Write(unsigned char* value, int64_t count)
		{
			if (count + (int64_t)sizeof(int64_t) > Free() || count >= (int64_t)UINT32_MAX)
				return false;
			Write<int64_t>(count);
			memcpy(_buffer + _offset, value, (size_t)count);
			_offset += count;
			return true;
		}

		bool Write(char* value, int64_t count)
		{
			if (count + (int64_t)sizeof(int64_t) > Free() || count >= (int64_t)UINT32_MAX)
				return false;
			Write<int64_t>(count);
			memcpy(_buffer + _offset, value, (size_t)count);
			_offset += count;
			return true;
		}

		bool Write(const char* value, int64_t count)
		{
			if (count + (int64_t)sizeof(int64_t) > Free() || count >= (int64_t)UINT32_MAX)
				return false;
			Write<int64_t>(count);
			memcpy(_buffer + _offset, value, (size_t)count);
			_offset += count;
			return true;
		}

		template <class T>
		bool WriteList(std::list<T>& list)
		{
			static auto t = T();
			if (8 + (int64_t)list.size() * _sizeof(t) > Free())
				return false;
			Write<size_t>(list.size());
			auto itr = list.begin();
			while (itr != list.end()) {
				Write<T>(*itr);
				itr++;
			}
			return true;
		}

		template <class T>
		bool WriteVector(std::vector<T>& vector)
		{
			static auto t = T();
			if (8 + (int64_t)vector.size() * _sizeof(t) > Free())
				return false;
			Write<size_t>(vector.size());
			for (int64_t i = 0; i < (int64_t)vector.size(); i++)
				Write<T>(vector[i]);
			return true;
		}

		template <class T>
		bool WriteDeque(std::deque<T>& deque)
		{
			static auto t = T();
			if (8 + (int64_t)deque.size() * _sizeof(t) > Free())
				return false;
			Write<size_t>(deque.size());
			for (int64_t i = 0; i < (int64_t)deque.size(); i++)
				Write<T>(deque[i]);
			return true;
		}

		template <class T>
		bool WriteDeque(Deque<T>& deque)
		{
			static auto t = T();
			if (8 + (int64_t)deque.size() * _sizeof(t) > Free())
				return false;
			Write<size_t>(deque.size());
			for (int64_t i = 0; i < (int64_t)deque.size(); i++)
				Write<T>(deque[i]);
			return true;
		}

		template <class T>
		T Read()
		{
			if (sizeof(T) > Free())
				throw std::out_of_range("Tried to read after the end of the buffer");
			T value = *((T*)(_buffer + _offset));
			_offset += sizeof(T);
			return value;
		}

		int64_t Read(unsigned char* out, int64_t maxlength)
		{
			if (8 > Free())
				throw std::out_of_range("Tried to read after the end of the buffer");
			int64_t len = Read<int64_t>();
			if (len > Free())
				throw std::out_of_range("Tried to read after the end of the buffer");
			if (len > maxlength)
				throw std::out_of_range("Tried to write after the end of the buffer");
			memcpy(out, _buffer + _offset, len);
			_offset += len;
			return len;
		}

		int64_t Read(char* out, int64_t maxlength)
		{
			if (8 > Free())
				throw std::out_of_range("Tried to read after the end of the buffer");
			int64_t len = Read<int64_t>();
			if (len > Free())
				throw std::out_of_range("Tried to read after the end of the buffer");
			if (len > maxlength)
				throw std::out_of_range("Tried to write after the end of the buffer");
			memcpy(out, _buffer + _offset, len);
			_offset += len;
			return len;
		}

		unsigned char* Read(int64_t* read)
		{
			if (8 > Free())
				throw std::out_of_range("Tried to read after the end of the buffer");
			int64_t len = Read<int64_t>();
			if (len > Free())
				throw std::out_of_range("Tried to read after the end of the buffer");
			if (len > 1073741824)
				throw std::out_of_range("Tried to read buffer greater 1 GB");
			unsigned char* buffer = new unsigned char[len];
			memcpy(buffer, _buffer + _offset, len);
			_offset += len;
			*read = len;
			return buffer;
		}

		template <class T>
		std::list<T> ReadList()
		{
			if (8 > Free())
				throw std::out_of_range("Tried to read after the end of the buffer");
			std::list<T> list;
			size_t len = Read<size_t>();
			if ((int64_t)len * (int64_t)sizeof(T) > Free())
				throw std::out_of_range("Tried to read after the end of the buffer");
			for (int64_t i = 0; i < (int64_t)len; i++)
				list.push_back(Read<T>());

			return list;
		}
		template <class T>
		std::vector<T> ReadVector()
		{
			if (8 > Free())
				throw std::out_of_range("Tried to read after the end of the buffer");
			std::vector<T> vector;
			size_t len = Read<size_t>();
			if ((int64_t)len * (int64_t)sizeof(T) > Free())
				throw std::out_of_range("Tried to read after the end of the buffer");
			for (int64_t i = 0; i < (int64_t)len; i++)
				vector.push_back(Read<T>());

			return vector;
		}
		template <class T>
		std::deque<T> ReadDeque()
		{
			if (8 > Free())
				throw std::out_of_range("Tried to read after the end of the buffer");
			std::deque<T> deque;
			size_t len = Read<size_t>();
			if ((int64_t)len * (int64_t)sizeof(T) > Free())
				throw std::out_of_range("Tried to read after the end of the buffer");
			for (int64_t i = 0; i < (int64_t)len; i++)
				deque.push_back(Read<T>());
			return deque;
		}

		template <class T>
		static size_t GetContainerSize(std::list<T>& list)
		{
			static auto t = T();
			return 8 + list.size() * _sizeof(t);
		}
		template <class T>
		static size_t GetContainerSize(std::vector<T>& vector)
		{
			static auto t = T();
			return 8 + vector.size() * _sizeof(t);
		}
		template <class T>
		static size_t GetContainerSize(std::deque<T>& deque)
		{
			static auto t = T();
			return 8 + deque.size() * _sizeof(t);
		}
		template <class T>
		static size_t GetContainerSize(Deque<T>& deque)
		{
			static auto t = T();
			return 8 + deque.size() * _sizeof(t);
		}
	};

	template <>
	bool ArrayBuffer::WriteList(std::list<std::string>& list);
	template <>
	bool ArrayBuffer::WriteVector(std::vector<std::string>& vector);
	template <>
	bool ArrayBuffer::WriteDeque(std::deque<std::string>& deque);
	template <>
	bool ArrayBuffer::WriteDeque(Deque<std::string>& deque);

	template <>
	bool ArrayBuffer::WriteList(std::list<String>& list);
	template <>
	bool ArrayBuffer::WriteVector(std::vector<String>& vector);
	template <>
	bool ArrayBuffer::WriteDeque(std::deque<String>& deque);
	template <>
	bool ArrayBuffer::WriteDeque(Deque<String>& deque);
	
	template <>
	bool ArrayBuffer::Write(bool& value);
	template <>
	bool ArrayBuffer::Write(size_t& value);
	template <>
	bool ArrayBuffer::Write(std::chrono::steady_clock::time_point& value);
	template <>
	bool ArrayBuffer::Write(std::chrono::seconds& value);
	template <>
	bool ArrayBuffer::Write(std::chrono::microseconds& value);
	template <>
	bool ArrayBuffer::Write(std::chrono::milliseconds& value);
	template <>
	bool ArrayBuffer::Write(std::chrono::nanoseconds& value);
	template <>
	bool ArrayBuffer::Write(std::string& value);
	template <>
	bool ArrayBuffer::Write(String& value);
	template <>
	bool ArrayBuffer::Write(bool&& value);
	template <>
	bool ArrayBuffer::Write(size_t&& value);
	template <>
	bool ArrayBuffer::Write(std::chrono::steady_clock::time_point&& value);
	template <>
	bool ArrayBuffer::Write(std::chrono::seconds&& value);
	template <>
	bool ArrayBuffer::Write(std::chrono::microseconds&& value);
	template <>
	bool ArrayBuffer::Write(std::chrono::milliseconds&& value);
	template <>
	bool ArrayBuffer::Write(std::chrono::nanoseconds&& value);
	template <>
	bool ArrayBuffer::Write(std::string&& value);
	template <>
	bool ArrayBuffer::Write(String& value);

	template <>
	std::list<std::string> ArrayBuffer::ReadList();
	template <>
	std::vector<std::string> ArrayBuffer::ReadVector();
	template <>
	std::deque<std::string> ArrayBuffer::ReadDeque();

	template <>
	bool ArrayBuffer::Read();
	template <>
	size_t ArrayBuffer::Read();
	template <>
	std::chrono::steady_clock::time_point ArrayBuffer::Read();
	template <>
	std::chrono::seconds ArrayBuffer::Read();
	template <>
	std::chrono::microseconds ArrayBuffer::Read();
	template <>
	std::chrono::milliseconds ArrayBuffer::Read();
	template <>
	std::chrono::nanoseconds ArrayBuffer::Read();
	template <>
	std::string ArrayBuffer::Read();
	
	template <>
	size_t ArrayBuffer::GetContainerSize(std::list<std::string>& list);
	template <>
	size_t ArrayBuffer::GetContainerSize(std::vector<std::string>& vector);
	template <>
	size_t ArrayBuffer::GetContainerSize(std::deque<std::string>& deque);
	template <>
	size_t ArrayBuffer::GetContainerSize(Deque<std::string>& deque);
}

namespace Buffer
{
	/// <summary>
	/// Writes an UInt32 to the buffer
	/// </summary>
	/// <param name="value">value to write</param>
	/// <param name="buffer">buffer to write to</param>
	/// <param name="offset">offset to write at, is updated with the written size</param>
	void Write(uint32_t value, unsigned char* buffer, size_t& offset);
	void Write(uint32_t value, std::ostream* buffer, size_t& offset);
	/// <summary>
	/// Writes an UInt64 to the buffer
	/// </summary>
	/// <param name="value">value to write</param>
	/// <param name="buffer">buffer to write to</param>
	/// <param name="offset">offset to write at, is updated with the written size</param>
	void Write(uint64_t value, unsigned char* buffer, size_t& offset);
	void Write(uint64_t value, std::ostream* buffer, size_t& offset);
	/// <summary>
	/// Writes a Boolean to the buffer
	/// </summary>
	/// <param name="value">value to write</param>
	/// <param name="buffer">buffer to write to</param>
	/// <param name="offset">offset to write at, is updated with the written size</param>
	void Write(bool value, unsigned char* buffer, size_t& offset);
	void Write(bool value, std::ostream* buffer, size_t& offset);
	/// <summary>
	/// Writes an Int32 to the buffer
	/// </summary>
	/// <param name="value">value to write</param>
	/// <param name="buffer">buffer to write to</param>
	/// <param name="offset">offset to write at, is updated with the written size</param>
	void Write(int32_t value, unsigned char* buffer, size_t& offset);
	void Write(int32_t value, std::ostream* buffer, size_t& offset);
	/// <summary>
	/// Wrotes an Int64 to the buffer
	/// </summary>
	/// <param name="value">value to write</param>
	/// <param name="buffer">buffer to write to</param>
	/// <param name="offset">offset to write at, is updated with the written size</param>
	void Write(int64_t value, unsigned char* buffer, size_t& offset);
	void Write(int64_t value, std::ostream* buffer, size_t& offset);
	/// <summary>
	/// Writes a Float to the buffer
	/// </summary>
	/// <param name="value">value to write</param>
	/// <param name="buffer">buffer to write to</param>
	/// <param name="offset">offset to write at, is updated with the written size</param>
	void Write(float value, unsigned char* buffer, size_t& offset);
	void Write(float value, std::ostream* buffer, size_t& offset);
	/// <summary>
	/// Writes a Double to the buffer
	/// </summary>
	/// <param name="value">value to write</param>
	/// <param name="buffer">buffer to write to</param>
	/// <param name="offset">offset to write at, is updated with the written size</param>
	void Write(double value, unsigned char* buffer, size_t& offset);
	void Write(double value, std::ostream* buffer, size_t& offset);
	/// <summary>
	/// Writes a String to the buffer
	/// </summary>
	/// <param name="value">value to write</param>
	/// <param name="buffer">buffer to write to</param>
	/// <param name="offset">offset to write at, is updated with the written size</param>
	void Write(std::string value, unsigned char* buffer, size_t& offset);
	void Write(std::string value, std::ostream* buffer, size_t& offset);
	/// <summary>
	/// writes nanoseconds to the buffer
	/// </summary>
	/// <param name=""></param>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	void Write(std::chrono::nanoseconds value, unsigned char* buffer, size_t& offset);
	void Write(std::chrono::nanoseconds value, std::ostream* buffer, size_t& offset);
	/// <summary>
	/// writes microseconds to the buffer
	/// </summary>
	/// <param name=""></param>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	void Write(std::chrono::microseconds value, unsigned char* buffer, size_t& offset);
	void Write(std::chrono::microseconds value, std::ostream* buffer, size_t& offset);
	/// <summary>
	/// writes milliseconds to the buffer
	/// </summary>
	/// <param name=""></param>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	void Write(std::chrono::milliseconds value, unsigned char* buffer, size_t& offset);
	void Write(std::chrono::milliseconds value, std::ostream* buffer, size_t& offset);
	/// <summary>
	/// writes seconds to the buffer
	/// </summary>
	/// <param name=""></param>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	void Write(std::chrono::seconds value, unsigned char* buffer, size_t& offset);
	void Write(std::chrono::seconds value, std::ostream* buffer, size_t& offset);
	/// <summary>
	/// Writes size_t to the buffer
	/// </summary>
	/// <param name="value"></param>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	void WriteSize(size_t value, unsigned char* buffer, size_t& offset);
	void WriteSize(size_t value, std::ostream* buffer, size_t& offset);
	/// <summary>
	/// Writes a timepoint to the buffer
	/// </summary>
	/// <param name="value"></param>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	void Write(std::chrono::steady_clock::time_point value, unsigned char* buffer, size_t& offset);
	void Write(std::chrono::steady_clock::time_point value, std::ostream* buffer, size_t& offset);
	/// <summary>
	/// Writes an unsigned char to the buffer
	/// </summary>
	/// <param name=""></param>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	void Write(unsigned char value, unsigned char* buffer, size_t& offset);
	void Write(unsigned char value, std::ostream* buffer, size_t& offset);
	/// <summary>
	/// Writes a buffer to the buffer
	/// </summary>
	/// <param name="value"></param>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <param name="count"></param>
	void Write(unsigned char* value, unsigned char* buffer, size_t& offset, size_t count);
	void Write(unsigned char* value, std::ostream* buffer, size_t& offset, size_t count);

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
	template <class T>
	T Read(std::istream* buffer, size_t& offset)
	{
		T value;
		buffer->read((char*)&value, sizeof(T));
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
	uint32_t ReadUInt32(std::istream* buffer, size_t& offset);
	/// <summary>
	/// Reads an UInt64 from the buffer
	/// </summary>
	/// <param name="buffer">buffer to read from</param>
	/// <param name="offset">Offset to read at, is updated with the read size</param>
	/// <returns>the read value</returns>
	uint64_t ReadUInt64(unsigned char* buffer, size_t& offset);
	uint64_t ReadUInt64(std::istream* buffer, size_t& offset);
	/// <summary>
	/// Reads a Boolean from the buffer
	/// </summary>
	/// <param name="buffer">buffer to read from</param>
	/// <param name="offset">Offset to read at, is updated with the read size</param>
	/// <returns>the read value</returns>
	bool ReadBool(unsigned char* buffer, size_t& offset);
	bool ReadBool(std::istream* buffer, size_t& offset);
	/// <summary>
	/// Reads an Int32 from the buffer
	/// </summary>
	/// <param name="buffer">buffer to read from</param>
	/// <param name="offset">Offset to read at, is updated with the read size</param>
	/// <returns>the read value</returns>
	int32_t ReadInt32(unsigned char* buffer, size_t& offset);
	int32_t ReadInt32(std::istream* buffer, size_t& offset);
	/// <summary>
	/// Reads an Int64 from the buffer
	/// </summary>
	/// <param name="buffer">buffer to read from</param>
	/// <param name="offset">Offset to read at, is updated with the read size</param>
	/// <returns>the read value</returns>
	int64_t ReadInt64(unsigned char* buffer, size_t& offset);
	int64_t ReadInt64(std::istream* buffer, size_t& offset);
	/// <summary>
	/// Reads a Float from the buffer
	/// </summary>
	/// <param name="buffer">buffer to read from</param>
	/// <param name="offset">Offset to read at, is updated with the read size</param>
	/// <returns>the read value</returns>
	float ReadFloat(unsigned char* buffer, size_t& offset);
	float ReadFloat(std::istream* buffer, size_t& offset);
	/// <summary>
	/// Reads a Double from the buffer
	/// </summary>
	/// <param name="buffer">buffer to read from</param>
	/// <param name="offset">Offset to read at, is updated with the read size</param>
	/// <returns>the read value</returns>
	double ReadDouble(unsigned char* buffer, size_t& offset);
	double ReadDouble(std::istream* buffer, size_t& offset);
	/// <summary>
	/// Reads a String from the buffer
	/// </summary>
	/// <param name="buffer">buffer to read from</param>
	/// <param name="offset">Offset to read at, is updated with the read size</param>
	/// <returns>the read value</returns>
	std::string ReadString(unsigned char* buffer, size_t& offset);
	std::string ReadString(std::istream* buffer, size_t& offset);
	/// <summary>
	/// Reads an unsigned char from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <returns></returns>
	unsigned char ReadUChar(unsigned char* buffer, size_t& offset);
	unsigned char ReadUChar(std::istream* buffer, size_t& offset);
	/// <summary>
	/// Reads a buffer from the buffer
	/// </summary>
	unsigned char* ReadBuffer(unsigned char* buffer, size_t& offset, size_t count);
	unsigned char* ReadBuffer(std::istream* buffer, size_t& offset, size_t count);

	/// <summary>
	/// Reads nanoseconds from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <returns></returns>
	std::chrono::nanoseconds ReadNanoSeconds(unsigned char* buffer, size_t& offset);
	std::chrono::nanoseconds ReadNanoSeconds(std::istream* buffer, size_t& offset);
	/// <summary>
	/// Reads milliseconds from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <returns></returns>
	std::chrono::milliseconds ReadMilliSeconds(unsigned char* buffer, size_t& offset);
	std::chrono::milliseconds ReadMilliSeconds(std::istream* buffer, size_t& offset);
	/// <summary>
	/// Reads Microseconds from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <returns></returns>
	std::chrono::microseconds ReadMicroSeconds(unsigned char* buffer, size_t& offset);
	std::chrono::microseconds ReadMicroSeconds(std::istream* buffer, size_t& offset);
	/// <summary>
	/// Reads seconds from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <returns></returns>
	std::chrono::seconds ReadSeconds(unsigned char* buffer, size_t& offset);
	std::chrono::seconds ReadSeconds(std::istream* buffer, size_t& offset);
	/// <summary>
	/// Reads a size from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <returns></returns>
	size_t ReadSize(unsigned char* buffer, size_t& offset);
	size_t ReadSize(std::istream* buffer, size_t& offset);
	/// <summary>
	/// Reads a timepoint from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <returns></returns>
	std::chrono::steady_clock::time_point ReadTime(unsigned char* buffer, size_t& offset);
	std::chrono::steady_clock::time_point ReadTime(std::istream* buffer, size_t& offset);

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
		void WriteList(std::list<std::string>& list, std::ostream* buffer, size_t& offset);
		void ReadList(std::list<std::string>& list, unsigned char* buffer, size_t& offset);
		void ReadList(std::list<std::string>& list, std::istream* buffer, size_t& offset);

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
		void WriteList(std::list<T>& list, std::ostream* buffer, size_t& offset)
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
		template <class T>
		void ReadList(std::list<T>& list, std::istream* buffer, size_t& offset)
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
		template <class T>
		size_t GetVectorSize(Vector<T>& vector)
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
		template <class T>
		void WriteVector(Vector<T>& vector, unsigned char* buffer, size_t& offset)
		{
			Buffer::WriteSize(vector.size(), buffer, offset);
			for (int64_t i = 0; i < (int64_t)vector.size(); i++)
				Buffer::Write(vector[i], buffer, offset);
		}
		template <class T>
		void WriteVector(std::vector<T>& vector, std::ostream* buffer, size_t& offset)
		{
			Buffer::WriteSize(vector.size(), buffer, offset);
			for (int64_t i = 0; i < (int64_t)vector.size(); i++)
				Buffer::Write(vector[i], buffer, offset);
		}
		template <class T>
		void WriteVector(Vector<T>& vector, std::ostream* buffer, size_t& offset)
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
		template <class T>
		std::vector<T> ReadVector(std::istream* buffer, size_t& offset)
		{
			std::vector<T> vector;
			size_t len = Buffer::ReadSize(buffer, offset);
			for (int64_t i = 0; i < (int64_t)len; i++)
				vector.push_back(Buffer::Read<T>(buffer, offset));
			return vector;
		}
	}

	/// <summary>
	/// Provides buffer operations for vectors of basic types
	/// </summary>
	namespace DequeBasic
	{
		template <class T>
		size_t GetDequeSize(std::deque<T>& deque)
		{
			return 8 + (deque.size() * sizeof(T));
		}
		template <class T>
		size_t GetDequeSize(Deque<T>& deque)
		{
			return 8 + (deque.size() * sizeof(T));
		}
		template <class T>
		size_t GetDequeSize(unsigned char* buffer, size_t offset)
		{
			size_t length = *((size_t*)(buffer + offset));
			return length;
		}
		template <class T>
		void WriteDeque(std::deque<T>& deque, unsigned char* buffer, size_t& offset)
		{
			Buffer::WriteSize(deque.size(), buffer, offset);
			for (int64_t i = 0; i < (int64_t)deque.size(); i++)
				Buffer::Write(deque[i], buffer, offset);
		}
		template <class T>
		void WriteDeque(Deque<T>& deque, unsigned char* buffer, size_t& offset)
		{
			Buffer::WriteSize(deque.size(), buffer, offset);
			for (int64_t i = 0; i < (int64_t)deque.size(); i++)
				Buffer::Write(deque[i], buffer, offset);
		}
		template <class T>
		void WriteDeque(std::deque<T>& deque, std::ostream* buffer, size_t& offset)
		{
			Buffer::WriteSize(deque.size(), buffer, offset);
			for (int64_t i = 0; i < (int64_t)deque.size(); i++)
				Buffer::Write(deque[i], buffer, offset);
		}
		template <class T>
		void WriteDeque(Deque<T>& deque, std::ostream* buffer, size_t& offset)
		{
			Buffer::WriteSize(deque.size(), buffer, offset);
			for (int64_t i = 0; i < (int64_t)deque.size(); i++)
				Buffer::Write(deque[i], buffer, offset);
		}
		template <class T>
		std::deque<T> ReadDeque(unsigned char* buffer, size_t& offset)
		{
			std::deque<T> deque;
			size_t len = Buffer::ReadSize(buffer, offset);
			for (int64_t i = 0; i < (int64_t)len; i++)
				deque.push_back(Buffer::Read<T>(buffer, offset));
			return deque;
		}
		template <class T>
		std::deque<T> ReadDeque(std::istream* buffer, size_t& offset)
		{
			std::deque<T> deque;
			size_t len = Buffer::ReadSize(buffer, offset);
			for (int64_t i = 0; i < (int64_t)len; i++)
				deque.push_back(Buffer::Read<T>(buffer, offset));
			return deque;
		}
	}
}
