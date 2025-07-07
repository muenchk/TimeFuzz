#include "BufferOperations.h"
#include "Utility.h"
#include "Logging.h"

namespace Buffer
{
	template <>
	int64_t _sizeof(std::string& value)
	{
		return value.length() + 8;
	}
	template <>
	int64_t _sizeof(String& value)
	{
		return value.size() + 8;
	}
	template <>
	int64_t _sizeof(size_t&)
	{
		return 8;
	}

	template <>
	bool ArrayBuffer::Write(bool& value)
	{
		if (1 > Free())
			return false;
		unsigned char* ptr = (unsigned char*)(_buffer + _offset);
		*ptr = (unsigned char)value;
		_offset += 1;  // size written
		return true;
	}
	template <>
	bool ArrayBuffer::Write(bool&& value)
	{
		if (1 > Free())
			return false;
		unsigned char* ptr = (unsigned char*)(_buffer + _offset);
		*ptr = (unsigned char)value;
		_offset += 1;  // size written
		return true;
	}

	template <>
	bool ArrayBuffer::Write(size_t& value)
	{
		if (8 > Free())
			return false;
		uint64_t* ptr = (uint64_t*)(_buffer + _offset);
		uint64_t sz = (uint64_t)value;
		*ptr = sz;
		_offset += 8;
		return true;
	}
	template <>
	bool ArrayBuffer::Write(size_t&& value)
	{
		if (8 > Free())
			return false;
		uint64_t* ptr = (uint64_t*)(_buffer + _offset);
		uint64_t sz = (uint64_t)value;
		*ptr = sz;
		_offset += 8;
		return true;
	}
	template <>
	bool ArrayBuffer::Write(std::chrono::steady_clock::time_point& value)
	{
		auto nanotp = std::chrono::time_point_cast<std::chrono::nanoseconds>(value);
		auto nanodur = nanotp.time_since_epoch();
		auto nano = std::chrono::duration_cast<std::chrono::nanoseconds>(nanodur);
		Write<std::chrono::nanoseconds>(nano);
		return true;
	}
	template <>
	bool ArrayBuffer::Write(std::chrono::steady_clock::time_point&& value)
	{
		auto nanotp = std::chrono::time_point_cast<std::chrono::nanoseconds>(value);
		auto nanodur = nanotp.time_since_epoch();
		auto nano = std::chrono::duration_cast<std::chrono::nanoseconds>(nanodur);
		Write<std::chrono::nanoseconds>(nano);
		return true;
	}
	template <>
	bool ArrayBuffer::Write(std::chrono::seconds& value)
	{
		return Write<uint64_t>((uint64_t)value.count());
	}
	template <>
	bool ArrayBuffer::Write(std::chrono::seconds&& value)
	{
		return Write<uint64_t>((uint64_t)value.count());
	}
	template <>
	bool ArrayBuffer::Write(std::chrono::microseconds& value)
	{
		return Write<uint64_t>((uint64_t)value.count());
	}
	template <>
	bool ArrayBuffer::Write(std::chrono::microseconds&& value)
	{
		return Write<uint64_t>((uint64_t)value.count());
	}
	template <>
	bool ArrayBuffer::Write(std::chrono::milliseconds& value)
	{
		return Write<uint64_t>((uint64_t)value.count());
	}
	template <>
	bool ArrayBuffer::Write(std::chrono::milliseconds&& value)
	{
		return Write<uint64_t>((uint64_t)value.count());
	}
	template <>
	bool ArrayBuffer::Write(std::chrono::nanoseconds& value)
	{
		return Write<uint64_t>((uint64_t)value.count());
	}
	template <>
	bool ArrayBuffer::Write(std::chrono::nanoseconds&& value)
	{
		return Write<uint64_t>((uint64_t)value.count());
	}
	template <>
	bool ArrayBuffer::Write(std::string& value)
	{
		if (_sizeof(value) > Free())
			return false;
		Write<size_t>(value.length());
		if (value.empty() == false) {
			WriteIntern(value.c_str(), (int64_t)value.length());
		}
		return true;
	}
	template <>
	bool ArrayBuffer::Write(std::string&& value)
	{
		if (_sizeof(value) > Free())
			return false;
		Write<size_t>(value.length());
		if (value.empty() == false) {
			WriteIntern(value.c_str(), (int64_t)value.length());
		}
		return true;
	}
	template <>
	bool ArrayBuffer::Write(String& value)
	{
		if (_sizeof(value) > Free())
			return false;
		Write<size_t>(value.size());
		if (value.empty() == false) {
			WriteIntern(value.c_str(), (int64_t)value.size());
		}
		return true;
	}
	template <>
	bool ArrayBuffer::Write(String&& value)
	{
		if (_sizeof(value) > Free())
			return false;
		Write<size_t>(value.size());
		if (value.empty() == false) {
			WriteIntern(value.c_str(), (int64_t)value.size());
		}
		return true;
	}

