
#include "Data.h"
#include "DeltaDebugging.h"
#include "Evaluation.h"
#include "Generation.h"
#include "Session.h"
#include "SessionData.h"
#include "SessionFunctions.h"
#include "LuaEngine.h"


#include <functional>
#include <algorithm>
#include <numeric>
#include <chrono>

Evaluation::Evaluation(std::shared_ptr<Session> session, std::filesystem::path resultpath)
{
	_session = session;
	_sessiondata = session->_sessiondata;
	_data = _sessiondata->data;
	_resultpath = resultpath;
}

void InputVisitor(std::shared_ptr<Input>)
{

}

std::string Evaluation::PrintInput(std::shared_ptr<Input> input, std::string& str, std::string& scriptargs, std::string& cmdargs, std::string& dump)
{
	str = "";
	// ID
	str += Utility::GetHex(input->GetFormID());
	// Parent ID
	str += ";" + Utility::GetHex(input->GetParentID());
	// Generation ID
	str += ";" + Utility::GetHex(input->GetGenerationID());
	// Derived Inputs
	str += ";" + std::to_string(input->GetDerivedInputs());
	// Derived Fails
	str += ";" + std::to_string(input->GetDerivedFails());
	// Flags
	str += ";" + Utility::GetHexFill(input->GetFlags());
	// Generation Time
	str += ";" + Logging::FormatTimeNS(input->GetGenerationTime().count());
	// Generation Length
	str += ";" + std::to_string(input->GetTargetLength());
	// Trimmed Length
	str += ";" + std::to_string(input->GetTrimmedLength());
	// Execution Time
	str += ";" + Logging::FormatTimeNS(input->GetExecutionTime().count());
	// Primary Score
	str += ";" + std::to_string(input->GetPrimaryScore());
	// Relative Primary Score
	int64_t lng = input->GetTrimmedLength();
	if (lng == -1)
		lng = input->GetTargetLength();
	str += ";" + std::to_string(lng / input->GetPrimaryScore());
	// Secondary Score
	str += ";" + std::to_string(input->GetSecondaryScore());
	// Relative Secondary Score
	str += ";" + std::to_string(lng / input->GetSecondaryScore());
	// Exit code
	str += ";" + std::to_string(input->GetExitCode());
	// Exit reason
	if (input->test)
		str += ";" + Utility::GetHex(input->test->_exitreason);
	else
		str += ";";
	// Result
	if (input->GetOracleResult() == OracleResult::Failing)
		str += ";Failing";
	else if (input->GetOracleResult() == OracleResult::Passing)
		str += ";Passing";
	else if (input->GetOracleResult() == OracleResult::Running)
		str += ";Running";
	else
		str += ";Unfinished";
	str += ";";
	// Individual Primary
	if (input->GetIndividualPrimaryScoresLength() > 0) {
		str += std::to_string(input->GetIndividualPrimaryScore(0));
		for (long c = 1; c < (long)input->GetIndividualPrimaryScoresLength(); c++) {
			str += "," + std::to_string(input->GetIndividualPrimaryScore(c));
		}
	}
	str += ";";
	// Individual Secondary
	if (input->GetIndividualSecondaryScoresLength() > 0) {
		str += std::to_string(input->GetIndividualSecondaryScore(0));
		for (long c = 1; c < (long)input->GetIndividualSecondaryScoresLength(); c++) {
			str += "," + std::to_string(input->GetIndividualSecondaryScore(c));
		}
	}
	str += ";";
	// Retries
	str += std::to_string(input->GetRetries());

	if (input->GetGenerated() == false) {
		// we are trying to add an _input that hasn't been generated or regenerated
		// try the generate it and if it succeeds add the test
		SessionFunctions::GenerateInput(input, _sessiondata);
	}
	if (input->GetGenerated()) {
		dump = input->ConvertToString();
		bool stateerror = false;
		cmdargs = Lua::GetCmdArgs(std::bind(&Oracle::GetCmdArgs, _sessiondata->_oracle, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), input->test, stateerror, false);
		if (_sessiondata->_oracle->GetOracletype() == Oracle::PUTType::Script)
			scriptargs = Lua::GetScriptArgs(std::bind(&Oracle::GetScriptArgs, _sessiondata->_oracle, std::placeholders::_1, std::placeholders::_2), input->test, stateerror);
		else
			scriptargs = "";
	} else {
		cmdargs = "";
		scriptargs = "";
		dump = "";
	}
	input->FreeMemory();
	if (input->derive)
		input->derive->FreeMemory();
	return str;
}

