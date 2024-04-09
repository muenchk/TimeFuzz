#pragma once

#include <filesystem>

#include "Generator.h"
#include "Oracle.h"

class Settings
{
public:
	/// <summary>
	/// Returns a singleton the the session
	/// </summary>
	/// <returns></returns>
	Settings* GetSingleton();

	/// <summary>
	/// Loads the settings for the working directory
	/// </summary>
	void Load();
	/// <summary>
	/// Saves the settings in the working directory
	/// </summary>
	void Save();

	/// <summary>
	/// Type of the oracle that is executed
	/// </summary>
	Oracle::OracleType oracle = Oracle::OracleType::PythonScript;
	/// <summary>
	/// path the the oracle executing program/script OR the PUT itself
	/// </summary>
	std::filesystem::path oraclepath;

	struct General
	{
		/// <summary>
		/// uses the number of available hardware threads for background computation
		/// </summary>
		bool usehardwarethreads = false;
		/// <summary>
		/// the number of backgroundthreads to use if [usehardwarethreads] is set to false
		/// </summary>
		int numthreads = 20;
	};

	General general;

	struct Optimization
	{
		/// <summary>
		/// input sequences or partial input sequences, that do not result in either a bug or a successful run, may be used as starting point for generation and expanded, until the produce a clear result
		/// [This should not be used with PUT that produce UNDEFINED oracle results]
		/// </summary>
		bool constructinputsitertively = true;
	};

	Optimization optimization;

	struct Methods
	{
		/// <summary>
		/// whether to use delta debugging
		/// </summary>
		bool deltadebugging = true;
	};

	Methods methods;

	struct Generation
	{
		/// <summary>
		/// maximum generated inputs per generation cycle
		/// </summary>
		int generationsize = 100;

		/// <summary>
		/// starting parameter for automatic generationsize scaling
		/// </summary>
		float generationtweakstart = 0.2;

		/// <summary>
		/// max parameter for automatic generationsize scaling
		/// </summary>
		float generationtweakmax = 10;
	};

	Generation generation;
};
