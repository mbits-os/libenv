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

	std::string Node::debug() const
	{
		std::ostringstream o;
		o << "<" << m_tag << ">";
		for (auto&& child : m_children)
			o << child->debug();
		o << "</" << m_tag << ">";
		return o.str();
	}

	std::string Node::text(const variables_t& vars, list_ctx& ctx) const
	{
		std::string out;
		for (auto& child : m_children)
			out += child->text(vars, ctx);
		return out;
	}

	std::string Node::markup(const variables_t& vars, const styler_ptr& styler, list_ctx& ctx) const
	{
		std::string out("<" + m_tag + ">");
		for (auto& child : m_children)
			out += child->markup(vars, styler, ctx);
		return out + "</" + m_tag + ">";
	}

	namespace inline_elem
	{
		std::string Token::debug() const
		{
			std::ostringstream o;
			o << m_token;
			return o.str();
		}

		std::string Variable::text(const variables_t& vars, list_ctx&) const
		{
			auto& it = vars.find(m_name);
			if (it == vars.end()) return std::string();
			return it->second;
		}

		std::string Variable::markup(const variables_t& vars, const styler_ptr&, list_ctx&) const
		{
			auto& it = vars.find(m_name);
			if (it == vars.end()) return std::string();
			return it->second;
		}

		namespace link
		{
			std::string Url::debug(const std::string& href, const segments_t& segments) const
			{
				std::ostringstream o;
				o << "url:" << href;
				for (auto&& seg : segments)
					o << "|" << seg;

				return o.str();
			}

			std::string Url::text(const std::string& href, const segments_t& segments, const variables_t&) const
			{
				if (!segments.empty())
					return segments[0];

				return "<" + href + ">";
			}

			std::string Url::markup(const std::string& href, const segments_t& segments, const variables_t&, const styler_ptr&) const
			{
				auto contents = href;
				if (!segments.empty())
					contents = segments[0];

				std::ostringstream o;
				o << "<a href=\"" << url::htmlQuotes(href) << "\">" << url::htmlQuotes(contents) << "</a>";
				return o.str();
			}
		
			std::string Image::debug(const std::string& href, const segments_t& segments) const
			{
				return "Image:" + href;
			}

			std::string Image::text(const std::string&, const segments_t&, const variables_t&) const
			{
				return std::string();
			}

			std::string Image::markup(const std::string& href, const segments_t& segments, const variables_t&, const styler_ptr& styler) const
			{
				std::string alt;
				if (!segments.empty())
					alt = " alt=\"" + segments[0] + "\"";

				if (styler)
					return styler->image(href, std::string(), alt);

				return "<img src=\"" + url::htmlQuotes(href) + "\"" + alt + "/>";
			}

			std::string Unknown::debug(const std::string& href, const segments_t& segments) const
			{
				std::ostringstream o;
				o << name() << "?" << href;
				for (auto&& seg : segments)
					o << "|" << seg;

				return o.str();
			}
		}

		std::string Link::produce(const Nodes& collection, const variables_t& vars)
		{
			if (collection.empty())
				return std::string();

			list_ctx ctx;
			std::ostringstream o;
			for (auto&& node : collection)
				o << node->text(vars, ctx);
			return o.str();
		}

		std::string Link::debug(const Nodes& collection)
		{
			if (collection.empty())
				return std::string();

			list_ctx ctx;
			std::ostringstream o;
			for (auto&& node : collection)
				o << node->debug();
			return o.str();
		}

		std::string Link::debug() const
		{
			if (!m_ns)
				return "<LINK:nullptr>";

			std::string href;
			link::segments_t segs;
			debug(href, segs);

			return "<LINK:" + m_ns->debug(href, segs) + ">";
		}

		std::string Link::text(const variables_t& vars, list_ctx&) const
		{
			if (!m_ns)
				return std::string();

			std::string href;
			link::segments_t segs;
			produce(href, segs, vars);
			return m_ns->text(href, segs, vars);
		}

		std::string Link::markup(const variables_t& vars, const styler_ptr& styler, list_ctx&) const
		{
			if (!m_ns)
				return std::string();

			std::string href;
			link::segments_t segs;
			produce(href, segs, vars);
			return m_ns->markup(href, segs, vars, styler);
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
				auto ns = m_children[0]->text(variables_t(), ctx);
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
		std::string Block::text(const variables_t& vars, list_ctx& ctx) const
		{
			return Node::text(vars, ctx) + "\n\n";
		}

		std::string Block::markup(const variables_t& vars, const styler_ptr& styler, list_ctx& ctx) const
		{
			std::ostringstream o;
			o << styler->begin_block(m_tag);
			for (auto& child : m_children)
				o << child->markup(vars, styler, ctx);
			o << styler->end_block(m_tag);
			return o.str();
		}
	}
}
