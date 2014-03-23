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

#ifndef __DOM_HPP__
#define __DOM_HPP__

#include <iterator>

namespace dom
{
	struct XmlDocument;
	struct XmlNode;
	struct XmlNodeList;
	struct XmlElement;
	struct XmlAttribute;
	struct XmlText;
	struct XmlDocumentFragment;

	typedef std::shared_ptr<XmlDocument> XmlDocumentPtr;
	typedef std::shared_ptr<XmlNode> XmlNodePtr;
	typedef std::shared_ptr<XmlNodeList> XmlNodeListPtr;
	typedef std::shared_ptr<XmlElement> XmlElementPtr;
	typedef std::shared_ptr<XmlAttribute> XmlAttributePtr;
	typedef std::shared_ptr<XmlText> XmlTextPtr;
	using XmlDocumentFragmentPtr = std::shared_ptr<XmlDocumentFragment>;

	struct NSData
	{
		const char* key;
		const char* ns;
	};

	typedef NSData* Namespaces;

	struct QName
	{
		std::string nsName;
		std::string localName;

		bool operator == (const QName& right) const
		{
			return nsName == right.nsName && localName == right.localName;
		}
		bool operator != (const QName& right) const
		{
			return !(*this == right);
		}
	};

	inline std::ostream& operator << (std::ostream& o, const QName& qname)
	{
		if (!qname.nsName.empty())
			o << "{" << qname.nsName << "}";
		return o << qname.localName;
	}

	enum NODE_TYPE
	{
		DOCUMENT_NODE  = 0,
		ELEMENT_NODE   = 1,
		ATTRIBUTE_NODE = 2,
		TEXT_NODE      = 3,
		DOCUMENT_FRAGMENT_NODE = 4
	};

	struct XmlNode
	{
		virtual ~XmlNode() {}
		virtual std::string nodeName() const = 0;
		virtual const QName& nodeQName() const = 0;
		virtual std::string nodeValue() const = 0;
		virtual std::string stringValue() { return nodeValue(); } // nodeValue for TEXT, ATTRIBUTE, and - coincidently - DOCUMENT; innerText for ELEMENT; used in xpath
		virtual void nodeValue(const std::string& val) = 0;
		virtual NODE_TYPE nodeType() const = 0;

		virtual XmlNodePtr parentNode() = 0;
		virtual XmlNodeListPtr childNodes() = 0;
		virtual XmlNodePtr firstChild() = 0;
		virtual XmlNodePtr lastChild() = 0;
		virtual XmlNodePtr previousSibling() = 0;
		virtual XmlNodePtr nextSibling() = 0;

		virtual XmlDocumentPtr ownerDocument() = 0;
		virtual bool insertBefore(const XmlNodePtr& child, const XmlNodePtr& before = nullptr) = 0;
		virtual bool insertBefore(const XmlNodeListPtr& children, const XmlNodePtr& before = nullptr) = 0;
		virtual bool appendChild(const XmlNodePtr& newChild) = 0;
		virtual bool replaceChild(const XmlNodePtr& newChild, const XmlNodePtr& oldChild) = 0;
		virtual bool replaceChild(const XmlNodeListPtr& newChildren, const XmlNodePtr& oldChild) = 0;
		virtual bool removeChild(const XmlNodePtr& child) = 0;

		virtual void* internalData() = 0;

		virtual XmlNodePtr find(const std::string& path, const Namespaces& ns) = 0;
		virtual XmlNodePtr find(const std::string& path) { return find(path, nullptr); }
		virtual XmlNodeListPtr findall(const std::string& path, const Namespaces& ns) = 0;
	};

	struct XmlChildNode : XmlNode
	{
		virtual bool before(const XmlNodePtr& node) = 0;
		virtual bool before(const std::string& data) = 0;
		virtual bool before(const XmlNodeListPtr& nodes) = 0;
		virtual bool after(const XmlNodePtr& node) = 0;
		virtual bool after(const std::string& data) = 0;
		virtual bool after(const XmlNodeListPtr& nodes) = 0;
		virtual bool replace(const XmlNodePtr& node) = 0;
		virtual bool replace(const std::string& data) = 0;
		virtual bool replace(const XmlNodeListPtr& nodes) = 0;
		virtual bool remove() = 0;
	};

