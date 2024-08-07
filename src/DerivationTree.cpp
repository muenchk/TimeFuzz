#include "DerivationTree.h"
#include "BufferOperations.h"

int DerivationTree::GetClassVersion()
{
	return version;
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
	return GetStaticSize;
}

bool WriteData(unsigned char* buffer, int offset)
{
	Buffer::Write(version, buffer, offset);
	return true;
}

bool ReadData(unsigned char* buffer, int offset, int length, LoadResolver* resolver)
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
