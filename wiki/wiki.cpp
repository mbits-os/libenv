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
#include <wiki.hpp>
#include <filesystem.hpp>
#include "wiki_parser.hpp"

namespace wiki
{
	document_ptr compile(const filesystem::path& file)
	{
		filesystem::status st{ file };
		if (!st.exists())
			return nullptr;

		// TODO: branch here for pre-compiled WIKI file

		size_t size = (size_t)st.file_size();

		std::string text;
		text.resize(size + 1);
		if (text.size() <= size)
			return nullptr;

		auto f = fopen(file.native().c_str(), "r");
		if (!f)
			return nullptr;
		fread(&text[0], 1, size, f);
		fclose(f);

		text[size] = 0;

		return compile(text);
		// TODO: stroe binary version of the document here
	}

	document_ptr compile(const std::string& text)
	{
		auto out = parser::Parser{}.parse(text);

		return nullptr;
	}
}
