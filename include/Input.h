#pragma once

#include <list>
#include <filesystem>
#include <chrono>
#include <lua.hpp>
#include <memory>

#include "DerivationTree.h"
#include "TaskController.h"
#include "Utility.h"
#include "Form.h"
#include "Types.h"

class ExecutionHandler;
class Test;
class SessionFunctions;

struct lua_State;

class Input : public Form
{
	friend class SessionFunctions;

	const int32_t classversion = 0x3;

	std::atomic_flag _derivedFlag = ATOMIC_FLAG_INIT;

	struct LoadData
	{
		FormID testid;
		FormID deriveid;
	};
	LoadData* _loadData = nullptr;

	#pragma region InheritedForm
public:
	size_t GetStaticSize(int32_t version) override;
	size_t GetDynamicSize() override;
	bool WriteData(std::ostream* buffer, size_t& offset, size_t length) override;
	bool ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver) override;
	/// <summary>
	/// Early initialiazation pass [is executed once all forms have been loaded
	/// </summary>
	/// <param name="resolver"></param>
	void InitializeEarly(LoadResolver* resolver) override;
	/// <summary>
	/// late initialization pass [is executed once all Initialize Early has been called for all forms]
	/// </summary>
	/// <param name="resolver"></param>
	void InitializeLate(LoadResolver* resolver) override;
	int32_t GetType() override {
		return FormType::Input;
	}
	static int32_t GetTypeStatic()
	{
		return FormType::Input;
	}
	void Delete(Data* data);
	bool CanDelete(Data* data) override;
	void Clear() override;
	inline static bool _registeredFactories = false;
	static void RegisterFactories();
	size_t MemorySize() override;

	#pragma endregion

private:
	struct ParentInformation
	{
		/// <summary>
		/// formID of the parent input
		/// </summary>
		FormID parentInput = 0;
		/// <summary>
		/// segments in the parent input { positionBegin, length }
		/// </summary>
		std::vector<std::pair<int64_t, int64_t>> segments;
		/// <summary>
		/// whether the segement information is complement
		/// </summary>
		bool complement = false;
		/// <summary>
		/// the number of user events backtracked upon creation of this input
		/// </summary>
		int32_t backtrack = 0;
	};

	/// <summary>
	/// whether the input has been tested and has finished
	/// </summary>
	bool _hasfinished = false;
	/// <summary>
	/// whether the input has been trimmed
	/// </summary>
	bool _trimmed = false;
	/// <summary>
	/// the length to which the input has been trimmed
	/// </summary>
	int64_t _trimmedlength = -1;
	/// <summary>
	/// the execution time of the associated test
	/// </summary>
	std::chrono::nanoseconds _executiontime = std::chrono::nanoseconds(0);
	/// <summary>
	/// the exit code of the associated test
	/// </summary>
	int32_t _exitcode = -1;
	/// <summary>
	/// the primary score of the input
	/// </summary>
	double _primaryScore = 0.f;
	/// <summary>
	/// the secondary score of the input
	/// </summary>
	double _secondaryScore = 0.f;

	/// <summary>
	/// stores the individual primary score values to all executed entries of [sequence]
	/// </summary>
	Deque<double> _primaryScoreIndividual;
	/// <summary>
	/// stores the individual secondary score values to all executed entries of [sequence]
	/// </summary>
	Deque<double> _secondaryScoreIndividual;

	/// <summary>
	/// whether the individual storage of primary score values is enabled
	/// </summary>
	bool _enablePrimaryScoreIndividual = false;
	/// <summary>
	/// whether the individual storage of secondary score values is enabled
	/// </summary>
	bool _enableSecondaryScoreIndividual = false;

	friend ExecutionHandler;
	friend Test;

	/// <summary>
	/// the python representation of the input
	/// </summary>
	String _pythonstring = "";
	/// <summary>
	/// whether the python representation has been calculated
	/// </summary>
	bool _pythonconverted = false;

	String _stringString = "";

	/// <summary>
	/// whether the sequence has been generated
	/// </summary>
	bool _generatedSequence = false;

	/// <summary>
	/// information about the parent Input
	/// </summary>
	ParentInformation _parent;

	/// <summary>
	/// The ID of the generation this input belongs to
	/// </summary>
	FormID _generationID = 0;

	/// <summary>
	/// number of inputs that have been derived from this input
	/// </summary>
	uint64_t _derivedInputs = 0;
	/// <summary>
	/// number of derived inputs that failed
	/// </summary>
	uint64_t _derivedFails = 0;

	/// <summary>
	/// runtime at which this input was generated
	/// </summary>
	std::chrono::nanoseconds _generationTime = std::chrono::nanoseconds(0);

	/// <summary>
	/// number of times this input was repeated
	/// [runtime only]
	/// </summary>
	short _retries = 0;

	std::mutex _generationLock;

