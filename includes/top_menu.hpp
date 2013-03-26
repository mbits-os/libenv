/*
* Copyright (C) 2013 Aggregate
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

#ifndef __TOP_MENU_HPP_
#define __TOP_MENU_HPP_

#include <fast_cgi.h>
#include <algorithm>
#include <stdlib.h>
#include <stdio.h>

#ifdef USE_POSIX
static inline int itoa(int value, char* str, int radix)
{
	const char* frmt = radix == 16 ? "%x" : "%d";
	return sprintf(str, frmt, value);
}
#endif

namespace FastCGI { namespace TopMenu {

	class MenuItem
	{
		std::string m_id;
	protected:
		int m_iconPosition;
		std::string m_label;
		std::string m_tip;
		std::string m_url;
	public:
		MenuItem(const std::string& id, int iconPosition = -1, const std::string& label = std::string(), const std::string& tip = std::string())
			: m_id(id)
			, m_iconPosition(iconPosition)
			, m_label(label)
			, m_tip(tip)
			, m_url("#")
		{
		}
		virtual ~MenuItem() {}

		const std::string& getId() { return m_id; }

		virtual void echoMarkup(Request& request, const std::string& topbarId, const std::string& menuId, const std::string& pre)
		{
			request << "			" << pre << "<li id='" << topbarId << "-" << menuId << "-" << m_id << "'>\n";
			echoMarkupContent(request, topbarId, menuId, pre);
			request << "			" << pre << "</li>\n";
		}

		virtual void echoMarkupContent(Request& request, const std::string& topbarId, const std::string& menuId, const std::string& pre)
		{
			std::string contentTag = "div";
			std::string link;
			std::string title;
			std::string label;
			std::string image;
			if (!m_url.empty())
			{
				contentTag = "a";
				link = " href=\"" + url::htmlQuotes(m_url) + "\"";
			}

			if (m_iconPosition != -1)
				image = "<span class=\"bar-icon\"></span>";

			if (!m_label.empty())
				label = "<span class=\"bar-label\">" + m_label + "</span>";

			if (!label.empty() && !image.empty())
				label = " " + label;

			if (!m_tip.empty())
				title = " title=\"" + m_tip + "\"";

			request << "				" << pre << "<" << contentTag << link << title << " class=\"bar-item\">" << image << label << "</" << contentTag << ">\n";
		}

		virtual std::string getCSSRules()
		{
			std::string style;
			/*if (!m_icon.empty())
			{
				style = "	background: url(\"" + url::htmlQuotes(m_icon) + "\") no-repeat;\n";
			}
			else */ if (m_iconPosition != -1)
			{
				char buffer[100];
				itoa(m_iconPosition * 24, buffer, 10);
				style = std::string("	background-position: 0px -") + buffer + "px;\n";
			}
			return style;
		}

		virtual void echoCSS(Request& request, const std::string& topbarId, const std::string& menuId)
		{
			std::string style = getCSSRules();

			if (!style.empty())
				request << "#" << topbarId << "-" << menuId << "-" << m_id << " > .bar-item .bar-icon {\r\n" << style << "}\r\n\r\n";
		}
	};

	typedef std::shared_ptr<MenuItem> MenuItemPtr;

	class SeparatorItem: public MenuItem
	{
	public:
		SeparatorItem(): MenuItem(std::string())
		{
		}

		void echoMarkup(Request& request, const std::string&, const std::string&, const std::string& pre)
		{
			request << "			"<< pre << "<li><hr/></li>\r\n";
		}
	};

	class HomeLink: public MenuItem
	{
		std::string m_product;
		std::string m_description;
	public:
		HomeLink(const std::string& id, int iconPosition = -1, const std::string& product = std::string(), const std::string& description = std::string(), const std::string& tip = std::string())
			: MenuItem(id, iconPosition, std::string(), tip)
			, m_product(product)
			, m_description(description)
		{
			m_url = "/";
		}

		void echoMarkup(Request& request, const std::string&, const std::string&, const std::string& pre)
		{
			std::string title;
			std::string image;
			if (m_iconPosition != -1)
				image = "<span class=\"bar-icon\"></span>";

			if (!m_tip.empty())
				title = " title=\"" + m_tip + "\"";

			request << "				" << pre << "<div class=\"bar-item\"><a href=\"/\"" << title << ">" << image
				<< "<b>" << m_product << "</b></a> <i>" << m_description<< "</i></div>\r\n";
		}
	};

	class Menu
	{
		std::string m_id;
		std::string m_class;
		std::list<MenuItemPtr> m_items;

	public:
		Menu(const std::string& id, const std::string& klass = std::string())
			: m_id(id)
			, m_class(klass)
		{
		}

		template<class Item>
		Menu& add(std::shared_ptr<Item> child)
		{
			if (child.get())
				m_items.push_back(std::static_pointer_cast<MenuItem>(child));
			return *this;
		}

		template<class Item>
		Menu& add(Item* child)
		{
			MenuItemPtr item(child);
			if (item.get())
				m_items.push_back(item);
			return *this;
		}

		Menu& separator()
		{
			return add(new (std::nothrow) SeparatorItem());
		}

		Menu& home(const std::string& id, int iconPosition = -1, const std::string& product = std::string(), const std::string& description = std::string(), const std::string& tip = std::string())
		{
			return add(new (std::nothrow) HomeLink(id, iconPosition, product, description, tip));
		}

		MenuItemPtr getItem(const std::string& id)
		{
			auto it = std::find_if(m_items.begin(), m_items.end(), [&id](MenuItemPtr ptr) -> bool
			{
				return ptr->getId() == id;
			});
			return it == m_items.end() ? MenuItemPtr() : *it;
		}

		virtual void echoMarkup(Request& request, const std::string& menuPre, const std::string& topbarId, const std::string& klass)
		{
			request << "\r\n\t\t\t\t<ul id=\"" << menuPre << "-" << m_id << "\" class=\"" << klass << "\">\r\n";
			std::for_each(m_items.begin(), m_items.end(), [&](MenuItemPtr ptr)
			{
				ptr->echoMarkup(request, topbarId, m_id, std::string());
			});
			request << "\t\t\t\t</ul>\r\n";
		}

		virtual void echoCSS(Request& request, const std::string& topbarId)
		{
			std::for_each(m_items.begin(), m_items.end(), [&](MenuItemPtr ptr)
			{
				ptr->echoCSS(request, topbarId, m_id);
			});
		}
	};

	class TopBar
	{
		Menu m_leftMenu;
		Menu m_rightMenu;
		std::string m_id;

	public:
		TopBar(const std::string& id, const std::string& leftMenu, const std::string& rightMenu)
			: m_leftMenu(leftMenu)
			, m_rightMenu(rightMenu, rightMenu + "-item-menu")
			, m_id(id)
		{
		}

		Menu& left() { return m_leftMenu; }
		Menu& right() { return m_rightMenu; }

		virtual void echoMarkup(Request& request)
		{
			request <<
				"\t\t\t\t<div id=\"" << m_id  << "\">\r\n"
				"\t\t\t\t<div class=\"navigation_links\">\r\n";
			m_leftMenu.echoMarkup(request, "links", m_id, "left-top-links top-links");
			m_rightMenu.echoMarkup(request, "links", m_id, "right-top-links top-links");
			request <<
				"\t\t\t\t</div>\r\n"
				"\t\t\t\t</div>\r\n"
				"\t\t\t\t<div class=\"" << m_id << "-standin\"></div>\r\n";
		}

		virtual void echoCSS(Request& request, bool inHTML)
		{
			if (inHTML) request << "\t<style type=\"text/css\">\n";
			m_leftMenu.echoCSS(request, m_id);
			m_rightMenu.echoCSS(request, m_id);
			if (inHTML) request << "\t</style>\n";
		}
	};

}} // FastCGI::TopMenu

#endif // __TOP_MENU_HPP_