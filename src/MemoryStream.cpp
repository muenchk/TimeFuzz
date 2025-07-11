#include <cmath>
#include <cstring>

#include "MemoryStream.h"

MemoryStream::MemoryStream(int64_t maxsize, bool fixedsize, int64_t blocksize)
{
	_maxsize = maxsize;
	__fixedsize = fixedsize;
	BLOCKSIZE = blocksize;
	if (fixedsize && maxsize > 0) {
		__fixedsize = true;
		_size = maxsize;
		BLOCKSIZE = maxsize;
		/////// ------------ alloc all
		_parts.push_back(new char[BLOCKSIZE]);
		_lpos = BLOCKSIZE-1;
	} else {
		_parts.push_back(new char[BLOCKSIZE]);
	}
}
MemoryStream::MemoryStream(char* data, int64_t count)
{
	__initdata = true;
	_size = count;
	__fixedsize = true;
	_parts.push_back(data);
}
MemoryStream::MemoryStream(int64_t blocksize)
{
	BLOCKSIZE = blocksize;
	_parts.push_back(new char[BLOCKSIZE]);
}

MemoryStream& MemoryStream::operator=(MemoryStream& other)
{
	//SpinlockA guard(_lock);
	//SpinlockA guardO(other._lock);
	if (this == &other)
		return *this;

	if (_parts.empty() == false) {
		for (int64_t c = 0; c < (int64_t)_parts.size(); c++) {
			delete[] _parts[c];
		}
		_parts.clear();
		_part = 0;
		_pos = 0;
		_lpart = 0;
		_lpos = 0;
	}
	if (other._parts.empty() == false) {
		for (int64_t c = 0; c < (int64_t)_parts.size(); c++) {
			delete[] _parts[c];
		}
		_parts.clear();
		_part = other._part;
		_pos = other._pos;
		_lpart = other._lpart;
		_lpos = other._lpos;

		// alloc space and copy parts
		BLOCKSIZE = other.BLOCKSIZE;
	}
	return *this;
}
MemoryStream& MemoryStream::operator=(MemoryStream&& other) noexcept
{
	//SpinlockA guard(_lock);
	//SpinlockA guardO(other._lock);
	if (this == &other)
		return *this;

	if (_parts.empty() == false) {
		for (int64_t c = 0; c < (int64_t)_parts.size(); c++) {
			delete[] _parts[c];
		}
		_parts.clear();
		_part = 0;
		_pos = 0;
		_lpart = 0;
		_lpos = 0;
	}
	std::swap(_parts, other._parts);
	return *this;
}

MemoryStream::~MemoryStream()
{
	//SpinlockA guard(_lock);
	if (__initdata) {
		_parts.clear();
	} else {
		for (int64_t c = 0; c < (int64_t)_parts.size(); c++) {
			delete[] _parts[c];
		}
		_parts.clear();
	}
}

#define INCPOS()             \
	_pos++;                  \
	if (_pos == BLOCKSIZE) { \
		_part++;             \
		_pos = 0;            \
	}
#define INCPOSX(x)                                          \
	{                                                       \
		_pos += x;                                          \
		while (_pos >= BLOCKSIZE) {                         \
			_part++;                                        \
			_pos -= BLOCKSIZE;                              \
		}                                                   \
		if (!(_part < _lpart || _pos < _lpos)) {            \
			_lpos = _pos;                                   \
			/* int64_t alloc = _part - _lpart; */           \
			_lpart = _part;                                 \
			_size = _lpart * BLOCKSIZE + _lpos;             \
			/*for (int64_t i = 0; i < alloc; i++)        \
			_parts.push_back(new char[BLOCKSIZE]);*/ \
		}                                                   \
	}

#define UpdateSize() \
	_size = _lpart * BLOCKSIZE + _lpos;


