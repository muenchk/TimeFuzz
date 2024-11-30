#include "BufferOperations.h"
#include "Utility.h"
#include "Logging.h"

#include <cstring>
#include <iostream>

namespace Buffer
{
	void Write(uint32_t value, unsigned char* buffer, size_t& offset)
	{
		uint32_t* ptr = (uint32_t*)(buffer + offset);
		*ptr = value;
		offset += 4;  // size written
	}
	void Write(uint32_t value, std::ostream* buffer, size_t& offset)
	{
		buffer->write((char*)&value, 4);
		offset += 4;  // size written
	}

	void Write(uint64_t value, unsigned char* buffer, size_t& offset)
	{
		uint64_t* ptr = (uint64_t*)(buffer + offset);
		*ptr = value;
		offset += 8;  // size written
	}
	void Write(uint64_t value, std::ostream* buffer, size_t& offset)
	{
		buffer->write((char*)&value, 8);
		offset += 8;  // size written
	}

	void Write(bool value, unsigned char* buffer, size_t& offset)
	{
		unsigned char* ptr = (unsigned char*)(buffer + offset);
		*ptr = (unsigned char)value;
		offset += 1;  // size written
	}
	void Write(bool value, std::ostream* buffer, size_t& offset)
	{
		char x = (char)value;
		buffer->write(&x, 1);
		offset += 1;  // size written
	}

	void Write(int32_t value, unsigned char* buffer, size_t& offset)
	{
		int32_t* ptr = (int32_t*)(buffer + offset);
		*ptr = value;
		offset += 4;  // size written
	}
	void Write(int32_t value, std::ostream* buffer, size_t& offset)
	{
		buffer->write((char*)&value, 4);
		offset += 4;  // size written
	}

	void Write(int64_t value, unsigned char* buffer, size_t& offset)
	{
		int64_t* ptr = (int64_t*)(buffer + offset);
		*ptr = value;
		offset += 8;  // size written
	}
	void Write(int64_t value, std::ostream* buffer, size_t& offset)
	{
		buffer->write((char*)&value, 8);
		offset += 8;  // size written
	}

	void Write(float value, unsigned char* buffer, size_t& offset)
	{
		float* ptr = (float*)(buffer + offset);
		*ptr = value;
		offset += 4;  // size written
	}
	void Write(float value, std::ostream* buffer, size_t& offset)
	{
		buffer->write((char*)&value, 4);
		offset += 4;  // size written
	}

	void Write(double value, unsigned char* buffer, size_t& offset)
	{
		double* ptr = (double*)(buffer + offset);
		*ptr = value;
		offset += 8;  // size written
	}

