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
#include <dom.hpp>
#include <dom_xpath.hpp>
#include <vector>
#include <iterator>
#include <string.h>
#include <expat.hpp>

namespace dom
{
	namespace impl
	{
		typedef std::vector< dom::XmlNodePtr > NodesInit;

		static inline XmlNodePtr createTextSibling(XmlNode* node, const std::string& data)
		{
			if (!node) return nullptr;
			auto doc = node->ownerDocument();
			if (!doc) return nullptr;
			return doc->createTextNode(data);
		}

		bool removeFromParent(const XmlNodePtr& node)
		{
			if (!node)
				return false;
			auto parent = node->parentNode();
			if (!parent)
				return true;
			return parent->removeChild(node);
		}

		struct XmlNodeImplInit
		{
			NODE_TYPE type;
			std::string _name, _value;
			std::vector<dom::XmlNodePtr> children;
			std::weak_ptr<dom::XmlDocument> document;
			std::weak_ptr<dom::XmlNode> parent;
			size_t index = (size_t) -1;
			QName qname;

			virtual void fixQName(bool forElem = true)
			{
				std::string::size_type col = _name.find(':');
				if (col == std::string::npos && !forElem) return;
				if (col == std::string::npos)
					fixQName(qname, std::string(), _name);
				else
					fixQName(qname, std::string(_name.c_str(), col), std::string(_name.c_str() + col + 1));
			}

			virtual void fixQName(QName& qname, const std::string& ns, const std::string& localName)
			{
				dom::XmlNodePtr parent = this->parent.lock();
				if (!parent) return;
				XmlNodeImplInit* p = (XmlNodeImplInit*)parent->internalData();
				if (!p) return;
				p->fixQName(qname, ns, localName);
			}
		};

		template <typename T, typename _Interface>
		class XmlNodeImpl: public _Interface, public XmlNodeImplInit, public std::enable_shared_from_this<T>
		{
		public:

			typedef XmlNodeImplInit Init;
			typedef _Interface Interface;

			XmlNodeImpl(const Init& init): Init(init)
			{
				qname.localName = init._name;
			}

			std::string nodeName() const override { return _name; }
			const QName& nodeQName() const override { return qname; }
			std::string nodeValue() const override { return _value; }
			void nodeValue(const std::string& val) override
			{
				if (type != ELEMENT_NODE)
					_value = val;
			}

			NODE_TYPE nodeType() const override { return type; }

			dom::XmlNodePtr parentNode() override { return parent.lock(); }
			inline dom::XmlNodeListPtr childNodes() override;
			dom::XmlNodePtr firstChild() override
			{
				if (!children.size()) return dom::XmlNodePtr();
				return children[0];
			}

			dom::XmlNodePtr lastChild() override
			{
				if (!children.size()) return dom::XmlNodePtr();
				return children[children.size() - 1];
			}

			dom::XmlNodePtr previousSibling() override
			{
				dom::XmlNodePtr par = parent.lock();
				if (!index || !par)
					return dom::XmlNodePtr();

				XmlNodeImplInit* plist = (XmlNodeImplInit*)par->internalData();
				if (!plist) return nullptr;
				return plist->children[index - 1];
			}

			dom::XmlNodePtr nextSibling() override
			{
				dom::XmlNodePtr par = parent.lock();
				if (!par)
					return dom::XmlNodePtr();

				XmlNodeImplInit* plist = (XmlNodeImplInit*)par->internalData();
				if (!plist) return nullptr;

				size_t ndx = index + 1;
				if (ndx >= plist->children.size()) return dom::XmlNodePtr();

				return plist->children[ndx];
			}

			dom::XmlDocumentPtr ownerDocument() override { return document.lock(); }

			size_t indexOf(const XmlNodePtr& node)
			{
				if (!node)
					return children.size();

				XmlNodeImplInit* p = (XmlNodeImplInit*)node->internalData();
				size_t index = p->index;
				if (index < children.size() && children[index] == node)
					return index;

				return children.size(); // same as if node was nullptr
			}

