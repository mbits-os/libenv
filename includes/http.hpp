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

#ifndef __HTTP_HPP__
#define __HTTP_HPP__

#include <dom.hpp>
#include <filesystem.hpp>

namespace http
{
	std::string getUserAgent();

	struct HttpResponse;
	struct XmlHttpRequest;
	typedef std::shared_ptr<HttpResponse> HttpResponsePtr;
	typedef std::shared_ptr<XmlHttpRequest> XmlHttpRequestPtr;

	typedef std::map<std::string, std::string> HTTPArgs;

	enum HTTP_METHOD
	{
		HTTP_GET,
		HTTP_POST
	};

	//http://www.w3.org/TR/XMLHttpRequest/

	struct HttpResponse
	{
		virtual ~HttpResponse() {}
		virtual int getStatus() const = 0;
		virtual std::string getStatusText() const = 0;
		virtual std::string getResponseHeader(const std::string& name) const = 0;
		virtual std::map<std::string, std::string> getResponseHeaders() const = 0;

		virtual bool wasRedirected() const = 0;
		virtual const std::string getFinalLocation() const = 0;
	};

	struct XmlHttpRequest: HttpResponse
	{
		static XmlHttpRequestPtr Create();

		enum READY_STATE
		{
			UNSENT = 0,
			OPENED = 1,
			HEADERS_RECEIVED = 2,
			LOADING = 3,
			DONE = 4
		};

		typedef void (*ONREADYSTATECHANGE)(XmlHttpRequest* xhr, void* userdata);

		struct OnReadyStateChange
		{
			virtual ~OnReadyStateChange() {}
			virtual void onReadyStateChange(XmlHttpRequest* xhr) {}
			static void Callback(XmlHttpRequest* xhr, void* userdata)
			{
				((OnReadyStateChange*)userdata)->onReadyStateChange(xhr);
			}
		};

		virtual void onreadystatechange(ONREADYSTATECHANGE handler, void* userdata) = 0;
		void onreadystatechange(OnReadyStateChange* handler)
		{
			onreadystatechange(OnReadyStateChange::Callback, handler);
		}
		virtual READY_STATE getReadyState() const = 0;

		virtual void open(HTTP_METHOD method, const std::string& url, bool async = true) = 0;
		virtual void setRequestHeader(const std::string& header, const std::string& value) = 0;
		virtual void setBody(const void* data, size_t length) = 0;

		virtual void send(const void* data, size_t length) { setBody(data, length); send(); } 
		virtual void send() = 0;
		virtual void abort() = 0;

		virtual size_t getResponseTextLength() const = 0;
		virtual const char* getResponseText() const = 0;
		virtual dom::XmlDocumentPtr getResponseXml() = 0;

		virtual void setDebug(bool debug = true) = 0;
		virtual void setShouldFollowLocation(bool follow) = 0;
		virtual void setMaxRedirects(size_t redirects) = 0;
	};

	void init(const filesystem::path& charsetPath);
	void reload(const filesystem::path& charsetPath);
}

#endif //__HTTP_HPP__