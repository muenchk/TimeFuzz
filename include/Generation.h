#include "Form.h"

#include <unordered_map>
#include <random>
#include <atomic>

class Input;
namespace DeltaDebugging
{
	class DeltaController;
}

class Generation : public Form
{
public:
	Generation();

	/// <summary>
	/// Returns the total size of this generation
	/// </summary>
	/// <returns></returns>
	int64_t GetSize();
	/// <summary>
	/// Returns the number of inputs in this generation that have been generated from a grammar
	/// </summary>
	/// <returns></returns>
	int64_t GetGeneratedSize();
	/// <summary>
	/// Returns the number of inputs in this generation that have been created through Delta Debugging
	/// </summary>
	/// <returns></returns>
	int64_t GetDDSize();
	/// <summary>
	/// Returns the number of inputs to be generated from a grammar in this generation
	/// </summary>
	/// <returns></returns>
	int64_t GetTargetSize();

	/// <summary>
	/// Returns wether there are inputs to be generated in this generation and how many may be generated by
	/// the calling function
	/// </summary>
	/// <returns></returns>
	std::pair<bool, int64_t> CanGenerate();

	/// <summary>
	/// Reduces the current counter for generated inputs by the given amount of [fails]
	/// </summary>
	/// <param name="fails"></param>
	void FailGeneration(int64_t fails);

	/// <summary>
	/// Sets the number of inputs to be generated from a grammar in this generation
	/// </summary>
	/// <param name="size"></param>
	void SetTargetSize(int64_t size);

	/// <summary>
	/// Adds an input generated from a grammar to this generation
	/// </summary>
	/// <param name="input"></param>
	void AddGeneratedInput(std::shared_ptr<Input> input);
	/// <summary>
	/// Removes a generated input from this generation
	/// </summary>
	/// <param name="input"></param>
	bool RemoveGeneratedInput(std::shared_ptr<Input> input);

	/// <summary>
	/// Adds an input created by delta debugging to this generation
	/// </summary>
	/// <param name="input"></param>
	void AddDDInput(std::shared_ptr<Input> input);
	/// <summary>
	/// Removes an input created by delta debugging from this generation
	/// </summary>
	/// <param name="input"></param>
	/// <returns></returns>
	bool RemoveDDInput(std::shared_ptr<Input> input);

	/// <summary>
	/// Adds a delta controller to this generation
	/// </summary>
	/// <param name="controller"></param>
	void AddDDController(std::shared_ptr<DeltaDebugging::DeltaController> ddcontroller);

	/// <summary>
	/// Decreases the number of active inputs
	/// </summary>
	void SetInputCompleted();

	/// <summary>
	/// Sets the number of this generation
	/// </summary>
	/// <param name="number"></param>
	void SetGenerationNumber(int32_t number);
	/// <summary>
	/// Returns the number of this generation
	/// </summary>
	/// <returns></returns>
	int32_t GetGenerationNumber();

	/// <summary>
	/// Sets the number of inputs that may be generated at a time
	/// </summary>
	/// <param name=""></param>
	void SetMaxSimultaneuosGeneration(int64_t generationstep);
	/// <summary>
	/// Sets the number of inputs that may be active at a time
	/// </summary>
	/// <param name=""></param>
	void SetMaxActiveInputs(int64_t activeInputs);
	/// <summary>
	/// Returns the number of inputs currently active
	/// </summary>
	/// <returns></returns>
	int64_t GetActiveInputs();

	/// <summary>
	/// Returns whether the current session is active or finished
	/// </summary>
	/// <returns></returns>
	bool IsActive();

	/// <summary>
	/// Returns whether there are still inputs that need to be generated
	/// </summary>
	/// <returns></returns>
	bool NeedsGeneration();

	/// <summary>
	/// Resets the number of active inputs
	/// </summary>
	void ResetActiveInputs();

	/// <summary>
	/// returns whether there are active delta debugging instances
	/// </summary>
	/// <returns></returns>
	bool IsDeltaDebuggingActive();

	/// <summary>
	/// Applies [visitor] to all dd controllers in this generation
	/// </summary>
	/// <param name="visitor"></param>
	void VisitDeltaDebugging(std::function<bool(std::shared_ptr<DeltaDebugging::DeltaController>)> visitor);

