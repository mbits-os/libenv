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
#include <opml.hpp>

#define PARSER_TAG OPML
#include "feed_parser_internal.hpp"

namespace feed
{
	RULE(opml::Outline)
	{
		FIND("@title", m_text);
		FIND("@text", m_text);
		FIND("@xmlUrl[../@type='rss']", m_url);
		FIND("outline", m_children);
	}
};

namespace opml
{
	dom::NSData ns[] = {
		{ NULL }
	};

	struct Opml: feed::FeedParser
	{
		Opml(): feed::FeedParser("OPML (1.0)", "/opml/body", ns)
		{
		}

		bool parse(dom::XmlDocumentPtr document, Outline& outline)
		{
			printf("%s Root: %s\n", m_name, getRoot().c_str());
			feed::Parser<Outline> parser;
			parser.setContext(&outline);
			dom::Namespaces ns = getNamespaces();
			dom::XmlNodePtr root = document->find(getRoot(), ns);
			if (!root)
				return false;
			return parser.parse(root, ns);
		}
	};

	bool parse(dom::XmlDocumentPtr document, Outline& outline)
	{
		return Opml().parse(document, outline);
	}
}
