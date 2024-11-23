#include "Record.h"
#include "ExecutionHandler.h"

template <>
void Records::CreateRecord(ExecutionHandler* value, std::ostream* buffer, size_t& offset, size_t& length)
{
	std::unique_lock<std::mutex> guard(value->_freezelock);
	offset = 0;
	size_t sz = value->GetDynamicSize();
	// record length + uint64_t record length + int32_t record type
	length = sz + 8 + 4;
	Buffer::WriteSize(sz, buffer, offset);
	Buffer::Write(ExecutionHandler::GetTypeStatic(), buffer, offset);
	value->WriteData(buffer, offset);
}