			bool insertBefore(const XmlNodePtr& newChild, const XmlNodePtr& before = nullptr) override
			{
				if (!newChild) return false;
				dom::XmlDocumentPtr doc = newChild->ownerDocument();
				if (!doc || doc != document.lock()) return false;

				size_t index = indexOf(before); // in case newChild == before
				if (!removeFromParent(newChild))
					return false;

				if (newChild->nodeType() == ATTRIBUTE_NODE)
					return ((T*)this)->appendAttr(newChild);

				XmlNodeImplInit* p = (XmlNodeImplInit*)newChild->internalData();
				p->parent = ((T*)this)->shared_from_this();

				auto it = children.begin();
				std::advance(it, index);
				children.insert(it, newChild);

				p->fixQName();

				size_t length = children.size();
				for (size_t i = index; i < length; ++i)
				{
					auto child = children[i];
					XmlNodeImplInit* p = (XmlNodeImplInit*)child->internalData();
					p->index = i;
				}

				return true;
			}
			bool insertBefore(const XmlNodeListPtr& children, const XmlNodePtr& before = nullptr) override
			{
				if (!children)
					return false;

				std::vector<XmlNodePtr> copy;
				copy.reserve(children->length());

				auto this_doc = document.lock();

				for (auto node : list_nodes(children))
				{
					if (!node)
						return false;
					dom::XmlDocumentPtr doc = node->ownerDocument();
					if (!doc || doc != this_doc)
						return false;

					copy.push_back(node);
				}

				size_t index = indexOf(before); // in case any new child == before

				for (auto&& node : copy)
				{
					if (!removeFromParent(node))
						return false;
				}

				auto it = this->children.begin();
				std::advance(it, index);

				for (auto&& node : copy)
				{
					if (node->nodeType() == ATTRIBUTE_NODE)
					{
						if (!((T*)this)->appendAttr(node))
							return false;
						continue;
					}

					XmlNodeImplInit* p = (XmlNodeImplInit*)node->internalData();
					p->parent = ((T*)this)->shared_from_this();
					it = this->children.insert(it, node);
					++it;
					p->fixQName();
				}

				size_t length = this->children.size();
				for (size_t i = index; i < length; ++i)
				{
					auto child = this->children[i];
					XmlNodeImplInit* p = (XmlNodeImplInit*)child->internalData();
					p->index = i;
				}

				return true;
			}

			bool appendChild(const dom::XmlNodePtr& newChild) override
			{
				return insertBefore(newChild);
			}
			bool replaceChild(const XmlNodePtr& newChild, const XmlNodePtr& oldChild) override
			{
				if (!oldChild)
					return false;

				if (!insertBefore(newChild, oldChild))
					return false;

				return removeChild(oldChild);
			}
			bool replaceChild(const XmlNodeListPtr& newChildren, const XmlNodePtr& oldChild) override
			{
				if (!oldChild)
					return false;

				if (!insertBefore(newChildren, oldChild))
					return false;

				return removeChild(oldChild);
			}

			bool removeChild(const XmlNodePtr& child) override
			{
				if (!child)
					return false;

				if (child->nodeType() == dom::ATTRIBUTE_NODE)
					return ((T*)this)->removeAttr(child);

				index = indexOf(child);
				if (index >= children.size())
					return false;

				XmlNodeImplInit* p = (XmlNodeImplInit*)child->internalData();
				p->parent.reset();
				p->index = (size_t)-1;

				auto it = children.begin();
				std::advance(it, index);
				children.erase(it);
				size_t length = children.size();
				for (size_t i = index; i < length; ++i)
				{
					auto child = children[i];
					XmlNodeImplInit* p = (XmlNodeImplInit*)child->internalData();
					p->index = i;
				}

				return true;
			}

			void* internalData() override { return (XmlNodeImplInit*)this; }

			bool appendAttr(const dom::XmlNodePtr& newChild) { return false; }
			bool removeAttr(const dom::XmlNodePtr& child) { return false; }

			XmlNodePtr find(const std::string& path, const Namespaces& ns) override
			{
				return xpath::XPath(path, ns).find(((T*)this)->shared_from_this());
			}
			XmlNodeListPtr findall(const std::string& path, const Namespaces& ns) override
			{
				return xpath::XPath(path, ns).findall(((T*)this)->shared_from_this());
			}
		};

		template <typename T, typename _Interface>
		class XmlChildNodeImpl : public XmlNodeImpl<T, _Interface>
		{
		public:

			typedef XmlNodeImpl<T, _Interface> Super;
			XmlChildNodeImpl(const XmlNodeImplInit& init) : Super(init)
			{
			}

