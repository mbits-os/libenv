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

#define PARSER_TAG RDF1
#include "feed_parser_internal.hpp"

/*
 * RDF (1.0)
 * ns = { 'rdf' : 'http://www.w3.org/1999/02/22-rdf-syntax-ns#', 'a' : 'http://purl.org/rss/1.0/', 'content': 'http://purl.org/rss/1.0/modules/content/'}
 * /rdf:RDF :: Feed
 *     a:channel             -> this
 *         a:title           ->     m_feed.m_title
 *         a:link            ->     m_feed.m_url
 *         a:description     ->     m_description
 *         a:language        ->     m_language
 *         a:copyright       ->     m_copyright
 *         atom:category*    ->     m_categories
 *             atom:category ->         <item>
 *         a:managingEditor  ->     m_author.m_url
 *         a:image           ->     m_image
 *             atom:title    ->         m_image.m_title
 *             atom:url      ->         m_image.m_url
 *     a:item*               -> m_entry
 *         a:guid            ->	    <item>.m_entryUniqueId
 *         a:title           ->	    <item>.m_entry.m_title
 *         a:link            ->	    <item>.m_entry.m_url
 *         a:author          ->	    <item>.m_author.m_url
 *         a:category*       ->	    <item>.m_categories
 *             a:category    ->	        <item>
 *         a:pubDate         ->	    <item>.m_publishDateTime
 *         a:description     ->	    <item>.m_description
 */

namespace feed
{
	namespace rdf10
	{
		using Sequence = std::list<std::string>;
		struct RdfEntry : Entry
		{
			std::string  m_about;
		};
		typedef std::list<RdfEntry> RdfEntries;

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
			RdfEntries  m_entry;
			std::string m_etag;
			std::string m_lastModified;
			Sequence    m_sequence;
		};
	}

	RULE(Enclosure)
	{
		FIND("@url",    m_url);
		FIND("@type",   m_type);
		FIND("@length", m_size);
	}

	RULE(rdf10::RdfEntry)
	{
		FIND("@rdf:about",      m_about);
		FIND_NAMED("a:title",   m_entry, m_title);
		FIND_NAMED("a:link",    m_entry, m_url);
		FIND("a:guid",          m_entryUniqueId);
		FIND("a:enclosure",     m_enclosures);
		FIND_AUTHOR("a:author", m_email);
		FIND("a:category",      m_categories);
		FIND_TIME("a:pubDate",  m_dateTime);
		FIND("a:description",   m_description);
		FIND("content:encoded", m_content);
	}

	RULE(rdf10::Feed)
	{
		FIND_NAMED("a:title",           m_feed, m_title);
		FIND_NAMED("a:link",            m_feed, m_url);
		FIND("a:description",           m_description);
		FIND("a:language",              m_language);
		FIND("a:copyright",             m_copyright);
		FIND("a:category",              m_categories);
		FIND_AUTHOR("a:managingEditor", m_email);
		//image                      -> m_image
		FIND("a:items/rdf:Seq/rdf:li/@rdf:resource", m_sequence);
		FIND("../a:item",               m_entry);
	};

	namespace rdf10
	{
		dom::NSData ns[] = {
			{ "rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#" },
			{ "a",   "http://purl.org/rss/1.0/" },
			{ "content", "http://purl.org/rss/1.0/modules/content/" },
			{ NULL }
		};

		struct RDF: FeedParser
		{
			RDF(): FeedParser("RDF (1.0)", "/rdf:RDF/a:channel", ns)
			{
			}

			FIND_ROOT;
		};

		// TODO:read sequence and use items referenced there...
		bool parse(const dom::XmlDocumentPtr& document, feed::Feed& feed)
		{
			Feed tmp;
			if (!RDF().parse(document, tmp))
				return false;

#define SWAP(fld) std::swap(feed.fld, tmp.fld)
			SWAP(m_feed);
			SWAP(m_self);
			SWAP(m_description);
			SWAP(m_author);
			SWAP(m_language);
			SWAP(m_copyright);
			SWAP(m_image);
			SWAP(m_categories);
			//SWAP(m_entry);
			SWAP(m_etag);
			SWAP(m_lastModified);
#undef SWAP

			if (tmp.m_sequence.empty())
			{
				for (auto&& entry : tmp.m_entry)
					feed.m_entry.push_back(entry);
			}
			else
			{
				for (auto&& item : tmp.m_sequence)
				{
					for (auto&& entry : tmp.m_entry)
					{
						if (entry.m_about != item) continue;
						feed.m_entry.push_back(entry);
						break;
					}
				}
			}
			return true;
		}
	}
}
