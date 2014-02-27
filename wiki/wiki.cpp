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
#include <iomanip>
#include <fcntl.h>

#ifdef _WIN32
#include <process.h>
#include <io.h>
#else
#include <unistd.h>
#define _getpid getpid
#endif

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
				from = compile(children, from, to, Right::end_tok, false);
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
				make_node<inline_elem::Element>(m_items, arg, children);
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

	class Document : public document
	{
		Nodes m_children;
	public:
		Document(const Nodes& children) : m_children(children) {}
		void text(stream& o, const variables_t& vars, list_ctx& ctx) const override
		{
			for (auto& child : m_children)
				child->text(o, vars, ctx);
		}

		void markup(stream& o, const variables_t& vars, const styler_ptr& styler, list_ctx& ctx) const override
		{
			styler->begin_document(o);
			for (auto& child : m_children)
				child->markup(o, vars, styler, ctx);
			styler->end_document(o);
		}

		void debug(stream& o) const override
		{
			for (auto& child : m_children)
				child->debug(o);
		}

		void store(const filesystem::path& obj)
		{
			binary::Writer wr;
			if (!wr.open(obj))
				return;

			if (!wr.store(m_children) || !wr.close())
				filesystem::remove(obj);
		}
	};

	/* public: */

	void cstream::write(const char* buffer, size_t size)
	{
		ref.write(buffer, size);
	}

	document_ptr compile(const filesystem::path& file, const filesystem::path& obj)
	{
		filesystem::status st{ file };
		if (!st.exists())
			return nullptr;

		if (st.mtime() <= filesystem::status{ obj }.mtime())
		{
			binary::Reader r;
			if (r.open(obj))
			{
				Nodes children;
				if (r.load(children))
					return std::make_shared<Document>(children);
			}
		}

		size_t size = (size_t)st.file_size();

		std::string text;
		text.resize(size + 1);
		if (text.size() <= size)
			return nullptr;

		auto f = fopen(file.native().c_str(), "r");
		if (!f)
			return nullptr;
		if (fread(&text[0], 1, size, f) != size)
		{
			fclose(f);
			return nullptr;
		};
		fclose(f);

		text[size] = 0;

		document_ptr out = compile(text);
		std::static_pointer_cast<Document>(out)->store(obj);
		return out;
	}

	document_ptr compile(const filesystem::path& file)
	{
		filesystem::status st{ file };
		if (!st.exists())
			return nullptr;

		size_t size = (size_t)st.file_size();

		std::string text;
		text.resize(size + 1);
		if (text.size() <= size)
			return nullptr;

		auto f = fopen(file.native().c_str(), "r");
		if (!f)
			return nullptr;

		if (fread(&text[0], 1, size, f) != size)
		{
			fclose(f);
			return nullptr;
		};
		fclose(f);

		text[size] = 0;
		return compile(text);
	}

	document_ptr compile(const std::string& text)
	{
		auto blocks = parser::Parser{}.parse(text);

		Nodes children;
		compiler::compile(children, blocks, false);
		return std::make_shared<Document>(children);
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
		cstream co{ o };
		variables_t vars;
		list_ctx ctx;
		for (auto&& node : nodes)
			node->text(co, vars, ctx);
		return o.str();
	}
}
