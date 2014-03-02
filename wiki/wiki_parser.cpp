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
#include <functional>

#include "wiki_parser.hpp"

#ifdef min
#undef min
#endif

#ifdef __GNUC__
#define STRING_CONST const
#else
#define STRING_CONST constexpr
#endif


namespace wiki { namespace parser {

	static STRING_CONST std::string bold{ "b" };
	static STRING_CONST std::string italic{ "i" };
	static STRING_CONST std::string space{ " " };

	void Parser::Block::append(line::Tokens&& tokens)
	{
		this->tokens.insert(this->tokens.end(), tokens.begin(), tokens.end());
	}

	void Parser::Block::append(Token&& token)
	{
		tokens.push_back(std::forward<Token>(token));
	}

	Parser::Parser()
	{
	}

	Parser::Text Parser::parse(const std::string& text)
	{
		auto line = text.begin();
		auto end = text.end();
		while (line != end)
		{
			auto lineEnd = line;
			while (lineEnd != end && *lineEnd != '\n') ++lineEnd;
			auto right = lineEnd;
			while (right != line && isspace(right[-1]))
				--right;

			parseLine(line, right);

			// move to the next line
			line = lineEnd;
			if (line != end) ++line;
		}
		reset();

		return std::move(m_out);
	}

	void Parser::block(pointer start, pointer end)
	{
		line::Parser subparser(start, end);
		auto tokens = subparser.parse();
		m_cur.append(std::move(tokens));
	}

	void Parser::header(pointer start, pointer end)
	{
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

		reset();
		m_cur.type = BLOCK::HEADER;
		m_cur.iArg = level;
		block(start + level, end - level);
	}

	void Parser::para(pointer start, pointer end)
	{
		BLOCK type = BLOCK::PARA;
		switch (m_cur.type)
		{
		case BLOCK::ITEM:
		case BLOCK::SIGNATURE:
			type = m_cur.type; break;
		};

		changeBlock(type, start);
		block(start, end);
	}

	void Parser::quote(pointer start, pointer end)
	{
		changeBlock(BLOCK::QUOTE, start);
		block(start, end);
	}

	void Parser::pre(pointer start, pointer end)
	{
		changeBlock(m_cur.type == BLOCK::ITEM ? BLOCK::ITEM: BLOCK::PRE, start);
		block(start, end);
	}

	void Parser::item(pointer start, pointer end)
	{
		auto textStart = start;
		while (textStart != end && (*textStart == '*' || *textStart == '#')) ++textStart;

		auto markerEnd = textStart;
		while (textStart != end && isspace((unsigned char)*textStart)) ++textStart;

		reset();
		changeBlock(BLOCK::ITEM, start);

		m_cur.sArg.assign(start, markerEnd);
		block(textStart, end);
	}

	void Parser::hr()
	{
		reset();
		m_cur.type = BLOCK::HR;
	}

	void Parser::sig(pointer start, pointer end)
	{
		changeBlock(BLOCK::SIGNATURE, start);
		block(start, end);
	}

	void Parser::changeBlock(BLOCK newBlock, pointer here)
	{
		if (m_cur.type != newBlock)
			reset(newBlock);
		else if (newBlock == BLOCK::PRE)
			m_cur.append(Token(TOKEN::LINE, here, here));
		else
		{
			if (m_cur.tokens.empty())
				return;

			if (m_cur.tokens.back().type == TOKEN::BREAK)
				return;

			m_cur.append(Token(TOKEN::TEXT, space.begin(), space.end()));
		}
	}

	void Parser::reset(BLOCK type)
	{
		if (m_cur.type != BLOCK::UNKNOWN)
			push();
		m_cur = Block(type);
	}

	static inline Parser::Text::iterator findPos(Parser::Text& stack, const std::string& address)
	{
		// auto 
		auto stackCur = stack.begin(), stackEnd = stack.end();
		auto addCur = address.begin(), addEnd = address.end();
		while (stackCur != stackEnd && addCur != addEnd)
		{
			char c = stackCur->type == Parser::BLOCK::OL ? '#' : '*';
			if (c != *addCur)
				break;

			++stackCur;
			++addCur;
		}
		return stackCur;
	}

