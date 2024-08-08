#pragma once

#include "Form.h"

#include <string>
class DerivationTree : public Form
{
private:
	const int32_t classversion = 0x1;

public:
	void Parse(std::string);
	/// <summary>
	/// resets all internal variables
	/// </summary>
	void Clear();

	#pragma region InheritedForm

	size_t GetStaticSize(int32_t version = 0x1) override;
	size_t GetDynamicSize() override;
	bool WriteData(unsigned char* buffer, size_t& offset) override;
	bool ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver) override;
	static int32_t GetType() {
		return FormType::DevTree;
	}
	void Delete(Data* data);

	#pragma endregion
};
