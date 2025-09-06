#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <exception>
#include <deque>
#include <memory>

#include "Threading.h"

template <class T>
class Vector;
template <class Y>
class Deque;

namespace std
{
	template <class Y>
	void swap(Vector<Y>& lhs, Vector<Y>& rhs)
	{
		lhs.swap(rhs);
	}
	template <class Y>
	void swap(Deque<Y>& lhs, Deque<Y>& rhs)
	{
		lhs.swap(rhs);
	}
}

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

template <class T>
class Vector
{
private:
	std::vector<T>* _vector = nullptr;

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
	Vector() {}
	Vector(std::vector<T>& vector)
	{
		_vector = new std::vector<T>(vector);
	}
	Vector(std::vector<T>&& vector)
	{
		_vector = new std::vector<T>(vector);
	}
	Vector(size_t _Count, const char _Ch)
	{
		_vector = new std::vector<T>(_Count, _Ch);
	}
	// copy assignment
	Vector& operator=(Vector& other)
	{
		SpinlockA guard(_lock);
		SpinlockA guardO(other._lock);
		if (this == &other)
			return *this;

		if (_vector) {
			delete _vector;
			_vector = nullptr;
		}
		if (other._vector)
			_vector = new std::vector<T>(*other._vector);
		return *this;
	}
	// move assignment
	Vector& operator=(Vector&& other) noexcept
	{
		SpinlockA guard(_lock);
		SpinlockA guardO(other._lock);
		if (this == &other)
			return *this;

		if (_vector)
			delete _vector;
		_vector = other._vector;
		other._vector = nullptr;
		return *this;
	}
	~Vector()
	{
		SpinlockA guard(_lock);
		if (_vector)
			delete _vector;
		_vector = nullptr;
	}

	void reset()
	{
		SpinlockA guard(_lock);
		if (_vector)
			delete _vector;
		_vector = nullptr;
	}

	size_t size()
	{
		SpinlockA guard(_lock);
		if (_vector)
			return _vector->size();
		else
			return 0;
	}

	T* data()
	{
		SpinlockA guard(_lock);
		if (_vector)
			return _vector->data();
		else
			return nullptr;
	}

	bool empty()
	{
		SpinlockA guard(_lock);
		if (_vector)
			return _vector->empty();
		else
			return true;
	}

	void clear()
	{
		SpinlockA guard(_lock);
		if (_vector) {
			delete _vector;
			_vector = nullptr;
		}
	}

	void push_back(T& value)
	{
		SpinlockA guard(_lock);
		if (_vector)
			_vector->push_back(value);
		else {
			_vector = new std::vector<T>();
			_vector->push_back(value);
		}
	}

	void push_back(T&& value)
	{
		SpinlockA guard(_lock);
		if (_vector)
			_vector->push_back(value);
		else {
			_vector = new std::vector<T>();
			_vector->push_back(value);
		}
	}

	std::vector<T>::iterator begin()
	{
		SpinlockA guard(_lock);
		if (_vector)
			return _vector->begin();
		else {
			_vector = new std::vector<T>();
			return _vector->begin();
		}
	}

	std::vector<T>::iterator end()
	{
		SpinlockA guard(_lock);
		if (_vector)
			return _vector->end();
		else {
			_vector = new std::vector<T>();
			return _vector->end();
		}
	}

	void shrink_to_fit()
	{
		SpinlockA guard(_lock);
		if (_vector)
			_vector->shrink_to_fit();
	}

	void swap(std::vector<T>& other)
	{
		SpinlockA guard(_lock);
		if (_vector)
			_vector->swap(other);
		else {
			_vector = new std::vector<T>();
			_vector->swap(other);
		}
	}

	void swap(Vector<T>& other)
	{
		SpinlockA guard(_lock);
		SpinlockA guardO(other._lock);
		if (_vector && other._vector)
			_vector->swap(*other._vector);
		else if (other._vector) {
			_vector = new std::vector<T>();
			_vector->swap(*other._vector);
		} else if (_vector) {
			other._vector = new std::vector<T>();
			_vector->swap(*other._vector);
		}
	}

	void reserve(size_t size)
	{
		SpinlockA guard(_lock);
		if (_vector)
			_vector->reserve(size);
		else {
			_vector = new std::vector<T>();
			_vector->reserve(size);
		}
	}

	T& operator[](std::size_t idx)
	{
		if (_vector)
			return (*_vector)[idx];
		else
			throw std::out_of_range("vector is empty");
	}

