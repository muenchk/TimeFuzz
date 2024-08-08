#pragma once

#include <functional>
#include <queue>
#include <thread>
#include <vector>
#include <condition_variable>

#include "Form.h"
#include "Function.h"

class LoadResolver;
class LoadResolverGrammar;

class TaskController : public Form
{
public:

	static TaskController* GetSingleton();

	void AddTask(Functions::BaseFunction* a_task);

	void Start(int32_t numthreads = 0);
	void Stop(bool completeall = true);

	~TaskController();

	bool Busy();

	void Freeze();

	void Unfreeze();

	#pragma region InheritedForm

	size_t GetDynamicSize() override;
	size_t GetStaticSize(int32_t version = 0x1) override;
	bool WriteData(unsigned char* buffer, size_t& offset) override;
	bool ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver);
	static int32_t GetType()
	{
		return FormType::TaskController;
	}
	void Delete(Data* data);

	#pragma endregion
	
private:

	friend class LoadResolver;
	friend class LoadResolverGrammar;


	void InternalLoop();

	bool terminate = false;
	bool wait = false;
	std::condition_variable condition;
	std::vector<std::thread> threads;
	std::queue<Functions::BaseFunction*> tasks;
	std::mutex lock;

	int32_t _numthreads = 0;
	const int32_t classversion = 0x1;
};
