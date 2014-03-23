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
#include <http.hpp>
#include <utils.hpp>
#include <string.h>

#if defined WIN32
#define PLATFORM L"Win32"
#elif defined _MACOSX_
#define PLATFORM L"Mac OSX"
#elif defined ANDROID
#define PLATFORM L"Android"
#elif defined __GNUC__
#define PLATFORM "UNIX"
#endif

#include <string>
#include "curl_http.hpp"
#include <expat.hpp>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <mt.hpp>
#include <htmlcxx/html/ParserDom.h>

namespace http
{
	struct Encoding
	{
		const char* m_name;
		const int* m_table;
		size_t m_size;
		Encoding(const char* name, const int* table, size_t size)
			: m_name(name)
			, m_table(table)
			, m_size(size)
		{
		}
	};

	class EncodingDB: public mt::AsyncData
	{
		typedef std::vector<Encoding> Encodings;

		Encodings m_encodings;
		filesystem::path m_dbPath;
		char* m_data;
		bool m_opened;

		struct File
		{
			FILE* m_ptr;
			size_t m_size;
			File(const filesystem::path& path)
			{
				m_ptr = fopen(path.native().c_str(), "rb");
				m_size = (size_t)filesystem::file_size(path);
			}

			~File()
			{
				if (*this)
					fclose(m_ptr);
			}

			operator bool () { return m_ptr != nullptr; }

			size_t read(void* dst, size_t bytes) { return fread(dst, 1, bytes, m_ptr); }
			template <typename T> bool read(T& t) { return read(&t, sizeof(T)) == sizeof(T); }

			size_t size() { return m_size; }
		};

		struct AutoCloser
		{
			char*& data;
			bool close;
			AutoCloser(char*& data): data(data), close(true) {}
			~AutoCloser()
			{
				if (close)
				{
					delete [] data;
					data = nullptr;
				}
			}
			void release() { close = false; }
		};

		struct FileHeader
		{
			unsigned int magic, count, strings;
		};

		struct TableHeader
		{
			unsigned int string, table, size;
		};

		void _close()
		{
			if (!m_opened)
				return;

			m_encodings.clear();

			delete [] m_data;
			m_data = nullptr;

			m_opened = false;
		}

		bool _open()
		{
			if (m_opened)
				return true;

			File file(m_dbPath);
			if (!file)
				return false;

			FileHeader header;
			if (!file.read(header)) return false;

			static const unsigned int MAGIC = 0x54455343;
			if (header.magic != MAGIC) return false;
			size_t stringsStart = sizeof(header) + sizeof(TableHeader)*header.count;
			size_t minimalOffset = stringsStart + header.strings;
			minimalOffset = ((minimalOffset + 3) >> 2) << 2;
			if (minimalOffset > file.size())
				return false;

			size_t buffer = file.size() - stringsStart;
			m_data = new (std::nothrow) char[buffer];
			if (!m_data)
				return false;
			AutoCloser guard(m_data);

			m_encodings.reserve(header.count);
			for (unsigned int i = 0; i < header.count; ++i)
			{
				TableHeader head;
				if (!file.read(head)) return false;

				if (head.table < minimalOffset)
					return false;
				if (head.table + head.size > file.size())
					return false;
				if (head.string < stringsStart || head.string >= (stringsStart + header.strings))
					return false;

				const char* name = m_data + head.string - stringsStart;
				const int* table = (int*)(m_data + head.table - stringsStart);

				m_encodings.emplace_back(name, table, head.size);
			}

			if (file.read(m_data, buffer) != buffer)
				return false;

			guard.release();
			return m_opened = true;
		}

		static EncodingDB& get()
		{
			static EncodingDB db;
			return db;
		}

		const Encoding* _find(std::string enc)
		{
			std::tolower(enc);
			auto it = std::lower_bound(m_encodings.begin(), m_encodings.end(), enc,
				[](const Encoding& enc, const std::string& key)
				{
					// this supposed to be equivalent of "less(enc.m_name, key)" (notice reversed arguments)
					return key.compare(enc.m_name) > 0;
				});
			if (it == m_encodings.end() || enc.compare(it->m_name) != 0)
				return nullptr;
			return &*it;
		}

