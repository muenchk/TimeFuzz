#pragma once

#include <unordered_map>
#include <functional>

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
		virtual bool ReadData(unsigned char* buffer, size_t& offset, size_t length) = 0;
		/// <summary>
		/// Writes the object information to the given buffer
		/// </summary>
		/// <param name="buffer"></param>
		/// <param name="offset"></param>
		/// <returns></returns>
		virtual bool WriteData(unsigned char* buffer, size_t& offset) = 0;
		/// <summary>
		/// Returns the byte length of this object
		/// </summary>
		/// <returns></returns>
		virtual size_t GetLength() = 0;
		/// <summary>
		/// Registers a pointer to this object
		/// </summary>
		/// <param name="ptr"></param>
		void RegisterPointer(void** ptr);
		/// <summary>
		/// Changes all registered pointers to this object to the [nullptr]
		/// </summary>
		void DeletePointers();

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

	void RegisterFactory(uint64_t classid, std::function<BaseFunction*()> factory);

	template <class T>
	T* FunctionFactory()
	{
		return BaseFunction::Create<T>();
	}

	BaseFunction* FunctionFactory(uint64_t classid);
}