			bool before(const XmlNodePtr& node) override
			{
				auto parent = static_cast<T*>(this)->parentNode();
				if (!parent)
					return false;
				return parent->insertBefore(node, Super::shared_from_this());
			}
			bool before(const std::string& data) override
			{
				auto node = createTextSibling(this, data);
				if (!node) return false;
				return before(node);
			}
			bool before(const XmlNodeListPtr& nodes) override
			{
				auto parent = static_cast<T*>(this)->parentNode();
				if (!parent)
					return false;
				return parent->insertBefore(nodes, Super::shared_from_this());
			}
			bool after(const XmlNodePtr& node) override
			{
				auto parent = static_cast<T*>(this)->parentNode();
				if (!parent)
					return false;
				return parent->insertBefore(node, parent->nextSibling());
			}
			virtual bool after(const std::string& data)
			{
				auto node = createTextSibling(this, data);
				if (!node) return false;
				return after(node);
			}
			bool after(const XmlNodeListPtr& nodes) override
			{
				auto parent = static_cast<T*>(this)->parentNode();
				if (!parent)
					return false;
				return parent->insertBefore(nodes, parent->nextSibling());
			}
			bool replace(const XmlNodePtr& node) override
			{
				auto parent = static_cast<T*>(this)->parentNode();
				if (!parent)
					return false;
				return parent->replaceChild(node, Super::shared_from_this());
			}
			bool replace(const std::string& data) override
			{
				auto node = createTextSibling(this, data);
				if (!node) return false;
				return replace(node);
			}
			bool replace(const XmlNodeListPtr& nodes) override
			{
				auto parent = static_cast<T*>(this)->parentNode();
				if (!parent)
					return false;
				return parent->replaceChild(nodes, Super::shared_from_this());
			}
			bool remove() override
			{
				auto parent = static_cast<T*>(this)->parentNode();
				if (!parent)
					return true; // orphaned nodeas are always removed
				return parent->removeChild(Super::shared_from_this());
			}
		};

		template <typename T, typename _Interface>
		class XmlParentNodeImpl : public XmlChildNodeImpl<T, _Interface>
		{
		public:
			typedef XmlChildNodeImpl<T, _Interface> Super;
			XmlParentNodeImpl(const XmlNodeImplInit& init) : Super(init)
			{
			}

			bool prepend(const XmlNodePtr& node) override
			{
				auto _final = static_cast<T*>(this);
				return _final->insertBefore(node, _final->firstChild());
			}

			bool prepend(const XmlNodeListPtr& nodes) override
			{
				auto _final = static_cast<T*>(this);
				return _final->insertBefore(nodes, _final->firstChild());
			}

			bool prepend(const std::string& data) override
			{
				auto node = createTextSibling(this, data);
				if (!node) return false;
				return prepend(node);
			}

			bool append(const XmlNodePtr& node) override
			{
				return static_cast<T*>(this)->insertBefore(node);
			}

			bool append(const XmlNodeListPtr& nodes) override
			{
				return static_cast<T*>(this)->insertBefore(nodes);
			}

			bool append(const std::string& data) override
			{
				auto node = createTextSibling(this, data);
				if (!node) return false;
				return append(node);
			}
		};

		class XmlNodeList: public dom::XmlNodeList
		{
		public:
			typedef NodesInit Init;
			Init children;

			XmlNodeList(const Init& init): children(init) {}
			~XmlNodeList() {}

			dom::XmlNodePtr item(size_t index) override
			{
				if (index >= children.size()) return dom::XmlNodePtr();
				return children[index];
			}

			size_t length() const override
			{
				return children.size();
			}

			bool remove() override
			{
				auto copy = children;
				for (auto&& node : copy)
				{
					if (!removeFromParent(node))
						return false;
				}

				return true;
			}
		};

		template <typename T, typename _Interface>
		dom::XmlNodeListPtr XmlNodeImpl<T, _Interface>::childNodes()
		{
			try {
			return std::make_shared<XmlNodeList>(children);
			} catch(std::bad_alloc) { return nullptr; }
		}

		class XmlAttribute: public XmlNodeImpl<XmlAttribute, dom::XmlAttribute>
		{
		public:
			XmlAttribute(const Init& init): XmlNodeImpl(init) {}

