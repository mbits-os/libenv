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
#include <utils.hpp>

#include "wiki_parser.hpp"

namespace wiki { namespace parser {

	void Parser::Block::append(line::Tokens&& tokens)
	{
		this->tokens.insert(this->tokens.end(), tokens.begin(), tokens.end());
	}

	Parser::Parser()
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
		reset();

		for (auto& block: m_out)
		{
			switch (block.type)
			{
			case BLOCK::UNKNOWN: std::cout << '?'; break;
			case BLOCK::HEADER: std::cout << 'H'; break;
			case BLOCK::PARA: std::cout << 'P'; break;
			case BLOCK::QUOTE: std::cout << 'Q'; break;
			case BLOCK::PRE: std::cout << 'R'; break;
			case BLOCK::ITEM: std::cout << 'L'; break;
			case BLOCK::SIGNATURE: std::cout << 'S'; break;
			case BLOCK::HR: std::cout << "HR"; break;
			}
			if (block.iArg) std::cout << block.iArg;
			if (!block.sArg.empty()) std::cout << '[' << block.sArg << ']';

			std::cout << ": (";
			bool first = true;
			for (auto& tok: block.tokens)
			{
				if (first) first = false;
				else std::cout << ", ";
				std::cout << tok;
			}
			std::cout << ")\n";
		}

		return true;
	}

	bool Parser::changeBlock(BLOCK newBlock)
	{
		if (m_cur.type != newBlock)
		{
			if (m_cur.type != BLOCK::UNKNOWN)
				m_out.push_back(std::move(m_cur));
			m_cur = Block(newBlock);
		}
		return true;
	}

	bool Parser::block(pointer start, pointer end)
	{
		line::Parser subparser(start, end);
		auto tokens = subparser.parse();
		m_cur.append(std::move(tokens));

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
			return para(start, end);

		if (!reset())
			return false;

		m_cur.type = BLOCK::HEADER;
		m_cur.iArg = level;

		return block(start + level, end - level);
	}

	bool Parser::para(pointer start, pointer end)
	{
		BLOCK type = BLOCK::PARA;
		switch (m_cur.type)
		{
		case BLOCK::ITEM:
		case BLOCK::SIGNATURE:
			type = m_cur.type; break;
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
		if (!changeBlock(m_cur.type == BLOCK::ITEM ? BLOCK::ITEM: BLOCK::PRE))
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

		m_cur.sArg.assign(start, markerEnd);
		return block(textStart, end);
	}

	bool Parser::hr()
	{
		if (!reset())
			return false;

		m_cur.type = BLOCK::HR;
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
		if (m_cur.type != BLOCK::UNKNOWN)
			m_out.push_back(std::move(m_cur));
		m_cur = Block(BLOCK::UNKNOWN);
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

		return para(line, end);
	}

	void line::Parser::text(size_t count)
	{
		if ((size_t)std::distance(m_prev, m_cur) > count)
		{
			auto end = m_cur - count;
			if (std::distance(m_prev, end) > 0)
				push(TOKEN::TEXT, m_prev, end);
		}
		m_prev = m_cur;
	}

	void line::Parser::repeated(char c, size_t repeats, TOKEN tok)
	{
		size_t count = 0;
		while (count < repeats && m_cur != m_end && *m_cur == c) ++m_cur, ++count;
		if (count < repeats)
			return;

		text(count);
		push(tok);
	}

	void line::Parser::boldOrItalics()
	{
		auto cur = m_cur;
		while (m_cur != m_end && *m_cur == '\'') ++m_cur;
		static const TOKEN tokens[] = {
			TOKEN::BAD,
			TOKEN::BAD,
			TOKEN::ITALIC,
			TOKEN::BOLD,
			TOKEN::BAD,
			TOKEN::BI
		};

		size_t count = std::distance(cur, m_cur);
		if (count < array_size(tokens) && tokens[count] != TOKEN::BAD)
		{
			text(count);
			push(tokens[count]);
		}
	}

	void line::Parser::htmlTag()
	{
		TOKEN tok = TOKEN::TAG_S;
		auto tmp = m_cur;
		++m_cur;
		if (m_cur != m_end && *m_cur == '/')
		{
			tok = TOKEN::TAG_E;
			++m_cur;
		}

		auto nameStart = m_cur;
		while (m_cur != m_end && *m_cur != '>') ++m_cur;

		if (m_cur == m_end) return;
		auto nameEnd = m_cur;
		++m_cur;

		if (nameStart != nameEnd && nameEnd[-1] == '/')
		{
			tok = TOKEN::TAG_CLOSED;
			--nameEnd;
		}

		text(std::distance(tmp, m_cur));
		push(tok, nameStart, nameEnd);
	}

	line::Tokens line::Parser::parse()
	{
		while (m_cur != m_end)
		{
			switch(*m_cur)
			{
			case '\'': boldOrItalics(); break;
			case '{': repeated('{', 3, TOKEN::VAR_S); break;
			case '}': repeated('}', 3, TOKEN::VAR_E); break;
			case '[': repeated('[', 2, TOKEN::HREF_S); in1stSEG = inHREF = true; break;
			case ']': repeated(']', 2, TOKEN::HREF_E); in1stSEG = inHREF = true; break;
			case ':':
				++m_cur;
				if (in1stSEG)
				{
					in1stSEG = false; // only one NS per HREF
					text(1);
					push(TOKEN::HREF_NS);
				}
				break;
			case '|':
				++m_cur;
				if (inHREF)
				{
					in1stSEG = false;
					text(1);
					push(TOKEN::HREF_SEG);
				}
				break;
			case '<': htmlTag(); break;
			default: ++m_cur;
			};
		}
		text(0);
		return std::move(m_out);
	}

}}  // wiki::parser