	operator std::vector<T>()
	{
		SpinlockA guard(_lock);
		if (_vector)
			return *_vector;
		else
			return std::vector<T>();
	}

	template <class Y>
	friend void std::swap(Vector<Y>& lhs, Vector<Y>& rhs);
};


template <class T>
class Deque
{
private:
	std::deque<T>* _deque = nullptr;

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
	Deque() {}
	Deque(std::deque<T>& deque)
	{
		_deque = new std::deque<T>(deque);
	}
	Deque(std::deque<T>&& deque)
	{
		_deque = new std::deque<T>(deque);
	}
	Deque(size_t _Count, const char _Ch)
	{
		_deque = new std::deque<T>(_Count, _Ch);
	}
	// copy assignment
	Deque& operator=(Deque& other)
	{
		SpinlockA guard(_lock);
		SpinlockA guardO(other._lock);
		if (this == &other)
			return *this;

		if (_deque) {
			delete _deque;
			_deque = nullptr;
		}
		if (other._deque)
			_deque = new std::deque<T>(*other._deque);
		return *this;
	}
	// move assignment
	Deque& operator=(Deque&& other) noexcept
	{
		SpinlockA guard(_lock);
		SpinlockA guardO(other._lock);
		if (this == &other)
			return *this;

		if (_deque)
			delete _deque;
		_deque = other._deque;
		other._deque = nullptr;
		return *this;
	}
	~Deque()
	{
		SpinlockA guard(_lock);
		if (_deque)
			delete _deque;
		_deque = nullptr;
	}

	void reset()
	{
		SpinlockA guard(_lock);
		if (_deque)
			delete _deque;
		_deque = nullptr;
	}

	size_t size()
	{
		SpinlockA guard(_lock);
		if (_deque)
			return _deque->size();
		else
			return 0;
	}

	T* data()
	{
		SpinlockA guard(_lock);
		if (_deque)
			return _deque->data();
		else
			return nullptr;
	}

	bool empty()
	{
		SpinlockA guard(_lock);
		if (_deque)
			return _deque->empty();
		else
			return true;
	}

	void clear()
	{
		SpinlockA guard(_lock);
		if (_deque) {
			delete _deque;
			_deque = nullptr;
		}
	}

	void push_back(T& value)
	{
		SpinlockA guard(_lock);
		if (_deque)
			_deque->push_back(value);
		else {
			_deque = new std::deque<T>();
			_deque->push_back(value);
		}
	}

	void push_back(T&& value)
	{
		SpinlockA guard(_lock);
		if (_deque)
			_deque->push_back(value);
		else {
			_deque = new std::deque<T>();
			_deque->push_back(value);
		}
	}

	std::deque<T>::iterator begin()
	{
		SpinlockA guard(_lock);
		if (_deque)
			return _deque->begin();
		else {
			_deque = new std::deque<T>();
			return _deque->begin();
		}
	}

	std::deque<T>::iterator end()
	{
		SpinlockA guard(_lock);
		if (_deque)
			return _deque->end();
		else {
			_deque = new std::deque<T>();
			return _deque->end();
		}
	}

	void shrink_to_fit()
	{
		SpinlockA guard(_lock);
		if (_deque)
			_deque->shrink_to_fit();
	}

	void swap(std::deque<T>& other)
	{
		SpinlockA guard(_lock);
		if (_deque)
			_deque->swap(other);
		else {
			_deque = new std::deque<T>();
			_deque->swap(other);
		}
	}

	void swap(Deque<T>& other)
	{
		SpinlockA guard(_lock);
		SpinlockA guardO(other._lock);
		if (_deque && other._deque)
			_deque->swap(*other._deque);
		else if (other._deque) {
			_deque = new std::deque<T>();
			_deque->swap(*other._deque);
		} else if (_deque) {
			other._deque = new std::deque<T>();
			_deque->swap(*other._deque);
		}
	}

	void reserve(size_t size)
	{
		SpinlockA guard(_lock);
		if (_deque)
			_deque->reserve(size);
		else {
			_deque = new std::deque<T>();
			_deque->reserve(size);
		}
	}

	T& operator[](std::size_t idx)
	{
		if (_deque)
			return (*_deque)[idx];
		else
			throw std::out_of_range("deque is empty");
	}

	operator std::deque<T>()
	{
		SpinlockA guard(_lock);
		if (_deque)
			return *_deque;
		else
			return std::deque<T>();
	}

