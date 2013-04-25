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
#include "wiki_parser.hpp"

namespace wiki { namespace parser {

	Parser::Parser()
		: m_blockType(BLOCK::UNKNOWN)
	{
	}

	bool Parser::parse(const std::string& text)
	{
		auto line = text.begin();
		auto end = text.end();
		while (line != end)
		{
			auto lineEnd = line;
			while (lineEnd != end && *lineEnd != '\n') ++lineEnd;
			auto right = lineEnd;
			if (right != end)
				while (right != lineEnd && isspace(*right)) --right;

			if (!parseLine(line, right))
				return false;

			// move to the next line
			line = lineEnd;
			if (line != end) ++line;
		}

		return true;
	}

	bool Parser::changeBlock(BLOCK newBlock)
	{
		if (m_blockType != newBlock)
		{
			switch (newBlock)
			{
			case BLOCK::UNKNOWN: std::cout << '?'; break;
			case BLOCK::TEXT: std::cout << 'P'; break;
			case BLOCK::QUOTE: std::cout << 'Q'; break;
			case BLOCK::PRE: std::cout << 'R'; break;
			case BLOCK::ITEM: std::cout << 'L'; break;
			case BLOCK::SIGNATURE: std::cout << 'S'; break;
			}
			std::cout << ":  ";
		}
		else
			std::cout << "    ";

		if (m_blockType != newBlock)
		{
			m_blockType = newBlock;
		}
		return true;
	}

	bool Parser::block(pointer start, pointer end)
	{
		std::cout << "( ";
		std::for_each(start, end, [](char c) { std::cout.put(c); });
		std::cout << " )\n" << std::flush;
		return true;
	}

	bool Parser::header(pointer start, pointer end)
	{
#ifdef min
#undef min
#endif

		auto left = start;
		while (left != end && *left == '=') ++left;

		auto level = std::distance(start, left);

		if (left == end) //only equal marks...
		{
			level /= 2;
		}
		else
		{
			auto right = end - 1;
			while (right != start && *right == '=') --right;
			++right;
			level = std::min(level, std::distance(right, end));
		}

		if (level == 0)
			return text(start, end);

		if (!reset())
			return false;

		std::cout << "H" << level << ": ";
		return block(start + level, end - level);
	}

	bool Parser::text(pointer start, pointer end)
	{
		BLOCK type = BLOCK::TEXT;
		switch (m_blockType)
		{
		case BLOCK::ITEM:
		case BLOCK::SIGNATURE:
			type = m_blockType; break;
		};

		if (!changeBlock(type))
			return false;

		return block(start, end);
	}

	bool Parser::quote(pointer start, pointer end)
	{
		if (!changeBlock(BLOCK::QUOTE))
			return false;

		return block(start, end);
	}

	bool Parser::pre(pointer start, pointer end)
	{
		if (!changeBlock(m_blockType == BLOCK::ITEM ? BLOCK::ITEM: BLOCK::PRE))
			return false;

		return block(start, end);
	}

	bool Parser::item(pointer start, pointer end)
	{
		auto textStart = start;
		while (textStart != end && (*textStart == '*' || *textStart == '#')) ++textStart;

		auto markerEnd = textStart;
		while (textStart != end && isspace((unsigned char)*textStart)) ++textStart;

		if (!reset())
			return false;

		if (!changeBlock(BLOCK::ITEM))
			return false;

		std::for_each(start, markerEnd, [](char c) { std::cout.put(c); });
		std::cout << " ";

		return block(textStart, end);
	}

	bool Parser::hr()
	{
		if (!reset())
			return false;

		std::cout << "HR\n" << std::flush;
		return true;
	}

	bool Parser::sig(pointer start, pointer end)
	{
		if (!changeBlock(BLOCK::SIGNATURE))
			return false;

		return block(start, end);
	}

	bool Parser::reset()
	{
		m_blockType = BLOCK::UNKNOWN;
		return true;
	}

	bool Parser::parseLine(std::string::const_iterator line, std::string::const_iterator end)
	{
		if (line == end)
			return reset();

		char command = *line;
		if (command == '=')
		{
			return header(line, end);
		}
		else if (command == ';')
		{
			auto tmp = line;
			++tmp;
			while (tmp != end && isspace((unsigned char)*tmp)) ++tmp;
			return quote(tmp, end);
		}
		else if (command == ' ')
		{
			auto tmp = line;
			while (tmp != end && isspace((unsigned char)*tmp)) ++tmp;
			return pre(tmp, end);
		}
		else if (command == '*' || command == '#')
		{
			return item(line, end);
		}
		else if (command == '-')
		{
			auto tmp = line;
			size_t count = 0;
			while (tmp != end && *tmp == '-') ++tmp, ++count;
			if (tmp == end) return hr();
			if (count == 2 && *tmp == ' ')
				return sig(++tmp, end);
		}

		return text(line, end);
	}
}}  // wiki::parser