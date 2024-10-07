#pragma once

enum ExitCodes
{
	Success = 0x0,
	Error = 0x1,
	ArgumentError = 0x2,
	StartupError = 0x10,
	IterationError = 0x20,
};
