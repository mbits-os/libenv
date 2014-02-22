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

#ifndef __WIKI_NODES_HPP__
#define __WIKI_NODES_HPP__

#include <memory>
#include <string>
#include <vector>
#include <map>

#include <utils.hpp>
#include <wiki.hpp>
#include "wiki_base.hpp"
#include "wiki_parser.hpp"

namespace wiki
{
	SHAREABLE(Node);
	SHAREABLE(Text);

	using Nodes = std::vector<NodePtr>;

	class Node
	{
	protected:
		TOKEN m_token = TOKEN::NOP;
		std::string m_tag;
		Nodes m_children;
	public:
		Node(const std::string& tag = std::string(), const Nodes& children = Nodes());
		Node(TOKEN token, const std::string& tag = std::string(), const Nodes& children = Nodes());
		virtual ~Node() {}

		virtual void normalize() { normalizeChildren(); }
		virtual void normalizeChildren();

		virtual void debug(stream& o) const;
		virtual void text(stream& o, const variables_t& vars, list_ctx& ctx) const;
		virtual void markup(stream& o, const variables_t& vars, const styler_ptr& styler, list_ctx& ctx) const;
		virtual TOKEN getToken() const { return m_token; }
		virtual bool isText() const { return false; }
		virtual std::string getText() const { return std::string(); }
	};

	namespace inline_elem
	{
		class Token : public Node
		{
		public:
			Token(TOKEN token) : Node(token) {}

			virtual void text(stream&, const variables_t&, list_ctx&) const override {}
			virtual void markup(stream&, const variables_t&, const styler_ptr&, list_ctx&) const override {}
			virtual void debug(stream& o) const override;
		};

		class Text : public Node
		{
			std::string m_text;
		public:
			Text(const std::string& text) : m_text(text) {}
			void append(std::string::const_iterator begin, std::string::const_iterator end) { m_text.append(begin, end); }
			void text(stream& o, const variables_t&, list_ctx&) const override { o << m_text; }
			void markup(stream& o, const variables_t&, const styler_ptr&, list_ctx&) const override { o << url::htmlQuotes(m_text); }
			void debug(stream& o) const override { o << m_text; }
			bool isText() const override { return true; }
			std::string getText() const override { return m_text; }
		};

		class Break : public Node
		{
		public:
			Break() : Node(TOKEN::BREAK, "br") {}
			void text(stream& o, const variables_t&, list_ctx& ctx) const override { o << '\n' << ctx.indentStr(); }
			void markup(stream& o, const variables_t&, const styler_ptr&, list_ctx& ctx) const override { o << ctx.indentStr() << "<br />\n"; }
			void debug(stream& o) const override { o << "<br/>\n"; }
		};

		class Line : public Node
		{
		public:
			Line() {}
			void text(stream& o, const variables_t&, list_ctx& ctx) const override { o << '\n' << ctx.indentStr(); }
			void markup(stream& o, const variables_t&, const styler_ptr&, list_ctx& ctx) const override { o << ctx.indentStr() << "<br />\n"; }
			void debug(stream& o) const override { o << "\\CR\\LF\n"; }
		};

		class Variable : public Node
		{
			std::string m_name;
		public:
			Variable(const std::string& name) : Node("VAR"), m_name(name) {}

			void text(stream& o, const variables_t& vars, list_ctx&) const override;
			void markup(stream& o, const variables_t& vars, const styler_ptr&, list_ctx&) const override;
			void debug(stream& o) const override { o << '[' << m_name << ']'; }
		};

		namespace link
		{
			SHAREABLE_STRUCT(Namespace);

			using segments_t = std::vector<std::string>;

			struct Namespace
			{
				Namespace(const std::string& name): m_name(name) {}
				virtual ~Namespace() {}
				virtual const std::string& name() const { return m_name; }
				virtual void text(stream& o, const std::string& href, const segments_t& segments, const variables_t& vars) const = 0;
				virtual void markup(stream& o, const std::string& href, const segments_t& segments, const variables_t& vars, const styler_ptr& styler) const = 0;
				virtual void debug(stream& o, const std::string& href, const segments_t& segments) const = 0;
			private:
				std::string m_name;
			};

