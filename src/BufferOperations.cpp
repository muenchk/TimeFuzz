#include "BufferOperations.h"

#include <cstring>

namespace Buffer
{
	void Write(uint32_t value, unsigned char* buffer, size_t& offset)
	{
		uint32_t* ptr = (uint32_t*)(buffer + offset);
		*ptr = value;
		offset += 4;  // size written
	}

	void Write(uint64_t value, unsigned char* buffer, size_t& offset)
	{
		uint64_t* ptr = (uint64_t*)(buffer + offset);
		*ptr = value;
		offset += 8;  // size written
	}

	void Write(bool value, unsigned char* buffer, size_t& offset)
	{
		unsigned char* ptr = (unsigned char*)(buffer + offset);
		*ptr = (unsigned char)value;
		offset += 1;  // size written
	}

	void Write(int32_t value, unsigned char* buffer, size_t& offset)
	{
		int32_t* ptr = (int32_t*)(buffer + offset);
		*ptr = value;
		offset += 4;  // size written
	}

	void Write(int64_t value, unsigned char* buffer, size_t& offset)
	{
		int64_t* ptr = (int64_t*)(buffer + offset);
		*ptr = value;
		offset += 8;  // size written
	}

	void Write(float value, unsigned char* buffer, size_t& offset)
	{
		float* ptr = (float*)(buffer + offset);
		*ptr = value;
		offset += 4;  // size written
	}

	void Write(std::string value, unsigned char* buffer, size_t& offset)
	{
		size_t length = value.length();
		*((size_t*)(buffer + offset)) = length;
		offset += 8;
		if (length != 0) {
			std::strncpy((char*)(buffer + offset), value.c_str(), length);
			offset += (int)length;
		}
	}

	void Write(std::chrono::nanoseconds value, unsigned char* buffer, size_t& offset)
	{
		uint64_t* ptr = (uint64_t*)(buffer + offset);
		*ptr = value.count();
		offset += 8;
	}

	void Write(std::chrono::microseconds value, unsigned char* buffer, size_t& offset)
	{
		uint64_t* ptr = (uint64_t*)(buffer + offset);
		*ptr = value.count();
		offset += 8;
	}

	void Write(std::chrono::milliseconds value, unsigned char* buffer, size_t& offset)
	{
		uint64_t* ptr = (uint64_t*)(buffer + offset);
		*ptr = value.count();
		offset += 8;
	}

	void WriteSize(size_t value, unsigned char* buffer, size_t& offset)
	{
		uint64_t* ptr = (uint64_t*)(buffer + offset);
		uint64_t sz = value;
		*ptr = sz;
		offset += 8;
	}

	void Write(std::chrono::seconds value, unsigned char* buffer, size_t& offset)
	{
		uint64_t* ptr = (uint64_t*)(buffer + offset);
		*ptr = value.count();
		offset += 8;
	}

	void Write(std::chrono::steady_clock::time_point value, unsigned char* buffer, size_t& offset)
	{
		auto nanotp = std::chrono::time_point_cast<std::chrono::nanoseconds>(value);
		auto nanodur = nanotp.time_since_epoch();
		auto nano = std::chrono::duration_cast<std::chrono::nanoseconds>(nanodur);
		Write(nano, buffer, offset);
	}

	uint32_t ReadUInt32(unsigned char* buffer, size_t& offset)
	{
		offset += 4;  // size read
		return *((uint32_t*)(buffer + offset - 4));
	}

	uint64_t ReadUInt64(unsigned char* buffer, size_t& offset)
	{
		offset += 8;  // size read
		return *((uint64_t*)(buffer + offset - 8));
	}

	bool ReadBool(unsigned char* buffer, size_t& offset)
	{
		offset += 1;  // size read
		return (bool)(*((unsigned char*)(buffer + offset - 1)));
	}

	int32_t ReadInt32(unsigned char* buffer, size_t& offset)
	{
		offset += 4;  // size read
		return *((int32_t*)(buffer + offset - 4));
	}

	int64_t ReadInt64(unsigned char* buffer, size_t& offset)
	{
		offset += 8;  // size read
		return *((int64_t*)(buffer + offset - 8));
	}

	float ReadFloat(unsigned char* buffer, size_t& offset)
	{
		offset += 4;  // size read
		return *((float*)(buffer + offset - 4));
	}

	std::string ReadString(unsigned char* buffer, size_t& offset)
	{
		size_t length = *((size_t*)(buffer + offset));
		offset += 8;
		if (length != 0) {
			std::string tmp = std::string((char*)(buffer + offset), length);
			offset += (int)length;
			return tmp;
		} else
			return "";
	}

	size_t CalcStringLength(std::string value)
	{
		size_t length = value.length();
		length += 8;
		return (int)length;
	}

	size_t CalcStringLength(unsigned char* buffer, size_t offset)
	{
		size_t length = *((size_t*)(buffer + offset));
		return length;
	}

	std::chrono::nanoseconds ReadNanoSeconds(unsigned char* buffer, size_t& offset)
	{
		offset += 8;
		return std::chrono::nanoseconds(*((uint64_t*)(buffer + offset - 8)));
	}

	std::chrono::milliseconds ReadMilliSeconds(unsigned char* buffer, size_t& offset)
	{
		offset += 8;
		return std::chrono::milliseconds(*((uint64_t*)(buffer + offset - 8)));
	}

	std::chrono::microseconds ReadMicroSeconds(unsigned char* buffer, size_t& offset)
	{
		offset += 8;
		return std::chrono::microseconds(*((uint64_t*)(buffer + offset - 8)));
	}

	std::chrono::seconds ReadSeconds(unsigned char* buffer, size_t& offset)
	{
		offset += 8;
		return std::chrono::seconds(*((uint64_t*)(buffer + offset - 8)));
	}

	size_t ReadSize(unsigned char* buffer, size_t& offset)
	{
		offset += 8;
		uint64_t sz = *((uint64_t*)(buffer + offset - 8));
		return (size_t)sz;
	}

	std::chrono::steady_clock::time_point ReadTime(unsigned char* buffer, size_t& offset)
	{
		auto nano = ReadNanoSeconds(buffer, offset);
		auto tp = std::chrono::time_point<std::chrono::steady_clock>(nano);
		return tp;
	}

	namespace List
	{

		size_t GetListLength(std::list<std::string>& list)
		{
			size_t sz = 0;
			// size of sequence
			sz += 8;
			for (auto& str : list)
				sz += Buffer::CalcStringLength(str);
			return sz;
		}

		size_t GetListLength(unsigned char* buffer, size_t offset)
		{
			size_t length = *((size_t*)(buffer + offset));
			return length;
		}

		void WriteList(std::list<std::string>& list, unsigned char* buffer, size_t& offset)
		{
			size_t off = offset;
			Buffer::WriteSize(list.size(), buffer, offset);
			auto itr = list.begin();
			while (itr != list.end()) {
				Buffer::Write(*itr, buffer, offset);
				itr++;
			}
			Buffer::WriteSize((size_t)(offset - off), buffer, off);
		}

		void ReadList(std::list<std::string>& list, unsigned char* buffer, size_t& offset)
		{
			// read length (includes the length field itself)
			size_t len = Buffer::ReadSize(buffer, offset) - 8;
			size_t read = 0;
			while (read < len && Buffer::CalcStringLength(buffer, offset) + read <= len) {
				list.push_back(Buffer::ReadString(buffer, offset));
			}
		}
	}

	namespace ListBasic
	{
		size_t GetListLength(unsigned char* buffer, size_t offset)
		{
			size_t length = *((size_t*)(buffer + offset));
			return length;
		}
	}
}