	static inline void fold(Parser::Text& stack, Parser::Text& out,
		std::iterator_traits<Parser::Text::iterator>::difference_type count)
	{
		auto pos = stack.begin() + count;

		bool moved = count == 0 && pos != stack.end();
		if (moved) ++pos;

		for (auto it = pos; it != stack.end(); ++it)
			(it - 1)->items.push_back(*it);

		if (moved)
			out.push_back(std::move(*--pos));

		stack.erase(pos, stack.end());
	}

	void Parser::push()
	{
		if (m_cur.type != BLOCK::ITEM)
		{
			if (!m_itemStack.empty())
				fold(m_itemStack, m_out, 0);

			return m_out.push_back(std::move(m_cur));
		}

		auto count = std::distance(m_itemStack.begin(), findPos(m_itemStack, m_cur.sArg));

		fold(m_itemStack, m_out, count);

		for (auto it = m_cur.sArg.begin() + count; it != m_cur.sArg.end(); ++it)
			m_itemStack.emplace_back(*it == '#' ? BLOCK::OL : BLOCK::UL);

		m_cur.sArg.clear();
		m_itemStack.back().items.push_back(std::move(m_cur));
	}

	void Parser::parseLine(std::string::const_iterator line, std::string::const_iterator end)
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

	std::string line::Parser::get_text(size_t count)
	{
		if ((size_t)std::distance(m_prev, m_cur) > count)
		{
			auto end = m_cur - count;
			if (std::distance(m_prev, end) > 0)
				return std::string(m_prev, end);
		}
		return std::string();
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
		static const APOS tokens[] = {
			APOS::BAD,
			APOS::BAD,
			APOS::ITALIC,
			APOS::BOLD,
			APOS::BAD,
			APOS::BOTH
		};

		size_t count = std::distance(cur, m_cur);
		if (count < array_size(tokens) && tokens[count] != APOS::BAD)
		{
			text(count);
			if (tokens[count] == APOS::BOTH)
				switchBoth();
			else
				switchBoldItalic(tokens[count]);
		}
	}

	void line::Parser::switchBoth()
	{
		APOS first, second;
		if (m_bi_stack.empty())
		{
			first = APOS::BOLD;
			second = APOS::ITALIC;
		}
		else if (m_bi_stack.size() == 1)
		{
			first = m_bi_stack[0];
			second = first == APOS::BOLD ? APOS::ITALIC : APOS::BOLD;
		}
		else
		{
			first = m_bi_stack[1];
			second = m_bi_stack[0];
		}

		//std::cout << "BOTH -> " << first << " " << second << std::endl;
		switchBoldItalic(first);
		switchBoldItalic(second);
	}

	void line::Parser::switchBoldItalic(APOS type)
	{
		//std::cout << type << " -> ";
		if (m_bi_stack.empty() ||
			(m_bi_stack.size() == 1 && m_bi_stack.top() != type))
		{
			m_bi_stack.push(type);
			//std::cout << TOKEN::TAG_S << "{" << type << "}" << std::endl;
			switchBoldItalic(TOKEN::TAG_S, type);
		}
		else if (m_bi_stack.top() == type)
		{
			m_bi_stack.pop();
			//std::cout << TOKEN::TAG_E << "{" << type << "}" << std::endl;
			switchBoldItalic(TOKEN::TAG_E, type);
		}
		else
		{
			auto tmp = m_bi_stack.top();
			m_bi_stack.pop();

			//std::cout
			//	<< TOKEN::TAG_E << "{" << tmp << "} "
			//	<< TOKEN::TAG_E << "{" << type << "} "
			//	<< TOKEN::TAG_S << "{" << tmp << "}" << std::endl;

			switchBoldItalic(TOKEN::TAG_E, tmp);
			switchBoldItalic(TOKEN::TAG_E, type);
			switchBoldItalic(TOKEN::TAG_S, tmp);
			m_bi_stack[0] = tmp; // pop the deeper one and push the tmp again...
		}
	}