	template <class Y>
	friend void std::swap(Deque<Y>& lhs, Deque<Y>& rhs);
};

// ------------------------- Shared Ptr -------------------------
// The following code is based upon microsofts implementation
// of shared and weak pointers. It is however not an exact copy.
// --------------------------------------------------------------
namespace Types
{
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	class __declspec(novtable) control_block
#elif defined(unix) || defined(__unix__) || defined(__unix)
	class control_block
#endif
	{
	public:
		//std::atomic<long> _ref_count = 1;
		//std::atomic<long> _weak_count = 1;
		short _ref_count = 1;
		short _weak_count = 1;
		std::atomic_flag _flag;

		long Incref()
		{
			Spinlock guard(_flag);
			return ++_ref_count;
		}
		long Decref()
		{
			Spinlock guard(_flag);
			return --_ref_count;
		}
		long Incweak()
		{
			Spinlock guard(_flag);
			return ++_weak_count;
		}
		long Decweak()
		{
			Spinlock guard(_flag);
			return --_weak_count;
		}

		bool Incref_nz()
		{
			Spinlock guard(_flag);
			if (_ref_count != 0) {
				++_ref_count;
				return true;
			}
			return false;
		}

		bool Incweak_nz()
		{
			Spinlock guard(_flag);
			if (_ref_count != 0) {
				++_ref_count;
				return true;
			}
			return false;
		}
	};

	template <typename T>
	class shared_ptr;
	template <typename T>
	class weak_ptr;
}
namespace std
{
	template <class T>
	struct std::less<Types::shared_ptr<T>>;
	template <class T>
	struct std::less<Types::weak_ptr<T>>;
}
namespace Types
{
	template <class T>
	class PtrBase
	{
	private:
		T* _ref = nullptr;
		control_block* _crtl = nullptr;

	public:
		long use_count() noexcept
		{
			return _crtl ? _crtl->_ref_count : 0;
		}
		long weak_count() noexcept
		{
			return _crtl ? _crtl->_weak_count : 0;
		}

		bool less(const PtrBase& other) noexcept
		{
			return _crtl < other._crtl;
		}

		PtrBase(const PtrBase&) = delete;
		PtrBase& operator=(const PtrBase&) = delete;

	protected:
		T* get()
		{
			return _ref;
		}

		constexpr PtrBase() noexcept = default;

		template <class Y>
		void move(PtrBase<Y>&& other) noexcept
		{
			_ref = other._ref;
			_crtl = other._crtl;

			other._ref = nullptr;
			other._crtl = nullptr;
		}
		template <class Y>
		void WeakMove(PtrBase<Y>&& other) noexcept
		{
			_crtl = other._crtl;
			other._crtl = nullptr;
			if (_crtl && _crtl->Incref_nz()) {
				_ref = other._ref;
				_crtl->Decref();
			}
			other._ref = nullptr;
		}

		template <class Y>
		void copy(const shared_ptr<Y>& other) noexcept
		{
			other.IncRef();
			_ref = other._ref;
			_crtl = other._crtl;
		}

		template <class Y>
		bool ContstructFromWeak(const weak_ptr<Y>& other) noexcept
		{
			if (other._crtl && other._crtl->Incref_nz()) {
				_ref = other.ref;
				_crtl = other._crtl;
			}
		}

		long Incref() noexcept
		{
			if (_crtl)
				_crtl->Incref();
		}

		long Decref() noexcept
		{
			if (_crtl) {
				long ret = 1;
				if (ret = _crtl->Decref(); ret == 0) {
					// we have deleted the last shared_ptr instance and can delete the underlying poínter
					delete _ref;
					_ref = nullptr;
					Decweak();
				}
				return ret;
			}
			return -1;
		}

		void ConstructWeak(const PtrBase& other) noexcept
		{
			if (other._crtl && other._crtl->Incweak()) {
				_crtl = other._crtl;
				if (other._crtl->Incref_nz()) {
					_ref = other._ref;
					other._crtl->Decref();
				}
			}
		}

		long Incweak() noexcept
		{
			if (_crtl)
				_crtl->Incweak();
		}

		long Decweak() noexcept
		{
			if (_crtl) {
				long ret = 1;
				if (ret = _crtl->Decweak(); ret == 0) {
					// we have deleted all weak refs. If there aren't any full refs delete ctrl object
					delete _crtl;
					_crtl = nullptr;
				}
				return ret;
			}
			return -1;
		}

