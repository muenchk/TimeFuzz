#pragma once

#include <memory>

#include "Input.h"
#include "Form.h"

class Grammar;

class Generator : public Form
{
public:
	bool Generate(std::shared_ptr<Input>& input, std::shared_ptr<Grammar> grammar, std::shared_ptr<SessionData> sessiondata, std::shared_ptr<Input> parent = {});

	void GenInputFromDevTree(std::shared_ptr<Input> input);
	/// <summary>
	/// resets all progress made
	/// </summary>
	virtual void Clean();
	/// <summary>
	/// 
	/// </summary>
	/// <param name="input"></param>

	#pragma region InheritedForm

	size_t GetStaticSize(int32_t version = 0x2) override;
	size_t GetDynamicSize() override;
	virtual bool WriteData(std::ostream* buffer, size_t& offset) override;
	virtual bool ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver) override;
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
	inline static bool _registeredFactories = false;
	static void RegisterFactories();

	virtual void Init();

	virtual void SetGrammar(std::shared_ptr<Grammar> grammar);

	virtual std::shared_ptr<Grammar> GetGrammar() { return _grammar; }

	#pragma endregion

private:

	const int32_t classversion = 0x1;

	std::shared_ptr<Grammar> _grammar;
};



class SimpleGenerator : public Generator
{
	void Clean() override;
};
