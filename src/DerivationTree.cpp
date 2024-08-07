#include "DerivationTree.h"
#include "BufferOperations.h"

int32_t DerivationTree::GetClassVersion()
{
	return classversion;
}

size_t DerivationTree::GetStaticSize(int32_t version)
{
	size_t size0x1 = 4; // version

	switch (version)
	{
	case 0x1:
		return size0x1;
	default:
		return 0;
	}
}

size_t DerivationTree::GetDynamicSize()
{
	return GetStaticSize();
}

bool DerivationTree::WriteData(unsigned char* buffer, size_t offset)
{
	Buffer::Write(classversion, buffer, offset);
	return true;
}

bool DerivationTree::ReadData(unsigned char* buffer, size_t offset, size_t /*length*/, LoadResolver* /*resolver*/)
{
	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version)
	{
	case 0x1:
		return true;
	default:
		return false;
	}
	return false;
}
