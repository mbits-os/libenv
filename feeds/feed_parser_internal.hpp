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

#ifndef __FEED_PARSER_HPP__
#define __FEED_PARSER_HPP__

#include <dom.hpp>
#include <utils.hpp> // for tyme::time_t

#ifndef PARSER_TAG
#error PARSER_TAG is missing
#endif

#define NAMED1(x, y) x ## y
#define NAMED(x, y) NAMED1(x, y)
#define Parser                 NAMED(PARSER_TAG, _Parser)
#define StructParser           NAMED(PARSER_TAG, _StructParser)
#define ListSelector           NAMED(PARSER_TAG, _ListSelector)
#define MemberSelector         NAMED(PARSER_TAG, _MemberSelector)
#define TimeMemberSelector     NAMED(PARSER_TAG, _TimeMemberSelector)
#define IndirectMemberSelector NAMED(PARSER_TAG, _IndirectMemberSelector)

#define RULE(type) \
	template <> \
	struct Parser<type>: StructParser<type> \
	{ \
		Parser(); \
		typedef type Type; \
	}; \
	Parser<type>::Parser()

#define FIND(xpath, dest) find(xpath, &Type::dest)
#define FIND_NAMED(xpath, parent, field) find(xpath, &Type::parent, &NamedUrl::field)
#define FIND_AUTHOR(xpath, field) find(xpath, &Type::m_author, &Author::field)
#define FIND_TIME(xpath, dest) findTime(xpath, &Type::dest)
#define FIND_ROOT \
	bool parse(dom::XmlDocumentPtr document, Feed& feed) \
	{ \
		printf("%s Root: %s\n", m_name, getRoot().c_str()); \
		Parser<Feed> parser; \
		parser.setContext(&feed); \
		dom::Namespaces ns = getNamespaces(); \
		dom::XmlNodePtr root = document->find(getRoot(), ns); \
		if (!root) \
			return false; \
		return parser.parse(root, ns); \
	}


namespace feed
{
	template <typename Type> struct Parser;

	struct Selector
	{
		virtual ~Selector() {}
		virtual bool parse(dom::XmlNodePtr node, dom::Namespaces ns, void* context) = 0;
		virtual std::string name() const = 0;
	};
	typedef std::shared_ptr<Selector> SelectorPtr;

	struct ParserBase
	{
		ParserBase(): m_ctx(nullptr) {}
		void setContext(void* ctx) { m_ctx = ctx; }
		virtual bool parse(dom::XmlNodePtr node, dom::Namespaces ns) = 0;
	protected:
		void* m_ctx;
	};

	template <>
	struct Parser<std::string>: ParserBase
	{
		bool parse(dom::XmlNodePtr node, dom::Namespaces ns)
		{
			std::string* ctx = (std::string*)m_ctx;
			if (!ctx)
				return false;

			*ctx = node->stringValue();
			return true;
		}
	};

	struct time_tag
	{
		tyme::time_t m_time;
	};

	template <>
	struct Parser<time_tag>: ParserBase
	{
		bool parse(dom::XmlNodePtr node, dom::Namespaces ns)
		{
			time_tag* ctx = (time_tag*)m_ctx;
			if (!ctx)
				return false;

			dom::NODE_TYPE type = node->nodeType();
			std::string contents = node->stringValue();

			printf("Found time: %s\n", contents.c_str());

			ctx->m_time = 0; //parse...
			return true;
		}
	};

	template <>
	struct Parser<size_t>: ParserBase
	{
		bool parse(dom::XmlNodePtr node, dom::Namespaces ns)
		{
			time_t* ctx = (time_t*)m_ctx;
			if (!ctx)
				return false;

			dom::NODE_TYPE type = node->nodeType();
			std::string contents = node->stringValue();
			*ctx = strtoul(contents.c_str(), nullptr, 10);

			return true;
		}
	};

	template <typename Type, typename Member>
	struct MemberSelector: Selector
	{
		std::string m_xpath;
		Member Type::* m_member;
		bool m_optional;

		MemberSelector(const std::string& xpath, Member Type::* member, bool optional = true)
			: m_xpath(xpath)
			, m_member(member)
			, m_optional(optional)
		{
		}

		bool parse(dom::XmlNodePtr node, dom::Namespaces ns, void* context)
		{
			dom::XmlNodePtr data = node->find(m_xpath, ns);
			if (!data)
				return m_optional;

			Type* ctx = (Type*)context;
			if (!ctx)
				return false;

			Member& new_ctx = ctx->*m_member;

			Parser<Member> subparser;
			subparser.setContext(&new_ctx);
			return subparser.parse(data, ns);
		}
		std::string name() const { return "MemberSelector(" + m_xpath + ")"; }
	};

	template <typename Type>
	struct TimeMemberSelector: Selector
	{
		std::string m_xpath;
		tyme::time_t Type::* m_member;
		bool m_optional;