	struct XmlParentNode : XmlChildNode
	{
		virtual bool prepend(const XmlNodePtr& node) = 0;
		virtual bool prepend(const XmlNodeListPtr& nodes) = 0;
		virtual bool prepend(const std::string& data) = 0;
		virtual bool append(const XmlNodePtr& node) = 0;
		virtual bool append(const XmlNodeListPtr& nodes) = 0;
		virtual bool append(const std::string& data) = 0;
	};

	struct XmlDocument : XmlNode
	{
		static XmlDocumentPtr create();
		static XmlDocumentPtr fromFile(const char* path);

		virtual XmlElementPtr documentElement() = 0;
		virtual void setDocumentElement(const XmlElementPtr& elem) = 0;
		virtual XmlDocumentFragmentPtr associatedFragment() = 0;
		virtual void setFragment(const XmlDocumentFragmentPtr& fragment) = 0;

		virtual XmlElementPtr createElement(const std::string& tagName) = 0;
		virtual XmlTextPtr createTextNode(const std::string& data) = 0;
		virtual XmlAttributePtr createAttribute(const std::string& name, const std::string& value) = 0;
		virtual XmlDocumentFragmentPtr createDocumentFragment() = 0;

		virtual XmlNodeListPtr getElementsByTagName(const std::string& tagName) = 0;
		virtual XmlElementPtr getElementById(const std::string& elementId) = 0;
	};

	struct XmlNodeList
	{
		virtual ~XmlNodeList() {}
		virtual XmlNodePtr item(size_t index) = 0;
		XmlElementPtr element(size_t index)
		{
			XmlNodePtr i = item(index);
			if (i && i->nodeType() == ELEMENT_NODE)
				return std::static_pointer_cast<XmlElement>(i);
			return XmlElementPtr();
		}
		XmlTextPtr text(size_t index)
		{
			XmlNodePtr i = item(index);
			if (i && i->nodeType() == TEXT_NODE)
				return std::static_pointer_cast<XmlText>(i);
			return XmlTextPtr();
		}
		XmlAttributePtr attr(size_t index)
		{
			XmlNodePtr i = item(index);
			if (i && i->nodeType() == ATTRIBUTE_NODE)
				return std::static_pointer_cast<XmlAttribute>(i);
			return nullptr;
		}
		virtual size_t length() const = 0;
		virtual bool remove() = 0;
	};

	struct XmlElement : XmlParentNode
	{
		virtual std::string tagName() const { return nodeName(); }
		virtual std::string stringValue() override { return innerText(); }
		virtual std::string getAttribute(const std::string& name) = 0;
		virtual XmlAttributePtr getAttributeNode(const std::string& name) = 0;
		virtual bool setAttribute(const XmlAttributePtr& attr) = 0;
		virtual bool removeAttribute(const XmlAttributePtr& attr) = 0;
		virtual bool setAttribute(const std::string& attr, const std::string& value) = 0;
		virtual bool removeAttribute(const std::string& attr) = 0;
		virtual XmlNodeListPtr getAttributes() = 0;
		virtual bool hasAttribute(const std::string& name) = 0;
		virtual XmlNodeListPtr getElementsByTagName(const std::string& tagName) = 0;
		virtual std::string innerText() = 0;
	};

	struct XmlAttribute: XmlNode
	{
		virtual std::string name() const { return nodeName(); }
		virtual std::string value() const { return nodeValue(); }
		virtual void value(const std::string& val) { nodeValue(val); }
		virtual XmlElementPtr ownerElement()
		{
			return std::static_pointer_cast<XmlElement>(parentNode());
		}
	};

	struct XmlText: XmlChildNode
	{
		virtual std::string data() const { return nodeValue(); }
	};

	struct XmlDocumentFragment : XmlParentNode
	{
		virtual XmlNodeListPtr getElementsByTagName(const std::string& tagName) = 0;
	};

	namespace range
	{
		template <typename NodeType>
		struct NodeSelector;

		template <>
		struct NodeSelector<XmlNode>
		{
			using node_t = XmlNode;
			using node_ptr = std::shared_ptr<node_t>;