		EncodingDB()
			: m_data(nullptr)
			, m_opened(false)
		{
		}
	public:
		static mt::AsyncData& guard() { return get(); }
		static void init(const filesystem::path& path) { get().m_dbPath = path; close(); }
		static void close() { get()._close(); }
		static bool open() { return get()._open(); }
		static const Encoding* find(const std::string& enc) { return get()._find(enc); }
	};

	namespace { std::string charsetPath; }

	void init(const filesystem::path& path)
	{
		Synchronize on(EncodingDB::guard());

		EncodingDB::init(path);
	}

	void reload(const filesystem::path& path)
	{
		Synchronize on(EncodingDB::guard());

		EncodingDB::init(path); // re-init.
	}

	bool loadCharset(const std::string& encoding, int(&table)[256])
	{
		Synchronize on(EncodingDB::guard());

		if (!EncodingDB::open())
			return false;

		auto obj = EncodingDB::find(encoding);

		if (!obj)
			return false;

		if (obj->m_size < sizeof(table))
			return false;

		memcpy(table, obj->m_table, sizeof(table));
		return true;
	}

	namespace impl
	{
		struct ContentData
		{
			char* content;
			size_t content_length;
			size_t capacity;
			ContentData()
				: content(nullptr)
				, content_length(0)
				, capacity(0)
			{
			}
			~ContentData()
			{
				delete [] content;
			}

			bool grow(size_t newsize)
			{
				if (capacity >= newsize)
					return true;

				auto copy_capacity = capacity;
				if (!copy_capacity)
					copy_capacity = 16;

				while (copy_capacity < newsize)
					copy_capacity <<= 1;

				auto tmp = new (std::nothrow) char[copy_capacity];
				if (!tmp)
					return false;

				if (content)
					memcpy(tmp, content, content_length);

				delete [] content;
				content = tmp;
				capacity = copy_capacity;
				return true;
			}

			bool append(const void* data, size_t len)
			{
				if (data && len)
				{
					if (!grow(content_length + len))
						return false;

					memcpy(content + content_length, data, len);
					content_length += len;
				}
				return true;
			}

			void clear()
			{
				delete [] content;
				content = nullptr;
				content_length = 0;
				capacity = 0;
			}
		};

		class XmlHttpRequest
			: public http::XmlHttpRequest
			, public http::HttpCallback
			, public std::enable_shared_from_this<XmlHttpRequest>
		{
			ONREADYSTATECHANGE handler;
			void* handler_data;

			HTTP_METHOD http_method;
			std::string url;
			bool async;
			Headers request_headers;
			READY_STATE ready_state;
			ContentData body;

			int http_status;
			std::string reason;
			Headers response_headers;
			ContentData response;
			dom::XmlDocumentPtr doc;

			bool send_flag, done_flag, debug;
			HttpEndpointPtr m_endpoint;

			bool m_followRedirects;
			size_t m_redirects;

			bool m_wasRedirected;
			std::string m_finalLocation;

			void onReadyStateChange()
			{
				if (handler)
					handler(this, handler_data);
			}
			bool rebuildDOM(const std::string& mimeType, const std::string& encoding);
			bool rebuildDOM(const char* forcedType = nullptr);
			bool parseXML(const std::string& encoding);
			bool parseHTML(const std::string& encoding);

			void clear_response()
			{
				http_status = 0;
				reason.clear();
				response_headers.clear();
				response.clear();
				doc.reset();
				m_wasRedirected = false;
				m_finalLocation.clear();
			}
		public:

			XmlHttpRequest()
				: handler(nullptr)
				, handler_data(nullptr)
				, http_method(HTTP_GET)
				, async(true)
				, ready_state(UNSENT)
				, http_status(0)
				, send_flag(false)
				, done_flag(false)
				, debug(false)
				, m_followRedirects(true)
				, m_redirects(10)
				, m_wasRedirected(false)
			{
			}

			~XmlHttpRequest()
			{
				if (m_endpoint)
					m_endpoint->releaseEndpoint();
			}

			void onreadystatechange(ONREADYSTATECHANGE handler, void* userdata) override;
			READY_STATE getReadyState() const override;

			void open(HTTP_METHOD method, const std::string& url, bool async = true) override;
			void setRequestHeader(const std::string& header, const std::string& value) override;

			void setBody(const void* body, size_t length) override;
			void send() override;
			void abort() override;

			int getStatus() const override;
			std::string getStatusText() const override;
			std::string getResponseHeader(const std::string& name) const override;
			std::map<std::string, std::string> getResponseHeaders() const override;
			size_t getResponseTextLength() const override;
			const char* getResponseText() const override;
			dom::XmlDocumentPtr getResponseXml() override;
			dom::XmlDocumentPtr getResponseHtml() override;

			bool wasRedirected() const override;
			const std::string getFinalLocation() const override;

			void setDebug(bool debug) override;
			void setMaxRedirects(size_t redirects) override;
			void setShouldFollowLocation(bool follow) override;

			void onStart() override;
			void onError() override;
			void onFinish() override;
			size_t onData(const void* data, size_t count) override;
			void onFinalLocation(const std::string& location) override;
			void onHeaders(const std::string& reason, int http_status, const Headers& headers) override;

			void appendHeaders() override;
			std::string getUrl() override;
			void* getContent(size_t& length) override;
			bool getDebug() override;
			bool shouldFollowLocation() override;
			long getMaxRedirs() override;
		};

