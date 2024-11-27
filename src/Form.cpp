#include "Form.h"
#include "BufferOperations.h"

#include <memory>
#include <mutex>
#include <shared_mutex>


FormID Form::GetFormID()
{
	return _formid;
}

void Form::SetFormID(FormID formid)
{
	_formid = formid;
}

void Form::FreeMemory()
{

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

bool Form::WriteData(std::ostream* buffer, size_t& offset)
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
	return true;
}

bool Form::ReadData(std::istream* buffer, size_t& offset, size_t /*length*/, LoadResolver* /*resolver*/)
{
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
	_flags.insert(flag);
	_flagsAlloc |= flag;
}

void Form::UnsetFlag(EnumType flag)
{
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