		void Swap(PtrBase& other) noexcept
		{  // swap pointers
			_STD swap(_ref, other._ref);
			_STD swap(_crtl, other._crtl);
		}

		friend class shared_ptr<T>;
		friend class weak_ptr<T>;

		friend struct std::less<weak_ptr<T>>;
		friend struct std::less<shared_ptr<T>>;
	};

	template <typename T>
	class shared_ptr : protected PtrBase<T>
	{
	public:
		constexpr shared_ptr() noexcept = default;

		constexpr shared_ptr(nullptr_t) noexcept {}  // construct empty shared_ptr

		explicit shared_ptr(T* ptr) noexcept
		{
			PtrBase<T>::_ref = ptr;
			PtrBase<T>::_crtl = new control_block;
		}

		shared_ptr(const shared_ptr& other) noexcept
		{  // construct shared_ptr object that owns same resource as _Other
			this->copy(other);
		}

		template <class _Ty2, std::enable_if_t<std::_SP_pointer_compatible<_Ty2, T>::value, int> = 0>
		shared_ptr(const shared_ptr<_Ty2>& other) noexcept
		{
			// construct shared_ptr object that owns same resource as _Other
			this->copy(other);
		}

		shared_ptr(shared_ptr&& other) noexcept
		{  // construct shared_ptr object that takes resource from other
			this->move(std::move(other));
		}

		template <class _Ty2, std::enable_if_t<std::_SP_pointer_compatible<_Ty2, T>::value, int> = 0>
		shared_ptr(shared_ptr<_Ty2>&& other) noexcept
		{  // construct shared_ptr object that takes resource from other
			this->move(std::move(other));
		}

		template <class _Ty2, std::enable_if_t<std::_SP_pointer_compatible<_Ty2, T>::value, int> = 0>
		explicit shared_ptr(const weak_ptr<_Ty2> other)
		{
			if (this->ContstructFromWeak(other) == false) {
				throw std::bad_weak_ptr{};
			}
		}

		~shared_ptr() noexcept
		{  // release resource
			this->Decref();
		}

		void swap(shared_ptr& _Other) noexcept
		{
			this->Swap(_Other);
		}

		shared_ptr& operator=(const shared_ptr& other) noexcept
		{
			shared_ptr(other).swap(*this);
			return *this;
		}

		template <class _Ty2, std::enable_if_t<std::_SP_pointer_compatible<_Ty2, T>::value, int> = 0>
		shared_ptr& operator=(const shared_ptr<_Ty2>& other) noexcept
		{
			shared_ptr(other).swap(*this);
			return *this;
		}

		shared_ptr& operator=(shared_ptr&& _Right) noexcept
		{  // take resource from _Right
			shared_ptr(_STD move(_Right)).swap(*this);
			return *this;
		}

		template <class _Ty2, std::enable_if_t<std::_SP_pointer_compatible<_Ty2, T>::value, int> = 0>
		shared_ptr& operator=(shared_ptr<_Ty2>&& _Right) noexcept
		{  // take resource from _Right
			shared_ptr(_STD move(_Right)).swap(*this);
			return *this;
		}

		void reset() noexcept
		{  // release resource and convert to empty shared_ptr object
			shared_ptr().swap(*this);
		}
		using PtrBase<T>::get;

		[[nodiscard]] T& operator*() const noexcept
		{
			return *(PtrBase<T>::get());
		}

		[[nodiscard]] T* operator->() const noexcept
		{
			return PtrBase<T>::get();
		}

		explicit operator bool() const noexcept
		{
			return PtrBase<T>::get() != nullptr;
		}

		friend struct std::less<shared_ptr<T>>;

		template <class Y, class... _Args>
		friend std::shared_ptr<Y> make_shared(_Args&&... args);

		template <class T>
		friend shared_ptr<T> make_shared();
	};

	template <class T, class... _Args>
	shared_ptr<T> make_shared(_Args&&... args)
	{
		shared_ptr ptr(new T(std::forward<_Args>(args)...));
		return ptr;
	}
	template <class T>
	shared_ptr<T> make_shared()
	{
		shared_ptr ptr(new T());
		return ptr;
	}

	template <typename T>
	class weak_ptr : public PtrBase<T>
	{
	public:
		constexpr weak_ptr() noexcept {}

		weak_ptr(const weak_ptr& other) noexcept
		{
			this->ConstructWeak(other);  // same type, no conversion
		}

