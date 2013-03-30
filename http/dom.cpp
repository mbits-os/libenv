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
#include <vector>
#include <iterator>

namespace dom
{
	namespace impl
	{
		struct XmlNodeImplInit
		{
			NODE_TYPE type;
			std::string _name, _value;
			std::vector<dom::XmlNodePtr> children;
			std::weak_ptr<dom::XmlDocument> document;
			std::weak_ptr<dom::XmlNode> parent;
			std::weak_ptr<dom::XmlNode> self;
			size_t index;
		};

		template <typename T, typename _Interface>
		class XmlNodeImpl: public _Interface, public XmlNodeImplInit
		{
		public:

			typedef XmlNodeImplInit Init;
			typedef _Interface Interface;

			XmlNodeImpl(const Init& init): Init(init) {}

			std::string nodeName() const { return _name; }
			std::string nodeValue() const { return _value; }
			void nodeValue(const std::string& val)
			{
				if (type != ELEMENT_NODE)
					_value = val;
			}

			NODE_TYPE nodeType() const { return type; }

			dom::XmlNodePtr parentNode() { return parent.lock(); }
			inline dom::XmlNodeListPtr childNodes();
			dom::XmlNodePtr firstChild()
			{
				if (!children.size()) return dom::XmlNodePtr();
				return children[0];
			}

			dom::XmlNodePtr lastChild()
			{
				if (!children.size()) return dom::XmlNodePtr();
				return children[children.size() - 1];
			}

			dom::XmlNodePtr previousSibling()
			{
				dom::XmlNodePtr par = parent.lock();
				if (!index || !par.get())
					return dom::XmlNodePtr();

				XmlNodeImplInit* plist = (XmlNodeImplInit*)par->internalData();
				return plist->children[index - 1];
			}

			dom::XmlNodePtr nextSibling()
			{
				dom::XmlNodePtr par = parent.lock();
				if (!par.get())
					return dom::XmlNodePtr();

				XmlNodeImplInit* plist = (XmlNodeImplInit*)par->internalData();

				size_t ndx = index + 1;
				if (ndx >= plist->children.size()) return dom::XmlNodePtr();

				return plist->children[ndx];
			}

			dom::XmlDocumentPtr ownerDocument() { return document.lock(); }
			bool appendChild(dom::XmlNodePtr newChild)
			{
				if (!newChild) return false;
				dom::XmlDocumentPtr doc = newChild->ownerDocument();
				if (!doc.get() || doc.get() != document.lock().get()) return false;

				if (newChild->nodeType() == ATTRIBUTE_NODE)
					return ((T*)this)->appendAttr(newChild);

				XmlNodeImplInit* p = (XmlNodeImplInit*)newChild->internalData();
				p->parent = self;
				p->index = children.size();
				children.push_back(newChild);

				return true;
			}

			void* internalData() { return (XmlNodeImplInit*)this; }

			bool appendAttr(dom::XmlNodePtr newChild) { return false; }
		};

		class XmlNodeList: public dom::XmlNodeList
		{
		public:
			typedef std::vector< dom::XmlNodePtr > Init;
			Init children;

			XmlNodeList(const Init& init): children(init) {}
			~XmlNodeList() {}

			dom::XmlNodePtr item(size_t index)
			{
				if (index >= children.size()) return dom::XmlNodePtr();
				return children[index];
			}

			size_t length() const
			{
				return children.size();
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

			dom::XmlNodePtr previousSibling()
			{
				return false;
			}

			dom::XmlNodePtr nextSibling()
			{
				return false;
			}
		};

		class XmlText: public XmlNodeImpl<XmlText, dom::XmlText>
		{
		public:
			XmlText(const Init& init): XmlNodeImpl(init) {}
		};

		class XmlElement: public XmlNodeImpl<XmlElement, dom::XmlElement>
		{
		public:
			//std::vector< dom::XmlAttributePtr > attrs;
			std::map< std::string, dom::XmlAttributePtr > lookup;

			XmlElement(const Init& init): XmlNodeImpl(init) {}

			std::string getAttribute(const std::string& name)
			{
				std::map< std::string, dom::XmlAttributePtr >::const_iterator
					_it = lookup.find(name);
				if (_it == lookup.end()) return std::string();
				return _it->second->value();
			}

			dom::XmlAttributePtr getAttributeNode(const std::string& name)
			{
				std::map< std::string, dom::XmlAttributePtr >::const_iterator
					_it = lookup.find(name);
				if (_it == lookup.end()) return dom::XmlAttributePtr();
				return _it->second;
			}

