/* parts of this files source code are licensed under the "The Code Project Open License (CPOL)" 
* available at https://www.codeproject.com/info/cpol10.aspx as of the
* date 2024/10/24
*/

/* parts of this files source code are taken from the examples of the official repository of liblzma
* date 2024/10/24
*/

#include <iostream>
#include <memory>

#include <lzma.h>

class Streambuf : public std::streambuf
{
public:
	Streambuf(std::istream* pIn);
	Streambuf(std::ostream* pOut);
	virtual int underflow() override;
	virtual int overflow(int c = EOF) override;
	Streambuf() : _bufferSize(2097152) {};

	void flush();

private:
	std::unique_ptr<char[]> _buffer;
	std::istream* _in;
	std::ostream* _out;
	const size_t _bufferSize;
};

class LZMAStreambuf : public Streambuf
{
public:
	LZMAStreambuf(std::istream* pIn);
	LZMAStreambuf(std::ostream* pOut, uint32_t compressionLevel = 9, bool extreme = false, int32_t threads = 1);

	virtual ~LZMAStreambuf();

	virtual int underflow() override final;
	virtual int overflow(int c = EOF) override final;

private:
	std::istream* _in;
	std::ostream* _out;
	std::unique_ptr<char[]> _compressedBuffer, _decompressedBuffer;
	const size_t _bufferSize;
	lzma_stream _lzmaStream;
	lzma_mt _lzmaOptions;
};
