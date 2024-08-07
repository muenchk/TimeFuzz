#pragma once

#include "Data.h"

#include <string>
class DerivationTree
{
private:
	const int32_t version = 0x1;

public:
	void Parse(std::string);
	/// <summary>
	/// resets all internal variables
	/// </summary>
	void Clear();

	/// <summary>
	/// returns the size of all fields with static size
	/// </summary>
	/// <returns></returns>
	size_t GetStaticSize(int32_t version = version);
	/// <summary>
	/// returns the save-size of the instance
	/// </summary>
	/// <returns></returns>
	size_t GetDynamicSize();
	/// <summary>
	/// returns the version of the class
	/// </summary>
	/// <returns></returns>
	int GetClassVersion();
	/// <summary>
	/// Writes the instance data to the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <returns></returns>
	bool WriteData(unsigned char* buffer, int offset);
	/// <summary>
	/// Reads the instance data from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <returns></returns>
	bool ReadData(unsigned char* buffer, int offset, int length, LoadResolver* resolver);
};
