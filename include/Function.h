#pragma once

#include <unordered_map>
#include <functional>
#include <cstdint>

class LoadResolver;

namespace Functions
{

	class BaseFunction
	{
	private:
		std::vector<void**> ptrs;
	public:
		/// <summary>
		/// Runs the function
		/// </summary>
		virtual void Run() = 0;
		/// <summary>
		/// Deletes the object and resets all internal variables and pointers
		/// </summary>
		virtual void Dispose();
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
		/// <summary>
		/// Registers a pointer to this object
		/// </summary>
		/// <param name="ptr"></param>
		void RegisterPointer(void** ptr);
		/// <summary>
		/// Changes all registered pointers to this object to the [nullptr]
		/// </summary>
		void DeletePointers();

		static uint64_t GetTypeStatic() { return 0; };
		virtual uint64_t GetType() = 0;

		template <class T, typename = std::enable_if<std::is_base_of<BaseFunction, T>::value>>
		T* As()
		{
			if (T::GetTypeStatic() == GetType())
				return dynamic_cast<T*>(this);
			else
				return nullptr;
		}

		template <class T, typename = std::enable_if<std::is_base_of<BaseFunction, T>::value>>
		static T* Create()
		{
			return new T();
		}

		static BaseFunction* Create(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver);
	};

	void RegisterFactory(uint64_t classid, std::function<BaseFunction*()> factory);

	template <class T>
	T* FunctionFactory()
	{
		return BaseFunction::Create<T>();
	}

	BaseFunction* FunctionFactory(uint64_t classid);
}
