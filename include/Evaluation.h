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

class File
{
public:
	std::string filename = "";
	std::string path = "";
	std::string content = "";
};

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

	void Evaluate(int64_t& total, int64_t& current);


};
