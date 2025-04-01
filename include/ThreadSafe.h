#pragma once

#include <optional>
#include <queue>
#include <atomic>
#include <set>
#include <Threading.h>

template <typename T>
class TSQueue
{
	std::queue<T> _queue;
	std::atomic_flag locked = ATOMIC_FLAG_INIT;

public:
	void Push(T val)
	{
		while (locked.test_and_set(std::memory_order_acquire)) {
			;
		}
		_queue.push(val);
		locked.clear(std::memory_order_release);
	}

	void Pop()
	{
		while (locked.test_and_set(std::memory_order_acquire)) {
			;
		}
		std::optional<T> val = _queue.empty() ? std::optional<T>() : std::optional<T>(_queue.front());
		_queue.pop();
		locked.clear(std::memory_order_release);
	}

	bool Empty()
	{
		while (locked.test_and_set(std::memory_order_acquire)) {
			;
		}
		bool empty = _queue.empty();
		locked.clear(std::memory_order_release);
		return empty;
	}

	size_t Size()
	{
		while (locked.test_and_set(std::memory_order_acquire)) {
			;
		}
		size_t size = _queue.size();
		locked.clear(std::memory_order_release);
		return size;
	}
};

template <class Key, class Compare = std::less<Key>, class Allocator = std::allocator<Key>>
class ts_set
{
private:
	std::atomic_flag _flag = ATOMIC_FLAG_INIT;
	std::set<Key, Compare, Allocator> _set;

public:

	ts_set()
	{
	}

	template <class _Iter>
	ts_set(_Iter _First, _Iter _Last)
	{
		SpinlockA guard(_flag);
		_set->insert(_First, _Last);
	}

	template <class _Iter>
	ts_set(_Iter _First, _Iter _Last, const Compare& _Pred)
	{
		SpinlockA guard(_flag);
		_set->insert(_First, _Last, _Pred);
	}

	template <class _Iter>
	ts_set(_Iter _First, _Iter _Last, const Allocator& _Al)
	{
		SpinlockA guard(_flag);
		_set->insert(_First, _Last,_Al);
	}

	template <class _Iter>
	ts_set(_Iter _First, _Iter _Last, const Compare& _Pred, const Allocator& _Al)
	{
		SpinlockA guard(_flag);
		_set->insert(_First, _Last,_Pred, _Al);
	}

	ts_set(std::initializer_list<Key> _Ilist)
	{
		SpinlockA guard(_flag);
		_set->insert(_Ilist);
	}

	ts_set(std::initializer_list<Key> _Ilist, const Compare& _Pred)
	{
		SpinlockA guard(_flag);
		_set->insert(_Ilist, _Pred);
	}

	ts_set(std::initializer_list<Key> _Ilist, const Allocator& _Al)
	{
		SpinlockA guard(_flag);
		_set->insert(_Ilist,_Al);
	}

	ts_set(std::initializer_list<Key> _Ilist, const Compare& _Pred, const Allocator& _Al)
	{
		SpinlockA guard(_flag);
		_set->insert(_Ilist, _Pred, _Al);
	}

	ts_set& operator=(std::initializer_list<Key> _Ilist)
	{
		SpinlockA guard(_flag);
		_set->clear();
		_set->insert(_Ilist);
		return *this;
	}

	auto begin()
	{
		SpinlockA guard(_flag);
		return _set.begin();
	}

	auto end()
	{
		SpinlockA guard(_flag);
		return _set.end();
	}

	auto rbegin()
	{
		SpinlockA guard(_flag);
		return _set.rbegin();
	}

	auto rend()
	{
		SpinlockA guard(_flag);
		return _set.rend();
	}

	bool empty()
	{
		SpinlockA guard(_flag);
		return _set.empty();
	}

	size_t size()
	{
		SpinlockA guard(_flag);
		return _set.size();
	}

	size_t max_size()
	{
		SpinlockA guard(_flag);
		return _set.max_size();
	}

	void clear()
	{
		SpinlockA guard(_flag);
		_set.clear();
	}

	void insert(const Key& value)
	{
		SpinlockA guard(_flag);
		_set.insert(value);
	}

	void insert(Key&& value)
	{
		SpinlockA guard(_flag);
		_set.insert(value);
	}

	void insert(std::set<Key, Compare, Allocator>::const_iterator pos, const Key& value)
	{
		SpinlockA guard(_flag);
		_set.insert(pos, value);
	}
	void insert(std::set<Key, Compare, Allocator>::const_iterator pos, Key&& value)
	{
		SpinlockA guard(_flag);
		_set.insert(pos, value);
	}
	void insert(std::initializer_list<Key> ilist)
	{
		SpinlockA guard(_flag);
		_set.insert(ilist);
	}
};
