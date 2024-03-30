#pragma once

#include <optional>
#include <queue>
#include <atomic>

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
