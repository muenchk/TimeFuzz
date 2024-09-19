#pragma once

#include <memory>

#include "Input.h"
#include "Form.h"

class Generator : public Form
{
public:
	void Generate();
	/// <summary>
	/// resets all progress made
	/// </summary>
	virtual void Clean();
	/// <summary>
	/// 
	/// </summary>
	/// <param name="input"></param>

	#pragma region InheritedForm

	size_t GetStaticSize(int32_t version = 0x1) override;
	size_t GetDynamicSize() override;
	virtual bool WriteData(unsigned char* buffer, size_t& offset) override;
	virtual bool ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver) override;
	int32_t GetType() override
	{
		return FormType::Generator;
	}
	static int32_t GetTypeStatic()
	{
		return FormType::Generator;
	}
	void Delete(Data* data) override;
	void Clear() override;

	#pragma endregion

private:

	const int32_t classversion = 0x1;
};



class SimpleGenerator : public Generator
{
	void Clean() override;
};