			dom::XmlNodePtr previousSibling() override
			{
				return false;
			}

			dom::XmlNodePtr nextSibling() override
			{
				return false;
			}
		};

		class XmlText: public XmlChildNodeImpl<XmlText, dom::XmlText>
		{
		public:
			XmlText(const Init& init): XmlChildNodeImpl(init) {}
		};

		class XmlElement: public XmlParentNodeImpl<XmlElement, dom::XmlElement>
		{
		public:
			bool nsRebuilt;
			typedef std::map< std::string, std::string > InternalNamespaces;
			InternalNamespaces namespaces;

			std::map< std::string, dom::XmlAttributePtr > lookup;

			XmlElement(const Init& init): XmlParentNodeImpl(init), nsRebuilt(false) {}

			std::string getAttribute(const std::string& name) override
			{
				std::map< std::string, dom::XmlAttributePtr >::const_iterator
					_it = lookup.find(name);
				if (_it == lookup.end()) return std::string();
				return _it->second->value();
			}

			dom::XmlAttributePtr getAttributeNode(const std::string& name) override
			{
				std::map< std::string, dom::XmlAttributePtr >::const_iterator
					_it = lookup.find(name);
				if (_it == lookup.end()) return dom::XmlAttributePtr();
				return _it->second;
			}

			bool setAttribute(const dom::XmlAttributePtr& attr) override
			{
				XmlNodeImplInit* p = (XmlNodeImplInit*)attr->internalData();
				if (p)
					p->parent = shared_from_this();

				std::map< std::string, dom::XmlAttributePtr >::const_iterator
					_it = lookup.find(attr->name());
				if (_it != lookup.end())
				{
					_it->second->value(attr->value());
					return true;
				}
				lookup[attr->name()] = attr;
				return true;
			}

			bool removeAttribute(const XmlAttributePtr& attr)
			{
				if (!attr)
					return false;
				return removeAttribute(attr->name());
			}

			bool setAttribute(const std::string& attr, const std::string& value)
			{
				auto doc = ownerDocument();
				if (!doc)
					return false;
				auto attribute = doc->createAttribute(attr, value);
				if (!attribute)
					return false;

				return setAttribute(attribute);
			}

			bool removeAttribute(const std::string& attr)
			{
				auto it = lookup.find(attr);
				if (it == lookup.end())
					return false;
				lookup.erase(it);
				return true;
			}

			struct get_1 {
				template<typename K, typename V>
				const V& operator()(const std::pair<K, V>& _item) { return _item.second; }
			};

			dom::XmlNodeListPtr getAttributes() override
			{
				std::vector< dom::XmlNodePtr > out;
				out.reserve(lookup.size());
				std::transform(lookup.begin(), lookup.end(), std::back_inserter(out), get_1());
				return std::make_shared<XmlNodeList>(out);
			}

			bool hasAttribute(const std::string& name) override
			{
				std::map< std::string, dom::XmlAttributePtr >::const_iterator
					_it = lookup.find(name);
				return _it != lookup.end();
			}

			void enumTagNames(const std::string& tagName, std::vector< dom::XmlNodePtr >& out)
			{
				if (tagName == _name) out.push_back(shared_from_this());

				for (auto&& node: children)
				{
					if (node->nodeType() == ELEMENT_NODE)
						((XmlElement*)node.get())->enumTagNames(tagName, out);
				}
			}

			dom::XmlNodeListPtr getElementsByTagName(const std::string& tagName) override
			{
				std::vector< dom::XmlNodePtr > out;
				enumTagNames(tagName, out);
				return std::make_shared<XmlNodeList>(out);
			}

			bool appendAttr(const dom::XmlNodePtr& newChild)
			{
				if (!newChild || newChild->nodeType() != dom::ATTRIBUTE_NODE)
					return false;
				return setAttribute(std::static_pointer_cast<dom::XmlAttribute>(newChild));
			}
			bool removeAttr(const dom::XmlNodePtr& child)
			{
				if (!child || child->nodeType() != dom::ATTRIBUTE_NODE)
					return false;
				return removeAttribute(std::static_pointer_cast<dom::XmlAttribute>(child));
			}

