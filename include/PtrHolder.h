#pragma once

#include <memory>

template <class T>
class PtrHolderClass;

class PtrHolder
{
public:
	int32_t type = 0;

	template<class T>
	static int32_t GetType()
	{
		T::GetType();
	}

	template<class T>
	PtrHolderClass<T>* As()
	{
		if (type == T::GetType())
			return (PtrHolderClass<T>*)this;
		else
			return nullptr;
	}
};

template <class T>
class PtrHolderClass : public PtrHolder
{
public:
	uint32_t formid;
	std::weak_ptr<T> ptr;
};
