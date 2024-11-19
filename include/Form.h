#pragma once

#include <cstdint>
#include <shared_mutex>
#include <string>
#include <memory>
#include <set>
#include <map>
#include <unordered_map>

#include <boost/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>
#include <boost/bimap/set_of.hpp>

typedef uint64_t FormID;

class LoadResolver;
class Data;
class Form;

template <class T, typename = std::enable_if<std::is_base_of<Form, T>::value>>
struct FormIDLess
{
	bool operator()(const std::shared_ptr<T>& lhs, const std::shared_ptr<T>& rhs) const
	{
		return lhs->GetFormID() < rhs->GetFormID();
	}
};

typedef uint64_t EnumType;

class Form
{
public:
	struct FormFlags
	{
		enum Flag : EnumType
		{
			/// <summary>
			/// No flags
			/// </summary>
			None = 0 << 0,
			/// <summary>
			/// Do not free memory allocated by this instance
			/// </summary>
			DoNotFree = (EnumType)1 << 62,
			/// <summary>
			/// Form has been deleted
			/// </summary>
			Deleted = (EnumType)1 << 63,
		};
	};

private:
	const int32_t formversion = 0x1;

protected:
	FormID _formid = 0;
	std::multiset<EnumType> _flags;
	EnumType _flagsAlloc = FormFlags::None;

	//typedef boost::bimap<boost::bimaps::multiset_of<FormID>, boost::bimaps::multiset_of<EnumType>> FlagMap;
	//FlagMap _flags;
	//EnumType _flags = FormFlags::None;
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
	/// Deletes all relevant for fields
	/// </summary>
	virtual void Delete(Data* data) = 0;
	/// <summary>
	/// Clears all internals
	/// </summary>
	virtual void Clear() = 0;
	/// <summary>
	/// Returns the formtype
	/// </summary>
	/// <returns></returns>
	static int32_t GetTypeStatic();
	virtual int32_t GetType() = 0;
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
	/// Registers all factiories necessary for the class
	/// </summary>
	static void RegisterFactories();

	/// <summary>
	/// Attempts to release as much memory as possible
	/// </summary>
	virtual void FreeMemory();

	/// <summary>
	/// Acquires a writers lock on the instance
	/// </summary>
	void Lock();
	/// <summary>
	/// Tries to aqcuire a writers lock and return whether it has been optained
	/// </summary>
	bool TryLock();
	/// <summary>
	/// Aqcuires a readers lock on the instance
	/// </summary>
	void LockRead();
	/// <summary>
	/// Tries to aqcuire a readers lock and return whether it has been optained
	/// </summary>
	bool TryLockRead();
	/// <summary>
	/// Releases the writers lock on the instance
	/// </summary>
	void Unlock();
	/// <summary>
	/// Releases the readers lock on the instance
	/// </summary>
	void UnlockRead();

	//virtual void SetFlag(EnumType flag, FormID setterID);

	//virtual void UnsetFlag(EnumType flag, FormID setterID);

	virtual void SetFlag(EnumType flag);

	virtual void UnsetFlag(EnumType flag);

	virtual bool HasFlag(EnumType flag);

	virtual EnumType GetFlags();

	/* virtual EnumType GetFlags()
	{
		return _flags;
	}*/

	template<class T, typename = std::enable_if<std::is_base_of<Form, T>::value>>
	T* As()
	{
		if (T::GetTypeStatic() == GetType())
			return dynamic_cast<T*>(this);
		else
			return nullptr;
	}
};

class FlagHolder
{
	EnumType _flag;
	std::shared_ptr<Form> _form;

public:
	FlagHolder()
	{

	}

	FlagHolder(std::shared_ptr<Form> form, EnumType flag)
	{
		if (form) {
			_form = form;
			_flag = flag;
			_form->SetFlag(flag);
		}
	}

	~FlagHolder()
	{
		if (_form) {
			_form->UnsetFlag(_flag);
			_form.reset();
		}
	}
};
 
struct FormType
{
	enum FormTypes
	{
		Input = 'INPU',             // Input
		Grammar = 'GRAM',           // Grammar
		DevTree = 'DEVT',           // Derivation Tree
		ExclTree = 'EXCL',          // ExclusionTree
		Generator = 'GENR',         // Generator
		Generation = 'GENE',         // Generation
		Session = 'SESS',           // Session
		Settings = 'SETT',          // Settings
		Test = 'TEST',              // Test
		TaskController = 'TASK',    // TaskController
		ExecutionHandler = 'EXEC',  // ExecutionHandler
		Oracle = 'ORAC',            // Oracle
		SessionData = 'SDAT',       // SessionData
		DeltaController = 'DDCR',   // DeltaController
	};

	static inline std::string ToString(int32_t type)
	{
		switch (type)
		{
		case FormType::Input:
			return "Input";
		case FormType::Grammar:
			return "Grammar";
		case FormType::DevTree:
			return "DevTree";
		case FormType::ExclTree:
			return "ExclTree";
		case FormType::Generator:
			return "Generator";
		case FormType::Session:
			return "Session";
		case FormType::Settings:
			return "Settings";
		case FormType::Test:
			return "Test";
		case FormType::TaskController:
			return "TaskController";
		case FormType::ExecutionHandler:
			return "ExecutionHandler";
		case FormType::Oracle:
			return "Oracle";
		case FormType::SessionData:
			return "SessionData";
		case FormType::DeltaController:
			return "DeltaController";
		default:
			return "Unknown";
		}
	}
};
