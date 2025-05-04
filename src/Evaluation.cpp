
#include "Data.h"
#include "DeltaDebugging.h"
#include "Evaluation.h"
#include "Generation.h"
#include "Session.h"
#include "SessionData.h"
#include "SessionFunctions.h"

#include <chrono>

Evaluation::Evaluation(std::shared_ptr<Session> session)
{
	_session = session;
	_sessiondata = session->_sessiondata;
	_data = _sessiondata->data;
}

void InputVisitor(std::shared_ptr<Input>)
{

}

Evaluation::CSV Evaluation::Evaluate()
{
	CSV csv;

	auto hashmap = _data->GetWeakHashmapCopy();
	auto generations = _data->GetFormArray<Generation>();
	auto dds = _data->GetFormArray<DeltaDebugging::DeltaController>();
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
		line += ";" + std::to_string(_sessiondata->GetGeneratedInputs());
		// Number of delta debugging tests
		header += ";DD Tests";
		line += ";" + std::to_string(SessionStatistics::TestsExecuted(_sessiondata) - _sessiondata->GetGeneratedInputs());
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
		line += ";" + std::to_string(((double)SessionStatistics::TestsExecuted(_sessiondata) / std::chrono::duration_cast<std::chrono::seconds>(SessionStatistics::Runtime(_sessiondata)).count()) * 60);

		// test exit stats
		auto stats = _sessiondata->GetTestExitStats();
		header += ";Natural Exit;LastInput Exit;Terminated Exit;Timeout Exit;FragmentTimeout Exit;Memory Exit;Pipe Exit;InitError Exit;Repeat Exit";
		line += ";" + std::to_string(stats.natural) + ";" + std::to_string(stats.lastinput) + ";" + std::to_string(stats.terminated) + ";" + std::to_string(stats.timeout) + ";" + std::to_string(stats.fragmenttimeout) + ";" + std::to_string(stats.memory) + ";" + std::to_string(stats.pipe) + ";" + std::to_string(stats.initerror) + ";" + std::to_string(stats.repeat);

		csv.General = header + "\n" + line + "\n";
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
		header = "ID;Generation Size;DD Size;Number of DD;Runtime;Tests per Minute;Average Test Execution Time;Average DD Runtime;Total primary score increase;Total secondary increase\n";
		int64_t gensize = 0, avgensize = 0, ddsize = 0, avddsize = 0, numberofdd = 0, avnumberofdd = 0;
		double testsperminute = 0.f, avtestsperminute = 0.f, totalprimaryscoreinc = 0.f, avtotalprimaryscoreinc = 0.f, totalsecondaryscoreinc = 0.f, avtotalsecondaryscoreinc = 0.f;
		std::chrono::nanoseconds averageddruntime, avaverageddruntime, averagetestexectime, avaveragetestexectime,runtime, avruntime;
		for (int i = 0; i < (int)generations.size(); i++)
		{
			std::string line = "";

			gensize = generations[i]->GetTrueGeneratedSize();
			avgensize += generations[i]->GetTrueGeneratedSize();
			ddsize = generations[i]->GetDDSize();
			avddsize += generations[i]->GetDDSize();
			numberofdd = generations[i]->GetNumberOfDDControllers();
			avnumberofdd += generations[i]->GetNumberOfDDControllers();
			testsperminute = (double)(gensize + ddsize) / std::chrono::duration_cast<std::chrono::seconds>(generations[i]->GetRunTime()).count();
			avtestsperminute += (double)(gensize + ddsize) / std::chrono::duration_cast<std::chrono::seconds>(generations[i]->GetRunTime()).count();
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
			averageddruntime = averageddruntime / ddcontrollers.size();
			avaverageddruntime = averageddruntime;
			averagetestexectime = std::chrono::nanoseconds(0);
			auto inputVisitor = [&averagetestexectime](std::shared_ptr<Input> form) {
				averagetestexectime = averagetestexectime + form->GetExecutionTime();
				return false;
			};
			generations[i]->VisitGeneratedInputs(inputVisitor);
			generations[i]->VisitDDInputs(inputVisitor);
			averagetestexectime = avaveragetestexectime / (gensize + ddsize);
			avaveragetestexectime += averagetestexectime;
			runtime = generations[i]->GetRunTime();
			avruntime += runtime;

			line += Utility::GetHex(generations[i]->GetFormID()) + ";" + std::to_string(gensize) + ";" + std::to_string(ddsize) + ";" + std::to_string(numberofdd) + ";" + Logging::FormatTimeNS(runtime.count()) + ";" + std::to_string(testsperminute) + ";" + Logging::FormatTimeNS(averagetestexectime.count()) + ";" + Logging::FormatTimeNS(averageddruntime.count()) + ";" + std::to_string(totalprimaryscoreinc) + ";" + std::to_string(totalsecondaryscoreinc) + "\n";
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

			csv.GenInputs.push_back({ "Generation_" + std::to_string(generations[i]->GetGenerationNumber()), inputs });
		}

		avline = "NONE;" + std::to_string(avgensize) + ";" + std::to_string(avddsize) + ";" + std::to_string(avnumberofdd) + ";" + Logging::FormatTimeNS(avruntime.count()) + ";" + std::to_string(avtestsperminute) + ";" + Logging::FormatTimeNS(avaveragetestexectime.count()) + ";" + Logging::FormatTimeNS(avaverageddruntime.count()) + ";" + std::to_string(avtotalprimaryscoreinc) + ";" + std::to_string(avtotalsecondaryscoreinc) + "\n";

		csv.GenerationAve = header + avline;
		csv.Generations = header + lines;
	}
	// ### DDs
	{
		std::string header;
		std::string avline;
		std::string lines;

		header = "ID;";
		int64_t size = 0,	 skipped = 0,	 prefix = 0,	approx = 0,    reducsteps = 0,	  avsizereducperstep = 0,    batchcount = 0,    totaltests = 0, level = 0;
		int64_t size_av = 0, skipped_av = 0, prefix_av = 0, approx_av = 0, reducsteps_av = 0, avsizereducperstep_av = 0, batchcount_av = 0, totaltests_av = 0, level_av = 0;
		int64_t size_to = 0, skipped_to = 0, prefix_to = 0, approx_to = 0, reducsteps_to = 0, avsizereducperstep_to = 0, batchcount_to = 0, totaltests_to = 0, level_to = 0;

		double totalreduction_l = 0.f, totalreduction_p = 0.f, totalreduction_s = 0.f;
		double totalreduction_l_av = 0.f, totalreduction_p_av = 0.f, totalreduction_s_av = 0.f;
		double totalreduction_l_to = 0.f, totalreduction_p_to = 0.f, totalreduction_s_to = 0.f;

		std::chrono::nanoseconds runtime = std::chrono::nanoseconds(0), avtestruntime = std::chrono::nanoseconds(0);
		std::chrono::nanoseconds runtime_av = std::chrono::nanoseconds(0), avtestruntime_av = std::chrono::nanoseconds(0);
		std::chrono::nanoseconds runtime_to = std::chrono::nanoseconds(0), avtestruntime_to = std::chrono::nanoseconds(0);

		
		for (int i = 0; i < (int)dds.size(); i++)
		{
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
			totalreduction_l = (double)dds[i]->GetInput()->EffectiveLength() / (double)dds[i]->GetOriginalInput()->EffectiveLength();
			totalreduction_l_av += (double)dds[i]->GetInput()->EffectiveLength() / (double)dds[i]->GetOriginalInput()->EffectiveLength();
			totalreduction_l_to += (double)dds[i]->GetInput()->EffectiveLength() / (double)dds[i]->GetOriginalInput()->EffectiveLength();
			totalreduction_p = dds[i]->GetInput()->GetPrimaryScore() / dds[i]->GetInput()->GetPrimaryScore();
			totalreduction_p_av += dds[i]->GetInput()->GetPrimaryScore() / dds[i]->GetInput()->GetPrimaryScore();
			totalreduction_p_to += dds[i]->GetInput()->GetPrimaryScore() / dds[i]->GetInput()->GetPrimaryScore();
			totalreduction_s = dds[i]->GetInput()->GetSecondaryScore() / dds[i]->GetInput()->GetSecondaryScore();
			totalreduction_s_av += dds[i]->GetInput()->GetSecondaryScore() / dds[i]->GetInput()->GetSecondaryScore();
			totalreduction_s_to += dds[i]->GetInput()->GetSecondaryScore() / dds[i]->GetInput()->GetSecondaryScore();
			runtime = dds[i]->GetRunTime();
			runtime_av += dds[i]->GetRunTime();
			runtime_to += dds[i]->GetRunTime();
			avtestruntime = std::chrono::nanoseconds(0);
			auto inputVisitor = [&avtestruntime](std::shared_ptr<Input> form) {
				avtestruntime = avtestruntime + form->GetExecutionTime();
				return false;
			};
			for (auto [inp, _] : *(dds[i]->GetResults()))
			{
				inputVisitor(inp);
			}
			avtestruntime_av += avtestruntime;
			avtestruntime_to += avtestruntime;

		}
		
	}
	// ### Inputs
	{

	}
}
