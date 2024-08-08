#pragma once

#include <cstdint>
#include <shared_mutex>

typedef uint64_t FormID;

class LoadResolver;

class Form
{
protected:
	FormID _formid = 0;
	std::shared_mutex _lock;

public:
	/// <summary>
	/// returns the total size of the fields with static size
	/// </summary>
	/// <returns></returns>
	virtual size_t GetStaticSize(int32_t version = 0);
	/// <summary>
	/// returns the total size of all fields of this instance
	/// </summary>
	/// <returns></returns>
	virtual size_t GetDynamicSize();
	/// <summary>
	/// saves all relevant information of this instance to the given buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <returns></returns>
	virtual bool WriteData(unsigned char* buffer, size_t &offset);
	/// <summary>
	/// reads all relevant information of this instance from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="length"></param>
	virtual bool ReadData(unsigned char* buffer, size_t &offset, size_t length, LoadResolver* resolver);
	/// <summary>
	/// Returns the formtype
	/// </summary>
	/// <returns></returns>
	static int32_t GetType();
	/// <summary>
	/// Returns the formid of the instance
	/// </summary>
	/// <returns></returns>
	FormID GetFormID();
	/// <summary>
	/// Sets the formid of the instance
	/// </summary>
	/// <param name="formid"></param>
	void SetFormID(FormID formid);

	/// <summary>
	/// Acquires a writers lock on the instance
	/// </summary>
	void Lock();
	/// <summary>
	/// Aqcuires a readers lock on the instance
	/// </summary>
	void LockRead();
	/// <summary>
	/// Releases the writers lock on the instance
	/// </summary>
	void Unlock();
	/// <summary>
	/// Releases the readers lock on the instance
	/// </summary>
	void UnlockRead();

	template<class T, typename = std::enable_if<std::is_base_of<Form, T>::value>>
	T* As()
	{
		if (T::GetType() == GetType())
			return dynamic_cast<T*>(this);
		else
			return nullptr;
	}
};
 
struct FormType
{
	enum FormTypes
	{
		Input = 'INPU',      // Input
		Grammar = 'GRAM',    // Grammar
		DevTree = 'DEVT',    // Derivation Tree
		ExclTree = 'EXCL',   // ExclusionTree
		Generator = 'GENR',  // Generator
		Session = 'SESS',    // Session
		Settings = 'SETT',   // Settings
		Test = 'TEST',       // Test
	};
};
