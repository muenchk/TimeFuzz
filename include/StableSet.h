#pragma once

#include <set>

namespace std
{
	template <class Key, class Compare = std::less<Key>, class Allocator = std::allocator<Key>>
	class stable_set
	{
	private:
		std::set<Key, Compare, Allocator> _set;
		int32_t _maxsize = 0;

		void trunc()
		{
			// while set size is larger than max, cut off smallest element
			while (_set.size() > _maxsize)
				_set.erase(std::prev(_set.end()));
		}

		stable_set() {}
		
	public:
		void setMaxSize(int32_t maxsize)
		{
			_maxsize = maxsize;
		}

		stable_set(int32_t maxsize)
		{
			_maxsize = maxsize;
		}

		template <class _Iter>
		stable_set(int32_t maxsize, _Iter _First, _Iter _Last)
		{
			_maxsize = maxsize;
			_set->insert(_First, _Last);
			trunc();
		}

		template <class _Iter>
		stable_set(int32_t maxsize, _Iter _First, _Iter _Last, const Compare& _Pred)
		{
			_maxsize = maxsize;
			_set->insert(_First, _Last);
			trunc();
		}

		template <class _Iter>
		stable_set(int32_t maxsize, _Iter _First, _Iter _Last, const Allocator& _Al)
		{
			_maxsize = maxsize;
			_set->insert(_First, _Last);
			trunc();
		}

		template <class _Iter>
		stable_set(int32_t maxsize, _Iter _First, _Iter _Last, const Compare& _Pred, const Allocator& _Al)
		{
			_maxsize = maxsize;
			_set->insert(_First, _Last);
			trunc();
		}

		stable_set(int32_t maxsize, initializer_list<Key> _Ilist)
		{
			_maxsize = maxsize;
			_set->insert(_Ilist);
			trunc();
		}

		stable_set(int32_t maxsize, initializer_list<Key> _Ilist, const Compare& _Pred)
		{
			_maxsize = maxsize;
			_set->insert(_Ilist);
			trunc();
		}

		stable_set(int32_t maxsize, initializer_list<Key> _Ilist, const Allocator& _Al)
		{
			_maxsize = maxsize;
			_set->insert(_Ilist);
			trunc();
		}

		stable_set(int32_t maxsize, initializer_list<Key> _Ilist, const Compare& _Pred, const Allocator& _Al)
		{
			_maxsize = maxsize;
			_set->insert(_Ilist);
			trunc();
		}

		stable_set& operator=(initializer_list<Key> _Ilist)
		{
			_set->clear();
			_set->insert(_Ilist);
			trunc();
			return *this;
		}
		
		auto begin()
		{
			return _set.begin();
		}

		auto end()
		{
			return _set.end();
		}

		auto rbegin()
		{
			return _set.rbegin();
		}

		auto rend()
		{
			return _set.rend();
		}

		bool empty()
		{
			return _set.empty();
		}

		size_t size()
		{
			return _set.size();
		}

		size_t max_size()
		{
			return _maxsize;
		}

		void clear()
		{
			_set.clear();
		}

		void insert(const Key& value)
		{
			_set.insert(value);
			trunc();
		}

		void insert(Key&& value)
		{
			_set.insert(value);
			trunc();
		}

		void insert(std::set<Key, Compare, Allocator>::const_iterator pos, const Key& value)
		{
			_set.insert(pos, value);
			trunc();
		}
		void insert(std::set<Key, Compare, Allocator>::const_iterator pos, Key&& value)
		{
			_set.insert(pos, value);
			trunc();
		}
		void insert(std::initializer_list<Key> ilist)
		{
			_set.insert(ilist);
			trunc();
		}
	};

	template <class Key, class Compare = std::less<Key>, class Allocator = std::allocator<Key>>
	class stable_multiset
	{
	private:
		std::multiset<Key, Compare, Allocator> _set;
		int32_t _maxsize = 0;

		void trunc()
		{
			// while set size is larger than max, cut off smallest element
			while (_set.size() > _maxsize)
				_set.erase(std::prev(_set.end()));
		}

		stable_multiset() {}

	public:
		void setMaxSize(int32_t maxsize)
		{
			_maxsize = maxsize;
		}

		stable_multiset(int32_t maxsize)
		{
			_maxsize = maxsize;
		}

		template <class _Iter>
		stable_multiset(int32_t maxsize, _Iter _First, _Iter _Last)
		{
			_maxsize = maxsize;
			_set->insert(_First, _Last);
			trunc();
		}

		template <class _Iter>
		stable_multiset(int32_t maxsize, _Iter _First, _Iter _Last, const Compare& _Pred)
		{
			_maxsize = maxsize;
			_set->insert(_First, _Last);
			trunc();
		}

		template <class _Iter>
		stable_multiset(int32_t maxsize, _Iter _First, _Iter _Last, const Allocator& _Al)
		{
			_maxsize = maxsize;
			_set->insert(_First, _Last);
			trunc();
		}

		template <class _Iter>
		stable_multiset(int32_t maxsize, _Iter _First, _Iter _Last, const Compare& _Pred, const Allocator& _Al)
		{
			_maxsize = maxsize;
			_set->insert(_First, _Last);
			trunc();
		}

		stable_multiset(int32_t maxsize, initializer_list<Key> _Ilist)
		{
			_maxsize = maxsize;
			_set->insert(_Ilist);
			trunc();
		}

		stable_multiset(int32_t maxsize, initializer_list<Key> _Ilist, const Compare& _Pred)
		{
			_maxsize = maxsize;
			_set->insert(_Ilist);
			trunc();
		}

		stable_multiset(int32_t maxsize, initializer_list<Key> _Ilist, const Allocator& _Al)
		{
			_maxsize = maxsize;
			_set->insert(_Ilist);
			trunc();
		}

		stable_multiset(int32_t maxsize, initializer_list<Key> _Ilist, const Compare& _Pred, const Allocator& _Al)
		{
			_maxsize = maxsize;
			_set->insert(_Ilist);
			trunc();
		}

		stable_multiset& operator=(initializer_list<Key> _Ilist)
		{
			_set->clear();
			_set->insert(_Ilist);
			trunc();
			return *this;
		}

		auto begin()
		{
			return _set.begin();
		}

		auto end()
		{
			return _set.end();
		}

		auto rbegin()
		{
			return _set.rbegin();
		}

		auto rend()
		{
			return _set.rend();
		}

		bool empty()
		{
			return _set.empty();
		}

		size_t size()
		{
			return _set.size();
		}

		size_t max_size()
		{
			return _maxsize;
		}

		void clear()
		{
			_set.clear();
		}

		void insert(const Key& value)
		{
			_set.insert(value);
			trunc();
		}

		void insert(Key&& value)
		{
			_set.insert(value);
			trunc();
		}

		void insert(std::multiset<Key, Compare, Allocator>::const_iterator pos, const Key& value)
		{
			_set.insert(pos, value);
			trunc();
		}
		void insert(std::multiset<Key, Compare, Allocator>::const_iterator pos, Key&& value)
		{
			_set.insert(pos, value);
			trunc();
		}
		void insert(std::initializer_list<Key> ilist)
		{
			_set.insert(ilist);
			trunc();
		}
	};
}