		TimeMemberSelector(const std::string& xpath, tyme::time_t Type::* member, bool optional = true)
			: m_xpath(xpath)
			, m_member(member)
			, m_optional(optional)
		{
		}

		bool parse(dom::XmlNodePtr node, dom::Namespaces ns, void* context)
		{
			dom::XmlNodePtr data = node->find(m_xpath, ns);
			if (!data)
				return m_optional;

			Type* ctx = (Type*)context;
			if (!ctx)
				return false;

			tyme::time_t& value = ctx->*m_member;
			time_tag new_ctx;

			Parser<time_tag> subparser;
			subparser.setContext(&new_ctx);
			if (!subparser.parse(data, ns))
				return false;
			value = new_ctx.m_time;

			return true;
		}
		std::string name() const { return "TimeMemberSelector(" + m_xpath + ")"; }
	};

	template <typename Type, typename Item>
	struct ListSelector: Selector
	{
		std::string m_xpath;
		std::list<Item> Type::* m_member;
		bool m_optional;
		ListSelector(const std::string& xpath, std::list<Item> Type::* member, bool optional = true)
			: m_xpath(xpath)
			, m_member(member)
			, m_optional(optional)
		{
		}

		bool parse(dom::XmlNodePtr node, dom::Namespaces ns, void* context)
		{
			dom::XmlNodeListPtr data = node->findall(m_xpath, ns);
			if (!data || !data->length())
				return m_optional;

			Type* ctx = (Type*)context;
			if (!ctx)
				return false;

			std::list<Item>& container = ctx->*m_member;

			size_t count = data->length();
			for (size_t i = 0; i < count; ++i)
			{
				Parser<Item> subparser;
				Item item;
				subparser.setContext(&item);

				dom::XmlNodePtr child = data->item(i);
				if (!child || !subparser.parse(child, ns))
				{
					if (m_optional)
						continue;
					return false;
				}

				container.push_back(item);
			}
			return true;
		}
		std::string name() const { return "ListSelector(" + m_xpath + ")"; }
	};

	template <typename Type, typename Struct, typename Member>
	struct IndirectMemberSelector: Selector
	{
		std::string m_xpath;
		Struct Type::* m_parent;
		Member Struct::* m_member;
		bool m_optional;
		IndirectMemberSelector(const std::string& xpath, Struct Type::* parent, Member Struct::* member, bool optional = true)
			: m_xpath(xpath)
			, m_parent(parent)
			, m_member(member)
			, m_optional(optional)
		{
		}

		bool parse(dom::XmlNodePtr node, dom::Namespaces ns, void* context)
		{
			dom::XmlNodePtr data = node->find(m_xpath, ns);
			if (!data)
				return m_optional;

			Type* ctx = (Type*)context;
			if (!ctx)
				return false;

			Struct& struct_ctx = ctx->*m_parent;
			Member& new_ctx = struct_ctx.*m_member;

			Parser<Member> subparser;
			subparser.setContext(&new_ctx);
			return subparser.parse(data, ns);
		}
		std::string name() const { return "IndirectMemberSelector(" + m_xpath + ")"; }
	};

	template <typename Type>
	struct StructParser: ParserBase
	{
		std::list<SelectorPtr> m_selectors;
		template <typename Member>
		void find(const std::string& xpath, Member Type::* dest)
		{
			m_selectors.push_back(std::make_shared< MemberSelector<Type, Member> >(xpath, dest));
		}

		void findTime(const std::string& xpath, tyme::time_t Type::* dest)
		{
			m_selectors.push_back(std::make_shared< TimeMemberSelector<Type> >(xpath, dest));
		}

		template <typename Item>
		void find(const std::string& xpath, std::list<Item> Type::* dest)
		{
			m_selectors.push_back(std::make_shared< ListSelector<Type, Item> >(xpath, dest));
		}

		template <typename Member, typename Struct>
		void find(const std::string& xpath, Struct Type::* parent, Member Struct::* dest)
		{
			m_selectors.push_back(std::make_shared< IndirectMemberSelector<Type, Struct, Member> >(xpath, parent, dest));
		}

		bool parse(dom::XmlNodePtr node, dom::Namespaces ns)
		{
			auto cur = m_selectors.begin(), end = m_selectors.end();
			for (; cur != end; ++cur)
			{
				SelectorPtr& selector = *cur;
				if (!selector->parse(node, ns, m_ctx))
					return false;
			}
			return true;
		};
	};

	struct FeedParser
	{
		const char* m_name;
		std::string m_root;
		dom::NSData* m_ns;
		FeedParser(const char* name, const char* root, dom::NSData* ns): m_name(name), m_root(root), m_ns(ns) {}

		const std::string& getRoot() const { return m_root; }
		dom::Namespaces getNamespaces() { return m_ns; }
	};
}

#endif //__FEED_PARSER_HPP__