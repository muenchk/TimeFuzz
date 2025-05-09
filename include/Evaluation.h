#pragma once

#include <memory>
#include <string>
#include <vector>
#include <filesystem>
#include <iostream>
#include <fstream>

class Session;
class SessionData;
class Data;

namespace DeltaDebugging
{
	class DeltaController;
}

class Evaluation
{
	struct CSV
	{
		std::string General = "";
		std::string GenerationAve = "";
		std::string Generations;
		std::vector<std::pair<std::string,std::string>> GenInputs;
		std::string DDAve = "";
		std::string DDs = "";
		std::vector<std::pair<std::string, std::string>> DDInputs;
		std::string InputAve = "";
		std::string TopKInputs = "";
	};

private:
	std::shared_ptr<Session> _session;
	std::shared_ptr<SessionData> _sessiondata;
	Data* _data;
	std::filesystem::path _resultpath;

	void WriteFile(std::string filename, std::string subpath, std::string content);
	std::string PrintInput(std::shared_ptr<Input> input, std::string& str, std::string& scriptargs, std::string& cmdargs, std::string& dump);

public:
	Evaluation(std::shared_ptr<Session> session,std::filesystem::path resultpath);
	Evaluation() {}

	void Evaluate(int64_t& total, int64_t& current);
	void Evaluate(std::shared_ptr<Generation> generation);
	void Evaluate(std::shared_ptr<DeltaDebugging::DeltaController> ddcontroller);
	void EvaluateTopK();
	void EvaluatePositive();
	void EvaluateGeneral();

	static Evaluation* GetSingleton();

	void SetSession(std::shared_ptr<Session> session)
	{
		_session = session;
		_sessiondata = session->_sessiondata;
		_data = _sessiondata->data;
	}

	void SetResultPath(std::filesystem::path resultpath)
	{
		_resultpath = resultpath;
	}

};