int64_t MemoryStream::gcount()
{
	_error = ErrorBit::None;
	return _gcount;
}
int64_t MemoryStream::tell()
{
	_error = ErrorBit::None;
	return BLOCKSIZE * _part + _pos;
}
void MemoryStream::seek(int64_t position)
{
	_error = ErrorBit::None;
	if (__fixedsize && position > _size || position > _maxsize) {
		_error = ErrorBit::badbit;
		return;
	}
	_part = (int64_t)trunc((double)position / BLOCKSIZE);
	_pos = position % BLOCKSIZE;
	if (_part == _lpart && _pos > _lpos) {
		_lpos = _pos;
		UpdateSize();
	}
	else if (_part > _lpart) {
		_lpos = _pos;
		int64_t alloc = _part - _lpart;
		_lpart = _part;
		UpdateSize();
		for (int64_t i = 0; i < alloc; i++)
			_parts.push_back(new char[BLOCKSIZE]);
	}
}
void MemoryStream::seek(std::streamoff off, std::ios_base::seekdir way)
{
	_error = ErrorBit::None;
	switch (way) {
	case std::ios_base::beg:
		if (off < 0 || off > _maxsize) {
			_error = ErrorBit::badbit;
			return;
		} else if (off > _size) {
			if (_part == _lpart && _pos > _lpos)
				_lpos = _pos;
			else if (_part > _lpart) {
				_lpos = _pos;
				int64_t alloc = _part - _lpart;
				_lpart = _part;
				_size = off;
				for (int64_t i = 0; i < alloc; i++)
					_parts.push_back(new char[BLOCKSIZE]);
			}
		} else {
			_part = (int64_t)trunc((double)off / BLOCKSIZE);
			_pos = off % BLOCKSIZE;
		}
		break;
	case std::ios_base::cur:
		off = BLOCKSIZE * _part + _pos + off;
		if (off < 0 || off > _maxsize) {
			_error = ErrorBit::badbit;
			return;
		} else if (off > _size || off > _maxsize) {
			if (_part == _lpart && _pos > _lpos)
				_lpos = _pos;
			else if (_part > _lpart) {
				_lpos = _pos;
				int64_t alloc = _part - _lpart;
				_lpart = _part;
				_size = off;
				for (int64_t i = 0; i < alloc; i++)
					_parts.push_back(new char[BLOCKSIZE]);
			}
		} else {
			_part = (int64_t)trunc((double)off / BLOCKSIZE);
			_pos = off % BLOCKSIZE;
		}
		break;
	case std::ios_base::end:
		off = BLOCKSIZE * _part + _pos + off;
		_part = (int64_t)trunc((double)off / BLOCKSIZE);
		_pos = off % BLOCKSIZE;
		if (__fixedsize && off > _size || off > _maxsize) {
			_error = ErrorBit::badbit;
			return;
		}
		if (_part == _lpart && _pos > _lpos)
			_lpos = _pos;
		else if (_part > _lpart) {
			_lpos = _pos;
			int64_t alloc = _part - _lpart;
			_lpart = _part;
			_size = off;
			for (int64_t i = 0; i < alloc; i++)
				_parts.push_back(new char[BLOCKSIZE]);
		}
	}
}

int MemoryStream::get()
{
	_error = ErrorBit::None;
	if (_part < _lpart || _pos < _lpos) {
		char ret = *(_parts[_part] + _pos);
		INCPOS();
		return ret;
	} else {
		_error = ErrorBit::eofbit;
		return 0;
	}
}
void MemoryStream::get(char& c)
{
	_error = ErrorBit::None;
	if (_part < _lpart || _pos < _lpos) {
		c = *(_parts[_part] + _pos);
		INCPOS();
	} else {
		_error = ErrorBit::eofbit;
		c = 0;
	}
}
void MemoryStream::get(char* s, int64_t n)
{
	_error = ErrorBit::None;
	int64_t max = _size - (_part * BLOCKSIZE + _pos);
	if (max > 0) {
		for (int64_t c = 0; c < n - 1 && c < max; c++) {
			if (_parts[_part][_pos] == '\n') {
				s[c] = '\0';
				INCPOS();
				return;
			} else {
				s[c] = _parts[_part][_pos];
				INCPOS();
			}
		}
		s[n - 1] = '\0';
		return;
	} else {
		_error = ErrorBit::eofbit;
		return;
	}
}

void MemoryStream::getline(char* s, int64_t n)
{
	return get(s, n);
}