			static node_ptr item(const XmlNodeListPtr& list, size_t index) { return list->item(index); }
		};

		template <>
		struct NodeSelector<XmlElement>
		{
			using node_t = XmlElement;
			using node_ptr = std::shared_ptr<node_t>;

			static node_ptr item(const XmlNodeListPtr& list, size_t index) { return list->element(index); }
		};

		template <>
		struct NodeSelector<XmlText>
		{
			using node_t = XmlText;
			using node_ptr = std::shared_ptr<node_t>;

			static node_ptr item(const XmlNodeListPtr& list, size_t index) { return list->text(index); }
		};

		template <>
		struct NodeSelector<XmlAttribute>
		{
			using node_t = XmlAttribute;
			using node_ptr = std::shared_ptr<node_t>;

			static node_ptr item(const XmlNodeListPtr& list, size_t index) { return list->attr(index); }
		};

		template <typename NodeType>
		struct NodeListRangeAdapter : NodeSelector<NodeType>
		{
			using selector_t = NodeSelector<NodeType>;
			using node_t = typename selector_t::node_t;
			using node_ptr = typename selector_t::node_ptr;

			class list_iterator
				: public std::iterator< std::bidirectional_iterator_tag, node_ptr >
			{
			public:
				typedef node_ptr pointer;
				typedef node_ptr reference;

				list_iterator(const XmlNodeListPtr& list, size_t offset) : m_list(list), m_offset(offset) {}

				list_iterator() = default;
				list_iterator(const list_iterator&) = default;
				list_iterator& operator=(const list_iterator&) = default;
				list_iterator(list_iterator&& oth) = default;
				list_iterator& operator=(list_iterator&& oth) = default;

				reference operator*() const
				{
					return selector_t::item(m_list, m_offset);
				}

				pointer operator->() const
				{
					return (std::pointer_traits<pointer>::pointer_to(**this));
				}

				list_iterator& operator++()
				{
					++m_offset;
					return *this;
				}

				list_iterator operator++(int)
				{	// postincrement
					list_iterator _Tmp = *this;
					++*this;
					return (_Tmp);
				}

				list_iterator& operator--()
				{
					++m_offset;
					return *this;
				}

				list_iterator operator--(int)
				{	// postdecrement
					list_iterator _Tmp = *this;
					--*this;
					return (_Tmp);
				}

				bool operator==(const list_iterator& _Right) const
				{	// test for iterator equality
					return (m_list == _Right.m_list && m_offset == _Right.m_offset);
				}

				bool operator!=(const list_iterator& _Right) const
				{	// test for iterator inequality
					return (!(*this == _Right));
				}
			private:
				XmlNodeListPtr m_list;
				size_t m_offset = 0;
			};

			using iterator = list_iterator;
			using const_iterator = list_iterator;

			XmlNodeListPtr list;
			NodeListRangeAdapter(const XmlNodeListPtr& list) : list(list) {}
			iterator begin() const { return iterator(list, 0); }
			iterator end() const
			{
				if (!list)
					return iterator(list, 0);
				return iterator(list, list->length());
			}
		};
	}

	inline range::NodeListRangeAdapter<XmlNode> list_nodes(const XmlNodeListPtr& list)
	{
		return range::NodeListRangeAdapter<XmlNode>(list);
	}

	inline range::NodeListRangeAdapter<XmlElement> list_elements(const XmlNodeListPtr& list)
	{
		return range::NodeListRangeAdapter<XmlElement>(list);
	}

	inline range::NodeListRangeAdapter<XmlText> list_texts(const XmlNodeListPtr& list)
	{
		return range::NodeListRangeAdapter<XmlText>(list);
	}

	inline range::NodeListRangeAdapter<XmlAttribute> list_atts(const XmlNodeListPtr& list)
	{
		return range::NodeListRangeAdapter<XmlAttribute>(list);
	}

	void Print(const XmlNodePtr& node, bool ignorews = false, size_t depth = 0);
	void Print(const XmlNodeListPtr& subs, bool ignorews = false, size_t depth = 0);
}

#endif // __DOM_HPP__