		template <class _Ty2, std::enable_if_t<std::_SP_pointer_compatible<_Ty2, T>::value, int> = 0>
		weak_ptr(const shared_ptr<_Ty2>& other) noexcept
		{
			this->ConstructWeak(other);  // shared_ptr keeps resource alive during conversion
		}

		template <class _Ty2, std::enable_if_t<std::_SP_pointer_compatible<_Ty2, T>::value, int> = 0>
		weak_ptr(const weak_ptr<_Ty2>& other) noexcept
		{
			this->ConstructWeak(other);
		}

		weak_ptr(weak_ptr&& other) noexcept
		{
			this->move(std::move(other));
		}

		template <class _Ty2, std::enable_if_t<std::_SP_pointer_compatible<_Ty2, T>::value, int> = 0>
		weak_ptr(weak_ptr<_Ty2>&& other) noexcept
		{
			this->WeakMove(std::move(other));
		}

		~weak_ptr() noexcept
		{
			this->Decweak();
		}

		void swap(weak_ptr& _Other) noexcept
		{
			this->Swap(_Other);
		}

		weak_ptr& operator=(const weak_ptr& other) noexcept
		{
			weak_ptr(other).swap(*this);
			return *this;
		}

		template <class _Ty2, std::enable_if_t<std::_SP_pointer_compatible<_Ty2, T>::value, int> = 0>
		weak_ptr& operator=(const weak_ptr<_Ty2>& other) noexcept
		{
			weak_ptr(other).swap(*this);
			return *this;
		}

		weak_ptr& operator=(weak_ptr&& other) noexcept
		{
			weak_ptr(std::move(other)).swap(*this);
			return *this;
		}

		template <class _Ty2, std::enable_if_t<std::_SP_pointer_compatible<_Ty2, T>::value, int> = 0>
		weak_ptr& operator=(weak_ptr<_Ty2>&& other) noexcept
		{
			weak_ptr(std::move(other)).swap(*this);
			return *this;
		}

		template <class _Ty2, std::enable_if_t<std::_SP_pointer_compatible<_Ty2, T>::value, int> = 0>
		weak_ptr& operator=(const shared_ptr<_Ty2>& other) noexcept
		{
			weak_ptr(other).swap(*this);
			return *this;
		}

		void reset() noexcept
		{  // release resource, convert to null weak_ptr object
			weak_ptr{}.swap(*this);
		}

		[[nodiscard]] bool expired() const noexcept
		{
			return this->use_count() == 0;
		}

		[[nodiscard]] shared_ptr<T> lock() const noexcept
		{  // convert to shared_ptr
			shared_ptr<T> ret;
			(void)ret.ContstructFromWeak(*this);
			return ret;
		}

		friend struct std::less<weak_ptr<T>>;
	};
}

template <class _Ty1, class _Ty2>
_NODISCARD Types::shared_ptr<_Ty1> dynamic_pointer_cast(const Types::shared_ptr<_Ty2>& _Other) noexcept
{
	// dynamic_cast for shared_ptr that properly respects the reference count control block
	const auto _Ptr = dynamic_cast<typename Types::shared_ptr<_Ty1>::element_type*>(_Other.get());

	if (_Ptr) {
		return Types::shared_ptr<_Ty1>(_Other, _Ptr);
	}

	return {};
}

template <class _Ty1, class _Ty2>
_NODISCARD Types::shared_ptr<_Ty1> dynamic_pointer_cast(Types::shared_ptr<_Ty2>&& _Other) noexcept
{
	// dynamic_cast for shared_ptr that properly respects the reference count control block
	const auto _Ptr = dynamic_cast<typename Types::shared_ptr<_Ty1>::element_type*>(_Other.get());

	if (_Ptr) {
		return Types::shared_ptr<_Ty1>(_STD move(_Other), _Ptr);
	}

	return {};
}

namespace std
{
	template <class T>
	struct less<Types::shared_ptr<T>>
	{
		bool operator()(const Types::shared_ptr<T>& _left, const Types::shared_ptr<T>& _right) const
		{
			return less<uintptr_t>{}(_left._crtl, _right._crtl);
		}
	};

	template <class T>
	struct less<Types::weak_ptr<T>>
	{
		bool operator()(const Types::weak_ptr<T>& _left, const Types::weak_ptr<T>& _right) const
		{
			return less<uintptr_t>{}(_left._crtl, _right._crtl);
		}
	};
}