	template <>
	bool ArrayBuffer::Read()
	{
		if (1 > Free())
			throw std::out_of_range("Tried to read after the end of the buffer");
		_offset += 1;  // size read
		return (bool)(*((unsigned char*)(_buffer + _offset - 1)));
	}
	template <>
	size_t ArrayBuffer::Read()
	{
		if (8 > Free())
			throw std::out_of_range("Tried to read after the end of the buffer");
		_offset += 8;
		uint64_t sz = *((uint64_t*)(_buffer + _offset - 8));
		return (size_t)sz;
	}
	template <>
	std::chrono::steady_clock::time_point ArrayBuffer::Read()
	{
		auto nano = Read<std::chrono::nanoseconds>();
		auto tp = std::chrono::time_point<std::chrono::steady_clock>(nano);
		return tp;
	}
	template <>
	std::chrono::seconds ArrayBuffer::Read()
	{
		return std::chrono::seconds(Read<uint64_t>());
	}
	template <>
	std::chrono::microseconds ArrayBuffer::Read()
	{
		return std::chrono::microseconds(Read<uint64_t>());
	}
	template <>
	std::chrono::milliseconds ArrayBuffer::Read()
	{
		return std::chrono::milliseconds(Read<uint64_t>());
	}
	template <>
	std::chrono::nanoseconds ArrayBuffer::Read()
	{
		return std::chrono::nanoseconds(Read<uint64_t>());
	}
	template <>
	std::string ArrayBuffer::Read()
	{
		if (8 > Free())
			throw std::out_of_range("Tried to read after the end of the buffer");
		size_t len = Read<size_t>();
		if (len != 0) {
			std::string tmp = std::string((char*)_buffer + _offset, len);
			_offset += len;
			return tmp;
		} else
			return "";
	}

	

	template <>
	bool ArrayBuffer::WriteList(std::list<std::string>& list)
	{
		int64_t sz = 8;
		auto itr = list.begin();
		while (itr != list.end()) {
			sz += _sizeof(*itr);
			itr++;
		}
		if (sz > Free())
			return false;
		Write<uint64_t>(sz);
		itr = list.begin();
		while (itr != list.end())
		{
			Write<std::string>(*itr);
			itr++;
		}
		return true;
	}
	template <>
	bool ArrayBuffer::WriteVector(std::vector<std::string>& vector)
	{
		int64_t sz = 8;
		for (int64_t i = 0; i < (int64_t)vector.size(); i++)
			sz += _sizeof(vector[i]);
		if (sz > Free())
			return false;
		Write<uint64_t>(sz);
		for (int64_t i = 0; i < (int64_t)vector.size(); i++)
			Write<std::string>(vector[i]);
		return true;
	}
	template <>
	bool ArrayBuffer::WriteDeque(std::deque<std::string>& deque)
	{
		int64_t sz = 8;
		for (int64_t i = 0; i < (int64_t)deque.size(); i++)
			sz += _sizeof(deque[i]);
		if (sz > Free())
			return false;
		Write<uint64_t>(sz);
		for (int64_t i = 0; i < (int64_t)deque.size(); i++)
			Write<std::string>(deque[i]);
		return true;
	}
	template <>
	bool ArrayBuffer::WriteDeque(Deque<std::string>& deque)
	{
		int64_t sz = 8;
		for (int64_t i = 0; i < (int64_t)deque.size(); i++)
			sz += _sizeof(deque[i]);
		if (sz > Free())
			return false;
		Write<uint64_t>(sz);
		for (int64_t i = 0; i < (int64_t)deque.size(); i++)
			Write<std::string>(deque[i]);
		return true;
	}
	template <>
	bool ArrayBuffer::WriteList(std::list<String>& list)
	{
		int64_t sz = 8;
		auto itr = list.begin();
		while (itr != list.end()) {
			sz += _sizeof(*itr);
			itr++;
		}
		if (sz > Free())
			return false;
		Write<uint64_t>(sz);
		itr = list.begin();
		while (itr != list.end()) {
			Write<std::string>(*itr);
			itr++;
		}
		return true;
	}
	template <>
	bool ArrayBuffer::WriteVector(std::vector<String>& vector)
	{
		int64_t sz = 8;
		for (int64_t i = 0; i < (int64_t)vector.size(); i++)
			sz += _sizeof(vector[i]);
		if (sz > Free())
			return false;
		Write<uint64_t>(sz);
		for (int64_t i = 0; i < (int64_t)vector.size(); i++)
			Write<std::string>(vector[i]);
		return true;
	}
	template <>
	bool ArrayBuffer::WriteDeque(std::deque<String>& deque)
	{
		int64_t sz = 8;
		for (int64_t i = 0; i < (int64_t)deque.size(); i++)
			sz += _sizeof(deque[i]);
		if (sz > Free())
			return false;
		Write<uint64_t>(sz);
		for (int64_t i = 0; i < (int64_t)deque.size(); i++)
			Write<std::string>(deque[i]);
		return true;
	}
	template <>
	bool ArrayBuffer::WriteDeque(Deque<String>& deque)
	{
		int64_t sz = 8;
		for (int64_t i = 0; i < (int64_t)deque.size(); i++)
			sz += _sizeof(deque[i]);
		if (sz > Free())
			return false;
		Write<uint64_t>(sz);
		for (int64_t i = 0; i < (int64_t)deque.size(); i++)
			Write<std::string>(deque[i]);
		return true;
	}
	

