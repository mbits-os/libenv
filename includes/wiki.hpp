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

#ifndef __WIKI__HPP__
#define __WIKI__HPP__

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <iosfwd>
#include <string.h>

namespace filesystem
{
	class path;
}

namespace wiki
{
	struct styler;
	struct document;

	using document_ptr = std::shared_ptr<document>;
	using styler_ptr = std::shared_ptr<styler>;
	using variables_t = std::map<std::string, std::string>;

	struct stream
	{
		virtual ~stream() {}
		virtual void write(const char* buffer, size_t size) = 0;

		stream& operator << (const char* str)
		{
			if (str)
				write(str, strlen(str));
			return *this;
		}

		stream& operator << (const std::string& str)
		{
			if (!str.empty())
				write(str.c_str(), str.length());
			return *this;
		}

		stream& operator << (char c)
		{
			write(&c, 1);
			return *this;
		}
	};

	struct cstream : stream
	{
		std::ostream& ref;
		explicit cstream(std::ostream& ref) : ref(ref) {}
		void write(const char* buffer, size_t size) override;
	};

	class list_ctx
	{
		std::vector<int> m_counters;
		std::string m_indent;
	public:
		void enter() { m_counters.push_back(0); }
		void leave() { m_counters.pop_back(); }
		int currentId() { return ++m_counters.back(); }
		int indentDepth() const { return m_counters.size(); }
		std::string indentStr() const { return m_indent; }
		void indentStr(const std::string& indent) { m_indent = indent; }
	};

	struct styler
	{
		virtual ~styler() {}
		virtual void begin_document(stream& o) = 0;
		virtual void end_document(stream& o) = 0;
		virtual void image(stream& o, const std::string& path, const std::string& styles, const std::string& alt) const = 0;
		virtual void begin_block(stream& o, const std::string& tag, const std::string& additional_style = std::string()) = 0;
		virtual void end_block(stream& o, const std::string& tag) = 0;
		virtual void hr(stream& o) = 0;
	};

	struct document
	{
		virtual ~document() {}

		virtual void text(stream& o, const variables_t& vars, list_ctx& ctx) const = 0;
		virtual void markup(stream& o, const variables_t& vars, const styler_ptr& styler, list_ctx& ctx) const = 0;
		virtual void debug(stream& o) const = 0;
	};

	document_ptr compile(const filesystem::path& file, const filesystem::path& obj);
	document_ptr compile(const filesystem::path& file);
	document_ptr compile(const std::string& text);
}

#endif // __WIKI__HPP__