			struct Url : Namespace
			{
				Url() : Namespace("url") {}
				void text(stream& o, const std::string& href, const segments_t& segments, const variables_t&) const override;
				void markup(stream& o, const std::string& href, const segments_t& segments, const variables_t&, const styler_ptr&) const override;
				void debug(stream& o, const std::string& href, const segments_t& segments) const override;
			};

			struct Image : Namespace
			{
				Image() : Namespace("Image") {}
				void text(stream& o, const std::string& href, const segments_t&, const variables_t&) const override;
				void markup(stream& o, const std::string& href, const segments_t& segments, const variables_t&, const styler_ptr& styler) const override;
				void debug(stream& o, const std::string& href, const segments_t& segments) const override;
			};

			struct Unknown : Namespace
			{
				Unknown(const std::string& ns) : Namespace(ns) {}
				void text(stream& o, const std::string&, const segments_t&, const variables_t&) const override
				{
					o << "(Unknown link type: " << name() << ')';
				}

				void markup(stream& o, const std::string&, const segments_t&, const variables_t&, const styler_ptr&) const override
				{
					o << "<em>Unknown link type: <strong>" << name() << "</strong>.</em>";
				}
				void debug(stream& o, const std::string& href, const segments_t& segments) const override;
			};
		}

		class Link : public Node
		{
			link::NamespacePtr m_ns;
			Nodes m_href;
			std::vector<Nodes> m_segs;

			void produce(std::string& href, link::segments_t& segs, const variables_t& vars) const
			{
				href = produce(m_href, vars);
				segs.clear();
				for (auto&& seg : m_segs)
					segs.push_back(produce(seg, vars));
			}

			static std::string produce(const Nodes& collection, const variables_t& vars);

			void debug(std::string& href, link::segments_t& segs) const
			{
				href = debug(m_href);
				segs.clear();
				for (auto&& seg : m_segs)
					segs.push_back(debug(seg));
			}

			static std::string debug(const Nodes& collection);

			bool is_text(size_t child) const
			{
				return m_children[child]->isText();
			}
			bool is_token(size_t child, TOKEN token) const
			{
				return m_children[child]->getToken() == token;
			}

		public:
			Link(const Nodes& children = Nodes()) : Node("LINK", children) {}

			void text(stream& o, const variables_t& vars, list_ctx&) const override;
			void markup(stream& o, const variables_t& vars, const styler_ptr& styler, list_ctx&) const override;
			void debug(stream& o) const override;

			void normalize() override;
		};
	}

	namespace block_elem
	{
		class Block : public Node
		{
			std::string m_sep;
		public:
			Block(const std::string& tag, const Nodes& children, const std::string& sep = " ")
				: Node(tag, children)
				, m_sep(sep)
			{
			}

			void text(stream& o, const variables_t& vars, list_ctx& ctx) const override;
			void markup(stream& o, const variables_t& vars, const styler_ptr& styler, list_ctx& ctx) const override;
			void debug(stream& o) const override;
		};

		class Header : public Block
		{
			int m_level;
		public:
			Header(int level, const Nodes& children) : Block("h" + std::to_string(level), children), m_level(level) {}
		};

		class Para : public Block
		{
		public:
			Para(const Nodes& children) : Block("p", children) {}
		};

		class Pre : public Block
		{
		public:
			Pre(const Nodes& children) : Block("pre", children, "\n") {}
		};

		class Quote : public Block
		{
		public:
			Quote(const Nodes& children) : Block("quote", children) {}

			void text(stream& o, const variables_t& vars, list_ctx& ctx) const override;
		};

		class OList : public Block
		{
		public:
			OList(const Nodes& children) : Block("ol", children) {}
		};

		class UList : public Block
		{
		public:
			UList(const Nodes& children) : Block("ul", children) {}
		};

		class Item : public Block
		{
		public:
			Item(const Nodes& children) : Block("li", children) {}

			void text(stream& o, const variables_t& vars, list_ctx& ctx) const override;
		};

		class HR : public Block
		{
		public:
			HR() : Block("hr", Nodes()) {}

			void text(stream& o, const variables_t&, list_ctx&) const override {}
			void markup(stream& o, const variables_t& vars, const styler_ptr& styler, list_ctx& ctx) const override { styler->hr(o); }
		};

		class Signature : public Block
		{
		public:
			Signature(const Nodes& children) : Block("sign", children) {}

			void text(stream& o, const variables_t& vars, list_ctx& ctx) const override;
		};
	}
}

#endif // __WIKI_NODES_HPP__