void Evaluation::WriteFile(std::string filename, std::string subpath, std::string content)
{
	std::ofstream _stream;
	if (subpath.empty() == false) {
		std::filesystem::create_directories(_resultpath / subpath);
		_stream = std::ofstream(_resultpath / subpath / filename, std::ios_base::out);
	}
	else
		_stream = std::ofstream(_resultpath / filename, std::ios_base::out);
	if (_stream.is_open()) {
		_stream.write(content.c_str(), content.size());
		_stream.flush();
		_stream.close();
	} else {
		if (subpath.empty() == false) {
			logcritical("Could not write to file: {}", (_resultpath / subpath / filename).string());
		} else {
			logcritical("Could not write to file: {}", (_resultpath / filename).string());
		}
	}
}

void Evaluation::Evaluate(int64_t& total, int64_t& current)
{
	bool reg = Lua::RegisterThread(_sessiondata);

	current = 0;
	total = 0;

	auto hashmap = _data->GetWeakHashmapCopy();
	auto generations = _data->GetFormArray<Generation>();
	auto dds = _data->GetFormArray<DeltaDebugging::DeltaController>();
	total += generations.size();
	total += dds.size();
	auto topk_p = _sessiondata->GetTopK(INT_MAX);
	auto topk_l = _sessiondata->GetTopK_Length(INT_MAX);
	auto topk_s = _sessiondata->GetTopK_Secondary(INT_MAX);
	total += topk_p.size() + topk_l.size() + topk_s.size();
	std::vector<std::shared_ptr<Input>> pos_inputs;
	_sessiondata->GetPositiveInputs(0, pos_inputs);
	total += pos_inputs.size();
	// ### General
	{
		std::string header;
		std::string line;
		// number of gens
		header += "Number of Generations;";
		line += std::to_string(generations.size()) + ";";
		// Number of DDs
		header += "Number of DeltaDebugging";
		line += std::to_string(dds.size());
		// Number of generated Tests
		header += ";Generated Tests";
		int64_t generated = std::accumulate(generations.begin(), generations.end(), (int64_t)0, [](int64_t init, std::shared_ptr<Generation> gen) {
			return init += gen->GetTrueGeneratedSize();
		});
		line += ";" + std::to_string(generated);
		// Number of delta debugging tests
		header += ";DD Tests";
		line += ";" + std::to_string(SessionStatistics::TestsExecuted(_sessiondata) - generated);
		// Number of prefix excluded Tests
		header += ";Prefix Excluded Tests";
		line += ";" + std::to_string(_sessiondata->GetGeneratedPrefix());
		// Number of approximation excluded Tests
		header += ";Approximation Excluded Tests";
		line += ";" + std::to_string(_sessiondata->GetExcludedApproximation());
		// total tests
		header += ";Total Tests";
		line += ";" + std::to_string(SessionStatistics::TestsExecuted(_sessiondata));
		// failed tests
		header += ";Negative Tests";
		line += ";" + std::to_string(SessionStatistics::NegativeTestsGenerated(_sessiondata));
		// positive tests
		header += ";Positive Tests";
		line += ";" + std::to_string(SessionStatistics::PositiveTestsGenerated(_sessiondata));
		// unfinished tests
		header += ";Unfinished Tests";
		line += ";" + std::to_string(SessionStatistics::UnfinishedTestsGenerated(_sessiondata));
		// undefined tests
		header += ";Undefined Tests";
		line += ";" + std::to_string(SessionStatistics::UndefinedTestsGenerated(_sessiondata));
		// runtime
		header += ";Runtime";
		line += ";" + Logging::FormatTimeNS(SessionStatistics::Runtime(_sessiondata).count());
		// average tests per minute
		header += ";Average Tests per Minute";
		line += ";" + std::to_string(((double)SessionStatistics::TestsExecuted(_sessiondata) / (SessionStatistics::Runtime(_sessiondata).count() / 1000000000)) * 60);

		// test exit stats
		auto stats = _sessiondata->GetTestExitStats();
		header += ";Natural Exit;LastInput Exit;Terminated Exit;Timeout Exit;FragmentTimeout Exit;Memory Exit;Pipe Exit;InitError Exit;Repeat Exit";
		line += ";" + std::to_string(stats.natural) + ";" + std::to_string(stats.lastinput) + ";" + std::to_string(stats.terminated) + ";" + std::to_string(stats.timeout) + ";" + std::to_string(stats.fragmenttimeout) + ";" + std::to_string(stats.memory) + ";" + std::to_string(stats.pipe) + ";" + std::to_string(stats.initerror) + ";" + std::to_string(stats.repeat);

		WriteFile("General.csv", "", header + "\n" + line + "\n");
	}
	std::string inputHeader = "ID;Parent ID;Generation ID;Derived Inputs;Derived Fails;Flags;Generation Time;Generation Length;Trimmed Length;Execution Time;Primary Score;Relative Primary Score;Secondary Score;Relative Secondary Score;Exit Code;Exit Reason;Result;Individual Primary;Individual Secondary;Retries\n";
	auto printInput = [](std::shared_ptr<Input> input) {
		std::string str = "";
		// ID
		str += Utility::GetHex(input->GetFormID());
		// Parent ID
		str += ";" + Utility::GetHex(input->GetParentID());
		// Generation ID
		str += ";" + Utility::GetHex(input->GetGenerationID());
		// Derived Inputs
		str += ";" + std::to_string(input->GetDerivedInputs());
		// Derived Fails
		str += ";" + std::to_string(input->GetDerivedFails());
		// Flags
		str += ";" + Utility::GetHexFill(input->GetFlags());
		// Generation Time
		str += ";" + Logging::FormatTimeNS(input->GetGenerationTime().count());
		// Generation Length
		str += ";" + std::to_string(input->GetTargetLength());
		// Trimmed Length
		str += ";" + std::to_string(input->GetTrimmedLength());
		// Execution Time
		str += ";" + Logging::FormatTimeNS(input->GetExecutionTime().count());
		// Primary Score
		str += ";" + std::to_string(input->GetPrimaryScore());
		// Relative Primary Score
		int64_t lng = input->GetTrimmedLength();
		if (lng == -1)
			lng = input->GetTargetLength();
		str += ";" + std::to_string(lng / input->GetPrimaryScore());
		// Secondary Score
		str += ";" + std::to_string(input->GetSecondaryScore());
		// Relative Secondary Score
		str += ";" + std::to_string(lng / input->GetSecondaryScore());
		// Exit code
		str += ";" + std::to_string(input->GetExitCode());
		// Exit reason
		if (input->test)
			str += ";" + Utility::GetHex(input->test->_exitreason);
		else
			str += ";";
		// Result
		if (input->GetOracleResult() == OracleResult::Failing)
			str += ";Failing";
		else if (input->GetOracleResult() == OracleResult::Passing)
			str += ";Passing";
		else if (input->GetOracleResult() == OracleResult::Running)
			str += ";Running";
		else str += ";Unfinished";
		str += ";";
		// Individual Primary
		if (input->GetIndividualPrimaryScoresLength() > 0) {
			str += std::to_string(input->GetIndividualPrimaryScore(0));
			for (long c = 1; c < (long)input->GetIndividualPrimaryScoresLength(); c++) {
				str += "," + std::to_string(input->GetIndividualPrimaryScore(c));
			}
		}
		str += ";";
		// Individual Secondary
		if (input->GetIndividualSecondaryScoresLength() > 0) {
			str += std::to_string(input->GetIndividualSecondaryScore(0));
			for (long c = 1; c < (long)input->GetIndividualSecondaryScoresLength(); c++) {
				str += "," + std::to_string(input->GetIndividualSecondaryScore(c));
			}
		}
		str += ";";
		// Retries
		str += std::to_string(input->GetRetries());

		return str;
	};

	// ### Generations
	{
		std::string header;
		std::string avline;
		std::string lines;
		header = "ID;Generation Number;Generation Size;DD Size;Number of DD;Runtime;Tests per Minute;Average Test Execution Time;Average DD Runtime;Total primary score increase;Total secondary increase\n";
		int64_t gennumber = 0, gensize = 0, avgensize = 0, ddsize = 0, avddsize = 0, numberofdd = 0, avnumberofdd = 0;
		double testsperminute = 0.f, avtestsperminute = 0.f, totalprimaryscoreinc = 0.f, avtotalprimaryscoreinc = 0.f, totalsecondaryscoreinc = 0.f, avtotalsecondaryscoreinc = 0.f;
		std::chrono::nanoseconds averageddruntime, avaverageddruntime, averagetestexectime, avaveragetestexectime,runtime, avruntime;
		for (int i = 0; i < (int)generations.size(); i++)
		{
			current++;
			if (generations[i]->GetGenerationNumber() == 0)
				continue;

			std::string line = "";

			gennumber = generations[i]->GetGenerationNumber();
			gensize = generations[i]->GetTrueGeneratedSize();
			avgensize += generations[i]->GetTrueGeneratedSize();
			ddsize = generations[i]->GetDDSize();
			avddsize += generations[i]->GetDDSize();
			numberofdd = generations[i]->GetNumberOfDDControllers();
			avnumberofdd += generations[i]->GetNumberOfDDControllers();
			testsperminute = ((double)(gensize + ddsize) / (generations[i]->GetRunTime().count() / 1000000000)) * 60;
			//testsperminute = (double)(gensize + ddsize) / std::chrono::duration_cast<std::chrono::seconds>(generations[i]->GetRunTime()).count()* 60;
			avtestsperminute += ((double)(gensize + ddsize) / (generations[i]->GetRunTime().count() / 1000000000)) * 60;
			//avtestsperminute += (double)(gensize + ddsize) / std::chrono::duration_cast<std::chrono::seconds>(generations[i]->GetRunTime()).count() *60;
			totalprimaryscoreinc = 0.f;
			totalsecondaryscoreinc = 0.f;
			auto sources = generations[i]->GetSources();
			for (int c = 0; c < (int)sources.size(); c++) {
				if (totalprimaryscoreinc < sources[c]->GetPrimaryScore())
					totalprimaryscoreinc = sources[c]->GetPrimaryScore();
				if (totalsecondaryscoreinc < sources[c]->GetSecondaryScore())
					totalsecondaryscoreinc = sources[c]->GetSecondaryScore();
			}
			avtotalprimaryscoreinc += totalprimaryscoreinc;
			avtotalsecondaryscoreinc += totalsecondaryscoreinc;
			std::vector<std::shared_ptr<DeltaDebugging::DeltaController>> ddcontrollers;
			averageddruntime = std::chrono::nanoseconds(0);
			generations[i]->GetDDControllers(ddcontrollers);
			for (int c = 0; c < (int)ddcontrollers.size(); c++)
				averageddruntime += ddcontrollers[c]->GetRunTime();
			if (ddcontrollers.size() > 0)
				averageddruntime = averageddruntime / ddcontrollers.size();
			avaverageddruntime = averageddruntime;
			averagetestexectime = std::chrono::nanoseconds(0);
			auto inputVisitor = [&averagetestexectime](std::shared_ptr<Input> form) {
				averagetestexectime = averagetestexectime + form->GetExecutionTime();
				return false;
			};
			generations[i]->VisitGeneratedInputs(inputVisitor);
			generations[i]->VisitDDInputs(inputVisitor);
			if (gensize + ddsize > 0)
				averagetestexectime = averagetestexectime / (gensize + ddsize);
			avaveragetestexectime += averagetestexectime;
			runtime = generations[i]->GetRunTime();
			avruntime += runtime;

			line += std::to_string(gennumber) + ";" + Utility::GetHex(generations[i]->GetFormID()) + ";" + std::to_string(gensize) + ";" + std::to_string(ddsize) + ";" + std::to_string(numberofdd) + ";" + Logging::FormatTimeNS(runtime.count()) + ";" + std::to_string(testsperminute) + ";" + Logging::FormatTimeNS(averagetestexectime.count()) + ";" + Logging::FormatTimeNS(averageddruntime.count()) + ";" + std::to_string(totalprimaryscoreinc) + ";" + std::to_string(totalsecondaryscoreinc) + "\n";
			lines += line;

			std::string inputs;
			inputs += inputHeader;

			switch (_sessiondata->_settings->generation.sourcesType) {
			case Settings::GenerationSourcesType::FilterLength:
				{
					std::set<std::shared_ptr<Input>, InputLengthGreater> inps;
					generations[i]->GetAllInputs(inps, false, true, 0, 0);
					auto itr = inps.begin();
					while (itr != inps.end())
					{
						inputs += printInput(*itr) + "\n";
						itr++;
					}
				}
				break;
			case Settings::GenerationSourcesType::FilterPrimaryScoreRelative:
				{
					std::set<std::shared_ptr<Input>, InputGainGreaterPrimary> inps;
					generations[i]->GetAllInputs(inps, false, true, 0, 0);
					auto itr = inps.begin();
					while (itr != inps.end()) {
						inputs += printInput(*itr) + "\n";
						itr++;
					}
				}
				break;
			case Settings::GenerationSourcesType::FilterPrimaryScore:
				{
					std::set<std::shared_ptr<Input>, InputGreaterPrimary> inps;
					generations[i]->GetAllInputs(inps, false, true, 0, 0);
					auto itr = inps.begin();
					while (itr != inps.end()) {
						inputs += printInput(*itr) + "\n";
						itr++;
					}

				}
				break;
			case Settings::GenerationSourcesType::FilterSecondaryScoreRelative:
				{
					std::set<std::shared_ptr<Input>, InputGainGreaterSecondary> inps;
					generations[i]->GetAllInputs(inps, false, true, 0, 0);
					auto itr = inps.begin();
					while (itr != inps.end()) {
						inputs += printInput(*itr) + "\n";
						itr++;
					}
				}
				break;
			case Settings::GenerationSourcesType::FilterSecondaryScore:
				{
					std::set<std::shared_ptr<Input>, InputGreaterSecondary> inps;
					generations[i]->GetAllInputs(inps, false, true, 0, 0);
					auto itr = inps.begin();
					while (itr != inps.end()) {
						inputs += printInput(*itr) + "\n";
						itr++;
					}
				}
				break;
			}

			WriteFile("Generation_" + std::to_string(generations[i]->GetGenerationNumber()) + ".csv", "", inputs);

		}

		avgensize /= generations.size();
		avddsize /= generations.size();
		avnumberofdd /= generations.size();
		avtestsperminute /= generations.size();
		avtotalprimaryscoreinc /= generations.size();
		avtotalsecondaryscoreinc /= generations.size();
		avaverageddruntime /= generations.size();
		avaveragetestexectime /= generations.size();
		avruntime /= generations.size();

		avline = "NONE;NONE;" + std::to_string(avgensize) + ";" + std::to_string(avddsize) + ";" + std::to_string(avnumberofdd) + ";" + Logging::FormatTimeNS(avruntime.count()) + ";" + std::to_string(avtestsperminute) + ";" + Logging::FormatTimeNS(avaveragetestexectime.count()) + ";" + Logging::FormatTimeNS(avaverageddruntime.count()) + ";" + std::to_string(avtotalprimaryscoreinc) + ";" + std::to_string(avtotalsecondaryscoreinc) + "\n";

		WriteFile("Generations_Average.csv", "", header + avline);
		WriteFile("Generations.csv", "", header + lines);
	}
	// ### DDs
	{
		if (dds.size() > 0) {
			std::string ddheader;
			std::string lines;

			ddheader = "ID;TotalTests;Skipped;Prefix;Approx;BatchCount;Level;Total Reduction %: Length;Total Reduction %: Primary;Total Reduction %: Secondary;Runtime;Average Test Runtime\n";
			std::string ddheaderind = "ID;TotalTests;Skipped;Prefix;Approx;BatchCount;Level;Original: Length;Reduced: Length;Total Reduction %: Length;Original: Primary;Reduced: Primary;Total Reduction %: Primary;Original: Secondary;Reduced: Secondary;Total Reduction %: Secondary;Runtime;Average Test Runtime\n";
			int64_t size = 0, skipped = 0, prefix = 0, approx = 0, reducsteps = 0, avsizereducperstep = 0, batchcount = 0, totaltests = 0, level = 0;
			int64_t size_av = 0, skipped_av = 0, prefix_av = 0, approx_av = 0, reducsteps_av = 0, avsizereducperstep_av = 0, batchcount_av = 0, totaltests_av = 0, level_av = 0;
			int64_t size_to = 0, skipped_to = 0, prefix_to = 0, approx_to = 0, reducsteps_to = 0, avsizereducperstep_to = 0, batchcount_to = 0, totaltests_to = 0, level_to = 0;

			double relativereduction_l = 0.f, relativereduction_p = 0.f, relativereduction_s = 0.f;
			double relativereduction_l_av = 0.f, relativereduction_p_av = 0.f, relativereduction_s_av = 0.f;
			double relativereduction_l_to = 0.f, relativereduction_p_to = 0.f, relativereduction_s_to = 0.f;
			double orig_l = 0.f, new_l = 0.f;
			double orig_p = 0.f, new_p = 0.f;
			double orig_s = 0.f, new_s = 0.f;

			std::chrono::nanoseconds runtime = std::chrono::nanoseconds(0), avtestruntime = std::chrono::nanoseconds(0);
			std::chrono::nanoseconds runtime_av = std::chrono::nanoseconds(0), avtestruntime_av = std::chrono::nanoseconds(0);
			std::chrono::nanoseconds runtime_to = std::chrono::nanoseconds(0), avtestruntime_to = std::chrono::nanoseconds(0);

			for (int i = 0; i < (int)dds.size(); i++) {
				current++;
				std::string line = "";
				size = dds[i]->GetTestsTotal();
				size_av += dds[i]->GetTestsTotal();
				size_to += dds[i]->GetTestsTotal();
				totaltests = dds[i]->GetTestsTotal();
				totaltests_av += dds[i]->GetTestsTotal();
				totaltests_to += dds[i]->GetTestsTotal();
				skipped = dds[i]->GetSkippedTests();
				skipped_av += dds[i]->GetSkippedTests();
				skipped_to += dds[i]->GetSkippedTests();
				prefix = dds[i]->GetPrefixTests();
				prefix_av += dds[i]->GetPrefixTests();
				prefix_to += dds[i]->GetPrefixTests();
				approx = dds[i]->GetApproxTests();
				approx_av += dds[i]->GetApproxTests();
				approx_to += dds[i]->GetApproxTests();
				batchcount = dds[i]->GetBatchIdent();
				batchcount_av += dds[i]->GetBatchIdent();
				batchcount_to += dds[i]->GetBatchIdent();
				level = dds[i]->GetLevel();
				level_av += dds[i]->GetLevel();
				level_to += dds[i]->GetLevel();
				orig_l = (double)dds[i]->GetOriginalInput()->EffectiveLength();
				new_l = (double)dds[i]->GetInput()->EffectiveLength();
				relativereduction_l = (double)dds[i]->GetInput()->EffectiveLength() / (double)dds[i]->GetOriginalInput()->EffectiveLength() * 100;
				relativereduction_l_av += (double)dds[i]->GetInput()->EffectiveLength() / (double)dds[i]->GetOriginalInput()->EffectiveLength() * 100;
				relativereduction_l_to += (double)dds[i]->GetInput()->EffectiveLength() / (double)dds[i]->GetOriginalInput()->EffectiveLength() * 100;
				orig_p = dds[i]->GetInput()->GetPrimaryScore();
				new_p = dds[i]->GetInput()->GetPrimaryScore();
				relativereduction_p = dds[i]->GetInput()->GetPrimaryScore() / dds[i]->GetOriginalInput()->GetPrimaryScore() * 100;
				relativereduction_p_av += dds[i]->GetInput()->GetPrimaryScore() / dds[i]->GetOriginalInput()->GetPrimaryScore() * 100;
				relativereduction_p_to += dds[i]->GetInput()->GetPrimaryScore() / dds[i]->GetOriginalInput()->GetPrimaryScore() * 100;
				orig_s = dds[i]->GetInput()->GetSecondaryScore();
				new_s = dds[i]->GetInput()->GetSecondaryScore();
				relativereduction_s = dds[i]->GetInput()->GetSecondaryScore() / dds[i]->GetOriginalInput()->GetSecondaryScore() * 100;
				relativereduction_s_av += dds[i]->GetInput()->GetSecondaryScore() / dds[i]->GetOriginalInput()->GetSecondaryScore() * 100;
				relativereduction_s_to += dds[i]->GetInput()->GetSecondaryScore() / dds[i]->GetOriginalInput()->GetSecondaryScore() * 100;
				runtime = dds[i]->GetRunTime();
				runtime_av += dds[i]->GetRunTime();
				runtime_to += dds[i]->GetRunTime();
				avtestruntime = std::chrono::nanoseconds(0);
				auto inputVisitor = [&avtestruntime](std::shared_ptr<Input> form) {
					avtestruntime = avtestruntime + form->GetExecutionTime();
					return false;
				};
				if (totaltests > 0)
					avtestruntime = avtestruntime / totaltests;
				for (auto [inp, _] : *(dds[i]->GetResults())) {
					inputVisitor(inp);
				}
				avtestruntime_av += avtestruntime;
				avtestruntime_to += avtestruntime;

				line = Utility::GetHex(dds[i]->GetFormID()) + ";" + std::to_string(totaltests) + ";" + std::to_string(skipped) + ";" + std::to_string(prefix) + ";" + std::to_string(approx) + ";" + std::to_string(batchcount) + ";" + std::to_string(level) + ";" + std::to_string(orig_l) + ";" + std::to_string(new_l) + ";" + std::to_string(relativereduction_l) + ";" + std::to_string(orig_p) + ";" + std::to_string(new_p) + ";" + std::to_string(relativereduction_p) + ";" + std::to_string(orig_s) + ";" + std::to_string(new_s) + ";" + std::to_string(relativereduction_s) + ";" + Logging::FormatTimeNS(runtime.count()) + ";" + Logging::FormatTimeNS(avtestruntime.count()) + "\n";
				lines += line;
			}
			size_av /= dds.size();
			skipped_av /= dds.size();
			prefix_av /= dds.size();
			approx_av /= dds.size();
			reducsteps_av /= dds.size();
			avsizereducperstep_av /= dds.size();
			batchcount_av /= dds.size();
			totaltests_av /= dds.size();
			relativereduction_l_av /= dds.size();
			relativereduction_p_av /= dds.size();
			relativereduction_s_av /= dds.size();
			runtime_av /= dds.size();
			avtestruntime_av /= dds.size();

			std::string avline = "-;" + std::to_string(totaltests_av) + ";" + std::to_string(skipped_av) + ";" + std::to_string(prefix_av) + ";" + std::to_string(approx_av) + ";" + std::to_string(batchcount_av) + ";" + std::to_string(level_av) + ";" + std::to_string(relativereduction_l_av) + ";" + std::to_string(relativereduction_p_av) + ";" + std::to_string(relativereduction_s_av) + ";" + Logging::FormatTimeNS(runtime_av.count()) + ";" + Logging::FormatTimeNS(avtestruntime_av.count()) + "\n";
			;
			std::string toline = "-;" + std::to_string(totaltests_to) + ";" + std::to_string(skipped_to) + ";" + std::to_string(prefix_to) + ";" + std::to_string(approx_to) + ";" + std::to_string(batchcount_to) + ";" + std::to_string(level_to) + ";" + std::to_string(relativereduction_l_to) + ";" + std::to_string(relativereduction_p_to) + ";" + std::to_string(relativereduction_s_to) + ";" + Logging::FormatTimeNS(runtime_to.count()) + ";" + Logging::FormatTimeNS(avtestruntime_to.count()) + "\n";

			WriteFile("DeltaDebugging_AV+TO.csv", "", ddheader + avline + toline);
			WriteFile("DeltaDebugging.csv", "", ddheaderind + lines);
		}
	}
	// ### Inputs
	{
		// collect top 100 and print their outputs and the inputs itself

		std::string scriptargs;
		std::string cmdargs;
		std::string dump;
		std::string content;

		std::string lines_topk_p;
		std::string lines_topk_l;
		std::string lines_topk_s;
		std::string positive;

		// topK primary
		for (int64_t i = 0; i < (int64_t)topk_p.size(); i++)
		{
			current++;
			PrintInput(topk_p[i], content, scriptargs, cmdargs, dump);
			lines_topk_p += content + "\n";
			WriteFile(Utility::GetHex(topk_p[i]->GetFormID()) + ".scriptargs.txt", "topk_primary", scriptargs);
			WriteFile(Utility::GetHex(topk_p[i]->GetFormID()) + ".cmdargs.txt", "topk_primary", cmdargs);
			WriteFile(Utility::GetHex(topk_p[i]->GetFormID()) + ".dump.txt", "topk_primary", dump);
		}
		WriteFile("topk_primary.csv", "", inputHeader + lines_topk_p);
		// topK length
		for (int64_t i = 0; i < (int64_t)topk_l.size(); i++) {
			current++;
			PrintInput(topk_l[i], content, scriptargs, cmdargs, dump);
			lines_topk_l += content + "\n";
			WriteFile(Utility::GetHex(topk_l[i]->GetFormID()) + ".scriptargs.txt", "topk_length", scriptargs);
			WriteFile(Utility::GetHex(topk_l[i]->GetFormID()) + ".cmdargs.txt", "topk_length", cmdargs);
			WriteFile(Utility::GetHex(topk_l[i]->GetFormID()) + ".dump.txt", "topk_length", dump);
		}
		WriteFile("topk_length.csv", "", inputHeader + lines_topk_l);
		// topk secondary
		for (int64_t i = 0; i < (int64_t)topk_s.size(); i++) {
			current++;
			PrintInput(topk_s[i], content, scriptargs, cmdargs, dump);
			lines_topk_s += content + "\n";
			WriteFile(Utility::GetHex(topk_s[i]->GetFormID()) + ".scriptargs.txt", "topk_secondary", scriptargs);
			WriteFile(Utility::GetHex(topk_s[i]->GetFormID()) + ".cmdargs.txt", "topk_secondary", cmdargs);
			WriteFile(Utility::GetHex(topk_s[i]->GetFormID()) + ".dump.txt", "topk_secondary", dump);
		}
		WriteFile("topk_secondary.csv", "", inputHeader + lines_topk_s);
		// positive
		for (int64_t i = 0; i < (int64_t)pos_inputs.size(); i++) {
			current++;
			PrintInput(pos_inputs[i], content, scriptargs, cmdargs, dump);
			positive += content + "\n";
			WriteFile(Utility::GetHex(pos_inputs[i]->GetFormID()) + ".scriptargs.txt", "positive", scriptargs);
			WriteFile(Utility::GetHex(pos_inputs[i]->GetFormID()) + ".cmdargs.txt", "positive", cmdargs);
			WriteFile(Utility::GetHex(pos_inputs[i]->GetFormID()) + ".dump.txt", "positive", dump);
		}
		WriteFile("positive.csv", "", inputHeader + positive);
	}

	if (reg)
		Lua::UnregisterThread();
}
