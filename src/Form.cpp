#include "Form.h"
#include "BufferOperations.h"

#include <memory>
#include <mutex>
#include <shared_mutex>

namespace Hashing
{
	size_t hash(std::multiset<EnumType> const& st)
	{
		auto itr = st.begin();
		std::size_t hs = 0;
		if (itr != st.end()) {
			hs = std::hash<EnumType>{}(*itr);
			itr++;
		}
		while (itr != st.end()) {
			hs = hs ^ (std::hash<EnumType>{}(*itr) << 1);
			itr++;
		}
		return hs;
	}

	size_t hash(std::list<uint64_t> const& st)
	{
		auto itr = st.begin();
		std::size_t hs = 0;
		if (itr != st.end()) {
			hs = std::hash<uint64_t>{}(*itr);
			itr++;
		}
		while (itr != st.end()) {
			hs = hs ^ (std::hash<uint64_t>{}(*itr) << 1);
			itr++;
		}
		return hs;
	}

	size_t hash(EnumType const& st)
	{
		return std::hash<EnumType>{}(st);
	}
}

FormID IForm::GetFormID()
{
	return _formid;
}
void IForm::SetFormID(FormID formid)
{
	_formid = formid;
}


size_t StrippedForm::GetStaticSize(int32_t version)
{
	static size_t size0x1 = 4     // version
	                        + 8   // _formid
	                        + 8;  // _flags
	switch (version) {
	case 0x1:
		return size0x1;
	default:
		return 0;
	}
}

size_t StrippedForm::GetDynamicSize()
{
	return StrippedForm::GetStaticSize(formversion);  // basic stuff
}

bool StrippedForm::WriteData(std::ostream* buffer, size_t& offset, size_t)
{
	Buffer::Write(formversion, buffer, offset);
	Buffer::Write(_formid, buffer, offset);
	Buffer::Write(_flagsAlloc, buffer, offset);

	__saved = true;

	return true;
}

bool StrippedForm::ReadData(std::istream* buffer, size_t& offset, size_t /*length*/, LoadResolver* /*resolver*/)
{
	// sets saved to true -> form has been read from save
	__saved = true;
	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version) {
	case 0x1:
		{
			_formid = Buffer::ReadUInt64(buffer, offset);
			_flagsAlloc = Buffer::ReadUInt64(buffer, offset);
			__flaghash = Hashing::hash(_flagsAlloc);
		}
		return true;
	default:
		return false;
	}
}
bool StrippedForm::ReadDataLegacy(std::istream* buffer, size_t& offset, size_t /*length*/, LoadResolver* /*resolver*/)
{
	// sets saved to true -> form has been read from save
	__saved = true;
	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version) {
	case 0x1:
		{
			_formid = Buffer::ReadUInt64(buffer, offset);
			_flagsAlloc = Buffer::ReadUInt64(buffer, offset);
			/* size_t size = Buffer::ReadSize(buffer, offset);
			for (size_t i = 0; i < size; i++) {
				FormID setterID = Buffer::ReadUInt64(buffer, offset);
				EnumType flag = Buffer::ReadUInt64(buffer, offset);
				_flags.insert(FlagMap::value_type(setterID, flag));
			}*/
			size_t size = Buffer::ReadSize(buffer, offset);
			for (size_t i = 0; i < size; i++) {
				Buffer::ReadUInt64(buffer, offset);
			}
			__flaghash = Hashing::hash(_flagsAlloc);
		}
		return true;
	default:
		return false;
	}
}

bool StrippedForm::CanDelete(Data*)
{
	return true;
}

void StrippedForm::ClearForm()
{
	_flagsAlloc = 0;
}

void StrippedForm::ClearFormInternal()
{
	_flagsAlloc = 0;
}

void StrippedForm::FreeMemory()
{

}

bool StrippedForm::Freed()
{
	return true;
}

size_t StrippedForm::MemorySize()
{
	return GetDynamicSize();
}

void StrippedForm::SetFlag(EnumType flag)
{
	_flagsAlloc |= flag;
}

void StrippedForm::UnsetFlag(EnumType flag)
{
	_flagsAlloc = _flagsAlloc & (0xFFFFFFFFFFFFFFFF xor flag);
}

bool StrippedForm::HasFlag(EnumType flag)
{
	return _flagsAlloc & flag;
}

EnumType StrippedForm::GetFlags()
{
	return _flagsAlloc;
}

void StrippedForm::SetChanged()
{
	__changed |= 1;
}
void StrippedForm::ClearChanged()
{
	__changed ^= __changed;
	__flaghash = Hashing::hash(_flagsAlloc);
}
bool StrippedForm::HasChanged()
{
	return __changed || __flaghash != Hashing::hash(_flagsAlloc);
}

bool StrippedForm::WasSaved()
{
	return __saved;
}




void Form::FreeMemory()
{

}

bool Form::Freed()
{
	return true;
}

size_t Form::MemorySize()
{
	return GetDynamicSize();
}

bool Form::CanDelete(Data*)
{
	return true;
}

bool Form::TryLock()
{
	return _lock.try_lock();
}

void Form::Lock()
{
	_lock.lock();
}

