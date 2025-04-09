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

	void Delete(T* obj)
	{
		if (_size >= _maxsize)
			delete obj;
		else {
			obj->__Clear(_alloc);
			_objects.push_back(obj);
			_size++;
		}
	}

	T* New()
	{
		if (_size > 0) {
			auto obj = _objects.front();
			_objects.pop_front();
			_size--;
			_realloc++;
			return obj;
		} else {
			_unalloc++;
			return new T();
		}
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
	static void InitThreadAllocators(std::thread::id, size_t minsize);
	static void DestroyThreadAllocators(std::thread::id);

	Allocator<DerivationTree::NonTerminalNode>* DerivationTree_NonTerminalNode();
	Allocator<DerivationTree::TerminalNode>* DerivationTree_TerminalNode();
	Allocator<DerivationTree::SequenceNode>* DerivationTree_SequenceNode();
};