	void Write(double value, std::ostream* buffer, size_t& offset)
	{
		buffer->write((char*)&value, 8);
		offset += 8;  // size written
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
	void Write(std::string value, std::ostream* buffer, size_t& offset)
	{
		uint64_t length = value.length();
		buffer->write((char*)&length, 8);
		offset += 8;
		if (length != 0) {
			buffer->write(value.c_str(), length);
			offset += (int)length;
		}
	}

	void Write(std::chrono::nanoseconds value, unsigned char* buffer, size_t& offset)
	{
		uint64_t* ptr = (uint64_t*)(buffer + offset);
		*ptr = value.count();
		offset += 8;
	}
	void Write(std::chrono::nanoseconds value, std::ostream* buffer, size_t& offset)
	{
		uint64_t val = value.count();
		buffer->write((char*)&val, 8);
		offset += 8;
	}

	void Write(std::chrono::microseconds value, unsigned char* buffer, size_t& offset)
	{
		uint64_t* ptr = (uint64_t*)(buffer + offset);
		*ptr = value.count();
		offset += 8;
	}
	void Write(std::chrono::microseconds value, std::ostream* buffer, size_t& offset)
	{
		uint64_t val = value.count();
		buffer->write((char*)&val, 8);
		offset += 8;
	}

	void Write(std::chrono::milliseconds value, unsigned char* buffer, size_t& offset)
	{
		uint64_t* ptr = (uint64_t*)(buffer + offset);
		*ptr = value.count();
		offset += 8;
	}
	void Write(std::chrono::milliseconds value, std::ostream* buffer, size_t& offset)
	{
		uint64_t val = value.count();
		buffer->write((char*)&val, 8);
		offset += 8;
	}

	void Write(std::chrono::seconds value, unsigned char* buffer, size_t& offset)
	{
		uint64_t* ptr = (uint64_t*)(buffer + offset);
		*ptr = value.count();
		offset += 8;
	}
	void Write(std::chrono::seconds value, std::ostream* buffer, size_t& offset)
	{
		uint64_t val = value.count();
		buffer->write((char*)&val, 8);
		offset += 8;
	}

	void WriteSize(size_t value, unsigned char* buffer, size_t& offset)
	{
		uint64_t* ptr = (uint64_t*)(buffer + offset);
		uint64_t sz = (uint64_t)value;
		*ptr = sz;
		offset += 8;
	}
	void WriteSize(size_t value, std::ostream* buffer, size_t& offset)
	{
		uint64_t sz = (uint64_t)value;
		buffer->write((char*)&sz, 8);
		offset += 8;
	}

	void Write(std::chrono::steady_clock::time_point value, unsigned char* buffer, size_t& offset)
	{
		auto nanotp = std::chrono::time_point_cast<std::chrono::nanoseconds>(value);
		auto nanodur = nanotp.time_since_epoch();
		auto nano = std::chrono::duration_cast<std::chrono::nanoseconds>(nanodur);
		Write(nano, buffer, offset);
	}
	void Write(std::chrono::steady_clock::time_point value, std::ostream* buffer, size_t& offset)
	{
		auto nanotp = std::chrono::time_point_cast<std::chrono::nanoseconds>(value);
		auto nanodur = nanotp.time_since_epoch();
		auto nano = std::chrono::duration_cast<std::chrono::nanoseconds>(nanodur);
		Write(nano, buffer, offset);
	}

	void Write(unsigned char value, unsigned char* buffer, size_t& offset)
	{
		*(buffer + offset) = value;
		offset++;
	}
	void Write(unsigned char value, std::ostream* buffer, size_t& offset)
	{
		buffer->write((char*)&value, 1);
		offset++;
	}

	void Write(unsigned char* value, unsigned char* buffer, size_t& offset, size_t count)
	{
		memcpy(buffer + offset, value, count);
		offset += count;
	}
	void Write(unsigned char* value, std::ostream* buffer, size_t& offset, size_t count)
	{
		buffer->write((char*)value, count);
		offset += count;
	}

	uint32_t ReadUInt32(unsigned char* buffer, size_t& offset)
	{
		offset += 4;  // size read
		return *((uint32_t*)(buffer + offset - 4));
	}
	uint32_t ReadUInt32(std::istream* buffer, size_t& offset)
	{
		uint32_t value;
		buffer->read((char*)&value, 4);
		offset += 4;  // size read
		return value;
	}

	uint64_t ReadUInt64(unsigned char* buffer, size_t& offset)
	{
		offset += 8;  // size read
		return *((uint64_t*)(buffer + offset - 8));
	}
	uint64_t ReadUInt64(std::istream* buffer, size_t& offset)
	{
		uint64_t value;
		buffer->read((char*)&value, 8);
		offset += 8;  // size read
		return value;
	}

	bool ReadBool(unsigned char* buffer, size_t& offset)
	{
		offset += 1;  // size read
		return (bool)(*((unsigned char*)(buffer + offset - 1)));
	}
	bool ReadBool(std::istream* buffer, size_t& offset)
	{
		char x;
		buffer->read(&x, 1);
		offset += 1;  // size read
		return (bool)x;
	}

	int32_t ReadInt32(unsigned char* buffer, size_t& offset)
	{
		offset += 4;  // size read
		return *((int32_t*)(buffer + offset - 4));
	}
	int32_t ReadInt32(std::istream* buffer, size_t& offset)
	{
		int32_t value;
		buffer->read((char*)&value, 4);
		offset += 4;  // size read
		return value;
	}

	int64_t ReadInt64(unsigned char* buffer, size_t& offset)
	{
		offset += 8;  // size read
		return *((int64_t*)(buffer + offset - 8));
	}
	int64_t ReadInt64(std::istream* buffer, size_t& offset)
	{
		int64_t value;
		buffer->read((char*)&value, 8);
		offset += 8;  // size read
		return value;
	}

	float ReadFloat(unsigned char* buffer, size_t& offset)
	{
		offset += 4;  // size read
		return *((float*)(buffer + offset - 4));
	}
	float ReadFloat(std::istream* buffer, size_t& offset)
	{
		float value;
		buffer->read((char*)&value, 4);
		offset += 4;  // size read
		return value;
	}

	double ReadDouble(unsigned char* buffer, size_t& offset)
	{
		offset += 8;  // size read
		return *((double*)(buffer + offset - 8));
	}
	double ReadDouble(std::istream* buffer, size_t& offset)
	{
		double value;
		buffer->read((char*)&value, 8);
		offset += 8;  // size read
		return value;
	}

	std::string ReadString(unsigned char* buffer, size_t& offset)
	{
		size_t length = *((size_t*)(buffer + offset));
		offset += 8;
		if (length != 0) {
			std::string tmp = std::string((char*)(buffer + offset), length);
			offset += (int)length;
			return tmp;
		} else {
			return "";
		}
	}
	std::string ReadString(std::istream* buffer, size_t& offset)
	{
		uint64_t length;
		buffer->read((char*)&length, 8);
		offset += 8;
		if (length != 0) {
			char* buf = new char[length];
			buffer->read(buf, length);
			std::string tmp = std::string(buf, length);
			delete[] buf;
			offset += (int)length;
			return tmp;
		} else {
			return "";
		}
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

	unsigned char ReadUChar(unsigned char* buffer, size_t& offset)
	{
		offset++;
		return *(buffer + offset - 1);
	}
	unsigned char ReadUChar(std::istream* buffer, size_t& offset)
	{
		offset++;
		char value;
		buffer->read(&value, 1);
		return value;
	}

	unsigned char* ReadBuffer(unsigned char* buffer, size_t& offset, size_t count)
	{
		unsigned char* value = new unsigned char[count];
		memcpy(value, buffer + offset, count);
		offset += count;
		return value;
	}
	unsigned char* ReadBuffer(std::istream* buffer, size_t& offset, size_t count)
	{
		unsigned char* value = new unsigned char[count];
		buffer->read((char*)value, count);
		offset += count;
		return value;
	}

	std::chrono::nanoseconds ReadNanoSeconds(unsigned char* buffer, size_t& offset)
	{
		offset += 8;
		return std::chrono::nanoseconds(*((uint64_t*)(buffer + offset - 8)));
	}
	std::chrono::nanoseconds ReadNanoSeconds(std::istream* buffer, size_t& offset)
	{
		uint64_t value;
		buffer->read((char*)&value, 8);
		offset += 8;
		return std::chrono::nanoseconds(value);
	}

	std::chrono::milliseconds ReadMilliSeconds(unsigned char* buffer, size_t& offset)
	{
		offset += 8;
		return std::chrono::milliseconds(*((uint64_t*)(buffer + offset - 8)));
	}
	std::chrono::milliseconds ReadMilliSeconds(std::istream* buffer, size_t& offset)
	{
		uint64_t value;
		buffer->read((char*)&value, 8);
		offset += 8;
		return std::chrono::milliseconds(value);
	}

	std::chrono::microseconds ReadMicroSeconds(unsigned char* buffer, size_t& offset)
	{
		offset += 8;
		return std::chrono::microseconds(*((uint64_t*)(buffer + offset - 8)));
	}
	std::chrono::microseconds ReadMicroSeconds(std::istream* buffer, size_t& offset)
	{
		uint64_t value;
		buffer->read((char*)&value, 8);
		offset += 8;
		return std::chrono::microseconds(value);
	}

	std::chrono::seconds ReadSeconds(unsigned char* buffer, size_t& offset)
	{
		offset += 8;
		return std::chrono::seconds(*((uint64_t*)(buffer + offset - 8)));
	}
	std::chrono::seconds ReadSeconds(std::istream* buffer, size_t& offset)
	{
		uint64_t value;
		buffer->read((char*)&value, 8);
		offset += 8;
		return std::chrono::seconds(value);
	}

	size_t ReadSize(unsigned char* buffer, size_t& offset)
	{
		offset += 8;
		uint64_t sz = *((uint64_t*)(buffer + offset - 8));
		return (size_t)sz;
	}
	size_t ReadSize(std::istream* buffer, size_t& offset)
	{
		uint64_t value;
		buffer->read((char*)&value, 8);
		offset += 8;
		return (size_t)value;
	}

	std::chrono::steady_clock::time_point ReadTime(unsigned char* buffer, size_t& offset)
	{
		auto nano = ReadNanoSeconds(buffer, offset);
		auto tp = std::chrono::time_point<std::chrono::steady_clock>(nano);
		return tp;
	}
	std::chrono::steady_clock::time_point ReadTime(std::istream* buffer, size_t& offset)
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

		void WriteList(std::list<std::string>& list, std::ostream* buffer, size_t& offset)
		{
			size_t off = offset;
			size_t len = 0;
			auto itr = list.begin();
			while (itr != list.end())
			{
				len += CalcStringLength(*itr);
				itr++;
			}
			Buffer::WriteSize(len, buffer, offset);
			itr = list.begin();
			while (itr != list.end()) {
				Buffer::Write(*itr, buffer, offset);
				itr++;
			}
		}

		void ReadList(std::list<std::string>& list, unsigned char* buffer, size_t& offset)
		{
			// read length (includes the length field itself)
			size_t len = Buffer::ReadSize(buffer, offset) - 8;
			size_t read = 0;
			std::string tmp = "";
			while (read < len && Buffer::CalcStringLength(buffer, offset) + read <= len) {
				tmp = Buffer::ReadString(buffer, offset);
				read += Buffer::CalcStringLength(tmp);
				list.push_back(tmp);
			}
		}

		void ReadList(std::list<std::string>& list, std::istream* buffer, size_t& offset)
		{
			// read length (includes the length field itself)
			size_t len = Buffer::ReadSize(buffer, offset);
			size_t read = 0;
			std::string tmp = "";
			while (read < len) {// && Buffer::CalcStringLength(buffer, offset) + read <= len) {
				tmp = Buffer::ReadString(buffer, offset);
				read += Buffer::CalcStringLength(tmp);
				list.push_back(tmp);
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