			bool setAttribute(dom::XmlAttributePtr attr)
			{
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

			struct get_1 {
				template<typename K, typename V>
				const V& operator()(const std::pair<K, V>& _item) { return _item.second; }
			};

			dom::XmlNodeListPtr getAttributes()
			{
				std::vector< dom::XmlNodePtr > out;
				std::transform(lookup.begin(), lookup.end(), std::back_inserter(out), get_1());
				return std::make_shared<XmlNodeList>(out);
			}

			bool hasAttribute(const std::string& name)
			{
				std::map< std::string, dom::XmlAttributePtr >::const_iterator
					_it = lookup.find(name);
				return _it != lookup.end();
			}

			void enumTagNames(const std::string& tagName, std::vector< dom::XmlNodePtr >& out)
			{
				if (tagName == _name) out.push_back(self.lock());
				std::vector< dom::XmlNodePtr >::const_iterator
					_cur = children.begin(), _end = children.end();

				for (; _cur != _end; ++_cur)
				{
					const dom::XmlNodePtr& node = *_cur;
					if (node->nodeType() == ELEMENT_NODE)
						((XmlElement*)node.get())->enumTagNames(tagName, out);
				}
			}

			dom::XmlNodeListPtr getElementsByTagName(const std::string& tagName)
			{
				std::vector< dom::XmlNodePtr > out;
				enumTagNames(tagName, out);
				return std::make_shared<XmlNodeList>(out);
			}

			bool appendAttr(dom::XmlNodePtr newChild)
			{
				if (!newChild.get() || newChild->nodeType() != dom::ATTRIBUTE_NODE)
					return false;
				return setAttribute(std::static_pointer_cast<dom::XmlAttribute>(newChild));
			}

			std::string innerText()
			{
				//special case:
				if (children.size() == 1 && children[0] && children[0]->nodeType() == TEXT_NODE)
					return children[0]->nodeValue();

				std::string out;

				std::vector< dom::XmlNodePtr >::const_iterator
					_cur = children.begin(), _end = children.end();

				for (; _cur != _end; ++_cur)
				{
					auto node = *_cur;
					if (node.get())
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
		};

		class XmlDocument: public dom::XmlDocument
		{
			dom::XmlElementPtr root;
			std::weak_ptr<dom::XmlDocument> self;

			friend XmlDocumentPtr dom::XmlDocument::create();
		public:

			XmlDocument() {}
			~XmlDocument() {}

			dom::XmlElementPtr documentElement() { return root; }
			void setDocumentElement(dom::XmlElementPtr elem) { root = elem; }
			dom::XmlElementPtr createElement(const std::string& tagName)
			{
				XmlNodeImplInit init;
				init.type = ELEMENT_NODE;
				init._name = tagName;
				//init._value;
				init.document = self;
				init.index = 0;
				auto out = std::make_shared<XmlElement>(init);
				((XmlNodeImplInit*)out->internalData())->self = out;
				return out;
			}

			dom::XmlTextPtr createTextNode(const std::string& data)
			{
				XmlNodeImplInit init;
				init.type = TEXT_NODE;
				//init._name;
				init._value = data;
				init.document = self;
				init.index = 0;
				auto out = std::make_shared<XmlText>(init);
				((XmlNodeImplInit*)out->internalData())->self = out;
				return out;
			}

			dom::XmlAttributePtr createAttribute(const std::string& name, const std::string& value)
			{
				XmlNodeImplInit init;
				init.type = ATTRIBUTE_NODE;
				init._name = name;
				init._value = value;
				init.document = self;
				init.index = 0;
				auto out = std::make_shared<XmlAttribute>(init);
				((XmlNodeImplInit*)out->internalData())->self = out;
				return out;
			}

			dom::XmlNodeListPtr getElementsByTagName(const std::string& tagName)
			{
				if (!root) return dom::XmlNodeListPtr();
				return root->getElementsByTagName(tagName);
			}

			dom::XmlElementPtr getElementById(const std::string& elementId)
			{
				return nullptr;
			}
		};
	}

	dom::XmlDocumentPtr dom::XmlDocument::create()
	{
		auto out = std::make_shared<impl::XmlDocument>();
		out->self = out;
		return out;
	}

	void Print(dom::XmlNodeListPtr subs, bool ignorews, size_t depth)
	{
		if (subs)
		{
			size_t count = subs->length();
			for(size_t i = 0; i < count; ++i)
				Print(subs->item(i), ignorews, depth);
		}
	}

	void Print(dom::XmlNodePtr node, bool ignorews, size_t depth)
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
				while (val[lo] && isspace(val[lo])) lo++;
				while (lo < hi && isspace(val[hi-1])) hi--;
				val = val.substr(lo, hi - lo);
				if (val.empty()) return;
			}
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
					sattrs += " " + node->nodeName() + "='" + node->nodeValue() + "'";
				}
			}

			if (subs && subs->length() == 1)
			{
				dom::XmlNodePtr sub = subs->item(0);
				if (sub && sub->nodeType() == TEXT_NODE)
				{
					out += node->nodeName();
					if (!sattrs.empty())
						out += "[" + sattrs + " ]";
					out += ": " + sub->nodeValue() + "\n";
					fprintf(stderr, "%s", out.c_str());
#ifdef WIN32
					OutputDebugStringA(out.c_str());
#endif
					return;
				}
			}
			out += "<" + node->nodeName() + sattrs + ">";
		}
		out += "\n";
		fprintf(stderr, "%s", out.c_str());
#ifdef WIN32
		OutputDebugStringA(out.c_str());
#endif

		Print(subs, ignorews, depth+1);
	}
}
