#pragma once

#include <thread>
#include <unordered_map>
#include <atomic>
#include <deque>

#include "DerivationTree.h"

class Allocatable;
class Allocators;

template <class T, typename = std::enable_if<std::is_base_of<Allocatable, T>::value>>
class Allocator
{
	std::deque<T*> _objects;
	Allocators* _alloc;

	size_t _maxsize = 10485760;
	size_t _size = 0;

	bool _enable = true;

public:
	uint64_t _unalloc = 0;
	uint64_t _realloc = 0;
	Allocator(Allocators* alloc)
	{ 
		_alloc = alloc;
	} 
	Allocator(Allocators* alloc, size_t maxsize)
	{
		_alloc = alloc;
		_maxsize = maxsize;
	}

	inline void Delete(T* obj)
	{
#ifndef CUSTOM_ALLOCATORS
		delete obj;
#else
		if (!_enable || _size >= _maxsize)
			delete obj;
		else {
			obj->__Clear(_alloc);
			_objects.push_back(obj);
			_size++;
		}
#endif
	}

	inline T* New()
	{
#ifndef CUSTOM_ALLOCATORS
		return new T();
#else
		if (_enable && _size > 0) {
			auto obj = _objects.front();
			_objects.pop_front();
			_size--;
			//_realloc++;
			return obj;
		} else {
			//_unalloc++;
			return new T();
		}
#endif
	}

	void Alloc(size_t size)
	{
		for (size_t i = 0; i < size && i < _maxsize; i++) {
			_size++;
			_objects.push_back(new T());
		}
	}

	~Allocator()
	{
		for (auto obj : _objects)
			delete obj;
		_objects.clear();
	}

	friend Allocators;
};

class Allocators
{
private:
	static inline std::unordered_map<std::thread::id, Allocators*> __threadmap;
	static inline std::atomic_flag __threadmapFlag = ATOMIC_FLAG_INIT;

	Allocator<DerivationTree::NonTerminalNode> _dev_nonterm = Allocator<DerivationTree::NonTerminalNode>(this);
	Allocator<DerivationTree::TerminalNode> _dev_term = Allocator<DerivationTree::TerminalNode>(this);
	Allocator<DerivationTree::SequenceNode> _dev_seq = Allocator<DerivationTree::SequenceNode>(this);

public:
	Allocators()
	{

	}
	static Allocators* GetThreadAllocators(std::thread::id);
	static void InitThreadAllocators(std::thread::id);
	static void InitThreadAllocators(std::thread::id, bool enable);
	static void InitThreadAllocators(std::thread::id, size_t minsize);
	static void InitThreadAllocators(std::thread::id, size_t minsize, bool enable);
	static void DestroyThreadAllocators(std::thread::id);

	void SetMaxSize(size_t maxsize);

	Allocator<DerivationTree::NonTerminalNode>* DerivationTree_NonTerminalNode();
	Allocator<DerivationTree::TerminalNode>* DerivationTree_TerminalNode();
	Allocator<DerivationTree::SequenceNode>* DerivationTree_SequenceNode();
};

