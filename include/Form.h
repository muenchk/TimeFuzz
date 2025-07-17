#pragma once

#include <cstdint>
#include <shared_mutex>
#include <string>
#include <memory>
#include <set>
#include <map>
#include <unordered_map>
#include <list>

#include <boost/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>
#include <boost/bimap/set_of.hpp>

#include "Types.h"

typedef uint64_t FormID;
typedef uint32_t FormIDS;

class LoadResolver;
class Data;
class Form;

typedef uint64_t EnumType;

namespace Hashing
{
	size_t hash(std::multiset<EnumType> const& st);
	size_t hash(std::list<uint64_t> const& st);
	size_t hash(EnumType const& st);
}

template <class T, typename = std::enable_if<std::is_base_of<Form, T>::value>>
struct FormIDLess
{
	bool operator()(const Types::shared_ptr<T>& lhs, const Types::shared_ptr<T>& rhs) const
	{
		if (!rhs)
			return true;
		return lhs->GetFormID() < rhs->GetFormID();
	}
};

class IForm
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

protected:
	FormID _formid = 0;
	EnumType _flagsAlloc = FormFlags::None;
	

	/// <summary>
	/// there have been changes to the forms values
	/// </summary>
	bool __changed = false;
	bool __saved = false;
	size_t __flaghash = 0;

public:
	/// <summary>
	/// returns the total size of the fields with static size
	/// </summary>
	/// <returns></returns>
	virtual size_t GetStaticSize(int32_t version) = 0;
	/// <summary>
	/// returns the total size of all fields of this instance
	/// </summary>
	/// <returns></returns>
	virtual size_t GetDynamicSize() = 0;
	/// <summary>
	/// saves all relevant information of this instance to the given buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <returns></returns>
	virtual bool WriteData(std::ostream* buffer, size_t& offset, size_t length) = 0;
	/// <summary>
	/// reads all relevant information of this instance from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="length"></param>
	virtual bool ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver) = 0;
	/// <summary>
	/// Early initialiazation pass [is executed once all forms have been loaded
	/// </summary>
	/// <param name="resolver"></param>
	virtual void InitializeEarly(LoadResolver* resolver) = 0;
	/// <summary>
	/// late initialization pass [is executed once all Initialize Early has been called for all forms]
	/// </summary>
	/// <param name="resolver"></param>
	virtual void InitializeLate(LoadResolver* resolver) = 0;
	/// <summary>
	/// Deletes all relevant for fields
	/// </summary>
	virtual void Delete(Data* data) = 0;
	/// <summary>
	/// returns whether a form can be deleted
	/// </summary>
	/// <param name="data"></param>
	/// <returns></returns>
	virtual bool CanDelete(Data* data) = 0;
	/// <summary>
	/// returns whether the form has been marked deleted
	/// </summary>
	/// <returns></returns>
	virtual bool IsDeleted()
	{
		return _flagsAlloc & FormFlags::Deleted;
	}

	/// <summary>
	/// clear form internals
	/// </summary>
	virtual void ClearForm() = 0;

protected:
	/// <summary>
	/// clear form internals
	/// </summary>
	virtual void ClearFormInternal() = 0;

public:
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
	virtual FormID GetFormID();
	/// <summary>
	/// Sets the formid of the instance
	/// </summary>
	/// <param name="formid"></param>
	virtual void SetFormID(FormID formid);

	/// <summary>
	/// Attempts to release as much memory as possible
	/// </summary>
	virtual void FreeMemory() = 0;
	/// <summary>
	/// returns whether the memory of this form has been freed
	/// </summary>
	/// <returns></returns>
	virtual bool Freed() = 0;
	/// <summary>
	/// returns an estimation of the memory used by this class
	/// </summary>
	/// <returns></returns>
	virtual size_t MemorySize() = 0;

	virtual void SetFlag(EnumType flag) = 0;

	virtual void UnsetFlag(EnumType flag) = 0;

	virtual bool HasFlag(EnumType flag) = 0;

	virtual bool WasSaved() = 0;

	virtual EnumType GetFlags() = 0;

	virtual void SetChanged() = 0;
	virtual void ClearChanged() = 0;
	virtual bool HasChanged() = 0;

	template <class T, typename = std::enable_if<std::is_base_of<IForm, T>::value>>
	T* As()
	{
		if (T::GetTypeStatic() == GetType())
			return dynamic_cast<T*>(this);
		else
			return nullptr;
	}
};