	template <>
	std::list<std::string> ArrayBuffer::ReadList()
	{
		if (8 > Free())
			throw std::out_of_range("Tried to read after the end of the buffer");
		uint64_t len = Read<uint64_t>() - 8;
		if ((int64_t)len > Free())
			throw std::out_of_range("Tried to read after the end of the buffer");
		uint64_t read = 0;
		std::string tmp = "";
		std::list<std::string> list;
		while (read < len && CalcStringLength(_buffer, _offset) + read <= len) {
			tmp = Read<std::string>();
			read += _sizeof(tmp);
			list.push_back(tmp);
		}
		return list;
	}
	template <>
	std::vector<std::string> ArrayBuffer::ReadVector()
	{
		if (8 > Free())
			throw std::out_of_range("Tried to read after the end of the buffer");
		uint64_t len = Read<uint64_t>() - 8;
		if ((int64_t)len > Free())
			throw std::out_of_range("Tried to read after the end of the buffer");
		uint64_t read = 0;
		std::string tmp = "";
		std::vector<std::string> vector;
		while (read < len && CalcStringLength(_buffer, _offset) + read <= len) {
			tmp = Read<std::string>();
			read += _sizeof(tmp);
			vector.push_back(tmp);
		}
		return vector;
	}
	template <>
	std::deque<std::string> ArrayBuffer::ReadDeque()
	{
		if (8 > Free())
			throw std::out_of_range("Tried to read after the end of the buffer");
		uint64_t len = Read<uint64_t>() - 8;
		if ((int64_t)len > Free())
			throw std::out_of_range("Tried to read after the end of the buffer");
		uint64_t read = 0;
		std::string tmp = "";
		std::deque<std::string> deque;
		while (read < len && CalcStringLength(_buffer, _offset) + read <= len) {
			tmp = Read<std::string>();
			read += _sizeof(tmp);
			deque.push_back(tmp);
		}
		return deque;
	}
	template <>
	size_t ArrayBuffer::GetContainerSize(std::list<std::string>& list)
	{
		size_t sz = 8;
		auto itr = list.begin();
		while (itr != list.end()) {
			sz += _sizeof(*itr);
			itr++;
		}
		return sz;
	}
	template <>
	size_t ArrayBuffer::GetContainerSize(std::vector<std::string>& vector)
	{
		size_t sz = 8;
		for (int64_t i = 0; i < (int64_t)vector.size(); i++)
			sz += _sizeof(vector[i]);
		return sz;
	}
	template <>
	size_t ArrayBuffer::GetContainerSize(std::deque<std::string>& deque)
	{
		size_t sz = 8;
		for (int64_t i = 0; i < (int64_t)deque.size(); i++)
			sz += _sizeof(deque[i]);
		return sz;
	}
	template <>
	size_t ArrayBuffer::GetContainerSize(Deque<std::string>& deque)
	{
		size_t sz = 8;
		for (int64_t i = 0; i < (int64_t)deque.size(); i++)
			sz += _sizeof(deque[i]);
		return sz;
	}
}
