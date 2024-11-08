#pragma once

#include <unordered_map>
#include <functional>
#include <cstdint>
#include <memory>

class LoadResolver;

namespace Functions
{
	enum class FunctionType
	{
		Light,
		Heavy,
	};

	class BaseFunction
	{
	public:
		/// <summary>
		/// Runs the function
		/// </summary>
		virtual void Run() = 0;
		/// <summary>
		/// Deletes the object and resets all internal variables and pointers
		/// </summary>
		virtual void Dispose() = 0;
		/// <summary>
		/// Reads the object information from the given buffer
		/// </summary>
		/// <param name="buffer"></param>
		/// <param name="offset"></param>
		/// <param name="length"></param>
		/// <returns></returns>
		virtual bool ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver) = 0;
		/// <summary>
		/// Writes the object information to the given buffer
		/// </summary>
		/// <param name="buffer"></param>
		/// <param name="offset"></param>
		/// <returns></returns>
		virtual bool WriteData(unsigned char* buffer, size_t& offset);
		/// <summary>
		/// Returns the byte length of this object
		/// </summary>
		/// <returns></returns>
		virtual size_t GetLength();

		static uint64_t GetTypeStatic() { return 0; };
		virtual uint64_t GetType() = 0;
		
		virtual FunctionType GetFunctionType() = 0;

		template <class T, typename = std::enable_if<std::is_base_of<BaseFunction, T>::value>>
		T* As()
		{
			if (T::GetTypeStatic() == GetType())
				return dynamic_cast<T*>(this);
			else
				return nullptr;
		}

		template <class T, typename = std::enable_if<std::is_base_of<BaseFunction, T>::value>>
		static std::shared_ptr<T> Create()
		{
			return dynamic_pointer_cast<T>(T::Create());
		}

		static std::shared_ptr<BaseFunction> Create(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver);
	};

	void RegisterFactory(uint64_t classid, std::function<std::shared_ptr<BaseFunction>()> factory);

	template <class T>
	std::shared_ptr<T> FunctionFactory()
	{
		return BaseFunction::Create<T>();
	}

	std::shared_ptr<BaseFunction> FunctionFactory(uint64_t classid);
}
