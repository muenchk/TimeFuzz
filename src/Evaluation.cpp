
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
	// ### Generations
	{
		std::string header;
		std::string avline;
		std::string lines;
		header = "Generation Size;DD Size;Number of DD;Runtime;Tests per Minute;Average Test Execution Time;Average DD Runtime;Total primary score increase;Total secondary increase\n";
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

			line += std::to_string(gensize) + ";" + std::to_string(ddsize) + ";" + std::to_string(numberofdd) + ";" + Logging::FormatTimeNS(runtime.count()) + ";" + std::to_string(testsperminute) + ";" + Logging::FormatTimeNS(averagetestexectime.count()) + ";" + Logging::FormatTimeNS(averageddruntime.count()) + ";" + std::to_string(totalprimaryscoreinc) + ";" + std::to_string(totalsecondaryscoreinc) + "\n";
			lines += line;
		}

		avline = std::to_string(avgensize) + ";" + std::to_string(avddsize) + ";" + std::to_string(avnumberofdd) + ";" + Logging::FormatTimeNS(avruntime.count()) + ";" + std::to_string(avtestsperminute) + ";" + Logging::FormatTimeNS(avaveragetestexectime.count()) + ";" + Logging::FormatTimeNS(avaverageddruntime.count()) + ";" + std::to_string(avtotalprimaryscoreinc) + ";" + std::to_string(avtotalsecondaryscoreinc) + "\n";

		csv.GenerationAve = header + avline;
		csv.Generations = header + lines;
	}
	// ### DDs
	{

	}
	// ### Inputs
	{

	}
}
