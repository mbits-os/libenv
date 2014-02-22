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
#include <sstream>
#include <wiki.hpp>
#include <filesystem.hpp>
#include "wiki_parser.hpp"
#include "wiki_nodes.hpp"

namespace wiki
{
	struct compiler
	{
		using Token = parser::Token;
		using Tokens = parser::line::Tokens;
		using Block = parser::Parser::Block;
		using Blocks = parser::Parser::Text;

		struct by_token
		{
			TOKEN end_tok;
			by_token(TOKEN end_tok)
				: end_tok(end_tok)
			{}

			bool reached(Tokens::const_iterator cur)
			{
				return cur->type == end_tok;
			}
		};

		struct by_argument
		{
			TOKEN end_tok;
			std::string arg;
			by_argument(TOKEN end_tok, std::string&& arg)
				: end_tok(end_tok)
				, arg(std::move(arg))
			{}

			bool reached(Tokens::const_iterator cur)
			{
				return cur->type == end_tok && arg == cur->arg.str();
			}
		};

		template <typename Final, typename Right = by_token>
		struct scanner: Right
		{
			Tokens::const_iterator from, to;

			template <typename... Args>
			scanner(Tokens::const_iterator from, Tokens::const_iterator to, Args&&... args)
				: Right(std::forward<Args>(args)...)
				, from(from)
				, to(to)
			{}

			Tokens::const_iterator scan()
			{
				Nodes children;

				++from;
				from = compile(children, from, to, end_tok, false);
				if (from != to && Right::reached(from))
					static_cast<Final*>(this)->visit(children);

				return from;
			}
		};

		template <typename Final>
		using arg_scanner = scanner<Final, by_argument>;

		struct items
		{
			Nodes& m_items;

			items(Nodes& _items) : m_items(_items) {}
		};

		struct variable : scanner<variable>, items
		{
			variable(Nodes& _items, Tokens::const_iterator from, Tokens::const_iterator to) : scanner<variable>(from, to, TOKEN::VAR_E), items(_items) {}
			void visit(Nodes& children)
			{
				make_node<inline_elem::Variable>(m_items, text(children));
			}
		};

		struct link : scanner<link>, items
		{
			link(Nodes& _items, Tokens::const_iterator from, Tokens::const_iterator to) : scanner<link>(from, to, TOKEN::HREF_E), items(_items) {}
			void visit(Nodes& children)
			{
				make_node<inline_elem::Link>(m_items, children)->normalize();
			}
		};

		struct element : arg_scanner<element>, items
		{
			element(Nodes& _items, Tokens::const_iterator from, Tokens::const_iterator to, std::string&& arg) : arg_scanner<element>(from, to, TOKEN::TAG_E, std::move(arg)), items(_items) {}
			void visit(Nodes& children)
			{
				make_node<Node>(m_items, arg, children);
			}
		};

		static inline Nodes compile(const Tokens& tokens, bool pre);
		static inline Tokens::const_iterator compile(Nodes& children, Tokens::const_iterator begin, Tokens::const_iterator end, TOKEN endtok, bool pre);
		static inline NodePtr compile(const Block& block, bool pre);
		static inline void compile(Nodes& children, const Blocks& blocks, bool pre);
		static inline std::string text(const Nodes& nodes);
		template <typename Class, typename... Args>
		static inline std::shared_ptr<Class> make_node(Nodes& items, Args&&... args)
		{
			auto elem = std::make_shared<Class>(std::forward<Args>(args)...);
			items.push_back(elem);
			return elem;
		}
		template <typename Class, typename... Args>
		static inline std::shared_ptr<Class> make_block(Args&&... args)
		{
			return std::make_shared<Class>(std::forward<Args>(args)...);
		}
	};

	/* public: */
	document_ptr compile(const filesystem::path& file)
	{
		filesystem::status st{ file };
		if (!st.exists())
			return nullptr;

		std::cout << "WIKI: " << file << std::endl;

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

		auto out = compile(text);
		// TODO: store binary version of the document here
		return out;
	}

	document_ptr compile(const std::string& text)
	{
		auto blocks = parser::Parser{}.parse(text);

		Nodes children;
		compiler::compile(children, blocks, false);

		variables_t vars;
		list_ctx ctx;

		for (auto&& child : children)
		{
			if (!child)
				std::cout << "(nullptr)";
			else
				std::cout << child->debug();
		}

		std::cout << std::endl;

		return nullptr;
	}

	/* private: */
	Nodes compiler::compile(const parser::line::Tokens& tokens, bool pre)
	{
		Nodes list;
		compile(list, tokens.begin(), tokens.end(), TOKEN::BAD, pre);
		return list;
	}

	compiler::Tokens::const_iterator compiler::compile(Nodes& items, Tokens::const_iterator begin, Tokens::const_iterator end, TOKEN endtok, bool pre)
	{
		std::shared_ptr<inline_elem::Text> text;
		std::shared_ptr<Node> elem;

		auto cur = begin;
		for (; cur != end; ++cur)
		{
			if (cur->type == endtok)
				return cur;

			if (cur->type != TOKEN::TEXT)
				text = nullptr;

			Nodes children;

			auto&& token = *cur;
			switch (token.type)
			{
			case TOKEN::TEXT:
				if (text)
					text->append(token.arg.begin(), token.arg.end());
				else
					text = make_node<inline_elem::Text>(items, token.arg.str());

				break;

			case TOKEN::BREAK:
				make_node<inline_elem::Break>(items);
				break;

			case TOKEN::LINE:
				make_node<inline_elem::Line>(items);
				break;

			case TOKEN::TAG_S:
				cur = element{ items, cur, end, token.arg.str() }.scan();
				break;

			case TOKEN::VAR_S:
				cur = variable{ items, cur, end }.scan();
				break;

			case TOKEN::HREF_S:
				cur = link{ items, cur, end }.scan();
				break;

			case TOKEN::HREF_NS:
			case TOKEN::HREF_SEG:
				make_node<inline_elem::Token>(items, token.type);
				break;
			}

			if (cur == end)
				break;
		}

		return cur;
	}

	void compiler::compile(Nodes& children, const parser::Parser::Text& blocks, bool pre)
	{
		for (auto&& block : blocks)
		{
			auto child = compile(block, pre);
			if (child)
				children.push_back(child);
		}
	}

	NodePtr compiler::compile(const parser::Parser::Block& block, bool pre)
	{
		using BLOCK = parser::Parser::BLOCK;

		if (!pre)
			pre = block.type == BLOCK::PRE;
		auto children = compile(block.tokens, pre);
		compile(children, block.items, pre);

		switch (block.type)
		{
		case BLOCK::HEADER:    return make_block<block_elem::Header>(block.iArg, children);
		case BLOCK::PARA:      return make_block<block_elem::Para>(children);
		case BLOCK::QUOTE:     return make_block<block_elem::Quote>(children);
		case BLOCK::PRE:       return make_block<block_elem::Pre>(children);
		case BLOCK::UL:        return make_block<block_elem::UList>(children);
		case BLOCK::OL:        return make_block<block_elem::OList>(children);
		case BLOCK::ITEM:      return make_block<block_elem::Item>(children);
		case BLOCK::HR:        return make_block<block_elem::HR>();
		case BLOCK::SIGNATURE: return make_block<block_elem::Signature>(children);
		default:
			break;
		}

		return nullptr;
	}

	std::string compiler::text(const Nodes& nodes)
	{
		std::ostringstream o;
		variables_t vars;
		list_ctx ctx;
		for (auto&& node : nodes)
			o << node->text(vars, ctx);
		return o.str();
	}
}
