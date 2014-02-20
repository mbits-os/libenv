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

#include <vector>
#include "wiki_base.hpp"

namespace wiki { namespace parser {

	struct Token
	{
		typedef std::string::const_iterator pointer;
		struct Arg : std::pair<pointer, pointer>
		{
			using Parent = std::pair<pointer, pointer>;
			using Parent::Parent;

			pointer begin() const { return first; }
			pointer end() const { return second; }
			std::string str() const { return{first, second}; }
		};
		TOKEN type;
		Arg arg;

		Token(TOKEN tok, pointer start, pointer end): type(tok), arg(start, end) {}
	};

	inline std::ostream& operator << (std::ostream& o, const Token& tok)
	{
		if (tok.type == TOKEN::TEXT)
		{
			o << '"';
			for (auto&& c : tok.arg)
				o.put(c);
			return o << '"';
		}

		o << tok.type;
		if (tok.arg.first != tok.arg.second)
		{
			o << " {";
			for (auto&& c : tok.arg)
				o.put(c);
			o << "}";
		}

		return o;
	}

	namespace line
	{
		typedef std::vector<Token> Tokens;

		class Parser
		{
			typedef Token::pointer pointer;

			class BoldItalicStack
			{
				APOS m_value[2];
				size_t m_size;
			public:
				BoldItalicStack() : m_size(0) {}
				void push(APOS apos) { m_value[m_size++] = apos; }
				void pop() { --m_size; }
				bool empty() const { return m_size == 0; }
				APOS top() const { return m_value[m_size - 1]; }
				size_t size() const { return m_size; }
				APOS operator[](size_t id) const { return m_value[id]; }
				APOS& operator[](size_t id) { return m_value[id]; }
			};

			Tokens m_out;
			pointer m_prev;
			pointer m_cur;
			pointer m_end;
			BoldItalicStack m_bi_stack;

			bool inWiki;
			bool inHREF;
			bool in1stSEG;

			void push(TOKEN token, pointer start, pointer end) { m_out.emplace_back(token, start, end); }
			void push(TOKEN token) { m_out.emplace_back(token, m_end, m_end); }
			void text(size_t count); //push [m_prev, m_cur - count) as a TOKEN::TEXT
			std::string get_text(size_t count); //return [m_prev, m_cur - count)
			void repeated(char c, size_t repeats, TOKEN tok);
			void boldOrItalics();
			void switchBoth();
			void switchBoldItalic(APOS type);
			void switchBoldItalic(TOKEN startEnd, APOS boldItalic);
			void htmlTag();
		public:
			Parser(pointer start, pointer end)
				: m_prev(start)
				, m_cur(start)
				, m_end(end)
				, inWiki(true)
				, inHREF(false)
				, in1stSEG(false)
			{
			}
			Tokens parse();
		};
	}

	class Parser
	{
	public:
		enum class BLOCK
		{
			UNKNOWN,
			HEADER,
			PARA,
			QUOTE,
			PRE,
			UL,
			OL,
			ITEM,
			HR,
			SIGNATURE
		};

		struct Block;
		typedef std::vector<Block> Text;
		typedef Token::pointer pointer;

		struct Block
		{
			BLOCK type;
			int iArg;
			std::string sArg;
			line::Tokens tokens;
			Text items;
			Block(BLOCK type = BLOCK::UNKNOWN): type(type), iArg(0) {}
			void append(line::Tokens&& tokens);
			void append(Token&& token);
		};

	private:
		Text m_out;
		Block m_cur;
		Text m_itemStack;

		void block(pointer start, pointer end);

		void header(pointer start, pointer end);
		void para(pointer start, pointer end);
		void quote(pointer start, pointer end);
		void pre(pointer start, pointer end);
		void item(pointer start, pointer end);
		void hr();
		void sig(pointer start, pointer end);
		void changeBlock(BLOCK newBlock, pointer here);
		void reset(BLOCK type = BLOCK::UNKNOWN);
		void push();

		void parseLine(pointer line, pointer end);
	public:
		Parser();
		Text parse(const std::string& text);
	};

}}; // wiki::parser

#endif //__WIKI_PARSER_HPP__