class StrippedForm : public IForm
{
private:
	static const int32_t formversion = 0x1;

public:
	/// <summary>
	/// returns the total size of the fields with static size
	/// </summary>
	/// <returns></returns>
	virtual size_t GetStaticSize(int32_t version) override;
	/// <summary>
	/// returns the total size of all fields of this instance
	/// </summary>
	/// <returns></returns>
	virtual size_t GetDynamicSize() override;
	/// <summary>
	/// saves all relevant information of this instance to the given buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <returns></returns>
	virtual bool WriteData(std::ostream* buffer, size_t& offset, size_t length) override;
	/// <summary>
	/// reads all relevant information of this instance from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="length"></param>
	virtual bool ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver) override;
	/// <summary>
	/// reads all relevant information of this instance from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="length"></param>
	virtual bool ReadDataLegacy(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver);
	/// <summary>
	/// Early initialiazation pass [is executed once all forms have been loaded
	/// </summary>
	/// <param name="resolver"></param>
	virtual void InitializeEarly(LoadResolver* resolver) override = 0;
	/// <summary>
	/// late initialization pass [is executed once all Initialize Early has been called for all forms]
	/// </summary>
	/// <param name="resolver"></param>
	virtual void InitializeLate(LoadResolver* resolver) override = 0;
	/// <summary>
	/// Deletes all relevant for fields
	/// </summary>
	virtual void Delete(Data* data) override = 0;
	/// <summary>
	/// returns whether a form can be deleted
	/// </summary>
	/// <param name="data"></param>
	/// <returns></returns>
	virtual bool CanDelete(Data* data) override;

	/// <summary>
	/// clear form internals
	/// </summary>
	virtual void ClearForm() override final;

protected:
	/// <summary>
	/// clear form internals
	/// </summary>
	virtual void ClearFormInternal() override final;

public:
	/// <summary>
	/// Clears all internals
	/// </summary>
	virtual void Clear() override = 0;
	/// <summary>
	/// Returns the formtype
	/// </summary>
	/// <returns></returns>
	static int32_t GetTypeStatic();
	virtual int32_t GetType() override = 0;

	/// <summary>
	/// Attempts to release as much memory as possible
	/// </summary>
	virtual void FreeMemory() override;
	/// <summary>
	/// returns whether the memory of this form has been freed
	/// </summary>
	/// <returns></returns>
	virtual bool Freed() override;
	/// <summary>
	/// returns an estimation of the memory used by this class
	/// </summary>
	/// <returns></returns>
	virtual size_t MemorySize() override;

	virtual void SetFlag(EnumType flag) override;

	virtual void UnsetFlag(EnumType flag) override;

	virtual bool HasFlag(EnumType flag) override;

	virtual bool WasSaved() override;

	virtual EnumType GetFlags() override;

	virtual void SetChanged() override;
	virtual void ClearChanged() override;
	virtual bool HasChanged() override;

};

class Form : public IForm
{
private:
	static const int32_t formversion = 0x1;

protected:
	std::multiset<EnumType> _flags;

	//typedef boost::bimap<boost::bimaps::multiset_of<FormID>, boost::bimaps::multiset_of<EnumType>> FlagMap;
	//FlagMap _flags;
	//EnumType _flags = FormFlags::None;
	std::shared_mutex _lock;
	//std::mutex _flaglock;

	std::atomic_flag _flaglock = ATOMIC_FLAG_INIT;

public:

	/// <summary>
	/// returns the total size of the fields with static size
	/// </summary>
	/// <returns></returns>
	virtual size_t GetStaticSize(int32_t version) override;
	/// <summary>
	/// returns the total size of all fields of this instance
	/// </summary>
	/// <returns></returns>
	virtual size_t GetDynamicSize() override;
	/// <summary>
	/// saves all relevant information of this instance to the given buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <returns></returns>
	virtual bool WriteData(std::ostream* buffer, size_t &offset, size_t length) override;
	/// <summary>
	/// reads all relevant information of this instance from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="length"></param>
	virtual bool ReadData(std::istream* buffer, size_t &offset, size_t length, LoadResolver* resolver) override;
	/// <summary>
	/// Early initialiazation pass [is executed once all forms have been loaded
	/// </summary>
	/// <param name="resolver"></param>
	virtual void InitializeEarly(LoadResolver* resolver) override = 0;
	/// <summary>
	/// late initialization pass [is executed once all Initialize Early has been called for all forms]
	/// </summary>
	/// <param name="resolver"></param>
	virtual void InitializeLate(LoadResolver* resolver) override = 0;
	/// <summary>
	/// Deletes all relevant for fields
	/// </summary>
	virtual void Delete(Data* data) override = 0;
	/// <summary>
	/// returns whether a form can be deleted
	/// </summary>
	/// <param name="data"></param>
	/// <returns></returns>
	virtual bool CanDelete(Data* data) override;
	/// <summary>
	/// clear form internals
	/// </summary>
	virtual void ClearForm() override final;

protected:
	/// <summary>
	/// clear form internals
	/// </summary>
	virtual void ClearFormInternal() override final;

public:
	/// <summary>
	/// Clears all internals
	/// </summary>
	virtual void Clear() override = 0;
	/// <summary>
	/// Returns the formtype
	/// </summary>
	/// <returns></returns>
	static int32_t GetTypeStatic();
	virtual int32_t GetType() override = 0;

