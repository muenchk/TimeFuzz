#pragma once

#include <memory>

class PtrHolder;

template <class T>
class PtrHolderClass : public PtrHolder
{
public:
	uint32_t formid;
	std::weak_ptr<T> ptr;
};

class PtrHolder
{
public:
	int type = 0;

	template<class T>
	static int GetType()
	{
		T::GetType();
	}

	template<class T>
	static PtrHolderClass<T>* As()
	{
		if (type == T::GetType())
			return (PtrHolderClass<T>*)this;
		else
			return nullptr;
	}
};
