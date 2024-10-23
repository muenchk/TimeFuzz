#include "Record.h"
#include "ExecutionHandler.h"

template <>
unsigned char* Records::CreateRecord(ExecutionHandler* value, size_t& length)
{
	std::unique_lock<std::mutex> guard(value->_freezelock);
	size_t offset = 0;
	size_t sz = value->GetDynamicSize();
	// record length + uint64_t record length + int32_t record type
	length = sz + 8 + 4;
	unsigned char* buffer = new unsigned char[length];
	Buffer::WriteSize(sz, buffer, offset);
	Buffer::Write(ExecutionHandler::GetTypeStatic(), buffer, offset);
	value->WriteData(buffer, offset);
	return buffer;
}
