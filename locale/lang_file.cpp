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
#include <locale.hpp>
#include <sys/types.h>
#include <sys/stat.h>
#include <fast_cgi/application.hpp>

#ifndef _WIN32
#define _stat stat
#endif

#define LANGTEXT_TAG 0x474E414Cu

// contents:
// 0 LANG
// 4 0
// 8 <count>
// 12 <offset0>
// 16 <length0>
// 20 <offset1>
// 24 <length1>
// ...
// <offset0> <string0>
// <offset0>+1 0

namespace lng
{
	LangFile::LangFile()
		: m_content(nullptr)
		, m_count(0)
		, m_offsets(nullptr)
		, m_strings(nullptr)
	{
	}

	struct auto_close
	{
		bool close;
		LangFile& file;
		auto_close(LangFile& file): close(true), file(file) {}
		void release() { close = false; }
		~auto_close()
		{
			if (close)
				file.close();
		}
	};

	ERR LangFile::open(const filesystem::path& path)
	{
		filesystem::status st{ path };
		if (!st.exists())
			return ERR_NO_FILE;
		size_t length = (size_t)st.file_size();

		close();
		m_content = new (std::nothrow) char[length];
		if (!m_content)
			return ERR_OOM;

		auto_close anchor(*this);

		auto nat = path.native();
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996)
#endif
		FILE* f = fopen(nat.c_str(), "rb");
#ifdef _MSC_VER
#pragma warning(pop)
#endif
		if (!f)
		{
			FLOG << "Could not open " << nat << ", errno: " << errno;
			return ERR_NO_FILE;
		}

		bool read = fread(m_content, length, 1, f) == 1;
		fclose(f);
		if (!read)
			return ERR_NO_FILE;

		// validation
		offset_t* ptr = (offset_t*)m_content;

		//header?
		if (length < sizeof(offset_t)* 3 ||
			ptr[0] != LANGTEXT_TAG ||
			ptr[1] != 0x00000000u)
		{
			return ERR_WRONG_HEADER;
		}

		m_count = ptr[2];
		if (m_count == 0)
		{
			//no more validation needed, no strings, close to release memory,
			//but return true to still use the file
			return ERR_OK;
		}

		// offsets?
		offset_t string_start = sizeof(offset_t)*(3 + 2*m_count);
		if (length < string_start)
			return ERR_OFFSETS_TRUNCATED;

		m_offsets = ptr + 3;
		m_strings = m_content + string_start;

		offset_t expected = string_start;
		for (offset_t i = 0; i < m_count; ++i)
		{
			if (m_offsets[i*2] != expected)
				return ERR_UNEXPECTED_OFFSET;

			m_offsets[i*2] -= string_start;
			expected += m_offsets[i*2 + 1] + 1;

			if (expected > length)
				return ERR_STRING_TRUNCATED;

			if (m_content[expected - 1] != 0)
				return ERR_STRING_UNTERMINATED;
		}

		anchor.release();
		return ERR_OK;

	}

	void LangFile::close()
	{
		delete [] m_content;
		m_count = 0;
		m_offsets = nullptr;
		m_strings = nullptr;
		m_content = nullptr;
	}

	const char* LangFile::getString(unsigned int i)
	{
		if (i >= m_count)
			return nullptr;
		return m_strings + m_offsets[i*2];
	}
}