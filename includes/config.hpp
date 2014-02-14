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
#ifndef __CONFIG_HPP__
#define __CONFIG_HPP__

#include <string>
#include <memory>

namespace config
{
	namespace base
	{
		struct section
		{
			virtual ~section() {}
			virtual bool has_value(const std::string& name) const = 0;
			virtual void set_value(const std::string& name, const std::string& svalue) = 0;
			virtual void set_value(const std::string& name, int ivalue) = 0;
			virtual void set_value(const std::string& name, bool bvalue) = 0;
			virtual std::string get_string(const std::string& name, const std::string& def_val) const = 0;
			virtual int get_int(const std::string& name, int def_val) const = 0;
			virtual bool get_bool(const std::string& name, bool def_val) const = 0;
		};
		typedef std::shared_ptr<section> section_ptr;

		struct config
		{
			virtual ~config() {}
			virtual section_ptr get_section(const std::string& name) = 0;
		};
		typedef std::shared_ptr<config> config_ptr;

		config_ptr file_config(const std::string&, bool);
	}

	namespace wrapper
	{
		template <typename T> struct get_value;
		template <typename Value> struct setting;

		struct section
		{
			template <typename Value> friend struct get_value;
			template <typename Value> friend struct setting;

			base::config_ptr m_impl;
			mutable base::section_ptr m_section;
			std::string m_section_name;

			section(const base::config_ptr& impl, const std::string& section)
				: m_impl(impl)
				, m_section_name(section)
			{}

		private:
			bool has_value(const std::string& name) const
			{
				ensure_section();
				return m_section->has_value(name);
			}
			void set_value(const std::string& name, const std::string& svalue)
			{
				ensure_section();
				return m_section->set_value(name, svalue);
			}
			void set_value(const std::string& name, int ivalue)
			{
				ensure_section();
				return m_section->set_value(name, ivalue);
			}
			void set_value(const std::string& name, bool bvalue)
			{
				ensure_section();
				return m_section->set_value(name, bvalue);
			}
			std::string get_string(const std::string& name, const std::string& def_val) const
			{
				ensure_section();
				return m_section->get_string(name, def_val);
			}
			int get_int(const std::string& name, int def_val) const
			{
				ensure_section();
				return m_section->get_int(name, def_val);
			}
			bool get_bool(const std::string& name, bool def_val) const
			{
				ensure_section();
				return m_section->get_bool(name, def_val);
			}
			void ensure_section() const
			{
				if (!m_section)
					m_section = m_impl->get_section(m_section_name);
				if (!m_section)
					throw std::runtime_error("Section " + m_section_name + " was not created");
			}
		};

		template <>
		struct get_value<std::string>
		{
			static std::string helper(section& sec, const std::string& name, const std::string& def_val)
			{
				return sec.get_string(name, def_val);
			}
		};

		template <>
		struct get_value<int>
		{
			static int helper(section& sec, const std::string& name, int def_val)
			{
				return sec.get_int(name, def_val);
			}
		};

		template <>
		struct get_value<bool>
		{
			static bool helper(section& sec, const std::string& name, bool def_val)
			{
				return sec.get_bool(name, def_val);
			}
		};

		template <typename Value>
		struct setting
		{
			typedef setting<Value> my_type;
			section& m_parent;
			std::string m_name;
			Value m_def_val;

			setting(section& parent, const std::string& name, const Value& def_val = Value())
				: m_parent(parent)
				, m_name(name)
				, m_def_val(def_val)
			{}

			bool is_set() const
			{
				return m_parent.has_value(m_name);
			}

			operator Value() const
			{
				return get_value<Value>::helper(m_parent, m_name, m_def_val);
			}

			my_type& operator = (const Value& v)
			{
				m_parent.set_value(m_name, v);
				return *this;
			}
		};

		template <typename Value>
		std::ostream& operator << (std::ostream& o, const setting<Value>& rhs)
		{
			return o << static_cast<Value>(rhs);
		}
	}
}

#endif // __LOG_HPP__