	/// <summary>
	/// Registers all factiories necessary for the class
	/// </summary>
	static void RegisterFactories();

	/// <summary>
	/// Attempts to release as much memory as possible
	/// </summary>
	virtual void FreeMemory() override;
	/// <summary>
	/// returns whether the memory of this form has been freed
	/// </summary>
	/// <returns></returns>
	virtual bool Freed() override;
	/// <summary>
	/// returns an estimation of the memory used by this class
	/// </summary>
	/// <returns></returns>
	virtual size_t MemorySize() override;

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

	virtual void SetFlag(EnumType flag) override;

	virtual void UnsetFlag(EnumType flag) override;

	virtual bool HasFlag(EnumType flag) override;

	virtual bool WasSaved() override;

	virtual EnumType GetFlags() override;

#define CheckChanged(x, y) \
	if (x != y) {          \
		SetChanged();      \
		x = y;             \
	}

	virtual void SetChanged() override;
	virtual void ClearChanged() override;
	virtual bool HasChanged() override;

	/* virtual EnumType GetFlags()
	{
		return _flags;
	}*/

	/// <summary>
	/// Prints a form
	/// </summary>
	/// <typeparam name="T"></typeparam>
	/// <typeparam name=""></typeparam>
	/// <param name="form"></param>
	/// <returns></returns>
	template <class T, typename = std::enable_if<std::is_base_of<Form, T>::value>>
	static std::string PrintForm(Types::shared_ptr<T> form)
	{
		auto GetHex = [](uint64_t val) {
			std::stringstream ss;
			ss << std::hex << val;
			return ss.str();
		};
		if (!form)
			return "None";
		return std::string("[") + typeid(T).name() + "<" + GetHex(form->GetFormID()) + ">]";
	}
};

template <class T, typename = std::enable_if<std::is_base_of<Form, T>::value>>
class FlagHolder
{
	EnumType _flag;
	Types::shared_ptr<T> _form;

	FlagHolder(FlagHolder<T>&) = delete;
	FlagHolder(FlagHolder<T>&&) = delete;
	FlagHolder<T>& operator=(const FlagHolder<T>&) = delete;
	FlagHolder<T>& operator=(const FlagHolder<T>&&) = delete;

public:
	FlagHolder()
	{

	}

	FlagHolder(Types::shared_ptr<T> form, EnumType flag)
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

template <class T, typename = std::enable_if<std::is_base_of<Form, T>::value>>
class ReadLockHolder
{
	Types::shared_ptr<T> _form;

	ReadLockHolder(ReadLockHolder<T>&) = delete;
	ReadLockHolder(ReadLockHolder<T>&&) = delete;
	ReadLockHolder<T>& operator=(const ReadLockHolder<T>&) = delete;
	ReadLockHolder<T>& operator=(const ReadLockHolder<T>&&) = delete;

public:
	ReadLockHolder()
	{
	}

	ReadLockHolder(Types::shared_ptr<T> form)
	{
		if (form) {
			_form = form;
			_form->LockRead();
		}
	}

	~ReadLockHolder()
	{
		if (_form) {
			_form->UnlockRead();
			_form.reset();
		}
	}
};

template <class T, typename = std::enable_if<std::is_base_of<Form, T>::value>>
class LockHolder
{
	Types::shared_ptr<T> _form;

	LockHolder(LockHolder<T>&) = delete;
	LockHolder(LockHolder<T>&&) = delete;
	LockHolder<T>& operator=(const LockHolder<T>&) = delete;
	LockHolder<T>& operator=(const LockHolder<T>&&) = delete;

public:
	LockHolder()
	{
	}

	LockHolder(Types::shared_ptr<T> form)
	{
		if (form) {
			_form = form;
			_form->Lock();
		}
	}

	~LockHolder()
	{
		if (_form) {
			_form->Unlock();
			_form.reset();
		}
	}
};


template <class T, typename = std::enable_if<std::is_base_of<Form, T>::value>>
class GenerationLockHolder
{
	Types::shared_ptr<T> _form;

	GenerationLockHolder(LockHolder<T>&) = delete;
	GenerationLockHolder(LockHolder<T>&&) = delete;
	GenerationLockHolder<T>& operator=(const GenerationLockHolder<T>&) = delete;
	GenerationLockHolder<T>& operator=(const GenerationLockHolder<T>&&) = delete;

public:
	GenerationLockHolder()
	{
	}

	GenerationLockHolder(Types::shared_ptr<T> form)
	{
		if (form) {
			_form = form;
			_form->LockGeneration();
		}
	}

	~GenerationLockHolder()
	{
		if (_form) {
			_form->UnlockGeneration();
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
		ExclTreeNode = 'EXCN',          // ExclusionTreeNode
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
