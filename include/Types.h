#pragma once

#include <string>
#include <atomic>
#include <exception>

#include "Threading.h"

class String
{
private:
	std::string* _str = nullptr;

	std::atomic_flag _lock = ATOMIC_FLAG_INIT;

protected:
	void lock()
	{
		while (_lock.test_and_set(std::memory_order_acquire))
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
			_lock.wait(true, std::memory_order_relaxed)
#endif
				;
	}
	void unlock()
	{
		_lock.clear(std::memory_order_release);
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
		_lock.notify_one();
#endif
	}

public:
	String() {}
	String(std::string& str)
	{
		_str = new std::string(str);
	}
	String(std::string&& str)
	{
		_str = new std::string(str);
	}
	String(const char* str)
	{
		_str = new std::string(str);
	}
	String(size_t _Count, const char _Ch)
	{
		_str = new std::string(_Count, _Ch);
	}
	// copy assignment
	String& operator=(String& other)
	{
		SpinlockA guard(_lock);
		SpinlockA guardO(other._lock);
		if (this == &other)
			return *this;

		if (_str) {
			delete _str;
			_str = nullptr;
		}
		if (other._str)
			_str = new std::string(*other._str);
		return *this;
	}
	// move assignment
	String& operator=(String&& other) noexcept
	{
		SpinlockA guard(_lock);
		SpinlockA guardO(other._lock);
		if (this == &other)
			return *this;

		if (_str)
			delete _str;
		_str = other._str;
		other._str = nullptr;
		return *this;
	}
	~String()
	{
		SpinlockA guard(_lock);
		if (_str)
			delete _str;
		_str = nullptr;
	}

	void reset()
	{
		SpinlockA guard(_lock);
		if (_str)
			delete _str;
		_str = nullptr;
	}

	size_t size()
	{
		SpinlockA guard(_lock);
		if (_str)
			return _str->size();
		else
			return 0;
	}

	const char* c_str()
	{
		SpinlockA guard(_lock);
		if (_str)
			return _str->c_str();
		else
			return "";
	}

	bool empty()
	{
		SpinlockA guard(_lock);
		if (_str)
			return _str->empty();
		else
			return true;
	}

	String& operator+=(std::string& rhs)
	{
		SpinlockA guard(_lock);
		if (_str) {
			*_str += rhs;
		} else {
			_str = new std::string(rhs);
		}
		return *this;
	}
	String& operator+=(const std::string& rhs)
	{
		SpinlockA guard(_lock);
		if (_str) {
			*_str += rhs;
		} else {
			_str = new std::string(rhs);
		}
		return *this;
	}
	String& operator+=(String& rhs)
	{
		SpinlockA guard(_lock);
		SpinlockA guardO(rhs._lock);
		if (_str) {
			if (rhs._str)
				*_str += *rhs._str;
		} else {
			if (rhs._str)
				_str = new std::string(*rhs._str);
		}
		return *this;
	}

	bool operator<(String& other)
	{
		SpinlockA guard(_lock);
		SpinlockA guardO(other._lock);
		if (_str && other._str)
			return *_str < *other._str;
		else if (_str)
			return true;
		else if (other._str)
			return false;
		else
			return false;
		return true;
	}
	bool operator>(String& rhs) { return rhs < *this; }
	bool operator<=(String& rhs) { return !(*this > rhs); }
	bool operator>=(String& rhs) { return !(*this < rhs); }
	bool operator==(String& rhs)
	{
		SpinlockA guard(_lock);
		SpinlockA guardO(rhs._lock);
		if (_str && rhs._str)
			return *_str == *rhs._str;
		else
			return false;
	}
	bool operator!=(String& rhs) { return !(*this == rhs); }

	char& operator[](std::size_t idx)
	{
		SpinlockA guard(_lock);
		if (_str)
			return (*_str)[idx];
		else
			throw std::out_of_range("string is empty");
	}

	operator std::string()
	{
		SpinlockA guard(_lock);
		if (_str)
			return *_str;
		else
			return std::string();
	}
};
