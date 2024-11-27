#pragma once

#include <set>

namespace std
{
	enum class SetOrdering
	{
		Length,
		Primary,
		Secondary
	};

	template <class Key, class CompareLength = std::less<Key>, class ComparePrimary = std::less<Key>, class CompareSecondary = std::less<Key>, class Allocator = std::allocator<Key>>
	class ordered_multiset
	{
	private:
		std::multiset<Key, CompareLength, Allocator> _setLength;
		std::multiset<Key, ComparePrimary, Allocator> _setPrimary;
		std::multiset<Key, CompareSecondary, Allocator> _setSecondary;
		SetOrdering _ordering = SetOrdering::Length;


	public:

		void set_ordering(SetOrdering ordering)
		{
			_ordering = ordering;
			reorder(ordering);
		}

		ordered_multiset() {}

		ordered_multiset(SetOrdering ordering)
		{
			_ordering = ordering;
		}

		template <class _Iter>
		ordered_multiset(SetOrdering ordering, _Iter _First, _Iter _Last)
		{
			_ordering = ordering;
			switch (_ordering)
			{
			case SetOrdering::Length:
				_setLength.insert(_First, _Last);
				break;
			case SetOrdering::Primary:
				_setPrimary.insert(_First, _Last);
				break;
			case SetOrdering::Secondary:
				_setSecondary.insert(_First, _Last);
				break;
			}
		}

		ordered_multiset(SetOrdering ordering, initializer_list<Key> _Ilist)
		{
			_ordering = ordering;
			switch (_ordering) {
			case SetOrdering::Length:
				_setLength.insert(_Ilist);
				break;
			case SetOrdering::Primary:
				_setPrimary.insert(_Ilist);
				break;
			case SetOrdering::Secondary:
				_setSecondary.insert(_Ilist);
				break;
			}
		}

		ordered_multiset& operator=(initializer_list<Key> _Ilist)
		{
			switch (_ordering) {
			case SetOrdering::Length:
				_setLength.clear();
				_setLength.insert(_Ilist);
				break;
			case SetOrdering::Primary:
				_setPrimary.clear();
				_setPrimary.insert(_Ilist);
				break;
			case SetOrdering::Secondary:
				_setSecondary.clear();
				_setSecondary.insert(_Ilist);
				break;
			}
			return *this;
		}

		auto begin()
		{
			switch (_ordering) {
			case SetOrdering::Length:
				return _setLength.begin();
			case SetOrdering::Primary:
				return _setPrimary.begin();
			case SetOrdering::Secondary:
				return _setSecondary.begin();
			}
			return _setLength.begin();
		}

		auto end()
		{
			switch (_ordering) {
			case SetOrdering::Length:
				return _setLength.end();
			case SetOrdering::Primary:
				return _setPrimary.end();
			case SetOrdering::Secondary:
				return _setSecondary.end();
			}
			return _setLength.end();
		}

		auto rbegin()
		{
			switch (_ordering) {
			case SetOrdering::Length:
				return _setLength.rbegin();
			case SetOrdering::Primary:
				return _setPrimary.rbegin();
			case SetOrdering::Secondary:
				return _setSecondary.rbegin();
			}
		}

		auto rend()
		{
			switch (_ordering) {
			case SetOrdering::Length:
				return _setLength.rend();
			case SetOrdering::Primary:
				return _setPrimary.rend();
			case SetOrdering::Secondary:
				return _setSecondary.rend();
			}
		}

		bool empty()
		{
			switch (_ordering) {
			case SetOrdering::Length:
				return _setLength.empty();
			case SetOrdering::Primary:
				return _setPrimary.empty();
			case SetOrdering::Secondary:
				return _setSecondary.empty();
			}
			return true;
		}

		size_t size()
		{
			switch (_ordering) {
			case SetOrdering::Length:
				return _setLength.size();
			case SetOrdering::Primary:
				return _setPrimary.size();
			case SetOrdering::Secondary:
				return _setSecondary.size();
			}
			return 0;
		}

		size_t max_size()
		{
			switch (_ordering) {
			case SetOrdering::Length:
				return _setLength.max_size();
			case SetOrdering::Primary:
				return _setPrimary.max_size();
			case SetOrdering::Secondary:
				return _setSecondary.max_size();
			}
			return 0;
		}

		void clear()
		{
			switch (_ordering) {
			case SetOrdering::Length:
				break;
			case SetOrdering::Primary:
				_setPrimary.clear();
				break;
			case SetOrdering::Secondary:
				_setSecondary.clear();
				break;
			}
		}

		void insert(const Key& value)
		{
			switch (_ordering) {
			case SetOrdering::Length:
				_setLength.insert(value);
				break;
			case SetOrdering::Primary:
				_setPrimary.insert(value);
				break;
			case SetOrdering::Secondary:
				_setSecondary.insert(value);
				break;
			}
		}

		void insert(Key&& value)
		{
			switch (_ordering) {
			case SetOrdering::Length:
				_setLength.insert(value);
				break;
			case SetOrdering::Primary:
				_setPrimary.insert(value);
				break;
			case SetOrdering::Secondary:
				_setSecondary.insert(value);
				break;
			}
		}

		void insert(std::initializer_list<Key> ilist)
		{
			switch (_ordering) {
			case SetOrdering::Length:
				_setLength.insert(ilist);
				break;
			case SetOrdering::Primary:
				_setPrimary.insert(ilist);
				break;
			case SetOrdering::Secondary:
				_setSecondary.insert(ilist);
				break;
			}
		}

		void erase(const Key& value)
		{
			switch (_ordering) {
			case SetOrdering::Length:
				_setLength.erase(value);
				break;
			case SetOrdering::Primary:
				_setPrimary.erase(value);
				break;
			case SetOrdering::Secondary:
				_setSecondary.erase(value);
				break;
			}
		}

		void erase(Key&& value)
		{
			switch (_ordering) {
			case SetOrdering::Length:
				_setLength.erase(value);
				break;
			case SetOrdering::Primary:
				_setPrimary.erase(value);
				break;
			case SetOrdering::Secondary:
				_setSecondary.erase(value);
				break;
			}
		}

		void reorder(SetOrdering ordering)
		{
			switch (_ordering) {
			case SetOrdering::Length:
				switch (ordering) {
				case SetOrdering::Length:
					return;
				case SetOrdering::Primary:
					_setPrimary.insert(_setLength.begin(), _setLength.end());
					_setLength.clear();
					break;
				case SetOrdering::Secondary:
					_setSecondary.insert(_setLength.begin(), _setLength.end());
					_setLength.clear();
					break;
				}
				break;
			case SetOrdering::Primary:

				switch (ordering) {
				case SetOrdering::Length:
					_setLength.insert(_setPrimary.begin(), _setPrimary.end());
					_setPrimary.clear();
					break;
				case SetOrdering::Primary:
					return;
				case SetOrdering::Secondary:
					_setSecondary.insert(_setPrimary.begin(), _setPrimary.end());
					_setPrimary.clear();
					break;
				}
				break;
			case SetOrdering::Secondary:

				switch (ordering) {
				case SetOrdering::Length:
					_setLength.insert(_setSecondary.begin(), _setSecondary.end());
					_setSecondary.clear();
					break;
				case SetOrdering::Primary:
					_setPrimary.insert(_setSecondary.begin(), _setSecondary.end());
					_setSecondary.clear();
					break;
				case SetOrdering::Secondary:
					return;
				}
				break;
			}
			_ordering = ordering;
		}
	};
}
