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
#include "wiki_parser.hpp"
#include "wiki_nodes.hpp"
#include <filesystem.hpp>
#include <fcntl.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace wiki
{
	namespace binary
	{
		static constexpr unsigned long MAGIC   = 0x494B4957; // "WIKI";
		static constexpr unsigned long VERSION = 0x00010000; // 1.0;

		template <typename T>
		struct unique_array
		{
			T* ptr = nullptr;
			size_t size = 0;
			bool resize(size_t size)
			{
				//profile::probe p{ "unique_array::realloc" };
				T* tmp = (T*)realloc(ptr, size);
				if (!tmp)
					return false;

				ptr = tmp;
				this->size = size;
				return true;
			}
			bool grow()
			{
				if (!size)
					return resize(10240);
				return resize(size << 1);
			}
			~unique_array() { free(ptr); }
		};

		struct FD
		{
			int fd;
			FD(int fd) : fd(fd) {}
			~FD() { if (fd != -1) close(fd); }

			explicit operator bool() const { return fd >= 0; }
			size_t read(void* ptr, size_t len) const { return ::read(fd, ptr, len); }
			size_t write(const void* ptr, size_t len) const { return ::write(fd, ptr, len); }
		};

		struct Writer::Impl
		{
			//std::ofstream m_out;
			std::string path;
			unique_array<char> m_data;
			size_t ptr = 0;

			bool open(const filesystem::path& o)
			{
				path = o.native();
				ptr = 0;
				return store(MAGIC) && store(VERSION);
			}

			bool close()
			{
#if 0
				FD fd{ ::open(path.c_str(), O_WRONLY | O_CREAT, 0644) };
				if (!fd)
					return false;

				return fd.write(m_data.ptr, ptr) == ptr;
#elif 1
				FILE* f = fopen(path.c_str(), "w");
				if (!f)
					return false;

				bool ret = (fwrite(m_data.ptr, 1, ptr, f) == ptr);
				fclose(f);
				return ret;
#else
				std::fstream m_out(path, std::ios::out | std::ios::binary);
				if (!m_out.is_open())
					return false;

				auto tmp = m_out.tellp();
				return ((size_t)(m_out.write((const char*)m_data.ptr, ptr).tellp() - tmp) == ptr);
#endif
			}

			size_t write(const void* buffer, size_t len)
			{
				size_t rest = m_data.size - ptr;
				while (rest < len)
				{
					if (!m_data.grow())
						return 0;

					rest = m_data.size - ptr;
				}
				memcpy(m_data.ptr + ptr, buffer, len);
				ptr += len;

				return len;
			}

			template <typename T>
			bool store(const T& t)
			{
				return write(&t, sizeof(T)) == sizeof(T);
			}

			bool store(const std::string& s)
			{
				return write(s.c_str(), s.length() + 1) == s.length() + 1;
			}

			bool store(const char* s);
		};

		Writer::Writer()
			: pimpl(new Impl())
		{
		}

		Writer::~Writer()
		{
			delete pimpl;
		}

		bool Writer::open(const filesystem::path& path)
		{
			return pimpl->open(path);
		}

		bool Writer::close()
		{
			return pimpl->close();
		}

		bool Writer::store(const Nodes& nodes)
		{
			if (!pimpl->store(nodes.size()))
				return false;

			for (auto&& node : nodes)
			{
				if (!node->store(*this))
					return false;
			}

			return true;
		}

		bool Writer::store(TAG id, const std::string& data, const Nodes& children)
		{
			if (!pimpl->store(id))
				return false;

			if (has_string((unsigned short)id) && !pimpl->store(data))
				return false;

			if (has_children((unsigned short)id) && !store(children))
				return false;

			return true;
		}

		struct Reader::Impl
		{
			unique_array<char> m_data;
			size_t ptr = 0;

			bool open(const filesystem::path& o)
			{
				//profile::probe p{ "Reader::open" };
				ptr = 0;
				auto size = filesystem::file_size(o);
				if (!size)
					return false;

				if (!m_data.resize(size))
					return false;

				std::ifstream in;
				{
					//profile::probe p{ "Open file" };
					in.open(o.native(), std::ios::in | std::ios::binary);
					if (!in.is_open())
						return false;
				}

				{
					//profile::probe p{ "Read file" };
					if (in.read(m_data.ptr, size).gcount() != size)
						return false;
				}

				{
					//profile::probe p{ "Reading header" };
					unsigned long magic, version;
					if (!load(magic) || !load(version))
						return false;
					return magic == MAGIC && version == VERSION;
				}
			}

			size_t read(void* buffer, size_t len)
			{
				size_t rest = m_data.size - ptr;
				if (rest > len)
					rest = len;
				memcpy(buffer, m_data.ptr + ptr, rest);
				ptr += len;
				return rest;
			}

			template <typename T>
			bool load(T& t)
			{
				return read(&t, sizeof(T)) == sizeof(T);
			}

			bool load(std::string& s)
			{
				s.clear();
				char c;
				while (load(c) && c)
					s.push_back(c);
				return c == 0;
			}
		};

		Reader::Reader()
			: pimpl(new Impl())
		{
		}

		Reader::~Reader()
		{
			delete pimpl;
		}

		bool Reader::open(const filesystem::path& path)
		{
			return pimpl->open(path);
		}

		bool Reader::load(Nodes& nodes)
		{
			nodes.clear();
			Nodes::size_type size;
			if (!pimpl->load(size))
				return false;

			nodes.reserve(size);
			if (nodes.capacity() < size)
				return false;

			for (decltype(size) i = 0; i < size; ++i)
			{
				NodePtr child;
				if (!load(child))
					return false;

				nodes.push_back(child);
			}

			return true;
		}

		bool Reader::load(NodePtr& out)
		{
			out.reset();

			TAG id;
			std::string data;
			Nodes children;

			if (!pimpl->load(id))
				return false;

			if (has_string((unsigned short)id) && !pimpl->load(data))
				return false;

			if (has_children((unsigned short)id) && !load(children))
				return false;

			out = create(id, data, children);

			return !!out;
		}

		NodePtr Reader::create(TAG id, const std::string& data, const Nodes& children)
		{
			switch (id)
			{
			case TAG::LINK		: return inline_elem::Link::make(data, children); // break HREF and SEGs apart
			case TAG::HEADER	: return std::make_shared<block_elem::Header>(atoi(data.c_str()), children);
			case TAG::ELEMENT	: return std::make_shared<inline_elem::Element>(data, children);
			case TAG::TEXT		: return std::make_shared<inline_elem::Text>(data);
			case TAG::VARIABLE	: return std::make_shared<inline_elem::Variable>(data);
			case TAG::HREF		: return std::make_shared<SimpleNode<binary::TAG::HREF>>("HREF", children);
			case TAG::SEG		: return std::make_shared<SimpleNode<binary::TAG::SEG>>("SEG", children);
			case TAG::PARA		: return std::make_shared<block_elem::Para>(children);
			case TAG::PRE		: return std::make_shared<block_elem::Pre>(children);
			case TAG::QUOTE		: return std::make_shared<block_elem::Quote>(children);
			case TAG::OLIST		: return std::make_shared<block_elem::UList>(children);
			case TAG::ULIST		: return std::make_shared<block_elem::OList>(children);
			case TAG::ITEM		: return std::make_shared<block_elem::Item>(children);
			case TAG::SIGNATURE	: return std::make_shared<block_elem::Signature>(children);
			case TAG::BREAK		: return std::make_shared<inline_elem::Break>();
			case TAG::LINE		: return std::make_shared<inline_elem::Line>();
			case TAG::HR		: return std::make_shared<block_elem::HR>();
			default:
				break;
			}
			return nullptr;
		}
	}
}
