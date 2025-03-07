#pragma once

#include "Form.h"

#include <memory>
#include <mutex>
#include <unordered_map>

class Test;

class IPCommManager
{
private:
	struct WriteData
	{
		std::weak_ptr<Test> _test;
		char* _data;
		size_t _offset;
		size_t _length;

		WriteData* next;

		WriteData(std::shared_ptr<Test> test, const char* data, size_t offset, size_t length);
		~WriteData();
	};

	std::unordered_map<FormID, WriteData*> _writeQueue;

	std::mutex _writeQueueLock;

public:
	static IPCommManager* GetSingleton();

	/// <summary>
	/// Schedules a new write request
	/// </summary>
	/// <param name="test">the test to perform the write action on</param>
	/// <param name="data">the data to write</param>
	/// <param name="offset">the offset to start writing at</param>
	/// <param name="length">the number of bytes to write</param>
	bool Write(std::shared_ptr<Test> test, const char* data, size_t offset, size_t length);

	void PerformWrites();
};