	/// <summary>
	/// returns whether this generation has sources
	/// </summary>
	/// <returns></returns>
	bool HasSources();
	/// <summary>
	/// Returns the number of sources available in this generation
	/// </summary>
	/// <returns></returns>
	int32_t GetNumberOfSources();
	/// <summary>
	/// Returns the sources of this generation
	/// </summary>
	/// <returns></returns>
	std::vector<std::shared_ptr<Input>> GetSources();
	/// <summary>
	/// Returns the sources of this generation
	/// </summary>
	/// <returns></returns>
	void GetSources(std::vector<std::shared_ptr<Input>>& sources);
	/// <summary>
	/// Returns a random source for expansion
	/// </summary>
	/// <returns></returns>
	std::shared_ptr<Input> GetRandomSource();
	/// <summary>
	/// Adds a new source to the generation
	/// </summary>
	/// <param name="input"></param>
	void AddSource(std::shared_ptr<Input> input);

	/// <summary>
	/// Checks whether the generations sources are still considered valid
	/// </summary>
	/// <param name"invalid">predicate returning true if the source is valid</param>
	/// <returns></returns>
	bool CheckSourceValidity(std::function<bool(std::shared_ptr<Input>)> valid);

	/// <summary>
	/// Returns the ddcontrollers active in this generation
	/// </summary>
	/// <param name="controllers"></param>
	void GetDDControllers(std::vector<std::shared_ptr<DeltaDebugging::DeltaController>>& controllers);
	/// <summary>
	/// returns the number of ddcontrollers in this instance
	/// </summary>
	/// <returns></returns>
	size_t GetNumberOfDDControllers();

	/// <summary>
	/// sets the generation as active and applies DoNotFree flags to the sources
	/// </summary>
	void SetActive();
	/// <summary>
	/// sets the generation as inactive and removes DoNotFree flags from the sources
	/// </summary>
	void SetInactive();

	/// <summary>
	/// Sets the meximum number of failing derivations for sources
	/// </summary>
	void SetMaxDerivedFailingInput(uint64_t maxDerivedFailingInputs);
	/// <summary>
	/// Sets the maximum number of derivations for sources
	/// </summary>
	/// <param name="maxDerivedInputs"></param>
	void SetMaxDerivedInput(uint64_t maxDerivedInputs);

private:
	/// <summary>
	/// total size of this generation
	/// </summary>
	int64_t _size = 0;
	/// <summary>
	/// number of inputs directly generated
	/// </summary>
	int64_t _generatedSize = 0;
	/// <summary>
	/// number of inputs created through delta debugging
	/// </summary>
	int64_t _ddSize = 0;
	/// <summary>
	/// number of inputs to be generated in this generation
	/// </summary>
	int64_t _targetSize = 0;

	/// <summary>
	/// specifies the number of inputs that may be generated by one call to CanGenerate
	/// </summary>
	int64_t _maxSimultaneousGeneration = 0;

	/// <summary>
	/// specifies maximum number of failing derivations for sources
	/// </summary>
	uint64_t _maxDerivedFailingInputs = 0;
	/// <summary>
	/// specifies maximum number of derivations for sources
	/// </summary>
	uint64_t _maxDerivedInputs = 0;

	/// <summary>
	/// Number of currently active inputs
	/// </summary>
	std::atomic<int64_t> _activeInputs = 0;
	/// <summary>
	/// Maximum number of active inputs at a time
	/// </summary>
	int64_t _maxActiveInputs = 0;

	/// <summary>
	/// map holding pointers to inputs generated in this generation
	/// </summary>
	std::unordered_map<FormID, std::shared_ptr<Input>> _generatedInputs;
	/// <summary>
	/// map holding pointers to inputs created in this generation by delta debugging
	/// </summary>
	std::unordered_map<FormID, std::shared_ptr<Input>> _ddInputs;

	/// <summary>
	/// maps holding pointers to all delta debugging controller that were executed in this generation
	/// </summary>
	std::unordered_map<FormID, std::shared_ptr<DeltaDebugging::DeltaController>> _ddControllers;

	/// <summary>
	/// vector holding the source inputs for this generation. The sources are used as basis for input expansion.
	/// </summary>
	std::vector<std::shared_ptr<Input>> _sources;

	//std::vector<std::shared_ptr<Input>>::iterator _sourcesIter;
	int32_t _sourcesIter = 0;

	std::atomic_flag _sourcesFlag = ATOMIC_FLAG_INIT;

	/// <summary>
	/// random engine
	/// </summary>
	/// <param name="seed"></param>
	/// <returns></returns>
	std::mt19937 randan;
	/// <summary>
	/// random distribution for _sources
	/// </summary>
	std::uniform_int_distribution<signed> _sourcesDistr;

	/// <summary>
	/// number of this generation
	/// </summary>
	int32_t _generationNumber = 0;

