#pragma once

#include <unordered_map>
#include <functional>

namespace Functions
{

	class BaseFunction
	{
	public:
		virtual void Run() = 0;
		virtual void Dispose() = 0;
		virtual bool ReadData(unsigned char* buffer, size_t& offset, size_t length) = 0;
		virtual bool WriteData(unsigned char* buffer, size_t& offset) = 0;

		static uint64_t GetType() { return 0; };

		template <class T, typename = std::enable_if<std::is_base_of<BaseFunction, T>::value>>
		T* As()
		{
			if (T::GetType() == GetType())
				return dynamic_cast<T*>(this);
			else
				return nullptr;
		}

		template <class T, typename = std::enable_if<std::is_base_of<BaseFunction, T>::value>>
		static T* Create()
		{
			return new T();
		}
	};

	void RegisterClass(uint64_t classid, std::function<BaseFunction*()> factory);

	template <class T>
	T* FunctionFactory()
	{
		return BaseFunction::Create<T>();
	}

	BaseFunction* FunctionFactory(uint64_t classid);
}
