#pragma once

#include <memory>

#include "Input.h"
#include "Form.h"

class Generator : public Form
{
public:
	void Generate();
	/// <summary>
	/// clears all internal variables
	/// </summary>
	void Clear();
	/// <summary>
	/// resets all progress made
	/// </summary>
	void Clean();
	void AddExclusion(std::shared_ptr<Input> input);
	void AddExclusionPrefix(std::shared_ptr<Input> input);

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
	void Delete(Data* data);

	#pragma endregion

private:

	const int32_t classversion = 0x1;
	
	class ExclusionNode
	{

	};
	class ExclusionTree
	{
		std::shared_ptr<ExclusionNode> head;
		size_t max_depth = 0;
		void Exclude(std::shared_ptr<Input> input);
		void ExcludePrefix(std::shared_ptr<Input> input);
		bool CheckExcluded(std::shared_ptr<Input> input);
	};
};
