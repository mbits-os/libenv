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

namespace wiki
{
	SHAREABLE(Node);
	SHAREABLE(Text);

	using Nodes = std::vector<NodePtr>;

	class Node
	{
	protected:
		std::string m_tag;
		Nodes m_children;
	public:
		Node(const std::string& tag = std::string(), const Nodes& children = Nodes());
		virtual ~Node() {}

		virtual void normalize() { normalizeChildren(); }
		virtual void normalizeChildren();

		virtual std::string text(const variables_t& vars, list_ctx& ctx) const;
		virtual std::string markup(const variables_t& vars, const styler_ptr& styler, list_ctx& ctx) const;
		virtual TOKEN getToken() const { return TOKEN::NOP; }
		virtual bool isText() const { return false; }
	};

	namespace inline_elem
	{
		class Text: public Node
		{
			std::string m_text;
		public:
			Text(const std::string& Text) : m_text(Text) {}
			std::string text(const variables_t&, list_ctx&) const override { return m_text; }
			std::string markup(const variables_t&, const styler_ptr&, list_ctx&) const override { return url::htmlQuotes(m_text); }
			bool isText() const override { return true; }
		};

		class Break: public Node
		{
		public:
			Break() : Node("br") {}
			std::string text(const variables_t&, list_ctx& ctx) const override { return "\n" + ctx.indentStr(); }
			std::string markup(const variables_t&, const styler_ptr&, list_ctx& ctx) const override { return ctx.indentStr() + "<br />\n"; }
		};

		class Variable: public Node
		{
			std::string m_name;
		public:
			Variable(const Nodes& children = Nodes()) : Node("VAR", children) {}

			void normalize() override;
			std::string text(const variables_t& vars, list_ctx&) const override;
			std::string markup(const variables_t& vars, const styler_ptr&, list_ctx&) const override;
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
				virtual std::string text(const std::string& href, const std::string& part, const segments_t& segments, const variables_t& vars) const = 0;
				virtual std::string markup(const std::string& href, const std::string& part, const segments_t& segments, const variables_t& vars, const styler_ptr& styler) const = 0;
			private:
				std::string m_name;
			};

			struct Url : Namespace
			{
				Url() : Namespace("url") {}
				std::string text(const std::string& href, const std::string& part, const segments_t& segments, const variables_t&) const override;
				std::string markup(const std::string& href, const std::string& part, const segments_t& segments, const variables_t&, const styler_ptr&) const override;
			};

			struct Image : Namespace
			{
				Image() : Namespace("Image") {}
				std::string text(const std::string&, const std::string&, const segments_t&, const variables_t&) const override;
				std::string markup(const std::string& href, const std::string& part, const segments_t& segments, const variables_t&, const styler_ptr& styler) const override;
			};

			struct Unknown : Namespace
			{
				Unknown(const std::string& ns) : Namespace(ns) {}
				std::string text(const std::string&, const std::string&, const segments_t&, const variables_t&) const override
				{
					return "(Unknown link type: " + name() + ")";
				}

				std::string markup(const std::string&, const std::string&, const segments_t&, const variables_t&, const styler_ptr&) const override
				{
					return "<em>Unknown link type: <strong>" + name() + "</strong>.</em>";
				}
			};
		}

		class Link : public Node
		{
			link::NamespacePtr m_ns;
			Nodes m_href;
			Nodes m_part;
			std::vector<Nodes> m_segs;

			void produce(std::string& href, std::string& part, link::segments_t& segs, const variables_t& vars) const
			{
				href = produce(m_href, vars);
				part = produce(m_part, vars);
				segs.clear();
				for (auto&& seg: m_segs)
					segs.push_back(produce(seg, vars));
			}

			static std::string produce(const Nodes& collection, const variables_t& vars);

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

			std::string text(const variables_t& vars, list_ctx&) const override;
			std::string markup(const variables_t& vars, const styler_ptr& styler, list_ctx&) const override;

			void normalize() override;
		};
	}

	namespace block_elem
	{
	}
}

#endif // __WIKI_NODES_HPP__