public:
	struct Flags
	{
		enum Flag : EnumType
		{
			/// <summary>
			/// No flags
			/// </summary>
			None = 0 << 0,
			/// <summary>
			/// This input cannot be derived from a derivation tree
			/// </summary>
			//NoDerivation = 1 << 0,
			/// <summary>
			/// Do not free memory allocated by this instance
			/// </summary>
			//DoNotFree = 1 << 1,
			/// <summary>
			/// The input is a duplicate of an already existing input and can be discarded
			/// </summary>
			Duplicate = 1 << 2,
			/// <summary>
			/// Input has already been deta debugged
			/// </summary>
			DeltaDebugged = 1 << 3,
			/// <summary>
			/// Input has been generated from a grammatic
			/// </summary>
			GeneratedGrammar = 1 << 4,
			/// <summary>
			/// Input has been generated as an extension of an existing input
			/// </summary>
			GeneratedGrammarParent = 1 << 5,
			/// <summary>
			/// Input has been generated by backtracking on an existing input instead of extension
			/// </summary>
			GeneratedGrammarParentBacktrack = 1 << 6,
			/// <summary>
			/// Input has been created as a result of splitting another input
			/// </summary>
			GeneratedDeltaDebugging = 1 << 7,
			/// <summary>
			/// This Flag indicates that primary score ranges may not be deleted, but need to be saved.
			/// If this flag is not present, FreeMemory may delete the values
			/// </summary>
			KeepIndividualScores = 1 << 8,
		};
	};

	std::shared_ptr<Test> test;
	Input();

	~Input();

	static int lua_ConvertToPython(lua_State* L);
	static int lua_ConvertToString(lua_State* L);
	static int lua_IsTrimmed(lua_State* L);
	static int lua_TrimInput(lua_State* L);
	static int lua_GetExecutionTime(lua_State* L);
	static int lua_GetExitCode(lua_State* L);
	static int lua_GetSequenceLength(lua_State* L);
	static int lua_GetSequenceFirst(lua_State* L);
	static int lua_GetSequenceNext(lua_State* L);
	static int lua_GetExitReason(lua_State* L);
	static int lua_GetCmdArgs(lua_State* L);
	static int lua_GetOutput(lua_State* L);
	static int lua_GetReactionTimeLength(lua_State* L);
	static int lua_GetReactionTimeFirst(lua_State* L);
	static int lua_GetReactionTimeNext(lua_State* L);
	static int lua_SetPrimaryScore(lua_State* L);
	static int lua_SetSecondaryScore(lua_State* L);
	static int lua_GetRetries(lua_State* L);

	static int lua_EnablePrimaryScoreIndividual(lua_State* L);
	static int lua_EnableSecondaryScoreIndividual(lua_State* L);
	static int lua_AddPrimaryScoreIndividual(lua_State* L);
	static int lua_AddSecondaryScoreIndividual(lua_State* L);

	static int lua_ClearScores(lua_State* L);
	static int lua_ClearTrim(lua_State* L);

	static int lua_IsOSWindows(lua_State* L);
	static void RegisterLuaFunctions(lua_State* L);

	void LockGeneration()
	{
		_generationLock.lock();
	}
	void UnlockGeneration()
	{
		_generationLock.unlock();
	}

	bool TryLockGeneration()
	{
		return _generationLock.try_lock();
	}

	/// <summary>
	/// converts the input to python code
	/// </summary>
	std::string ConvertToPython(bool update = false);
	/// <summary>
	/// converts the input to a single string
	/// </summary>
	/// <returns></returns>
	std::string ConvertToString();
	/// <summary>
	/// converts the input to a stream
	/// </summary>
	void ConvertToStream();
	/// <summary>
	/// returns the number of entries in the input
	/// </summary>
	/// <returns></returns>
	size_t Length();
	/// <summary>
	/// returns the current length of the sequence
	/// </summary>
	/// <returns></returns>
	size_t GetSequenceLength();
	/// <summary>
	/// converts the input to a string
	/// </summary>
	/// <param name="index"></param>
	/// <returns></returns>
	std::string& operator[](size_t index);
	/// <summary>
	/// Adds an entry at the end of the input
	/// </summary>
	/// <param name="entry"></param>
	void AddEntry(std::string entry);
	void ReserveSequence(size_t size)
	{
		_sequence.reserve(size);
	}
	/// <summary>
	/// Marks the input as containing a valid input sequence
	/// </summary>
	void SetGenerated(bool generated = true) { _generatedSequence = generated; }
	/// <summary>
	/// Returns whether the input sequence has been generated
	/// </summary>
	/// <returns></returns>
	bool GetGenerated() { return _generatedSequence; }
	/// <summary>
	/// converts the input to a string
	/// </summary>
	/// <returns></returns>
	std::string ToString();
	/// <summary>
	/// returns the hash of the input
	/// </summary>
	/// <returns></returns>
	std::size_t Hash();
	/// <summary>
	/// iterator pointing to the first element of the input
	/// </summary>
	/// <returns></returns>
	[[nodiscard]] std::vector<std::string>::iterator begin();
	/// <summary>
	/// iterator pointing beyond the last element of the input
	/// </summary>
	/// <returns></returns>
	[[nodiscard]] std::vector<std::string>::iterator end();

	/// <summary>
	/// Returns the execution time of the test if it has already finished, otherwise -1
	/// </summary>
	/// <returns></returns>
	std::chrono::nanoseconds GetExecutionTime();

	/// <summary>
	/// Returns the exitcode of the test if it has already finished, otherwise -1
	/// </summary>
	/// <returns></returns>
	int32_t GetExitCode();

	/// <summary>
	/// Returns whether the corresponding test has finished
	/// </summary>
	/// <returns></returns>
	bool Finished();

	/// <summary>
	/// Returns the oracle result
	/// </summary>
	/// <returns></returns>
	EnumType GetOracleResult();

	/// <summary>
	/// returns the primary score of the input
	/// </summary>
	double GetPrimaryScore();

	/// <summary>
	/// returns the secondary score of the input
	/// </summary>
	/// <returns></returns>
	double GetSecondaryScore();

	/// <summary>
	/// trims the input to the given length
	/// </summary>
	/// <param name="executed"></param>
	void TrimInput(int32_t executed);

	/// <summary>
	/// returns the trimmed length of the input. [If the input isn't trimmed, returns -1]
	/// </summary>
	/// <returns></returns>
	int64_t GetTrimmedLength();

	/// <summary>
	/// returns whether the input has been trimmed
	/// </summary>
	/// <returns></returns>
	bool IsTrimmed();

	/// <summary>
	/// Parses inputs from a python file.
	/// [The file should contain a variable name [inputs = ...]
	/// </summary>
	/// <param name="path"></param>
	/// <returns></returns>
	static std::vector<std::shared_ptr<Input>> ParseInputs(std::filesystem::path path);

	/// <summary>
	/// derivation/parse tree of the input
	/// </summary>
	std::shared_ptr<DerivationTree> derive;

	/// <summary>
	/// copies all internal values except [Test] and [DerivationTree] to [other]
	/// </summary>
	/// <param name="other"></param>
	void DeepCopy(std::shared_ptr<Input> other);

	/// <summary>
	/// Attempts to release as much memory as possible
	/// </summary>
	void FreeMemory() override;
	/// <summary>
	/// returns whether the memmory of this form has been freed
	/// </summary>
	/// <returns></returns>
	bool Freed() override;	

	/// <summary>
	/// Sets the input as generated as split of a parent input, also sets corresponding flags
	/// </summary>
	/// <param name="parentInput"></param>
	/// <param name="positionBegin"></param>
	/// <param name="length"></param>
	void SetParentSplitInformation(FormID parentInput, std::vector<std::pair<int64_t, int64_t>> segments, bool complement);
	/// <summary>
	/// Sets the input as generated extension from a parent input, also sets corresponding flags
	/// </summary>
	/// <param name="parentInput"></param>
	void SetParentGenerationInformation(FormID parentInput);
	/// <summary>
	/// [DEBUG] sets parent ID manually, do not use in normal operation
	/// </summary>
	/// <param name="parentInput"></param>
	void SetParentID(FormID parentInput);
	/// <summary>
	/// Sets the input as generated from a grammatic
	/// </summary>
	/// <param name="parentInput"></param>
	void SetGenerationInformation();
	/// <summary>
	/// returns the formid of the parent input
	/// </summary>
	/// <returns></returns>
	FormID GetParentID();
	/// <summary>
	/// Returns the begin of this sequence in the sequence of the parent Input
	/// </summary>
	/// <returns></returns>
	int64_t GetParentSplitBegin();
	/// <summary>
	/// Returns the length of this sequence as a subset of the parent Input
	/// </summary>
	/// <returns></returns>
	int64_t GetParentSplitLength();
	/// <summary>
	/// returns the parent split information
	/// </summary>
	/// <returns></returns>
	std::vector<std::pair<int64_t, int64_t>> GetParentSplits();
	/// <summary>
	/// Returns wether the ParentSplitBegin and ParentSplitLength values are of the complement of this sequence
	/// </summary>
	/// <returns></returns>
	bool GetParentSplitComplement();

	void SetParentBacktrack(int32_t backtrack = 0)
	{
		CheckChanged(_parent.backtrack, backtrack);
	}

	int32_t GetParentBacktrack()
	{
		return _parent.backtrack;
	}

	/// <summary>
	/// Returns the ID of the generation this input belongs to
	/// </summary>
	/// <returns></returns>
	FormID GetGenerationID();
	/// <summary>
	/// Sets the generation this input belongs to
	/// </summary>
	/// <param name="genID"></param>
	void SetGenerationID(FormID genID);

	/// <summary>
	/// Increments the number of inputs derived from this one
	/// </summary>
	void IncDerivedInputs();
	/// <summary>
	/// increments the number of derived inputs that fail
	/// </summary>
	void IncDerivedFails();
	/// <summary>
	/// Returns the number of inputs derived from this one
	/// </summary>
	uint64_t GetDerivedInputs();
	/// <summary>
	/// returns the number of inputs derived from this one that fail
	/// </summary>
	/// <returns></returns>
	uint64_t GetDerivedFails();

	/// <summary>
	/// Sets the runtime at which this input was generated
	/// </summary>
	/// <param name="genTime"></param>
	void SetGenerationTime(std::chrono::nanoseconds genTime);
	/// <summary>
	/// returns the generation time of this input
	/// </summary>
	/// <returns></returns>
	std::chrono::nanoseconds GetGenerationTime();

	int64_t GetTargetLength();

	int64_t EffectiveLength();

	bool IsIndividualPrimaryScoresEnabled();

	bool IsIndividualSecondaryScoresEnabled();

	size_t GetIndividualPrimaryScoresLength();

	size_t GetIndividualSecondaryScoresLength();

	double GetIndividualPrimaryScore(size_t position);

	double GetIndividualSecondaryScore(size_t position);

	std::vector<std::pair<size_t, size_t>> FindIndividualPrimaryScoreRangesWithoutChanges(size_t max = UINT_MAX - 1);

	std::vector<std::pair<size_t, size_t>> FindIndividualSecondaryScoreRangesWithoutChanges(size_t max = UINT_MAX - 1);

	static std::string PrintForm(std::shared_ptr<Input> form)
	{
		if (!form)
			return "None";
		return std::string("[") + typeid(Input).name() + "<FormID:" + Utility::GetHex(form->GetFormID()) + "><ParentID:" + Utility::GetHex(form->GetParentID()) +  "><Length:" + std::to_string(form->Length()) + "><PrimScore:" + std::to_string(form->GetPrimaryScore()) + "><SeconScore:" + std::to_string(form->GetSecondaryScore()) + ">]";
	}

	short GetRetries()
	{
		return _retries;
	}

	void IncRetries()
	{
		_retries++;
		SetChanged();
	}

	int64_t _olderinputs = -1;

private:
	/// <summary>
	/// the string representation of the input
	/// </summary>
	String _stringrep = "";
	/// <summary>
	/// the oracle result of the input
	/// </summary>
	EnumType _oracleResult;
	/// <summary>
	/// the underlying representation of the input sequence
	/// </summary>
	Vector<std::string> _sequence;

	std::vector<std::string>::iterator _lua_sequence_next;

	/// <summary>
	/// originally generated sequence [stores sequence after trimming]
	/// </summary>
	Vector<std::string> _orig_sequence;

	bool ReadData0x1(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver);
	bool ReadData0x2(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver);

public:
	void Debug_ClearSequence();
};
