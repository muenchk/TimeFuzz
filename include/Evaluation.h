#pragma once

#include <memory>
#include <string>
#include <vector>

class Session;
class SessionData;
class Data;

class Evaluation
{
	struct CSV
	{
		std::string General = "";
		std::string GenerationAve = "";
		std::string Generations;
		std::string DDAve = "";
		std::string DDs;
		std::string InputAve = "";
	};

private:
	std::shared_ptr<Session> _session;
	std::shared_ptr<SessionData> _sessiondata;
	Data* _data;

public:
	Evaluation(std::shared_ptr<Session> session);

	CSV Evaluate();
};
