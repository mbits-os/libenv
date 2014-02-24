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

#ifndef __FILTER_HPP__
#define __FILTER_HPP__

#include <memory>
#include <sstream>
#include <string>
#include <utils.hpp>

namespace filter
{
	SHAREABLE(Filter);
	SHAREABLE(BufferingFilter);

	class Filter
	{
		FilterPtr m_downstream;
	protected:
		void next(char c)
		{
			if (m_downstream)
				m_downstream->onChar(c);
		}
		void next(const char *c)
		{
			if (!m_downstream)
				return;
			if (!c || !*c)
				return;
			while (*c)
				m_downstream->onChar(*c++);
		}
	public:
		virtual ~Filter() {}
		virtual void pipe(const FilterPtr& downstream) { m_downstream = downstream; }
		virtual void onChar(char c) = 0;
		virtual void bodyEnd()
		{
			if (m_downstream)
				m_downstream->bodyEnd();
		}
		virtual void close()
		{
			if (m_downstream)
				m_downstream->close();
		}
		void put(char c)
		{
			onChar(c);
		}

		void put(const char *c)
		{
			if (!c || !*c)
				return;
			while (*c)
				onChar(*c++);
		}

		void put(const std::string& s)
		{
			for (char c: s)
				onChar(c);
		}
	};

	class BufferingFilter: public Filter
	{
		std::string m_buffer;
	public:
		BufferingFilter() {}
		void onChar(char c) override { m_buffer.push_back(c); }
		std::string::size_type size() { return m_buffer.size(); }
		void flush()
		{
			next(m_buffer.c_str());
			m_buffer.clear();
		}
	};

 	class OutputStreamFilter: public Filter
	{
		std::ostreambuf_iterator<char> m_iter;
	public:
		OutputStreamFilter(std::ostream& out): m_iter(out) {}
		void onChar(char c) override { *m_iter++ = c; }
	};

	class StringFilter: public Filter
	{
		std::string m_out;
	public:
		StringFilter() {}
		void onChar(char c) override { m_out.push_back(c); }
		std::string& str() { return m_out; }
	};

	class FileDescriptor
	{
		int m_fd;
	public:
		FileDescriptor() = delete;
		FileDescriptor(const FileDescriptor&) = delete;
		FileDescriptor& operator = (const FileDescriptor&) = delete;

		explicit FileDescriptor(int fd): m_fd(fd) {}
		FileDescriptor(FileDescriptor&& right)
		{
			m_fd = right.m_fd;
			right.m_fd = 0;
		}
		~FileDescriptor() { close(); }

		void set(int fd) { close(); m_fd = fd; }
		FileDescriptor& operator = (FileDescriptor&& rhs)
		{
			int old = m_fd;
			m_fd = rhs.m_fd;
			rhs.m_fd = old;
			return *this;
		}
		size_t read(void* buffer, size_t len);
		size_t write(const void* buffer, size_t len);
		void close();
	};

	struct Pipe
	{
		Pipe(): reader(0), writer(0) {}
		FileDescriptor reader, writer;
		bool open();
	};

	class FdFilter: public Filter
	{
		FileDescriptor m_fd;
	public:
		explicit FdFilter(FileDescriptor&& fd): m_fd(std::move(fd)) {}
		void onChar(char c) override { m_fd.write(&c, 1); }
		void close() override { m_fd.close(); }
	};


	template <typename Final>
	struct Base64Encoder
	{
		unsigned long m_accumulator;
		size_t m_ptr;
		Base64Encoder(): m_accumulator(0), m_ptr(0) {}
		void output(char c) {}
		void output(unsigned long value)
		{
			static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
			static_cast<Final*>(this)->output(alphabet[(size_t)value]);
		}
		void encode(char c)
		{
			m_accumulator <<= 8;
			m_accumulator |= (unsigned char)c;
			m_ptr += 8;
			while (m_ptr >= 6)
			{
				m_ptr -= 6;
				unsigned long value = m_accumulator >> m_ptr;
				value &= 0x3F;
				output(value);
			}
		}

		void close()
		{
			if (m_ptr != 0)
			{
				m_accumulator <<= (6 - m_ptr);
				m_accumulator &= 0x3F;
				output(m_accumulator);
				if (m_ptr != 0) output('='); // 2 and 4
				if (m_ptr == 2) output('='); // only 2
			}
		}
	};

	// to have BASE64-encoded block, one should
	// ... | Base64 | Base64Block | ...

	class Base64: public Filter, public Base64Encoder<Base64>
	{
		struct StringEncoder: Base64Encoder<StringEncoder>
		{
			std::string m_sink;
			void output(char c) { m_sink.push_back(c); }
			std::string encode(const std::string& in)
			{
				// The size for the encoded value is in.size * 4.0 / 3 (ceiled);
				m_sink.reserve(((in.size() + 2) / 3) * 4);

				for (char c : in) Base64Encoder<StringEncoder>::encode(c);
				close();

				return std::move(m_sink);
			}
		};
		struct HeaderEncoder: Base64Encoder<HeaderEncoder>
		{
			std::string m_sink;
			void output(char c) { m_sink.push_back(c); }
			std::string encode(const std::string& in)
			{
				static const char pre[] = "=?UTF-8?B?";
				static const char post[] = "?=";

				// The size for the encoded value is in.size * 4.0 / 3 (ceiled);
				// the rest is for the prefix and postfix
				m_sink.reserve(((in.size() + 2) / 3) * 4 + sizeof(pre) + sizeof(post));

				m_sink.append(pre);
				for (char c : in) Base64Encoder<HeaderEncoder>::encode(c);
				close();
				m_sink.append(post);

				return std::move(m_sink);
			}
		};
	public:
		void onChar(char c) override { Base64Encoder<Base64>::encode(c); }
		void output(char c) { next(c); }
		static std::string encode(const std::string& message)
		{
			return std::move(StringEncoder().encode(message));
		}
		static std::string header(const std::string& in, const std::string& additional = std::string())
		{
			for (char test: additional)
			{
				std::string::size_type pos = in.find(test);
				if (pos == std::string::npos)
					continue;
				return std::move(HeaderEncoder().encode(in));
			}
			for (char c: in)
			{
				if ((unsigned char)c < 0x80)
					continue;
				return std::move(HeaderEncoder().encode(in));
			}
			return in;
		}
	};

	class Base64Block: public Filter
	{
		size_t m_ptr;
	public:
		Base64Block(): m_ptr(0) {}
		void onChar(char c) override;
		void bodyEnd() override;
	};

	// to have fully quoted printable, one should
	// ... | Quoting | QuotedPrintable | ...

	class Quoting: public Filter
	{
	public:
		void onChar(char c) override;
	};

	class QuotedPrintable: public Filter
	{
		size_t m_line;
		size_t m_ptr;
	public:
		QuotedPrintable(): m_line(77), m_ptr(0) {}
		void onChar(char c) override;
	};

}

#endif // __FILTER_HPP__