			std::string innerText() override
			{
				//special case:
				if (children.size() == 1 && children[0] && children[0]->nodeType() == TEXT_NODE)
					return children[0]->nodeValue();

				std::string out;

				for (auto&& node: children)
				{
					if (node)
						switch(node->nodeType())
						{
						case TEXT_NODE: out += node->nodeValue(); break;
						case ELEMENT_NODE:
							out += std::static_pointer_cast<dom::XmlElement>(node)->innerText();
							break;
						};
				}

				return out;
			}

			void fixQName(bool forElem = true) override
			{
				XmlNodeImpl<XmlElement, dom::XmlElement>::fixQName(forElem);
				if (!forElem) return;
				for (auto&& pair: lookup)
				{
					if (strncmp(pair.first.c_str(), "xmlns", 5) == 0 &&
						(pair.first.length() == 5 || pair.first[5] == ':'))
					{
						continue;
					}
					XmlNodeImplInit* p = (XmlNodeImplInit*)pair.second->internalData();
					if (!p) continue;
					p->fixQName(false);
				};
			}

			void fixQName(QName& qname, const std::string& ns, const std::string& localName) override
			{
				if (!nsRebuilt)
				{
					nsRebuilt = true;
					namespaces.clear();
					for (auto&& pair: lookup)
					{
						if (strncmp(pair.first.c_str(), "xmlns", 5) != 0) continue;
						if (pair.first.length() != 5 && pair.first[5] != ':') continue;
						if (pair.first.length() == 5) namespaces[""] = pair.second->value();
						else namespaces[std::string(pair.first.c_str() + 6)] = pair.second->value();
					};
				}
				InternalNamespaces::const_iterator _it = namespaces.find(ns);
				if (_it != namespaces.end())
				{
					qname.nsName = _it->second;
					qname.localName = localName;
					return;
				}
				XmlNodeImpl<XmlElement, dom::XmlElement>::fixQName(qname, ns, localName);
			}
		};

		class XmlDocumentFragment : public XmlParentNodeImpl<XmlDocumentFragment, dom::XmlDocumentFragment>
		{
			void enumTagNames(const std::string& tagName, std::vector< dom::XmlNodePtr >& out)
			{
				for (auto&& node : children)
				{
					if (node->nodeType() == ELEMENT_NODE)
						((XmlElement*)node.get())->enumTagNames(tagName, out);
				}
			}

		public:
			XmlDocumentFragment(const Init& init) : XmlParentNodeImpl(init) {}

			dom::XmlNodeListPtr getElementsByTagName(const std::string& tagName) override
			{
				std::vector< dom::XmlNodePtr > out;
				enumTagNames(tagName, out);
				return std::make_shared<XmlNodeList>(out);
			}
		};

		class XmlDocument: public dom::XmlDocument, public std::enable_shared_from_this<XmlDocument>
		{
			QName m_qname;
			dom::XmlElementPtr root;
			dom::XmlDocumentFragmentPtr fragment;

			friend XmlDocumentPtr dom::XmlDocument::create();
		public:

			XmlDocument()
			{
				m_qname.localName = "#document";
			}
			~XmlDocument() {}

			std::string nodeName() const override { return m_qname.localName; }
			const QName& nodeQName() const override { return m_qname; }
			std::string nodeValue() const override { return std::string(); }
			void nodeValue(const std::string&) override {}
			NODE_TYPE nodeType() const override { return DOCUMENT_NODE; }

			XmlNodePtr parentNode() override { return nullptr; }
			XmlNodePtr firstChild() override { return documentElement(); }
			XmlNodePtr lastChild() override { return documentElement(); }
			XmlNodePtr previousSibling() override { return nullptr; }
			XmlNodePtr nextSibling() override { return nullptr; }
			XmlNodeListPtr childNodes() override
			{
				if (fragment)
					return fragment->childNodes();

				if (!root)
					return nullptr;

				try {
					NodesInit children;
					children.push_back(root);
					return std::make_shared<XmlNodeList>(children);
				} catch(std::bad_alloc) { return nullptr; }
			}

