#include "Form.h"
#include "BufferOperations.h"


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

size_t Form::GetStaticSize(int32_t /*version*/)
{
	return 8 + 8; // _formid, _flags
}

size_t Form::GetDynamicSize()
{
	return Form::GetStaticSize(0);
}

bool Form::WriteData(unsigned char* buffer, size_t &offset)
{
	Buffer::Write(_formid, buffer, offset);
	Buffer::Write(_flags, buffer, offset);
	return true;
}

bool Form::ReadData(unsigned char* buffer, size_t &offset, size_t /*length*/, LoadResolver* /*resolver*/)
{
	_formid = Buffer::ReadUInt64(buffer, offset);
	_flags = Buffer::ReadUInt64(buffer, offset);
	return true;
}