void MemoryStream::unget()
{
	if (_part == 0 && _pos == 0)
	{
		_error = ErrorBit::badbit;
	} else {
		_pos--;
		if (_pos < 0)
		{
			_pos = BLOCKSIZE - _pos;
			_part--;
		}
	}
}

void MemoryStream::ignore(int64_t n, int delim)
{
	_error = ErrorBit::None;
	if (n == 0)
		return;
	int64_t max = _size - (_part * BLOCKSIZE + _pos);
	if (max > 0) {
		for (int64_t c = 0; c < n && c < max; c++) {
			if (_parts[_part][_pos] == delim) {
				INCPOS();
				return;
			} else {
				INCPOS();
			}
		}
		return;
	} else if (delim != EOF) {
		_error = ErrorBit::eofbit;
		return;
	}
}

int MemoryStream::peek()
{
	_error = ErrorBit::None;
	if (_part < _lpart || _pos < _lpos)
		return _parts[_part][_pos];
	else {
		_error = ErrorBit::eofbit;
		return 0;
	}
}

int64_t MemoryStream::read(char* s, int64_t n)
{
	_error = ErrorBit::None;
	int64_t max = _size - (_part * BLOCKSIZE + _pos);
	int64_t count = 0;
	if (max == 0)
	{
		_error = ErrorBit::eofbit;
		_gcount = 0;
		return 0;
	}
	else if (max >= n)
	{
		while (n > 0)
		{
			int64_t cp = BLOCKSIZE - _pos;
			if (cp <= n) {
				memcpy(s + count, _parts[_part] + _pos, cp);
				_pos = 0;
				_part++;
				n -= cp;
				count += cp;
			}
			else
			{
				memcpy(s + count, _parts[_part] + _pos, n);
				_pos += n;
				n = 0;
				count += n;
			}
		}
	} else {
		while (n > 0 && max > 0) {
			int64_t cp = BLOCKSIZE - _pos;
			if (cp <= n && cp <= max) {
				memcpy(s + count, _parts[_part] + _pos, cp);
				_pos = 0;
				_part++;
				n -= cp;
				count += cp;
				max -= cp;
			} else {
				memcpy(s + count, _parts[_part] + _pos, std::min(n, max));
				_pos -= std::min(n, max);
				n -= std::min(n, max);
				max -= std::min(n, max);
				count += std::min(n, max);
			}
		}
		if (max == 0)
			_error = ErrorBit::eofbit;
	}
	_gcount = count;
	return count;
}

void MemoryStream::put(char c)
{
	_error = ErrorBit::None;
	if (_lpart == _part && _lpos == _pos && (__fixedsize || _size + 1 > _maxsize))
	{
		_error = ErrorBit::failbit;
		return;
	}
	_parts[_part][_pos] = c;
	INCPOS();
	if (_part == _lpart && _pos > _lpos)
		_lpos = _pos;
	else if (_part > _lpart) {
		_lpos = _pos;
		_lpart = _part;
		_size++;
		_parts.push_back(new char[BLOCKSIZE]);
	}
}

int64_t MemoryStream::write(const char* s, int64_t n)
{
	_error = ErrorBit::None;
	int64_t npos = _part * BLOCKSIZE + _pos + n;
	if (__fixedsize && npos > _size || npos > _maxsize) {
		_error = ErrorBit::failbit;
		return 0;
	}
	int64_t npart = (int64_t)trunc((double)npos / BLOCKSIZE);
	int64_t alloc = npart - _lpart;
	for (int64_t i = 0; i < alloc; i++)
		_parts.push_back(new char[BLOCKSIZE]);

	int64_t count = 0;
	while (n > 0) {
		int64_t cp = BLOCKSIZE - _pos;
		if (cp <= n) {
			memcpy(_parts[_part] + _pos, s + count, cp);
			_pos = 0;
			_part++;
			n -= cp;
			count += cp;
		} else {
			memcpy(_parts[_part] + _pos, s + count, n);
			_pos += n;
			n = 0;
			count += n;
		}
	}
	if (_part > _lpart)
	{
		_lpart = _part;
		_lpos = _pos;
		UpdateSize();
	}
	else if (_part == _lpart && _pos > _lpos)
	{
		_lpos = _pos;
		UpdateSize();
	}
	_gcount = count;
	return count;
}
