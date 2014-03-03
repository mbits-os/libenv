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

#ifndef __FORMS_BASE_CONTROL_H__
#define __FORMS_BASE_CONTROL_H__

#include <map>
#include <string>
#include <functional>

namespace FastCGI {
	class Control;
	using ControlPtr = std::shared_ptr<Control>;
	using Controls = std::list<ControlPtr>;

	class Request;
	inline std::ostream& req_ostream(Request&);

	struct BasicRenderer;
	using ChildrenCallback = std::function<void(Request&, BasicRenderer&)>;

#ifndef REQUEST_OSTREAM
#define REQUEST_OSTREAM
	template <typename T> inline Request& operator << (Request& r, T&& in) { req_ostream(r) << in; return r; }
#endif

	typedef std::map<std::string, std::string> Strings;

	class ControlBase
	{
	protected:
		std::string m_name;
		std::string m_label;
		std::string m_hint;
		std::string m_value;
		bool        m_userValue;
		Strings     m_attrs;
		bool        m_hasError;

	public:
		virtual void getControlString(Request& request, BasicRenderer& renderer) = 0;
		virtual void getSimpleControlString(Request& request, BasicRenderer& renderer) { getControlString(request, renderer); }
		virtual bool getLabelString(Request& request, BasicRenderer& renderer) = 0;
		virtual bool getHintString(Request& request, BasicRenderer& renderer) = 0;

		void getAttributes(Request& request, BasicRenderer& renderer);
		void getElement(Request& request, BasicRenderer& renderer, const std::string& name);
		void getElement(Request& request, BasicRenderer& renderer, const std::string& name, const std::string& content);
		void getElement(Request& request, BasicRenderer& renderer, const std::string& name, const ChildrenCallback& op);

	public:
		ControlBase(const std::string& name, const std::string& label, const std::string& hint)
			: m_name(name)
			, m_label(label)
			, m_hint(hint)
			, m_userValue(false)
			, m_hasError(false)
		{
			if (!name.empty())
			{
				setAttr("name", name);
				setAttr("id", name);
			}
		}

		virtual ~ControlBase() {}

		virtual ControlBase* findControl(const std::string& name)
		{
			if (m_name == name) return this;
			return nullptr;
		}

		void setAttr(const std::string& key, const std::string& value)
		{
			m_attrs[key] = value;
		}

		std::string getAttr(const std::string& key)
		{
			auto it = m_attrs.find(key);
			if (it == m_attrs.end())
				return std::string();
			return it->second;
		}

		void removeAttr(const std::string& key)
		{
			auto it = m_attrs.find(key);
			if (it != m_attrs.end())
				m_attrs.erase(it);
		}

		virtual void bindData(Request& request, const Strings& data);
		virtual void bindUI()
		{
			if (!m_value.empty())
				setAttr("value", m_value);
		}
		void bind(Request& request, const Strings& data)
		{
			m_userValue = false;
			m_value.erase();
			bindData(request, data);
			bindUI();
		}

		virtual void render(Request& request, BasicRenderer& renderer) = 0;
		virtual void renderSimple(Request& request, BasicRenderer& renderer) = 0;

		bool hasUserData() const { return m_userValue && !m_value.empty(); }
		const std::string& getData() const { return m_value; }
		void setError(bool val = true) { m_hasError = val; }
	};

} // FastCGI

#endif // __FORMS_BASE_CONTROL_H__
