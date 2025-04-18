#include "IPCommManager.h"
#include "Test.h"
#include "Logging.h"

IPCommManager::WriteData::WriteData(std::shared_ptr<Test> test, const char* data, size_t offset, size_t length)
{
	_test = test;
	_data = new char[length];
	memcpy(_data, data + offset, length);
	_offset = 0;
	_length = length;
	next = nullptr;
}

IPCommManager::WriteData::~WriteData()
{
	delete[] _data;
}

IPCommManager* IPCommManager::GetSingleton()
{
	static IPCommManager singleton;
	return std::addressof(singleton);
}

bool IPCommManager::Write(std::shared_ptr<Test> test, const char* data, size_t offset, size_t length)
{
	if (test && data != nullptr && length > 0) {
		if (test->IsValid() == false)
			return false;
		
		_writeQueueSem.acquire();
		//std::unique_lock<std::mutex> guard(_writeQueueLock);

		auto write = new WriteData(test, data, offset, length);

		auto itr = _writeQueue.find(test->GetFormID());
		if (itr != _writeQueue.end()) {
			// found existing write request
			// traverse list and add request at the end
			auto tmp = itr->second;
			while (tmp->next != nullptr)
				tmp = tmp->next;
			tmp->next = write;
		} else {
			_writeQueue.insert_or_assign(test->GetFormID(), write);
		}
		_writeQueueSem.release();
	}
	// return true if the write request is somewhat correct, if its an empty write or test isn't set its still acceptable, only if we cannot write to the test anymore it's not
	return true;
}

void IPCommManager::PerformWritesInternal()
{
	StartProfiling;
	// do something
	auto itr = _writeQueue.begin();
	while (itr != _writeQueue.end()) {
		if (itr->second == nullptr) {
			itr = _writeQueue.erase(itr);
			continue;
		}
		if (itr->second && (!itr->second->_test.lock() || itr->second->_test.lock() && (itr->second->_test.lock()->IsValid() == false || itr->second->_test.lock()->IsRunning() == false))) {
			// weak pointer expired
			// delete write queue
			WriteData *tmp = itr->second, *tmp2 = itr->second->next;
			while (tmp2 != nullptr) {
				delete tmp;
				tmp = tmp2;
				tmp2 = tmp2->next;
			}
			delete tmp;
			itr = _writeQueue.erase(itr);
			continue;
		}
		// write pointer is valid and test pointer too
		// now write some stuff

		bool skipitrinc = false;
		if (auto test = itr->second->_test.lock(); test) {
			auto write = itr->second;
			long written = 1;
			while (written != 0 && write != nullptr) {
				written = test->Write(write->_data, write->_offset, write->_length);
				// update write information
				write->_offset += written;
				write->_length -= written;
				if (written == -1) {
					itr = _writeQueue.erase(itr);
					skipitrinc = true;
					while (write != nullptr) {
						auto next = write->next;
						delete write;
						write = next;
					}
					break;
				} else if (write->_length <= 0) {
					auto next = write->next;
					if (next != nullptr) {
						_writeQueue.insert_or_assign(test->GetFormID(), next);
						delete write;
						write = next;
					} else {
						itr = _writeQueue.erase(itr);
						skipitrinc = true;
						delete write;
						write = nullptr;
						break;
					}
				} else {
					// couldn't write everything, so break loop
					break;
				}
			}
		}
		if (!skipitrinc)
			itr++;
	}
	_writeQueueSem.release();
	profile(TimeProfiling, "Write pipe content");
}

void IPCommManager::PerformWrites()
{
	//std::unique_lock<std::mutex> guard(_writeQueueLock);
	if (_writeQueueSem.try_acquire()) {
		if (_writeQueue.size() > 0) {
			std::thread(std::bind(&IPCommManager::PerformWritesInternal, this)).detach();
		} else
			_writeQueueSem.release();
	}
}
