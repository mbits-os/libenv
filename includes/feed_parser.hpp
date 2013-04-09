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

#ifndef __FEED_PARSER_H__
#define __FEED_PARSER_H__

#include <utils.hpp>

namespace dom
{
	struct XmlDocument;
	typedef std::shared_ptr<XmlDocument> XmlDocumentPtr;
};

namespace feed
{
	struct Enclosure
	{
		std::string m_url;
		std::string m_type;
		size_t m_size;
		Enclosure(): m_size(0) {}
	};
	typedef std::list<Enclosure> Enclosures;

	typedef std::list<std::string> Categories;

	struct NamedUrl
	{
		std::string  m_title;
		std::string  m_url;
	};

	struct Author
	{
		std::string m_name;
		std::string m_email;
	};

	struct Entry
	{
		std::string  m_entryUniqueId;

		NamedUrl     m_entry;
		std::string  m_description;
		Author       m_author;

		Enclosures   m_enclosures;
		Categories   m_categories;

		tyme::time_t m_dateTime;

		std::string  m_content;
		Entry(): m_dateTime(-1) {}
	};
	typedef std::list<Entry> Entries;

	struct Feed
	{
		NamedUrl    m_feed;
		std::string m_self;
		std::string m_description;
		Author      m_author;
		std::string m_language;
		std::string m_copyright;
		NamedUrl    m_image;
		Categories  m_categories;
		Entries     m_entry;
		std::string m_etag;
		std::string m_lastModified;
	};

	bool parse(dom::XmlDocumentPtr document, Feed& feed);
};

#endif //__FEED_PARSER_H__