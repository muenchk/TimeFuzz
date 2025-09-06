/* parts of this files source code are licensed under the "The Code Project Open License (CPOL)" 
* available at https://www.codeproject.com/info/cpol10.aspx as of the
* date 2024/10/24
*/

/* parts of this files source code are taken from the examples of the official repository of liblzma
* date 2024/10/24
*/


#include "LZMAStreamBuf.h"
#include "Logging.h"
#include <cassert>


Streambuf::Streambuf(std::istream* pIn) :
	_bufferSize(2097152),  // free to chose
	_in(pIn)
{
	_buffer.reset(new char[_bufferSize + 1]);

	setg(&_buffer[0], &_buffer[1], &_buffer[1]);
}

Streambuf::Streambuf(std::ostream* pOut) :
	_bufferSize(2097152),  // free to chose
	_out(pOut)
{
	_buffer.reset(new char[_bufferSize]);

	setp(_buffer.get(), _buffer.get() + _bufferSize);
}

int Streambuf::underflow()
{
	// Do nothing if data is still available (sanity check)
	if (this->gptr() < this->egptr())
		return traits_type::to_int_type(*this->gptr());

	// Read from the file, maximum m_nBufLen bytes
	_in->read(_buffer.get(), _bufferSize);

	// check for possible I/O error
	if (_in->bad())
		throw std::runtime_error("LZMAStreamBuf: Error while reading the provided input stream!");

	setg(_buffer.get(), _buffer.get(), _buffer.get() + _in->gcount());
	return traits_type::to_int_type(*(_buffer.get()));
}

void Streambuf::flush()
{
	overflow(traits_type::eof());
}

int Streambuf::overflow(int c)
{
	char buf;
	if (!traits_type::eq_int_type(c, traits_type::eof())) {
		if (pptr() == nullptr)
			setp(&buf, &buf + 1);
		*pptr() = traits_type::to_char_type(c);
		pbump(1);
	}

	if (pptr() != pbase()) {
		_out->write(pbase(), pptr() - pbase());
		setp(_buffer.get(), _buffer.get() + _bufferSize);
	}
	return c;
}



LZMAStreambuf::LZMAStreambuf(std::istream* pIn) :
	_bufferSize(2097152),  // free to chose
	_in(pIn),
	_lzmaStream(LZMA_STREAM_INIT)
{
	_compressedBuffer.reset(new char[_bufferSize]);
	_decompressedBuffer.reset(new char[_bufferSize]);

	// Initially indicate that the buffer is empty
	setg(&_decompressedBuffer[0], &_decompressedBuffer[1], &_decompressedBuffer[1]);

	// try to open the decoder:
	lzma_ret ret = lzma_stream_decoder(&_lzmaStream, std::numeric_limits<uint64_t>::max(), LZMA_CONCATENATED);
	switch (ret) {
	case LZMA_MEM_ERROR:
		logcritical("Cannot allocate memory for Easy LZMA encoder.");
		throw std::runtime_error("LZMA decoder could not be opened");
		break;
	case LZMA_OPTIONS_ERROR:
		logcritical("Preset for Easy LZMA encoder is unknown.");
		throw std::runtime_error("LZMA decoder could not be opened");
		break;
	case LZMA_UNSUPPORTED_CHECK:
		logcritical("Preset for Easy LZMA encoder is unknown.");
		throw std::runtime_error("LZMA decoder could not be opened");
		break;
	case LZMA_OK:
		break;
	default:
		logcritical("Unknown error in Easy LZMA encoder.");
		throw std::runtime_error("LZMA decoder could not be opened");
		break;
	}

	_lzmaStream.avail_in = 0;
}