	/// <summary>
	/// returns true if the input is acceptable
	/// </summary>
	/// <param name="input"></param>
	/// <param name="allowFailing"></param>
	/// <param name="min_length_unfinished"></param>
	/// <param name="min_length_failing"></param>
	/// <returns></returns>
	bool CheckOracleResultAndLength(std::shared_ptr<Input>& input, bool allowFailing, size_t min_length_unfinished, size_t min_length_failing);

public:  // templates
	template <typename Less>
	void GetAllInputs(std::set<std::shared_ptr<Input>, Less>& output, bool includeSources, bool allowFailing, size_t min_length_unfinished = 0, size_t min_length_failing = 0)
	{
		for (auto [id, input] : _generatedInputs) {
			if (CheckOracleResultAndLength(input, allowFailing, min_length_unfinished, min_length_failing))
				output.insert(input);
		}
		for (auto [id, input] : _ddInputs) {
			if (CheckOracleResultAndLength(input, allowFailing, min_length_unfinished, min_length_failing))
				output.insert(input);
		}
		if (includeSources)
			for (auto input : _sources) {
				if (CheckOracleResultAndLength(input, allowFailing, min_length_unfinished, min_length_failing))
					output.insert(_sources);
			}
	}

	template <typename Less>
	void GetAllInputs(std::set<std::shared_ptr<Input>, Less>& output, bool includeSources, double minPrimaryScore, double minSecondaryScore, bool allowFailing, size_t min_length_unfinished = 0, size_t min_length_failing = 0)
	{
		for (auto [id, input] : _generatedInputs) {
			if (CheckOracleResultAndLength(input, allowFailing, min_length_unfinished, min_length_failing) &&
				input->GetPrimaryScore() >= minPrimaryScore && input->GetSecondaryScore() >= minSecondaryScore)
				output.insert(input);
		}
		for (auto [id, input] : _ddInputs) {
			if (CheckOracleResultAndLength(input, allowFailing, min_length_unfinished, min_length_failing) &&
				input->GetPrimaryScore() >= minPrimaryScore && input->GetSecondaryScore() >= minSecondaryScore)
				output.insert(input);
		}
		if (includeSources)
			for (auto input : _sources) {
				if (CheckOracleResultAndLength(input, allowFailing, min_length_unfinished, min_length_failing) &&
					input->GetPrimaryScore() >= minPrimaryScore && input->GetSecondaryScore() >= minSecondaryScore)
					output.insert(input);
			}
	}

	#pragma region Time
private:
	/// <summary>
	/// start time of the generation
	/// </summary>
	std::chrono::nanoseconds _gen_begin = std::chrono::nanoseconds(0);
	/// <summary>
	/// end time of the generation
	/// </summary>
	std::chrono::nanoseconds _gen_end = std::chrono::nanoseconds(0);

public:
	/// <summary>
	/// Returns start time of the generation
	/// </summary>
	/// <returns></returns>
	std::chrono::nanoseconds GetStartTime();
	/// <summary>
	/// Sets the start time of the generation
	/// </summary>
	/// <param name="begin"></param>
	void SetStartTime(std::chrono::nanoseconds begin);
	/// <summary>
	/// returns the end time of the generation
	/// </summary>
	/// <returns></returns>
	std::chrono::nanoseconds GetEndTime();
	/// <summary>
	/// Sets the end time of the generation
	/// </summary>
	/// <param name="end"></param>
	void SetEndTime(std::chrono::nanoseconds end);
	/// <summary>
	/// returns the total runtime of the generation
	/// </summary>
	/// <returns></returns>
	std::chrono::nanoseconds GetRunTime();


	#pragma endregion

	#pragma region FORM
private:
	const int32_t classversion = 0x2;
	static inline bool _registeredFactories = false;

public:
	/// <summary>
	/// returns the total size of the fields with static size
	/// </summary>
	/// <returns></returns>
	size_t GetStaticSize(int32_t version) override;
	/// <summary>
	/// returns the total size of all fields of this instance
	/// </summary>
	/// <returns></returns>
	size_t GetDynamicSize() override;
	/// <summary>
	/// saves all relevant information of this instance to the given buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <returns></returns>
	bool WriteData(std::ostream* buffer, size_t& offset) override;
	/// <summary>
	/// reads all relevant information of this instance from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="length"></param>
	bool ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver) override;
	/// <summary>
	/// Deletes all relevant for fields
	/// </summary>
	void Delete(Data* data) override;
	/// <summary>
	/// Clears all internals
	/// </summary>
	void Clear() override;
	/// <summary>
	/// Returns the formtype
	/// </summary>
	/// <returns></returns>
	static int32_t GetTypeStatic() { return FormType::Generation; }
	int32_t GetType() override { return FormType::Generation; }
	/// <summary>
	/// Registers all factiories necessary for the class
	/// </summary>
	static void RegisterFactories();
	size_t MemorySize() override;

	#pragma endregion
};
