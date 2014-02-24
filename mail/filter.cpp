/*
 * Copyright (C) 2013 midnightBITS
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "pch.h"
#include <filter.hpp>

#if WIN32
#include <io.h>
#include <fcntl.h>

static inline int pipe(int* fd) { return _pipe(fd, 2048, O_BINARY); }

#else
#include <unistd.h>
#define _read read
#define _close close
#endif

namespace filter
{
#ifdef POSIX
#define _read read
#define _write write
#define _close close
#endif

	size_t FileDescriptor::read(void* buffer, size_t len) { return ::_read(m_fd, buffer, len); }
	size_t FileDescriptor::write(const void* buffer, size_t len) { return ::_write(m_fd, buffer, len); }
	void FileDescriptor::close() { if (m_fd) ::_close(m_fd); m_fd = 0; }

	void Base64Block::onChar(char c)
	{
		next(c);
		if (++m_ptr == 75)
		{
			m_ptr = 0;
			next("\r\n");
		}
	}

	void Quoting::onChar(char c)
	{
		if (c == '=' || c == '.' || ((unsigned char)c > 0x80))
		{
			static const char hex[] = "0123456789ABCDEF";
			next('=');
			next(hex[((unsigned char)c >> 4) & 0xF]);
			next(hex[ (unsigned char)c       & 0xF]);
		}
		else next(c);
	}

	void QuotedPrintable::onChar(char c)
	{
		if (c == '\r') return;
		if (c == '\n')
		{
			next("\r\n");
			m_ptr = 0;
			m_line = 77;
			return;
		}

		if (c == '=' && m_ptr >= 75)
		{
			m_line = m_ptr + 3;
		}

		next(c);

		m_ptr++;
		if (m_ptr == m_line)
		{
			next("=\r\n");
			m_ptr = 0;
			m_line = 77;
		}
	}

	bool Pipe::open()
	{
		int fd[2];
		if (pipe(fd) == -1)
			return false;

		reader.set(fd[0]);
		writer.set(fd[1]);

		return true;
	}
};
