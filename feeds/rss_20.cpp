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

#define PARSER_TAG RSS2
#include "feed_parser_internal.hpp"

/*
 * RSS (2.0)
 * ns = { 'content' : 'http://purl.org/rss/1.0/modules/content/', 'dc' : 'http://purl.org/dc/elements/1.1/' }
 * /rss/channel :: Feed
 *     title               -> m_feed.m_title
 *     link                -> m_feed.m_url
 *     description         -> m_description [1]
 *     dc:description      -> m_description [1]
 *     language            -> m_language
 *     copyright           -> m_copyright [2]
 *     dc:rights           -> m_copyright [2]
 *     category*           -> m_categories
 *         category        ->     <item>
 *     managingEditor      -> m_author.m_url
 *     image               -> m_image
 *         title           ->     m_image.m_title
 *         url             ->     m_image.m_url
 *     item*               -> m_entry
 *         guid            ->    <item>.m_entryUniqueId
 *         title           ->    <item>.m_entry.m_title
 *         link            ->    <item>.m_entry.m_url
 *         enclosure       ->    <item>.m_enclosures
 *             @url        ->        <item>.m_url
 *             @type       ->        <item>.m_type
 *             @length     ->        <item>.m_size
 *         author          ->    <item>.m_author.m_url
 *         dc:creator      ->    <item>.m_author.m_url
 *         category*       ->    <item>.m_categories
 *             category    ->        <item>
 *         pubDate         ->    <item>.m_publishDateTime
 *         dc:date         ->    <item>.m_publishDateTime
 *         content:encoded ->    <item>.m_description
 *         description     ->    <item>.m_description
 */

namespace feed
{
	RULE(Enclosure)
	{
		FIND("@url", m_url);
		FIND("@type", m_type);
		FIND("@length", m_size);
	}

	RULE(Entry)
	{
		FIND_NAMED("title",       m_entry, m_title);
		FIND_NAMED("link",        m_entry, m_url);
		FIND("guid",              m_entryUniqueId);
		FIND("enclosure",         m_enclosures);
		FIND_AUTHOR("author",     m_email);
		FIND_AUTHOR("dc:creator", m_email);
		FIND("category",          m_categories);
		FIND_TIME("pubDate",      m_dateTime);
		FIND_TIME("dc:date",      m_dateTime);
		FIND("description",       m_description);
		FIND("content:encoded",   m_content);
	}

	RULE(Feed)
	{
		FIND_NAMED("title",           m_feed, m_title);
		FIND_NAMED("link",            m_feed, m_url);
		FIND("description",           m_description);
		FIND("dc:description",        m_description);
		FIND("language",              m_language);
		FIND("copyright",             m_copyright);
		FIND("dc:rights",             m_copyright);
		FIND("category",              m_categories);
		FIND_AUTHOR("managingEditor", m_email);
		//image                      -> m_image
		FIND("item",                  m_entry);
	};

	namespace rss20
	{
		dom::NSData ns[] = {
			{ "content", "http://purl.org/rss/1.0/modules/content/" },
			{ "dc", "http://purl.org/dc/elements/1.1/" },
			{ NULL }
		};

		struct RSS: FeedParser
		{
			RSS(): FeedParser("RSS (2.0)", "/rss/channel", ns)
			{
			}

			FIND_ROOT;
		};

		bool parse(dom::XmlDocumentPtr document, Feed& feed)
		{
			return RSS().parse(document, feed);
		}
	}
}
