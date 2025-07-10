#pragma once
#include <iostream>
#include <vector>

#include "Threading.h"

enum class ErrorBit
{
	None = 0,
	eofbit = 0x1,
	failbit = 0x2,
	badbit = 0x4,
};

class MemoryStream
{
private:

	//std::atomic_flag _lock = ATOMIC_FLAG_INIT;
	std::vector<char*> _parts;

	int64_t BLOCKSIZE = 4096;

	/// <summary>
	/// pointer to current part in [_parts]
	/// </summary>
	int64_t _part = 0;
	/// <summary>
	/// current position in the current part
	/// </summary>
	int64_t _pos = 0;
	/// <summary>
	/// total number of bytes avail
	/// </summary>
	int64_t _size = 0;
	/// <summary>
	/// last part written
	/// </summary>
	int64_t _lpart = 0;
	/// <summary>
	/// last position in the last written part
	/// </summary>
	int64_t _lpos = 0;

	/// <summary>
	/// maximum size of memory stream
	/// </summary>
	int64_t _maxsize = INT64_MAX;

	bool __initdata = false;
	bool __fixedsize = false;

	ErrorBit _error = ErrorBit::None;

	int64_t _gcount = 0;

public:
	MemoryStream(int64_t maxsize, bool fixedsize, int64_t blocksize = 4096);
	MemoryStream(char* data, int64_t count);
	MemoryStream(int64_t blocksize = 4096);

	ErrorBit error() {
		return _error;
	}

	// copy assignment
	MemoryStream& operator=(MemoryStream& other);
	// move assignment
	MemoryStream& operator=(MemoryStream&& other) noexcept;

	~MemoryStream();

	int64_t gcount();
	int64_t tellg();
	void seekg(int64_t position);
	void seekg(std::streamoff off, std::ios_base::seekdir way);

	int get();
	void get(char& c);
	void get(char* s, int64_t n);
	void getline(char* s, int64_t n);
	void unget();

	void ignore(int64_t n, int delim = EOF);

	int peek();

	int64_t read(char* s, int64_t n);

	void put(char c);
	int64_t write(const char* s, int64_t n);
	int64_t tellp();
	void seekp(int64_t position);
	void seekp(std::streamoff off, std::ios_base::seekdir way);
};