	void line::Parser::switchBoldItalic(TOKEN startEnd, APOS boldItalic)
	{
		const std::string& tag = boldItalic == APOS::BOLD ? bold : italic;
		push(startEnd, tag.begin(), tag.end());
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

		struct TagInfo
		{
			typedef std::function<void(Parser& _this, pointer nameStart, pointer nameEnd, TOKEN type)> Call;
			const char* m_name;
			size_t m_len;
			Call m_call;
			TOKEN m_deny;
			TagInfo(const char* name, TOKEN deny, Call call)
				: m_name(name)
				, m_len(strlen(name))
				, m_call(call)
				, m_deny(deny)
			{
			}

			bool lessThan(pointer nameStart, pointer nameEnd) const
			{
				auto dist = std::distance(nameStart, nameEnd);
				typedef decltype(dist) distance_t;
				auto count = std::min((distance_t)m_len, dist);
				int ret = strncmp(m_name, &*nameStart, count);

				if (ret != 0) return ret < 0;
				if (dist != (distance_t)m_len)
					return (distance_t)m_len < dist;
				return false; // equal or greater
			}

			bool equals(pointer nameStart, pointer nameEnd) const
			{
				auto dist = std::distance(nameStart, nameEnd);
				typedef decltype(dist) distance_t;
				auto count = std::min((distance_t)m_len, dist);
				int ret = strncmp(m_name, &*nameStart, count);

				if (ret != 0) return false;
				if (dist != (distance_t)m_len)
					return false;
				return true; // equal
			}
		};

		static auto onBold = [](Parser& _this, pointer nameStart, pointer nameEnd, TOKEN type)
		{
			_this.push(type, bold.begin(), bold.end());
		};

		static auto onItalic = [](Parser& _this, pointer nameStart, pointer nameEnd, TOKEN type)
		{
			_this.push(type, italic.begin(), italic.end());
		};

		static auto onHtmlTag = [](Parser& _this, pointer nameStart, pointer nameEnd, TOKEN type)
		{
			_this.push(type, nameStart, nameEnd);
		};

		static TagInfo tags[] = {
			TagInfo("b", TOKEN::TAG_CLOSED, onBold),
			TagInfo("br", TOKEN::TAG_E, [](Parser& _this, pointer nameStart, pointer nameEnd, TOKEN type) { _this.push(TOKEN::BREAK); }),
			TagInfo("em", TOKEN::TAG_CLOSED, onItalic),
			TagInfo("i", TOKEN::TAG_CLOSED, onItalic),
			TagInfo("nowiki", TOKEN::TAG_CLOSED, [](Parser& _this, pointer nameStart, pointer nameEnd, TOKEN type) { _this.inWiki = type == TOKEN::TAG_E; }),
			TagInfo("strong", TOKEN::TAG_CLOSED, onBold),
			TagInfo("sub", TOKEN::TAG_CLOSED, onHtmlTag),
			TagInfo("sup", TOKEN::TAG_CLOSED, onHtmlTag),
			TagInfo("tt", TOKEN::TAG_CLOSED, onHtmlTag),
		};

		auto pos = std::lower_bound(tags, tags + array_size(tags), Token::Arg(nameStart, nameEnd),
			[](const TagInfo& info, const Token::Arg arg) { return info.lessThan(arg.first, arg.second); });
		if (pos != tags + array_size(tags) && pos->equals(nameStart, nameEnd) && pos->m_deny != tok)
		{
			pos->m_call(*this, nameStart, nameEnd, tok);
		}
	}

	line::Tokens line::Parser::parse()
	{
		while (m_cur != m_end)
		{
			if (inWiki)
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
						auto proto = std::tolower(get_text(1));
						if (proto != "http" &&
							proto != "https" &&
							proto != "ftp" &&
							proto != "mailto")
						{
							text(1);
							push(TOKEN::HREF_NS);
						}
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
			else
			{
				if (*m_cur == '<')
					htmlTag();
				else ++m_cur;
			}
		}
		text(0);
		return std::move(m_out);
	}

}}  // wiki::parser