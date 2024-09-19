#include "Generator.h"
#include "BufferOperations.h"

void Generator::Clean()
{

}

void Generator::Clear()
{

}

size_t Generator::GetStaticSize(int32_t version)
{
	static size_t size0x1 = 4  // version
	                        + Form::GetDynamicSize(); // _formid
	switch (version)
	{
	case 0x1:
		return size0x1;
	default:
		return 0;
	}
}

size_t Generator::GetDynamicSize()
{
	return GetStaticSize(classversion);
}

bool Generator::WriteData(unsigned char* buffer, size_t& offset)
{
	Buffer::Write(classversion, buffer, offset);
	Form::WriteData(buffer, offset);
	return true;
}

bool Generator::ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver)
{
	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version) {
	case 0x1:
		Form::ReadData(buffer, offset, length, resolver);
		return true;
	default:
		return false;
	}
}

void Generator::Delete(Data*)
{
	Clear();
}





void SimpleGenerator::Clean()
{

}