		void XmlHttpRequest::onreadystatechange(ONREADYSTATECHANGE handler, void* userdata)
		{
			//Synchronize on(*this);
			this->handler = handler;
			this->handler_data = userdata;
			if (ready_state != UNSENT)
				onReadyStateChange();
		}

		http::XmlHttpRequest::READY_STATE XmlHttpRequest::getReadyState() const
		{
			return ready_state;
		}

		void XmlHttpRequest::open(HTTP_METHOD method, const std::string& url, bool async)
		{
			abort();

			//Synchronize on(*this);

			send_flag = false;
			clear_response();
			request_headers.clear();

			http_method = method;
			this->url = url;
			this->async = async;

			ready_state = OPENED;
			onReadyStateChange();
		}

		void XmlHttpRequest::setRequestHeader(const std::string& header, const std::string& value)
		{
			//Synchronize on(*this);

			if (ready_state != OPENED || send_flag) return;

			auto _h = std::tolower(header);
			Headers::iterator _it = request_headers.find(_h);
			if (_it == request_headers.end())
			{
				if (_h == "accept-charset") return;
				if (_h == "accept-encoding") return;
				if (_h == "connection") return;
				if (_h == "content-length") return;
				if (_h == "cookie") return;
				if (_h == "cookie2") return;
				if (_h == "content-transfer-encoding") return;
				if (_h == "date") return;
				if (_h == "expect") return;
				if (_h == "host") return;
				if (_h == "keep-alive") return;
				if (_h == "referer") return;
				if (_h == "te") return;
				if (_h == "trailer") return;
				if (_h == "transfer-encoding") return;
				if (_h == "upgrade") return;
				if (_h == "user-agent") return;
				if (_h == "via") return;
				if (_h.substr(0, 6) == "proxy-") return;
				if (_h.substr(0, 4) == "sec-") return;
				request_headers[_h] = header +": " + value;
			}
			else
			{
				_it->second += ", " + value;
			}
		}

		void XmlHttpRequest::setBody(const void* body, size_t length)
		{
			//Synchronize on(*this);

			if (ready_state != OPENED || send_flag) return;

			this->body.clear();

			if (http_method != HTTP_POST) return;

			this->body.append(body, length);
		}

		void XmlHttpRequest::send()
		{
			abort();

			//Synchronize on(*this);

			if (ready_state != OPENED || send_flag) return;

			send_flag = true;
			done_flag = false;
			if (!m_endpoint)
				m_endpoint = http::GetEndpoint(shared_from_this());
			if (m_endpoint)
				m_endpoint->send(async);
		}

		void XmlHttpRequest::abort()
		{
			if (m_endpoint)
				m_endpoint->abort();
		}

		int XmlHttpRequest::getStatus() const
		{
			return http_status;
		}

		std::string XmlHttpRequest::getStatusText() const
		{
			return reason;
		}

		std::string XmlHttpRequest::getResponseHeader(const std::string& name) const
		{
			Headers::const_iterator _it = response_headers.find(std::tolower(name));
			if (_it == response_headers.end()) return std::string();
			return _it->second;
		}

