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

#ifndef __WIKI_BASE_HPP__
#define __WIKI_BASE_HPP__

#include <utils.hpp>
#include <vector>

namespace filesystem
{
	class path;
};

namespace wiki
{
	enum class TOKEN
	{
		BAD = -2,
		NOP,
		TEXT,
		VAR_S,
		VAR_E,
		HREF_S,
		HREF_E,
		HREF_NS,
		HREF_SEG,
		TAG_S,
		TAG_E,
		TAG_CLOSED,
		BREAK,
		LINE
	};

	enum class APOS
	{
		BAD,
		BOLD,
		ITALIC,
		BOTH
	};

	inline std::ostream& operator << (std::ostream& o, TOKEN tok)
	{
#define PRINT(x) case TOKEN::x: return o << #x;
		switch (tok)
		{
			PRINT(BAD);
			PRINT(NOP);
			PRINT(TEXT);
			PRINT(VAR_S);
			PRINT(VAR_E);
			PRINT(HREF_S);
			PRINT(HREF_E);
			PRINT(HREF_NS);
			PRINT(HREF_SEG);
			PRINT(TAG_S);
			PRINT(TAG_E);
			PRINT(TAG_CLOSED);
			PRINT(BREAK);
			PRINT(LINE);
		};
		return o << "unknown(" << (int)tok << ")";
#undef PRINT
	}

	inline std::ostream& operator << (std::ostream& o, APOS tok)
	{
#define PRINT(x) case APOS::x: return o << #x;
		switch (tok)
		{
			PRINT(BAD);
			PRINT(ITALIC);
			PRINT(BOLD);
			PRINT(BOTH);
		};
		return o << "unknown(" << (int)tok << ")";
#undef PRINT
	}

	enum class TAG
	{
		CLOSE,
		OPEN,
		EMPTY,
	};

	enum class BINARY
	{
		NOP,
		TEXT,
		ITALIC,
		BOLD,
	};

	SHAREABLE(Node);
	SHAREABLE(Text);
	SHAREABLE(Styler);

	using Nodes = std::vector<NodePtr>;

	namespace binary
	{
		inline constexpr unsigned short make_tag(char up, char lo) { return ((unsigned short)lo << 8) | ((unsigned short)up); }
		// same as is_upper for ASCII
		inline bool has_children(unsigned short id) { return ((id >> 8) & 0x20) == 0x20; }
		inline bool has_string(unsigned short id)   { return ( id       & 0x20) == 0x20; }

		enum class TAG : unsigned short
		{
			LINK		= make_tag('l', 'i'),
			HEADER		= make_tag('h', 'e'),
			ELEMENT		= make_tag('e', 'l'),
			TEXT		= make_tag('t', 'E'),
			VARIABLE	= make_tag('v', 'A'),
			HREF		= make_tag('H', 'r'),
			SEG			= make_tag('S', 'e'),
			PARA		= make_tag('P', 'a'),
			PRE			= make_tag('P', 'r'),
			QUOTE		= make_tag('Q', 'u'),
			OLIST		= make_tag('O', 'l'),
			ULIST		= make_tag('U', 'l'),
			ITEM		= make_tag('I', 't'),
			SIGNATURE	= make_tag('S', 'i'),
			BREAK		= make_tag('B', 'R'),
			LINE		= make_tag('L', 'I'),
			HR			= make_tag('H', 'R')
		};

		class Writer
		{
			struct Impl;
			Impl* pimpl;
		public:
			Writer();
			~Writer();
			Writer(const Writer&) = delete;
			Writer& operator=(const Writer&) = delete;

			bool open(const filesystem::path&);
			bool close();
			bool store(const Nodes& nodes);
			bool store(TAG id, const std::string& data, const Nodes& children);
		};

		class Reader
		{
			struct Impl;
			Impl* pimpl;

			static NodePtr create(TAG id, const std::string& data, const Nodes& children);
		public:
			Reader();
			~Reader();
			Reader(const Reader&) = delete;
			Reader& operator=(const Reader&) = delete;

			bool open(const filesystem::path&);
			bool load(Nodes& nodes);
			bool load(NodePtr& item);
		};
	}
}

#endif // __WIKI_BASE_HPP__

