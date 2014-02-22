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
#include "wiki_nodes.hpp"
#include <sstream>

namespace wiki
{
	Node::Node(const std::string& tag, const Nodes& children)
		: m_tag(tag)
		, m_children(children)
	{
	}

	Node::Node(TOKEN token, const std::string& tag, const Nodes& children)
		: m_token(token)
		, m_tag(tag)
		, m_children(children)
	{
	}

	void Node::normalizeChildren()
	{
		for (auto& child : m_children)
			child->normalize();
	}

	void Node::debug(std::ostream& o) const
	{
		o << "<" << m_tag << ">";
		for (auto&& child : m_children)
			child->debug(o);
		o << "</" << m_tag << ">";
	}

	void Node::text(std::ostream& o, const variables_t& vars, list_ctx& ctx) const
	{
		for (auto& child : m_children)
			child->text(o, vars, ctx);
	}

	void Node::markup(std::ostream& o, const variables_t& vars, const styler_ptr& styler, list_ctx& ctx) const
	{
		std::string out("<" + m_tag + ">");
		for (auto& child : m_children)
			child->markup(o, vars, styler, ctx);
	}

	namespace inline_elem
	{
		void Token::debug(std::ostream& o) const
		{
			o << m_token;
		}

		void Variable::text(std::ostream& o, const variables_t& vars, list_ctx&) const
		{
			auto& it = vars.find(m_name);
			if (it != vars.end())
				o << it->second;
		}

		void Variable::markup(std::ostream& o, const variables_t& vars, const styler_ptr&, list_ctx&) const
		{
			auto& it = vars.find(m_name);
			if (it != vars.end())
				o << it->second;
		}

		namespace link
		{
			void Url::debug(std::ostream& o, const std::string& href, const segments_t& segments) const
			{
				o << "url:" << href;
				for (auto&& seg : segments)
					o << "|" << seg;
			}

			void Url::text(std::ostream& o, const std::string& href, const segments_t& segments, const variables_t&) const
			{
				if (!segments.empty())
					o << segments[0];
				else
					o << '<' << href << '>';
			}

			void Url::markup(std::ostream& o, const std::string& href, const segments_t& segments, const variables_t&, const styler_ptr&) const
			{
				auto contents = href;
				if (!segments.empty())
					contents = segments[0];

				o << "<a href=\"" << url::htmlQuotes(href) << "\">" << url::htmlQuotes(contents) << "</a>";
			}
		
			void Image::debug(std::ostream& o, const std::string& href, const segments_t& segments) const
			{
				o << "Image:" << href;
			}

			void Image::text(std::ostream& o, const std::string&, const segments_t&, const variables_t&) const
			{
			}

			void Image::markup(std::ostream& o, const std::string& href, const segments_t& segments, const variables_t&, const styler_ptr& styler) const
			{
				std::string alt;
				if (!segments.empty())
					alt = " alt=\"" + segments[0] + "\"";

				if (styler)
					styler->image(o, href, std::string(), alt);
				else
					o << "<img src=\"" << url::htmlQuotes(href) << "\"" << alt << "/>";
			}

			void Unknown::debug(std::ostream& o, const std::string& href, const segments_t& segments) const
			{
				o << name() << "?" << href;
				for (auto&& seg : segments)
					o << "|" << seg;
			}
		}

		std::string Link::produce(const Nodes& collection, const variables_t& vars)
		{
			if (collection.empty())
				return std::string();

			list_ctx ctx;
			std::ostringstream o;
			for (auto&& node : collection)
				node->text(o, vars, ctx);
			return o.str();
		}

		std::string Link::debug(const Nodes& collection)
		{
			if (collection.empty())
				return std::string();

			list_ctx ctx;
			std::ostringstream o;
			for (auto&& node : collection)
				node->debug(o);
			return o.str();
		}

		void Link::debug(std::ostream& o) const
		{
			if (!m_ns)
			{
				o << "<LINK:nullptr>";
				return;
			}

			std::string href;
			link::segments_t segs;
			debug(href, segs);

			o << "<LINK:";
			m_ns->debug(o, href, segs);
			o << '>';
		}

		void Link::text(std::ostream& o, const variables_t& vars, list_ctx&) const
		{
			if (!m_ns)
				return;

			std::string href;
			link::segments_t segs;
			produce(href, segs, vars);
			m_ns->text(o, href, segs, vars);
		}

		void Link::markup(std::ostream& o, const variables_t& vars, const styler_ptr& styler, list_ctx&) const
		{
			if (!m_ns)
				return;

			std::string href;
			link::segments_t segs;
			produce(href, segs, vars);
			m_ns->markup(o, href, segs, vars, styler);
		}

		void Link::normalize()
		{
			auto len = m_children.size();
			decltype(len) base = 0;

			Nodes new_children;

			if (len > 1 && is_text(0) && is_token(1, TOKEN::HREF_NS))
			{
				base = 2;
				list_ctx ctx;
				std::ostringstream o;
				m_children[0]->text(o, variables_t(), ctx);
				auto ns = o.str();
				if (ns == "Image")
					m_ns = std::make_shared<link::Image>();
				else
					m_ns = std::make_shared<link::Unknown>(ns);
			}
			else
			{
				m_ns = std::make_shared<link::Url>();
			}

			auto seg = base;
			while (seg < len && !is_token(seg, TOKEN::HREF_SEG)) ++seg;

			for (auto i = base; i < seg; ++i)
				m_href.push_back(m_children[i]);

			new_children.push_back(std::make_shared<Node>("HREF", m_href));

			m_segs.clear();
			seg++;
			while (seg < len)
			{
				auto segend = seg;
				while (segend < len && !is_token(segend, TOKEN::HREF_SEG)) ++segend;

				Nodes nodes;
				for (auto i = seg; i < segend; ++i)
					nodes.push_back(m_children[i]);

				m_segs.push_back(nodes);
				new_children.push_back(std::make_shared<Node>("SEG", nodes));
				seg = segend + 1;
			}

			Nodes empty;
			m_children.swap(new_children);
			normalizeChildren();
			m_children.swap(empty);
		}
	}

	namespace block_elem
	{
		void Block::text(std::ostream& o, const variables_t& vars, list_ctx& ctx) const
		{
			Node::text(o, vars, ctx);
			o << "\n\n";
		}

		void Block::markup(std::ostream& o, const variables_t& vars, const styler_ptr& styler, list_ctx& ctx) const
		{
			styler->begin_block(o, m_tag);
			for (auto& child : m_children)
				child->markup(o, vars, styler, ctx);
			styler->end_block(o, m_tag);
		}

		void Block::debug(std::ostream& o) const
		{
			o << '\n';
			Node::debug(o);
			o << '\n';
		}

		void Quote::text(std::ostream& o, const variables_t& vars, list_ctx& ctx) const
		{
			std::string indent = ctx.indentStr();
			o << indent << "> ";
			ctx.indentStr(indent + "> ");
			for (auto& child : m_children)
				child->text(o, vars, ctx);
			ctx.indentStr(indent);
		}

		void Item::text(std::ostream& o, const variables_t& vars, list_ctx& ctx) const
		{
			o << ctx.indentStr() << " - ";
			for (auto& child : m_children)
				child->text(o, vars, ctx);
		}

		void Signature::text(std::ostream& o, const variables_t& vars, list_ctx& ctx) const
		{
			o << "-- \n";
			Node::text(o, vars, ctx);
			o << "\n\n";
		}
	}
}
