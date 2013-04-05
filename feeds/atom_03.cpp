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
#include <feed_parser.hpp>

#define PARSER_TAG Atom03
#include "feed_parser_internal.hpp"

/*
 * Atom (0.3):
 */

namespace feed
{
	RULE(Author)
	{
		FIND("atom:name",  m_name);
		FIND("atom:email", m_email);
	}

	RULE(Enclosure)
	{
		FIND("@href", m_url);
		FIND("@type", m_type);
		FIND("@length", m_size);
	}

	RULE(Entry)
	{
		FIND_NAMED("atom:title",            m_entry, m_title);
		FIND_NAMED("atom:link[@rel='alternate']/@href", m_entry, m_url);
		FIND("atom:id",                     m_entryUniqueId);
		FIND("atom:link[@rel='enclosure']", m_enclosures);
		FIND("atom:author",                 m_author);
		FIND_TIME("atom:issued",            m_dateTime);
		FIND_TIME("atom:modified",          m_dateTime);
		FIND("atom:category",               m_categories);
		FIND("atom:summary",                m_description);
		FIND("atom:content",                m_content);
	}

	RULE(Feed)
	{
		FIND_NAMED("atom:title",   m_feed, m_title);
		FIND_NAMED("atom:link[@rel='alternate']/@href", m_feed, m_url);
		FIND("atom:author",        m_author);
		FIND("atom:tagline",       m_description);
		FIND("xml:lang",           m_language);
		FIND("atom:copyright",     m_copyright);
		FIND("atom:category",      m_categories);
		FIND("atom:entry",         m_entry);
	};

	namespace atom03
	{
		dom::NSData ns[] = {
			{ "atom", "http://purl.org/atom/ns#" },
			{ NULL }
		};

		struct Atom03: FeedParser
		{
			Atom03(): FeedParser("Atom (0.3)", "/atom:feed", ns)
			{
			}

			FIND_ROOT;
		};

		bool parse(dom::XmlDocumentPtr document, Feed& feed)
		{
			return Atom03().parse(document, feed);
		}
	}
}