LZMAStreambuf::LZMAStreambuf(std::ostream* pOut, uint32_t compressionLevel, bool extreme, int32_t threads) :
	_bufferSize(2097152),  // free to chose
	_out(pOut),
	_lzmaStream(LZMA_STREAM_INIT)
{
	_compressedBuffer.reset(new char[_bufferSize]);
	_decompressedBuffer.reset(new char[_bufferSize]);

	setp(_decompressedBuffer.get(), _decompressedBuffer.get() + _bufferSize);

	lzma_ret ret = LZMA_OK;
	// try to open the encoder:
//#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	if (threads == 1) {
//#endif
		ret = lzma_easy_encoder(&_lzmaStream, compressionLevel | (extreme ? LZMA_PRESET_EXTREME : 0), LZMA_CHECK_CRC64);
//#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	} else {
		_lzmaOptions.check = LZMA_CHECK_CRC64;
		_lzmaOptions.preset = compressionLevel | (extreme ? LZMA_PRESET_EXTREME : 0);
		_lzmaOptions.timeout = 0;
		_lzmaOptions.threads = threads;
		_lzmaOptions.filters = nullptr;
		_lzmaOptions.block_size = 2097152;
		_lzmaOptions.flags = 0;
		_lzmaOptions.memlimit_threading = 3221225472;
		ret = lzma_stream_encoder_mt(&_lzmaStream, &_lzmaOptions);
	}
//#endif

	switch (ret) {
	case LZMA_MEM_ERROR:
		logcritical("Cannot allocate memory for Easy LZMA encoder.");
		throw std::runtime_error("LZMA encoder could not be opened");
		break;
	case LZMA_OPTIONS_ERROR:
		logcritical("Preset for Easy LZMA encoder is unknown.");
		throw std::runtime_error("LZMA encoder could not be opened");
		break;
	case LZMA_UNSUPPORTED_CHECK:
		logcritical("Preset for Easy LZMA encoder is unknown.");
		throw std::runtime_error("LZMA encoder could not be opened");
		break;
	case LZMA_OK:
		break;
	default:
		logcritical("Unknown error in Easy LZMA encoder.");
		throw std::runtime_error("LZMA encoder could not be opened");
		break;
	}
	_lzmaStream.avail_in = 0;
	_lzmaStream.next_out = reinterpret_cast<unsigned char*>(_compressedBuffer.get());
	_lzmaStream.total_out = 0;
	_lzmaStream.avail_out = _bufferSize;
}

LZMAStreambuf::~LZMAStreambuf()
{
	lzma_end(&_lzmaStream);
}

int LZMAStreambuf::underflow()
{
	lzma_action action = LZMA_RUN;
	lzma_ret ret = LZMA_OK;

	// Do nothing if data is still available (sanity check)
	if (this->gptr() < this->egptr())
		return traits_type::to_int_type(*this->gptr());

	while (true) {
		_lzmaStream.next_out =
			reinterpret_cast<unsigned char*>(_decompressedBuffer.get());
		_lzmaStream.avail_out = _bufferSize;

		if (_lzmaStream.avail_in == 0) {
			// Read from the file, maximum m_nBufLen bytes
			_in->read(&_compressedBuffer[0], _bufferSize);

			// check for possible I/O error
			if (_in->bad()) {
				return EOF;
				//throw std::runtime_error("LZMAStreamBuf: Error while reading the provided input stream!");
			}

			_lzmaStream.next_in =
				reinterpret_cast<unsigned char*>(_compressedBuffer.get());
			_lzmaStream.avail_in = _in->gcount();
		}

		// check for eof of the compressed file;
		// if yes, forward this information to the LZMA decoder
		if (_in->eof())
			action = LZMA_FINISH;

		// DO the decoding
		ret = lzma_code(&_lzmaStream, action);

		// check for data
		// NOTE: avail_out gives that amount of data which is available for LZMA to write!
		//         NOT the size of data which has been written for us!
		if (_lzmaStream.avail_out < _bufferSize) {
			const size_t nDataAvailable = _bufferSize - _lzmaStream.avail_out;

			// Let std::streambuf know how much data is available in the buffer now
			setg(&_decompressedBuffer[0], &_decompressedBuffer[0],
				&_decompressedBuffer[0] + nDataAvailable);
			return traits_type::to_int_type(_decompressedBuffer[0]);
		}

		if (ret != LZMA_OK) {
			if (ret == LZMA_STREAM_END) {
				// This return code is desired if eof of the source file has been reached
				assert(action == LZMA_FINISH);
				assert(_in->eof());
				assert(_lzmaStream.avail_out == _bufferSize);
				return traits_type::eof();
			}

			// an error has occurred while decoding; reset the buffer
			setg(nullptr, nullptr, nullptr);

			// Throwing an exception will set the bad bit of the istream object
			std::stringstream err;
			err << "Error " << ret << " occurred while decoding LZMA file!";
			throw std::runtime_error(err.str().c_str());
		}
	}
}