			XmlDocumentPtr ownerDocument() override { return shared_from_this(); }
			bool insertBefore(const XmlNodePtr& child, const XmlNodePtr& before = nullptr) override { return false; }
			bool insertBefore(const XmlNodeListPtr& children, const XmlNodePtr& before = nullptr) override { return false; }
			bool appendChild(const XmlNodePtr& newChild) override { return false; }
			bool replaceChild(const XmlNodePtr& newChild, const XmlNodePtr& oldChild) override { return false; }
			bool replaceChild(const XmlNodeListPtr& newChildren, const XmlNodePtr& oldChild) override { return false; }
			bool removeChild(const XmlNodePtr& child) override { return false; }
			void* internalData() override { return nullptr; }

			dom::XmlElementPtr documentElement() override { return root; }
			void setDocumentElement(const dom::XmlElementPtr& elem) override
			{
				root = elem;
				fragment = nullptr;
				if (elem)
					((XmlNodeImplInit*)elem->internalData())->fixQName();
			}
			dom::XmlDocumentFragmentPtr associatedFragment() override { return fragment; }
			void setFragment(const XmlDocumentFragmentPtr& f) override
			{
				root = nullptr;
				fragment = f;
				if (f)
					((XmlNodeImplInit*)f->internalData())->fixQName();
			}
			dom::XmlElementPtr createElement(const std::string& tagName) override
			{
				XmlNodeImplInit init;
				init.type = ELEMENT_NODE;
				init._name = tagName;
				//init._value;
				init.document = shared_from_this();
				init.index = 0;
				return std::make_shared<XmlElement>(init);
			}

			dom::XmlTextPtr createTextNode(const std::string& data) override
			{
				XmlNodeImplInit init;
				init.type = TEXT_NODE;
				//init._name;
				init._value = data;
				init.document = shared_from_this();
				init.index = 0;
				return std::make_shared<XmlText>(init);
			}

			dom::XmlAttributePtr createAttribute(const std::string& name, const std::string& value) override
			{
				XmlNodeImplInit init;
				init.type = ATTRIBUTE_NODE;
				init._name = name;
				init._value = value;
				init.document = shared_from_this();
				init.index = 0;
				return std::make_shared<XmlAttribute>(init);
			}

			dom::XmlDocumentFragmentPtr createDocumentFragment() override
			{
				XmlNodeImplInit init;
				init.type = DOCUMENT_FRAGMENT_NODE;
				init._name = "#document-fragment";
				init.document = shared_from_this();
				init.index = 0;
				return std::make_shared<XmlDocumentFragment>(init);
			}

			dom::XmlNodeListPtr getElementsByTagName(const std::string& tagName) override
			{
				if (root)
					return root->getElementsByTagName(tagName);
				if (fragment)
					return fragment->getElementsByTagName(tagName);
				return nullptr;
			}

			dom::XmlElementPtr getElementById(const std::string& elementId) override
			{
				return nullptr;
			}

			XmlNodePtr find(const std::string& path, const Namespaces& ns) override
			{
				return xpath::XPath(path, ns).find(shared_from_this());
			}
			XmlNodeListPtr findall(const std::string& path, const Namespaces& ns) override
			{
				return xpath::XPath(path, ns).findall(shared_from_this());
			}
		};
	}

	XmlDocumentPtr XmlDocument::create()
	{
		return std::make_shared<impl::XmlDocument>();
	}

	class DOMParser: public xml::ExpatBase<DOMParser>
	{
		dom::XmlElementPtr elem;
		std::string text;

		void addText()
		{
			if (text.empty()) return;
			if (elem)
				elem->appendChild(doc->createTextNode(text));
			text.clear();
		}
	public:

		dom::XmlDocumentPtr doc;

		bool create(const char* cp)
		{
			doc = dom::XmlDocument::create();
			if (!doc) return false;
			return xml::ExpatBase<DOMParser>::create(cp);
		}

		void onStartElement(const XML_Char *name, const XML_Char **attrs)
		{
			addText();
			auto current = doc->createElement(name);
			if (!current) return;

			for (; *attrs; attrs += 2)
				current->setAttribute(attrs[0], attrs[1]);

			if (elem)
				elem->appendChild(current);
			else
				doc->setDocumentElement(current);
			elem = current;
		}

		void onEndElement(const XML_Char *name)
		{
			addText();
			if (!elem) return;
			dom::XmlNodePtr node = elem->parentNode();
			elem = std::static_pointer_cast<dom::XmlElement>(node);
		}

		void onCharacterData(const XML_Char *pszData, int nLength)
		{
			text += std::string(pszData, nLength);
		}
	};

