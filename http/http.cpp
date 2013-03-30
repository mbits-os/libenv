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

namespace http
{
	namespace impl
	{
		struct ContentData
		{
			void* content;
			size_t content_length;
			ContentData()
				: content(NULL)
				, content_length(0)
			{
			}
			~ContentData()
			{
				free(content);
			}

			bool append(const void* data, size_t len)
			{
				void* copy = NULL;
				if (data && len)
				{
					void* copy = realloc(content, content_length + len);
					if (!copy) return false;
					content = copy;
					memcpy((char*)content + content_length, data, len);
					content_length += len;
				}
				return true;
			}

			void clear()
			{
				free(content);
				content = NULL;
				content_length = 0;
			}
		};

		class XmlHttpRequest
			: public http::XmlHttpRequest
			, public http::HttpCallback
		{
			std::weak_ptr<HttpCallback> m_self;

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

			bool send_flag, done_flag, debug;

			void onReadyStateChange()
			{
				if (handler)
					handler(this, handler_data);
			}
			//bool RebuildDOM();
			//bool ParseXML(const std::string& encoding);

			void clear_response()
			{
				http_status = 0;
				reason.clear();
				response_headers.clear();
				response.clear();
			}
		public:

			XmlHttpRequest()
				: handler(NULL)
				, handler_data(NULL)
				, http_method(HTTP_GET)
				, async(true)
				, ready_state(UNSENT)
				, http_status(0)
				, send_flag(false)
				, done_flag(false)
				, debug(false)
			{
			}

			~XmlHttpRequest()
			{
			}

			void setSelfRef(HttpCallbackPtr self) { m_self = self; }
			void onreadystatechange(ONREADYSTATECHANGE handler, void* userdata);
			READY_STATE getReadyState() const;

			void open(HTTP_METHOD method, const std::string& url, bool async = true);
			void setRequestHeader(const std::string& header, const std::string& value);

			void setBody(const void* body, size_t length);
			void send();
			void abort();

			int getStatus() const;
			std::string getStatusText() const;
			std::string getResponseHeader(const std::string& name) const;
			size_t getResponseTextLength() const;
			const char* getResponseText() const;

			void setDebug(bool debug);

			void onStart();
			void onError();
			void onFinish();
			size_t onData(const void* data, size_t count);
			void onHeaders(const std::string& reason, int http_status, const Headers& headers);

			void appendHeaders(APPENDHEADER cb, void* data);
			std::string getUrl();
			void* getContent(size_t& length);
			bool getDebug();
		};

		void XmlHttpRequest::onreadystatechange(ONREADYSTATECHANGE handler, void* userdata)
		{
			//Synchronize on(*this);
			this->handler = handler;
			this->handler_data = userdata;
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
			request_headers.empty();

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
			http::Send(m_self.lock(), async);
		}

		void XmlHttpRequest::abort()
		{
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

		size_t XmlHttpRequest::getResponseTextLength() const
		{
			return response.content_length;
		}

		const char* XmlHttpRequest::getResponseText() const
		{
			return (const char*)response.content;
		}

		void XmlHttpRequest::setDebug(bool debug)
		{
			this->debug = debug;
		}

		void XmlHttpRequest::onStart()
		{
			if (!body.content || !body.content_length)
				http_method = HTTP_GET;
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

		void XmlHttpRequest::onHeaders(const std::string& reason, int http_status, const Headers& headers)
		{
			//Synchronize on(*this);
			this->http_status = http_status;
			this->reason = reason;
			this->response_headers = headers;

			ready_state = HEADERS_RECEIVED;
			onReadyStateChange();
		}

		void XmlHttpRequest::appendHeaders(APPENDHEADER cb, void* data)
		{
			//Synchronize on(*this);
			for (Headers::const_iterator _cur = request_headers.begin(); _cur != request_headers.end(); ++_cur)
				cb(_cur->second, data);
		}

		std::string XmlHttpRequest::getUrl() { return url; }
		void* XmlHttpRequest::getContent(size_t& length) { if (http_method == HTTP_POST) { length = body.content_length; return body.content; } return NULL; }
		bool XmlHttpRequest::getDebug() { return debug; }

		static void GetMimeAndEncoding(std::string ct, std::string& mime, std::string& enc)
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
				while (cand[low] && isspace(cand[low])) ++low;
				while (hi > low && isspace(cand[hi-1])) --hi;

				if (std::tolower(cand.substr(low, hi - low)) == "charset")
				{
					pos = ct.find_first_of(';');
					enc = ct.substr(0, pos);
					low = 0; hi = enc.length();
					while (enc[low] && isspace(enc[low])) ++low;
					while (hi > low && isspace(enc[hi-1])) --hi;
					enc = std::toupper(enc.substr(low, hi-low));
					return;
				}
				pos = ct.find_first_of(';');
				if (pos == std::string::npos) return;
				ct = ct.substr(pos+1);
			};
		}
	} // http::impl

	XmlHttpRequestPtr XmlHttpRequest::Create()
	{
		try {
		auto xhr = std::make_shared<impl::XmlHttpRequest>();
		HttpCallbackPtr self(xhr);
		xhr->setSelfRef(self);
		return xhr;
		} catch(std::bad_alloc) { return nullptr; }
	}
}