int LZMAStreambuf::overflow(int c)
{
	// for this function:
	//    lzmaStream->next_out = _compressedBuffer
	//    lzmaStream->next_in = inBuffer

	lzma_action action = LZMA_RUN;
	lzma_ret ret = LZMA_OK;

	static std::unique_ptr<char[]> inBuffer = std::make_unique<char[]>(_bufferSize * 2);
	// buffer for temporary storage of intermediate _input data
	static std::unique_ptr<char[]> tmpBuffer = std::make_unique<char[]>(_bufferSize * 2);
	int64_t tmpPtr = 0;

	// check if there is still uncompressed data available to lzma and copy it to our new buffer
	size_t avail = 0;
	if (_lzmaStream.avail_in > 0) {
		if (_lzmaStream.total_in == 0)
			// if we haven't read those bytes at all, no need to copy them
			avail = _lzmaStream.avail_in;
		else {
			// there are still bytes remaining in the _input.
			avail = _lzmaStream.avail_in;
			memcpy(tmpBuffer.get(), inBuffer.get() + _lzmaStream.total_in, avail);
			memcpy(inBuffer.get(), tmpBuffer.get(), avail);
		}
	}
	// if there is stuff available that needs to be compressed we need to copy it to the in Buffer
	if (pptr() != pbase()) {
		// copy from public _input buffer _decompressedBuffer to our inBuffer for lzma
		// copy the number of bytes available
		memcpy(inBuffer.get() + avail, _decompressedBuffer.get(), pptr() - pbase());
		avail += pptr() - pbase();
		// set _output buffer pointers
		setp(_decompressedBuffer.get(), _decompressedBuffer.get() + _bufferSize);
	}
	// set the _input buffer for lzma
	_lzmaStream.next_in = reinterpret_cast<unsigned char*>(inBuffer.get());
	_lzmaStream.total_in = 0;
	_lzmaStream.avail_in = avail;

	if (c == traits_type::eof())
	{
		// we are writing the end of file, and should flush all data in our buffers
		// we don't need to write c itself
		// set the action to LZMA_FINISH to finish up all compression
		action = LZMA_FINISH;
		while (true)
		{
			ret = lzma_code(&_lzmaStream, action);
			// if ret is not OK or the end of the stream we have an error at our hands
			if (ret != LZMA_OK && ret != LZMA_STREAM_END) {
				logcritical("Error occured while encoding LZMA file, Code: {}", (int32_t)ret);
				throw std::runtime_error("Error occured while encoding LZMA file.");
			}
			_out->write(_compressedBuffer.get(), _lzmaStream.total_out);
			_lzmaStream.next_out = reinterpret_cast<unsigned char*>(_compressedBuffer.get());
			_lzmaStream.total_out = 0;
			_lzmaStream.avail_out = _bufferSize;
			if (ret == LZMA_STREAM_END)
			{
				_out->flush();
				return traits_type::eof();
			}
		}
	}
	else
	{
		// we are just writing stuff so set the action to RUN and just run with it
		action = LZMA_RUN;
		while (true) {
			ret = lzma_code(&_lzmaStream, action);
			// if ret is not ok write some meaningful message
			if (ret != LZMA_OK && ret != LZMA_BUF_ERROR) { 
				// fatal
				logcritical("Error occured while encoding LZMA file, Code: {}", (int32_t)ret);
				throw std::runtime_error("Error occured while encoding LZMA file.");
			}
			// check if there is data to write back
			if (_lzmaStream.total_out > 0) {
				// buf error just means that theres either not enough _input, which can be fixed on the next call
				// or it means that not enough _output space is available which can be fixed by writing off data
				_out->write(_compressedBuffer.get(), _lzmaStream.total_out);
				_lzmaStream.next_out = reinterpret_cast<unsigned char*>(_compressedBuffer.get());
				_lzmaStream.total_out = 0;
				_lzmaStream.avail_out = _bufferSize;
			}
			if (_lzmaStream.avail_in == 0 || ret == LZMA_BUF_ERROR) {
				// write c to the buffer so it can be written next time
				if (pptr() == nullptr) {
					setp(_decompressedBuffer.get(), _decompressedBuffer.get() + _bufferSize);
				}
				*pptr() = traits_type::to_char_type(c);
				pbump(1);
				return traits_type::not_eof(c);
			}
		}
	}
}