		std::map<std::string, std::string> XmlHttpRequest::getResponseHeaders() const
		{
			return response_headers;
		}

		size_t XmlHttpRequest::getResponseTextLength() const
		{
			return response.content_length;
		}

		const char* XmlHttpRequest::getResponseText() const
		{
			return (const char*)response.content;
		}

		dom::XmlDocumentPtr XmlHttpRequest::getResponseXml()
		{
			// Synchronize on (*this);

			if (ready_state != DONE || done_flag) return false;

			if (!doc && response.content && response.content_length)
				if (!rebuildDOM("text/xml"))
					return dom::XmlDocumentPtr();

			return doc; 
		}

		dom::XmlDocumentPtr XmlHttpRequest::getResponseHtml()
		{
			// Synchronize on (*this);

			if (ready_state != DONE || done_flag) return false;

			if (!doc && response.content && response.content_length)
				if (!rebuildDOM("text/html"))
					return dom::XmlDocumentPtr();

			return doc; 
		}

		bool XmlHttpRequest::wasRedirected() const { return m_wasRedirected; }
		const std::string XmlHttpRequest::getFinalLocation() const { return m_finalLocation; }

		void XmlHttpRequest::setDebug(bool debug)
		{
			this->debug = debug;
		}

		void XmlHttpRequest::setMaxRedirects(size_t redirects)
		{
			m_redirects = redirects;
		}

		void XmlHttpRequest::setShouldFollowLocation(bool follow)
		{
			m_followRedirects = follow;
		}

		void XmlHttpRequest::onStart()
		{
			if (!body.content || !body.content_length)
				http_method = HTTP_GET;

			//onReadyStateChange();
		}

		void XmlHttpRequest::onError()
		{
			ready_state = DONE;
			done_flag = true;
			onReadyStateChange();
		}

		void XmlHttpRequest::onFinish()
		{
			ready_state = DONE;
			onReadyStateChange();
		}

		size_t XmlHttpRequest::onData(const void* data, size_t count)
		{
			//Synchronize on(*this);

			size_t ret = response.append(data, count) ? count : 0;
			if (ret)
			{
				if (ready_state == HEADERS_RECEIVED) ready_state = LOADING;
				if (ready_state == LOADING)          onReadyStateChange();
			}
			return ret;
		}

		void XmlHttpRequest::onFinalLocation(const std::string& location)
		{
			m_wasRedirected = true;
			m_finalLocation = location;
		}

		void XmlHttpRequest::onHeaders(const std::string& reason, int http_status, const Headers& headers)
		{
			//Synchronize on(*this);
			this->http_status = http_status;
			this->reason = reason;
			this->response_headers = headers;

			ready_state = HEADERS_RECEIVED;
			onReadyStateChange();
		}

		void XmlHttpRequest::appendHeaders()
		{
			if (m_endpoint)
			{
				for (auto&& pair : request_headers)
					m_endpoint->appendHeader(pair.second);
			}
		}

		std::string XmlHttpRequest::getUrl() { return url; }
		void* XmlHttpRequest::getContent(size_t& length) { if (http_method == HTTP_POST) { length = body.content_length; return body.content; } return nullptr; }
		bool XmlHttpRequest::getDebug() { return debug; }
		bool XmlHttpRequest::shouldFollowLocation() { return m_followRedirects; }
		long XmlHttpRequest::getMaxRedirs() { return m_redirects; }

		static void getMimeAndEncoding(std::string ct, std::string& mime, std::string& enc)
		{
			mime.empty();
			enc.empty();
			size_t pos = ct.find_first_of(';');
			mime = std::tolower(ct.substr(0, pos));
			if (pos == std::string::npos) return;
			ct = ct.substr(pos+1);

			while(1)
			{
				pos = ct.find_first_of('=');
				if (pos == std::string::npos) return;
				std::string cand = ct.substr(0, pos);
				ct = ct.substr(pos+1);
				size_t low = 0, hi = cand.length();
				while (cand[low] && isspace((unsigned char)cand[low])) ++low;
				while (hi > low && isspace((unsigned char)cand[hi-1])) --hi;

				if (std::tolower(cand.substr(low, hi - low)) == "charset")
				{
					pos = ct.find_first_of(';');
					enc = ct.substr(0, pos);
					low = 0; hi = enc.length();
					while (enc[low] && isspace((unsigned char)enc[low])) ++low;
					while (hi > low && isspace((unsigned char)enc[hi-1])) --hi;
					enc = enc.substr(low, hi-low);
					return;
				}
				pos = ct.find_first_of(';');
				if (pos == std::string::npos) return;
				ct = ct.substr(pos+1);
			};
		}

