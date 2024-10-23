#include "Generator.h"
#include "BufferOperations.h"
#include "Logging.h"

#include <random>

static std::mt19937 randan((unsigned int)(std::chrono::system_clock::now().time_since_epoch().count()));

void Generator::Clean()
{

}

void Generator::Clear()
{

}

void Generator::RegisterFactories()
{
	if (!_registeredFactories) {
		_registeredFactories = !_registeredFactories;
	}
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

void Generator::Init()
{

}

void Generator::Generate(std::shared_ptr<Input>& input)
{
	std::uniform_int_distribution<signed> dist(1000, 10000);
	int x = dist(randan);
	//static std::vector<std::string> buttons = { "[]", "[ \'RIGHT\' ]", "[ \'LEFT\' ]", "[ \'Z\' ]", "[ \'RIGHT\',  \'Z\'  ]", "[ \'LEFT\',  \'Z\'  ]" };
	static std::vector<std::string> buttons = { "[]", "[ \'RIGHT\' ]", "[ \'Z\' ]", "[ \'RIGHT\',  \'Z\'  ]"};
	static std::uniform_int_distribution<signed> butdist(0, (int)buttons.size()-1);
	for (int i = 0; i < x; i++) {
		input->AddEntry(buttons[butdist(randan)]);
	}
}


void SimpleGenerator::Clean()
{

}
