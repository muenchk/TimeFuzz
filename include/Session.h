#pragma once

#include <functional>

#include "Generator.h"
#include "Oracle.h"
#include "Settings.h"
#include "TaskController.h"

class Session
{
public:
	/// <summary>
	/// Returns a singleton the the session
	/// </summary>
	/// <returns></returns>
	Session* GetSingleton();

	/// <summary>
	/// Starts the fuzzing session
	/// </summary>
	void StartSession(Settings* settings);

private:
	/// <summary>
	/// The Task used to execute the PUT and retrieve the oracle result
	/// </summary>
	std::shared_ptr<Oracle> _oracle;

	/// <summary>
	/// the current iteration of the fuzzing main routine
	/// </summary>
	int _iteration = 0;

	/// <summary>
	/// Task controller for the active session
	/// </summary>
	std::shared_ptr<TaskController> _controller;

	/// <summary>
	/// Generator for fuzzing
	/// </summary>
	std::shared_ptr<Generator> _generator;

	Settings* _settings = nullptr;

	/// <summary>
	/// Does one iteration of the session
	/// </summary>
	void Iterate();
	/// <summary>
	/// Makes sure no other tasks are currently running on data
	/// </summary>
	void Snap();

	/// <summary>
	/// Saves the current program state to disc
	/// </summary>
	void Save();
	/// <summary>
	/// Loads the program state from disc
	/// </summary>
	void Load();
};