		bool XmlHttpRequest::rebuildDOM(const std::string& mime, const std::string& encoding)
		{
			if (mime == "" ||
				mime == "text/xml" ||
				mime == "application/xml" || 
				(mime.length() > 4 && mime.substr(mime.length()-4) == "+xml"))
			{
				return parseXML(encoding);
			}

			if (mime == "text/html")
			{
				return parseHTML(encoding);
			}

			return true;
		}

		bool XmlHttpRequest::rebuildDOM(const char* forcedType)
		{
			std::string mime;
			std::string enc;
			getMimeAndEncoding(getResponseHeader("Content-Type"), mime, enc);
			if (forcedType)
				return rebuildDOM(forcedType, enc);
			return rebuildDOM(mime, enc);
		}

		class XHRParser: public xml::ExpatBase<XHRParser>
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
				return xml::ExpatBase<XHRParser>::create(cp);
			}

			bool onUnknownEncoding(const XML_Char* name, XML_Encoding* info)
			{
				info->data = nullptr;
				info->convert = nullptr;
				info->release = nullptr;

				if (!loadCharset(name, info->map))
				{
					printf("Unknown encoding: %s\n", name);
					return false;
				}

				return true;
			}

			void onStartElement(const XML_Char *name, const XML_Char **attrs)
			{
				addText();
				auto current = doc->createElement(name);
				if (!current) return;
				for (; *attrs; attrs += 2)
				{
					auto attr = doc->createAttribute(attrs[0], attrs[1]);
					if (!attr) continue;
					current->setAttribute(attr);
				}
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

		bool XmlHttpRequest::parseXML(const std::string& encoding)
		{
			const char* cp = NULL;
			if (!encoding.empty()) cp = encoding.c_str();
			XHRParser parser;
			if (!parser.create(cp)) return false;
			parser.enableElementHandler();
			parser.enableCharacterDataHandler();
			parser.enableUnknownEncodingHandler();
			if (!parser.parse((const char*)response.content, response.content_length))
				return false;
			doc = parser.doc;
			return true;
		}

		namespace utf8
		{
			constexpr uint32_t unicodeReplacement{ 0x0000FFFD };
			inline size_t length(uint32_t utf32)
			{
				if (utf32 < 0x00000080) return 1;
				if (utf32 < 0x00000800) return 2;
				if (utf32 < 0x00010000) return 3;
				if (utf32 < 0x00200000) return 4;
				if (utf32 < 0x04000000) return 5;
				return 6;
			}

			inline void move(char*& dst, uint32_t& utf32)
			{
				*--dst = (utf32 & 0x3f) | 0x80;
				utf32 >>= 6;
			}

			inline void conv(std::string& utf8, uint32_t utf32)
			{
				static const uint32_t firstByteMark[7] = { 0x00, 0x00, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc };

				char seq[6];
				if (utf32 & 0x80000000)
					utf32 = unicodeReplacement;

				auto bytes = length(utf32);
				char* dst = seq + bytes;
				switch (bytes)
				{
				case 6: move(dst, utf32);
				case 5: move(dst, utf32);
				case 4: move(dst, utf32);
				case 3: move(dst, utf32);
				case 2: move(dst, utf32);
				case 1: *--dst = utf32 | firstByteMark[bytes];
				}

				utf8.append(seq, bytes);
			}

			std::string fromUtf32(const uint32_t* string, size_t length)
			{
				size_t len = 0;
				auto c = string;
				for (size_t i = 0; i < length; ++i, ++c)
				{
					auto utf32 = *c;
					if (utf32 & 0x80000000)
						utf32 = unicodeReplacement;
					len += utf8::length(utf32);
				}

				std::string utf8;
				utf8.reserve(len + 1);

				c = string;
				for (size_t i = 0; i < length; ++i, ++c)
					conv(utf8, *c);

				return utf8;
			}

			std::string fromUtf32(uint32_t c)
			{
				return fromUtf32(&c, 1);
			}

			template <template <typename T, typename A> class C, typename A>
			std::string fromUtf32(const C<uint32_t, A>& c)
			{
				return fromUtf32(c.data(), c.size());
			}
		}

		struct TextConverter
		{
			virtual ~TextConverter() {}
			virtual std::string conv(const std::string&) = 0;
		};
		using TextConverterPtr = std::shared_ptr<TextConverter>;

		struct Identity: TextConverter
		{
			std::string conv(const std::string& s) override { return s; }
		};

		struct eXpatConverter : TextConverter
		{
			int map[256];

			std::string conv(const std::string& s) override
			{
				std::vector<uint32_t> data;

				data.reserve(s.length());
				for (auto&& c : s)
				{
					uint32_t utf32 = map[(unsigned char)c];
					if (utf32 < 0)
						utf32 = utf8::unicodeReplacement;
					data.push_back(utf32);
				}
				return utf8::fromUtf32(data);
			}

			static TextConverterPtr create(const std::string& cp)
			{
				auto conv = std::make_shared<eXpatConverter>();
				if (!conv)
					return nullptr;

				if (!loadCharset(cp, conv->map))
					return nullptr;

				return conv;
			}
		};

		class HTMLParser : htmlcxx::HTML::ParserSax
		{
			dom::XmlElementPtr elem;
			dom::XmlDocumentFragmentPtr container;
			std::string text;
			TextConverterPtr converter;
			std::string conv(const std::string& s) { return converter->conv(s); }
			bool switchConv(std::string cp)
			{
				std::tolower(cp);
				if (cp.empty() || cp == "utf-8" || cp == "utf8")
					converter = std::make_shared<Identity>();
				else
					converter = eXpatConverter::create(cp);

				return !!converter;
			}

			inline void expand(const char* entity, const char* val)
			{
				std::string::size_type len = strlen(entity);
				std::string::size_type ntity = text.find(entity);
				while (ntity != std::string::npos)
				{
					text = text.substr(0, ntity) + val + text.substr(ntity + len);
					ntity = text.find(entity, ntity);
				}
			}

			inline void expandNumericals()
			{
				std::string::size_type ntity = text.find("&#");
				while (ntity != std::string::npos)
				{
					std::string::size_type ptr = ntity + 2;
					char hex = text[ptr];

					uint32_t utf32 = 0;
					if (hex == 'x' || hex == 'X')
					{
						++ptr;

						while (isxdigit((unsigned char)text[ptr]))
						{
							switch (text[ptr])
							{
							case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
								utf32 *= 16;
								utf32 += text[ptr] - 'A' + 10;
								break;
							case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
								utf32 *= 16;
								utf32 += text[ptr] - 'a' + 10;
								break;
							default:
								utf32 *= 10;
								utf32 += text[ptr] - '0';
							};

							++ptr;
						}
					}
					else
					while (isdigit((unsigned char)text[ptr]))
					{
						utf32 *= 10;
						utf32 += text[ptr] - '0';
						++ptr;
					}

					if (text[ptr] == ';')
					{
						++ptr;
						if (utf32 == 0xA0) utf32 = ' ';
						text = text.substr(0, ntity) + utf8::fromUtf32(utf32) + text.substr(ptr);
					}
					else
						ntity += 2; //to skip this &# in the next find

					ntity = text.find("&#", ntity);
				}
			};

			inline void addText()
			{
				if (text.empty()) return;

				text = conv(text);

				expand("&nbsp;", " ");
				expand("&quot;", "\"");
				expand("&lt;", "<");
				expand("&gt;", ">");
				expandNumericals();
				expand("&amp;", "&");

				dom::XmlTextPtr node = doc->createTextNode(text);
				if (node)
				{
					if (elem)
						elem->appendChild(node);
					else
						container->appendChild(node);
				}
				text.clear();
			}

		public:

			HTMLParser()
				: converter(std::make_shared<Identity>())
			{
			}

			dom::XmlDocumentPtr doc;

			bool create(const std::string& cp)
			{
				doc = dom::XmlDocument::create();
				if (!doc)
					return false;
				container = doc->createDocumentFragment();
				if (!container)
					return false;
				return switchConv(cp);
			}

			void foundTag(htmlcxx::HTML::Node node, bool isEnd) override
			{
				std::string nodeName = std::tolower(node.tagName());

				if (!isEnd)
				{
					addText();

					dom::XmlElementPtr current = doc->createElement(nodeName);
					if (!current) return;

					node.parseAttributes();
					for (auto&& attr : node.attributes())
					{
						// fixing issues with htmlcxx
						if (attr.first.empty() || std::isdigit((unsigned char)attr.first[0]))
							continue;

						auto node = doc->createAttribute(std::tolower(attr.first), attr.second);
						if (!node) continue;
						current->setAttribute(node);
					}

					if (elem)
						elem->appendChild(current);
					else
						container->appendChild(current);

					if (nodeName == "meta")
					{
						std::string
							equiv = std::tolower(current->getAttribute("http-equiv")),
							content = current->getAttribute("content");
						if (equiv == "content-type")
						{
							std::string charset;
							std::string::size_type semi = content.find(';');
							std::string::size_type len = content.length();
							while (semi != std::string::npos)
							{
								++semi;
								while (semi < len && isspace((unsigned char)content[semi])) ++semi;
								if (std::tolower(content.substr(semi, 7)) == "charset")
								{
									semi += 7;
									while (semi < len && isspace((unsigned char)content[semi])) ++semi;
									if (semi < len && content[semi] == '=')
									{
										++semi;
										std::string::size_type last = content.find(';', semi);
										if (last == std::string::npos)
											charset = content.substr(semi);
										else
											charset = content.substr(semi, last - semi);
										std::trim(charset);
										std::tolower(charset);
									}
									break;
								}
								semi = content.find(';', semi);
							}

							if (switchConv(charset))
							{
								// printf("\rUnexpected character set: %s\n", charset.c_str());
							}
						}
						return;
					}

					if (nodeName != "area" &&
						nodeName != "base" &&
						nodeName != "basefont" &&
						nodeName != "br" &&
						nodeName != "col" &&
						nodeName != "frame" &&
						nodeName != "hr" &&
						nodeName != "img" &&
						nodeName != "input" &&
						nodeName != "isindex" &&
						nodeName != "link" &&
						//nodeName != "meta" && //always true
						nodeName != "param" &&
						nodeName != "nextid" &&
						nodeName != "bgsound" &&
						nodeName != "embed" &&
						nodeName != "keygen" &&
						nodeName != "spacer" &&
						nodeName != "wbr")
					{
						elem = current;
					}

					return;
				}

				dom::XmlNodePtr xmlnode = elem;
				while (xmlnode)
				{
					if (xmlnode->nodeName() == nodeName)
						break;

					xmlnode = xmlnode->parentNode();
				}

				if (!xmlnode) return;
				elem = std::static_pointer_cast<dom::XmlElement>(xmlnode);

				addText();

				if (!elem) return;

				elem = std::static_pointer_cast<dom::XmlElement>(elem->parentNode());
			}

			void foundText(htmlcxx::HTML::Node node) override
			{
				text += node.text().c_str();
			}

			void endParsing() override
			{
				addText();

				auto list = container->childNodes();
				if (list)
				{
					if (list->length() == 1)
					{
						auto e = list->element(0);
						if (e)
						{
							doc->setDocumentElement(e);
							return;
						}
					}

					doc->setFragment(container);
				}
			}

			void parse(const char* begin, long long size)
			{
				return htmlcxx::HTML::ParserSax::parse(begin, begin + size);
			}
		};

		bool XmlHttpRequest::parseHTML(const std::string& encoding)
		{
			HTMLParser parser;
			if (!parser.create(encoding)) return false;
			parser.parse((const char*)response.content, response.content_length);
			doc = parser.doc;
			return true;
		}
	} // http::impl

	XmlHttpRequestPtr XmlHttpRequest::Create()
	{
		try {
			return std::make_shared<impl::XmlHttpRequest>();
		} catch(std::bad_alloc) {
			return nullptr;
		}
	}
}
