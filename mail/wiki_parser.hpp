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

#ifndef __WIKI_PARSER_HPP__
#define __WIKI_PARSER_HPP__

namespace wiki { namespace parser {

	enum class TOKEN {
		TEXT,
	};

	struct Token
	{
		typedef std::string::const_iterator pointer;
		typedef std::pair<pointer, pointer> Arg;
		TOKEN type;
		Arg arg;
	};

	class Parser
	{
		enum class BLOCK
		{
			UNKNOWN,
			TEXT,
			QUOTE,
			PRE,
			ITEM,
			SIGNATURE
		};

		typedef Token::pointer pointer;

		BLOCK m_blockType;
		bool changeBlock(BLOCK newBlock);

		bool block(pointer start, pointer end);

		bool header(pointer start, pointer end);
		bool text(pointer start, pointer end);
		bool quote(pointer start, pointer end);
		bool pre(pointer start, pointer end);
		bool item(pointer start, pointer end);
		bool hr();
		bool sig(pointer start, pointer end);
		bool reset();

		bool parseLine(pointer line, pointer end);
	public:
		Parser();
		bool parse(const std::string& text);
	};
}}; // wiki::parser

#endif //__WIKI_PARSER_HPP__