void Form::LockRead()
{
	_lock.lock_shared();
}

bool Form::TryLockRead()
{
	return _lock.try_lock_shared();
}

void Form::Unlock()
{
	_lock.unlock();
}

void Form::UnlockRead()
{
	_lock.unlock_shared();
}

size_t Form::GetStaticSize(int32_t version)
{
	static size_t size0x1 = 4     // version
	                        + 8   // _formid
	                        + 8;  // _flags
	switch (version)
	{
	case 0x1:
		return size0x1;
	default:
		return 0;
	}
}

size_t Form::GetDynamicSize()
{
	return Form::GetStaticSize(formversion)  // basic stuff
	       //+ 8 + 16 * _flags.left.size(); // sizeof(), content of _flags (8+8, ID + flag)
	       + 8 + 8 * _flags.size();  // sizeof(), content of _flags
}

bool Form::WriteData(std::ostream* buffer, size_t& offset, size_t)
{
	Buffer::Write(formversion, buffer, offset);
	Buffer::Write(_formid, buffer, offset);
	Buffer::Write(_flagsAlloc, buffer, offset);
	/*Buffer::WriteSize(_flags.left.size(), buffer, offset);
	for (auto [setterID, flag] : _flags.left) {
		Buffer::Write(setterID, buffer, offset);
		Buffer::Write(flag, buffer, offset);
	}*/
	Buffer::WriteSize(_flags.size(), buffer, offset);
	for (auto flag : _flags)
	{
		Buffer::Write(flag, buffer, offset);
	}

	__saved = true;
	
	return true;
}

bool Form::ReadData(std::istream* buffer, size_t& offset, size_t /*length*/, LoadResolver* /*resolver*/)
{
	// sets saved to true -> form has been read from save
	__saved = true;
	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version)
	{
	case 0x1:
		{
			_formid = Buffer::ReadUInt64(buffer, offset);
			_flagsAlloc = Buffer::ReadUInt64(buffer, offset);
			/* size_t size = Buffer::ReadSize(buffer, offset);
			for (size_t i = 0; i < size; i++) {
				FormID setterID = Buffer::ReadUInt64(buffer, offset);
				EnumType flag = Buffer::ReadUInt64(buffer, offset);
				_flags.insert(FlagMap::value_type(setterID, flag));
			}*/
			size_t size = Buffer::ReadSize(buffer, offset);
			for (size_t i = 0; i < size; i++) {
				_flags.insert(Buffer::ReadUInt64(buffer, offset));
			}
			__flaghash = Hashing::hash(_flags);
		}
		return true;
	default:
		return false;
	}
}

/* void Form::SetFlag(EnumType flag, FormID setterID)
{
	_flags.insert(FlagMap::value_type(setterID, flag));
	_flagsAlloc |= flag;
}

 void Form::UnsetFlag(EnumType flag, FormID setterID)
{
	auto [itrbegin, itrend] = _flags.left.equal_range(setterID);
	while (itrbegin != itrend)
	{
		if (itrbegin->second == flag) {
			_flags.erase(FlagMap::value_type(itrbegin->first, itrbegin->second));
			//_flags.erase(itrbegin);
			// check wether all instances of the flag have been removed
			if (_flags.right.find(flag) == _flags.right.end())
				_flagsAlloc = _flagsAlloc & (0xFFFFFFFFFFFFFFFF xor flag);
			return;
		}
		itrbegin++;
	}
}*/

void Form::SetFlag(EnumType flag)
{
	//std::unique_lock<std::mutex> guard(_flaglock);
	//std::unique_lock<std::shared_mutex> guard(_lock);
	Spinlock guard(_flaglock);
	_flags.insert(flag);
	_flagsAlloc |= flag;
}

void Form::UnsetFlag(EnumType flag)
{
	//std::unique_lock<std::mutex> guard(_flaglock);
	//std::unique_lock<std::shared_mutex> guard(_lock);
	Spinlock guard(_flaglock);
	auto itr = _flags.find(flag);
	if (itr != _flags.end()) {
		_flags.erase(itr);
		if (_flags.find(flag) == _flags.end())
			_flagsAlloc = _flagsAlloc & (0xFFFFFFFFFFFFFFFF xor flag);
	}
}

bool Form::HasFlag(EnumType flag)
{
	return _flagsAlloc & flag;
}

EnumType Form::GetFlags()
{
	return _flagsAlloc;
}

void Form::SetChanged()
{
	__changed |= 1;
}
void Form::ClearChanged()
{
	__changed ^= __changed;
	__flaghash = Hashing::hash(_flags);
}
bool Form::HasChanged()
{
	return __changed || __flaghash != Hashing::hash(_flags);
}

bool Form::WasSaved()
{
	return __saved;
}

void Form::ClearForm()
{
	//std::unique_lock<std::mutex> guard(_flaglock);
	//std::unique_lock<std::shared_mutex> guard(_lock);
	Spinlock guard(_flaglock);
	_flagsAlloc = 0;
	_flags.clear();
}

void Form::ClearFormInternal()
{
	//std::unique_lock<std::mutex> guard(_flaglock);
	_flagsAlloc = 0;
	_flags.clear();
}