	XmlDocumentPtr XmlDocument::fromFile(const char* path)
	{
		DOMParser parser;
		if (!parser.create(nullptr)) return nullptr;
		parser.enableElementHandler();
		parser.enableCharacterDataHandler();

		FILE* f = fopen(path, "rb");
		if (!f)
			return nullptr;

		char buffer[8192];
		size_t read;
		while ((read = fread(buffer, 1, sizeof(buffer), f)) > 0)
		{
			if (!parser.parse(buffer, read, false))
			{
				fclose(f);
				return nullptr;
			}
		}
		fclose(f);

		if (!parser.parse(buffer, 0))
			return nullptr;

		return parser.doc;
	}

	void Print(const dom::XmlNodeListPtr& subs, bool ignorews, size_t depth)
	{
		if (subs)
		{
			size_t count = subs->length();
			for(size_t i = 0; i < count; ++i)
				Print(subs->item(i), ignorews, depth);
		}
	}

	template <typename T>
	inline std::string qName(std::shared_ptr<T> ptr)
	{
		QName qname = ptr->nodeQName();
		if (qname.nsName.empty()) return qname.localName;
		return "{" + qname.nsName + "}" + qname.localName;
	}

	void Print(const dom::XmlNodePtr& node, bool ignorews, size_t depth)
	{
		dom::XmlNodeListPtr subs = node->childNodes();

		NODE_TYPE type = node->nodeType();
		std::string out;
		for (size_t i = 0; i < depth; ++i) out += "    ";

		if (type == TEXT_NODE)
		{
			std::string val = node->nodeValue();
			if (ignorews)
			{
				size_t lo = 0, hi = val.length();
				while (lo < hi && val[lo] && isspace((unsigned char)val[lo])) lo++;
				while (lo < hi && isspace((unsigned char)val[hi-1])) hi--;
				val = val.substr(lo, hi - lo);
				if (val.empty()) return;
			}
			if (val.length() > 80)
				val = val.substr(0, 77) + "[...]";
			out += "# " + val;
		}
		else if (type == ELEMENT_NODE)
		{
			std::string sattrs;
			auto e = std::static_pointer_cast<dom::XmlElement>(node);
			dom::XmlNodeListPtr attrs;
			if (e) attrs = e->getAttributes();
			if (attrs)
			{
				size_t count = attrs->length();
				for(size_t i = 0; i < count; ++i)
				{
					dom::XmlNodePtr node = attrs->item(i);
					sattrs += " " + qName(node) + "='" + node->nodeValue() + "'";
				}
			}

			if (subs && subs->length() == 1)
			{
				dom::XmlNodePtr sub = subs->item(0);
				if (sub && sub->nodeType() == TEXT_NODE)
				{
					out += qName(node);
					if (!sattrs.empty())
						out += "[" + sattrs + " ]";

					std::string val = sub->nodeValue();
					if (ignorews)
					{
						size_t lo = 0, hi = val.length();
						while (val[lo] && isspace((unsigned char)val[lo])) lo++;
						while (lo < hi && isspace((unsigned char)val[hi-1])) hi--;
						val = val.substr(lo, hi - lo);
						if (val.empty()) return;
					}

					if (val.length() > 80)
						val = val.substr(0, 77) + "[...]";

					out += ": " + val + "\n";
					fprintf(stderr, "%s", out.c_str());
#ifdef WIN32
					OutputDebugStringA(out.c_str());
#endif
					return;
				}
			}
			if (!subs || subs->length() == 0)
			{
					out += qName(node);
					if (!sattrs.empty())
						out += "[" + sattrs + " ]";
					out += "\n";
					fprintf(stderr, "%s", out.c_str());
#ifdef WIN32
					OutputDebugStringA(out.c_str());
#endif
					return;
			}
			out += "<" + qName(node) + sattrs + ">";
		}
		out += "\n";
		fprintf(stderr, "%s", out.c_str());
#ifdef WIN32
		OutputDebugStringA(out.c_str());
#endif

		Print(subs, ignorews, depth+1);
	}

	XmlNodeListPtr createList(const std::vector<XmlNodePtr>& list)
	{
		try {
		return std::make_shared<impl::XmlNodeList>(list);
		} catch(std::bad_alloc) { return nullptr; }
	}
}
