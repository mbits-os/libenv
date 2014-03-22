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
#include "curl_http.hpp"
#include <curl/curl.h>
#ifdef WIN32
#include <windows.h>
#else
#include <sys/utsname.h>
#endif
#include <string.h>
#include <sstream>

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
#include <http.hpp>
#include <utils.hpp>
#include <mt.hpp>

namespace std
{
	ostream& operator<<(ostream& o, void(*fn)(ostream&))
	{
		fn(o);
		return o;
	}
}

namespace http
{
	static void getOSVersion(std::ostream& s)
	{
#if defined WIN32

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4996) // GetVersionEx
#endif

		OSVERSIONINFO verinfo = { sizeof(OSVERSIONINFO) };
		s << "Windows ";

		GetVersionEx(&verinfo);

		if (verinfo.dwPlatformId == VER_PLATFORM_WIN32_NT) s << "NT ";
		s << verinfo.dwMajorVersion << "." << verinfo.dwMinorVersion;

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#else
		utsname name;
		if (uname(&name))
			s << PLATFORM;
		else
			s << name.sysname;
#endif
	}

	std::string getUserAgent()
	{
		std::ostringstream ret;
		ret << "reedr/1.0 (" << getOSVersion << ")";
		return ret.str();
	}

	void Init()
	{
		curl_global_init(CURL_GLOBAL_ALL);
	}

	template <typename Final>
	struct CurlBase
	{
		typedef unsigned long long size_type;

		CURL* m_curl;

		CurlBase(): m_curl(nullptr)
		{
			m_curl = curl_easy_init();
		}
		virtual ~CurlBase()
		{
			if (m_curl)
				curl_easy_cleanup(m_curl);
			m_curl = nullptr;
		}
		explicit operator bool () const { return m_curl != nullptr; }

		void followLocation()
		{
			curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, 1L);
			curl_easy_setopt(m_curl, CURLOPT_AUTOREFERER, 1L);
		}

		void dontFollowLocation()
		{
			curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, 0L);
			curl_easy_setopt(m_curl, CURLOPT_AUTOREFERER, 0L);
			curl_easy_setopt(m_curl, CURLOPT_MAXREDIRS, -1L);
		}

		void setMaxRedirs(long redirs)
		{
			curl_easy_setopt(m_curl, CURLOPT_MAXREDIRS, redirs);
		}

		void setConnectTimeout(long timeout)
		{
			curl_easy_setopt(m_curl, CURLOPT_CONNECTTIMEOUT, timeout);
		}

		void setUrl(const std::string& url)
		{
			curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
		}

		void setUA(const std::string& userAgent)
		{
			curl_easy_setopt(m_curl, CURLOPT_USERAGENT, userAgent.c_str());
		}

		void setHeaders(curl_slist* headers)
		{
			curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, headers);
		}

		void setPostData(const void* data, size_t length)
		{
			curl_easy_setopt(m_curl, CURLOPT_POSTFIELDSIZE, length);
			curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, data);
		}

		void setSSLVerify(bool verify = true)
		{
			long val = verify ? 1 : 0;
			curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, val);
			curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYHOST, val);
			//curl_easy_setopt(m_curl, CURLOPT_CERTINFO, 1L);
			//curl_easy_setopt(m_curl, CURLOPT_SSL_CTX_FUNCTION, curl_ssl_ctx_callback); 
		}

		void setWrite()
		{
			auto _this = static_cast<Final*>(this);
			curl_easy_setopt(m_curl, CURLOPT_WRITEHEADER, _this);
			curl_easy_setopt(m_curl, CURLOPT_HEADERFUNCTION, curl_onHeader);
			curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, _this);
			curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, curl_onData);
		}

		void setProgress()
		{
			curl_easy_setopt(m_curl, CURLOPT_NOPROGRESS, 0L);
			curl_easy_setopt(m_curl, CURLOPT_PROGRESSFUNCTION, curl_onProgress);
			curl_easy_setopt(m_curl, CURLOPT_PROGRESSDATA, static_cast<Final*>(this));
		}

		void setDebug(bool debug = true)
		{
			curl_easy_setopt(m_curl, CURLOPT_VERBOSE, debug ? 1L : 0L);
			if (debug)
			{
				curl_easy_setopt(m_curl, CURLOPT_DEBUGFUNCTION, curl_onTrace);
				curl_easy_setopt(m_curl, CURLOPT_DEBUGDATA, static_cast<Final*>(this));
			}
			else
			{
				curl_easy_setopt(m_curl, CURLOPT_DEBUGFUNCTION, nullptr);
				curl_easy_setopt(m_curl, CURLOPT_DEBUGDATA, nullptr);
			}
		}

		CURLcode fetch()
		{
			return curl_easy_perform(m_curl);
		}

	protected:
		size_type onData(const char* data, size_type length) { return 0; }
		size_type onHeader(const char* data, size_type length) { return 0; }
		size_type onUnderflow(void* data, size_type length) { return 0; }
		bool onProgress(double dltotal, double dlnow, double ultotal, double ulnow) { return true; }
		int onTrace(curl_infotype type, char *data, size_t size) { return 0; }
	private:
#define CURL_(name) static size_t curl_##name(const void * _Str, size_t _Size, size_t _Count, Final* _this) { return (size_t)(_this->name((const char*)_Str, (size_type)_Size * _Count) / _Size); }
		CURL_(onData);
		CURL_(onHeader);
#undef CURL_
		static size_t curl_onUnderflow(void * _Str, size_t _Size, size_t _Count, Final* _this) { return (size_t)(_this->onUnderflow(_Str, (size_type)_Size * _Count) / _Size); }
		static int curl_onProgress(Final* _this, double dltotal, double dlnow, double ultotal, double ulnow) { return _this->onProgress(dltotal, dlnow, ultotal, ulnow) ? 1 : 0; }
		static int curl_onTrace(CURL *handle, curl_infotype type, char *data, size_t size, Final *_this) { return _this->onTrace(type, data, size); }
	};

	class CurlEndpoint;
	class Curl: public CurlBase<Curl>
	{
		int m_status;
		std::string m_statusText;
		std::string m_lastKey;
		Headers m_headers;
		bool m_headersLocked;
		std::weak_ptr<CurlEndpoint> m_owner;
		std::weak_ptr<HttpCallback> m_callback;
		bool m_wasRedirected;
		std::string m_finalLocation;

		void wasRedirected();

	public:
		explicit Curl()
			: m_status(0)
			, m_headersLocked(true)
			, m_wasRedirected(false)
		{
		}

		std::shared_ptr<CurlEndpoint> getOwner() const { return m_owner.lock(); }
		void setOwner(const std::shared_ptr<CurlEndpoint>& owner) { m_owner = owner; }
		std::shared_ptr<HttpCallback> getCallback() const { return m_callback.lock(); }
		void setCallback(const std::shared_ptr<HttpCallback>& callback) { m_callback = callback; }

		void setUrl(const std::string& url)
		{
			m_finalLocation = url;
			CurlBase<Curl>::setUrl(url);
		}

		inline bool isRedirect() const;
		void sendHeaders() const;

		size_type onData(const char* data, size_type length);
		size_type onHeader(const char* data, size_type length);
		size_type onUnderflow(void* data, size_type length) { return 0; } // still not implemented
		inline bool onProgress(double, double, double, double);
		int onTrace(curl_infotype type, char *data, size_t size);
	};

	class CurlEndpoint : public http::HttpEndpoint, public std::enable_shared_from_this<CurlEndpoint>
	{
		std::weak_ptr<HttpCallback> m_callback;
		bool aborting = false;
		curl_slist* headers = nullptr;
		Curl m_curl;

	public:
		CurlEndpoint(const HttpCallbackPtr& obj) : m_callback(obj) {}
		~CurlEndpoint()
		{
			if (headers)
			{
				curl_slist_free_all(headers);
				headers = nullptr;
			}
		}
		void attachTo(const HttpCallbackPtr& obj) { m_callback = obj; }
		void send(bool async) override
		{
			aborting = false;
			if (headers)
			{
				curl_slist_free_all(headers);
				headers = nullptr;
			}

			run(); // sync until job are ported
		}
		void releaseEndpoint() override;
		void abort() override { aborting = true; }
		void appendHeader(const std::string& header) override
		{
			headers = curl_slist_append(headers, header.c_str());
		}

		bool isAborting() const { return aborting; }

		void run();
		std::weak_ptr<HttpCallback> callback() const { return m_callback; }
	};

	inline bool Curl::onProgress(double, double, double, double)
	{
		auto owner = getOwner();
		if (!owner)
			return true;
		return owner->isAborting();
	}

	using CurlEndpointPtr = std::shared_ptr<CurlEndpoint>;

	struct CurlModule : mt::AsyncData
	{
		std::list<CurlEndpointPtr> m_available;
	public:
		static CurlModule& instance()
		{
			static CurlModule _this;
			return _this;
		}

		HttpEndpointPtr getEndpoint(const HttpCallbackPtr& obj)
		{
			Synchronize on(*this);
			if (m_available.empty())
				return std::make_shared<CurlEndpoint>(obj);

			auto endpoint = m_available.front();
			m_available.pop_front();

			endpoint->attachTo(obj);
			return endpoint;
		}

		void makeAvailable(const CurlEndpointPtr& abandoned)
		{
			Synchronize on(*this);
			m_available.push_back(abandoned);
		}
	};

	HttpEndpointPtr GetEndpoint(const HttpCallbackPtr& obj)
	{
		return CurlModule::instance().getEndpoint(obj);
	}

	void CurlEndpoint::releaseEndpoint()
	{
		m_callback.reset();
		m_curl.setCallback(nullptr);
		CurlModule::instance().makeAvailable(shared_from_this());
	}

	void CurlEndpoint::run()
	{
		auto http_callback = m_callback.lock();
		if (!http_callback)
			return;

		http_callback->onStart();

		if (!m_curl)
		{
			http_callback->onError();
			return;
		}

		m_curl.setCallback(http_callback);
		auto owner = m_curl.getOwner();
		if (!owner) // ont-time init
		{
			m_curl.setOwner(shared_from_this());
			m_curl.setConnectTimeout(5);
			m_curl.setUA(getUserAgent());
			m_curl.setProgress();
			m_curl.setSSLVerify(false);
			m_curl.setWrite();
		}
		owner.reset();

		http_callback->appendHeaders();

		m_curl.setUrl(http_callback->getUrl());
		m_curl.setHeaders(headers);

		if (http_callback->shouldFollowLocation())
		{
			m_curl.followLocation();
			long redirs = http_callback->getMaxRedirs();
			if (redirs != 0)
				m_curl.setMaxRedirs(redirs);
		}
		else
		{
			m_curl.dontFollowLocation();
		}

		m_curl.setDebug(http_callback->getDebug());

		size_t length;
		void* content = http_callback->getContent(length);

		if (content && length)
		{
			//curl_httppost; HTTPPOST_CALLBACK;
			m_curl.setPostData(content, length);
		}

		CURLcode ret = m_curl.fetch();
		if (m_curl.isRedirect())
			m_curl.sendHeaders(); // we must have hit max or a circular

		if (ret == CURLE_OK)
			http_callback->onFinish();
		else
			http_callback->onError();
	}

	namespace Transfer
	{
		template <bool equal>
		struct Data { static Curl::size_type onData(const HttpCallbackPtr& http_callback, const char* data, Curl::size_type length); };

		template <>
		struct Data<true> { 
			static Curl::size_type onData(const HttpCallbackPtr& http_callback, const char* data, Curl::size_type length)
			{
				return http_callback->onData(data, (size_t)length);
			}
		};

		template <>
		struct Data<false> { 
			static Curl::size_type onData(const HttpCallbackPtr& http_callback, const char* data, Curl::size_type length)
			{
				Curl::size_type written = 0;
				while (length)
				{
					Curl::size_type chunk = (size_t)-1;
					if (chunk > length) chunk = length;
					length -= chunk;
					size_t st_chunk = (size_t)chunk;
					size_t ret = http_callback->onData(data, st_chunk);
					data += st_chunk;
					written += ret;
					if (ret != st_chunk)
						break;
				}

				return written;
			}
		};

		static Curl::size_type onData(const HttpCallbackPtr& http_callback, const char* data, Curl::size_type length)
		{
			return Data<sizeof(Curl::size_type) <= sizeof(size_t)>::onData(http_callback, data, length);
		}

		template <bool equal>
		struct String { static void append(std::string& out, const char* data, Curl::size_type length); };

		template <>
		struct String<true> {
			static void append(std::string& out, const char* data, Curl::size_type length)
			{
				out.append(data, (size_t)length);
			}
		};

		template <>
		struct String<false> {
			static void append(std::string& out, const char* data, Curl::size_type length)
			{
				while (length)
				{
					Curl::size_type chunk = (std::string::size_type)-1;
					if (chunk > length) chunk = length;
					length -= chunk;
					std::string::size_type st_chunk = (size_t)chunk;
					out.append(data, st_chunk);
					data += st_chunk;
				}
			}
		};

		static void appendString(std::string& out, const char* data, Curl::size_type length)
		{
			return String<sizeof(Curl::size_type) <= sizeof(std::string::size_type)>::append(out, data, length);
		}
	};

	inline bool Curl::isRedirect() const
	{
		/*
		 * 01 
		 * 02 301 Moved Permanently
		 * 04 302 Found
		 * 08 303 See Other
		 * 10 
		 * 20 
		 * 40 
		 * 80 307 Temporary Redirect
		 */
		static int codes = 0x8E;
		static int max_redirect = 7;
#define BIT_POS (m_status % 100)
#define BIT (1 << BIT_POS)
		return m_status / 100 == 3 ? ( BIT_POS <= max_redirect ? (BIT & codes) == BIT : false) : false;
	}

	Curl::size_type Curl::onData(const char* data, size_type length)
	{
		// Redirects should not have bodies anyway
		// And if we redirect, there will be a new header soon...
		if (isRedirect()) return length;

		auto callback = getCallback();
		if (!callback)
			return 0;
		return Transfer::onData(callback, data, length);
	}

#define C_WS while (read < length && isspace((unsigned char)*data)) ++data, ++read;
#define C_NWS while (read < length && !isspace((unsigned char)*data)) ++data, ++read;

	Curl::size_type Curl::onHeader(const char* data, size_type length)
	{
		size_t read = 0;

		bool rn_present = false;
		if (length > 1 && data[length-2] == '\r' && data[length-1] == '\n')
		{
			length -= 2;
			rn_present = true;
		}

		///////////////////////////////////////////////////////////////////////////////////
		//
		//                    FIRST LINE
		//
		///////////////////////////////////////////////////////////////////////////////////
		if (m_headersLocked)
		{
			if (m_status != 0)
			{
				wasRedirected();
			}

			m_headersLocked = false;
			m_status = 0;
			m_statusText.clear();
			m_lastKey.clear();
			m_headers.clear();

			C_NWS;
			C_WS;

			while (read < length && isdigit((unsigned char)*data))
			{
				m_status *= 10;
				m_status += *data - '0';
				++data, ++read;
			}
			if (!isspace((unsigned char)*data)) return read;

			C_WS;
			Transfer::appendString(m_statusText, data, length - read);

			if (rn_present)
				length += 2; // move back the \r\n

			return length;
		}

		///////////////////////////////////////////////////////////////////////////////////
		//
		//                    TERMINATOR
		//
		///////////////////////////////////////////////////////////////////////////////////
		if (length == 0 && rn_present)
		{
			m_headersLocked = true;
			if (!isRedirect())
				sendHeaders();
			return 2;
		}

		///////////////////////////////////////////////////////////////////////////////////
		//
		//                    CONTINUATION
		//
		///////////////////////////////////////////////////////////////////////////////////
		if (length && isspace((unsigned char)*data))
		{
			C_WS;
			std::string& lastValue = m_headers[m_lastKey];
			lastValue += ' ';
			Transfer::appendString(lastValue, data, length - read);

			if (rn_present)
				length += 2; // move back the \r\n

			return length;
		}

		///////////////////////////////////////////////////////////////////////////////////
		//
		//                    ANY OTHER LINE
		//
		///////////////////////////////////////////////////////////////////////////////////
		auto mark = read;
		auto ptr = data;

		while (mark < length && *ptr != ':') ++mark, ++ptr;
		if (mark == length)
		{
			if (rn_present)
				length += 2; // move back the \r\n

			return length;
		}

		auto pos = mark;
		auto last = ptr;

		//rewind
		while (pos > read && isspace((unsigned char)last[-1])) --last, --pos;
		if (pos == read)
		{
			if (rn_present)
				length += 2; // move back the \r\n

			return length;
		}
		std::string key;
		Transfer::appendString(key, data, pos - read);
		std::tolower(key);

		read = mark + 1;
		data = ptr + 1;
		C_WS;

		auto _it = m_headers.find(key);
		if (_it != m_headers.end())
		{
			_it->second += ", ";
			Transfer::appendString(_it->second, data, length - read);
		}
		else
			Transfer::appendString(m_headers[key], data, length - read);

		if (rn_present)
			length += 2; // move back the \r\n

		return length;
	}

	void Curl::wasRedirected()
	{
		m_wasRedirected = true;
		auto it = m_headers.find("location");
		if (it != m_headers.end())
		{
			//the spec says it should be a full URL, but there should be validation anyway here...
			m_finalLocation = it->second;
		}
	}

	void Curl::sendHeaders() const
	{
		auto callback = getCallback();
		if (!callback)
			return;

		if (m_wasRedirected)
			callback->onFinalLocation(m_finalLocation);
		callback->onHeaders(m_statusText, m_status, m_headers);
	}

	int Curl::onTrace(curl_infotype type, char *data, size_t size)
	{
		const char *text = nullptr;

		switch (type)
		{
		case CURLINFO_TEXT:
			//text = "TEXT";
			fprintf(stderr, "# %s", data);
#ifdef WIN32
			OutputDebugStringA("# ");
			OutputDebugStringA(data);
#endif
			return 0;
		default: /* in case a new one is introduced to shock us */
			fprintf(stderr, "trace(%d)\n", type);
			return 0;

		case CURLINFO_HEADER_OUT:
			//text = "HEADER_OUT";
			break;
		case CURLINFO_DATA_OUT:
			//text = "DATA_OUT";
			break;
		case CURLINFO_SSL_DATA_OUT:
			text = "=> Send SSL data";
			return 0;
			break;
		case CURLINFO_HEADER_IN:
			//text = "HEADER_IN";
			break;
		case CURLINFO_DATA_IN:
			//text = "DATA_IN";
			break;
		case CURLINFO_SSL_DATA_IN:
			text = "<= Recv SSL data";
			return 0;
			break;
		}

		if (text) fprintf(stderr, "%s, %ld bytes (0x%lx)\n", text, (long)size, (long)size); 
		fwrite(data, 1, size, stderr);

#ifdef WIN32
		char buffer [8192];
		size_t SIZE = size;
		auto ptr = data;
		while (SIZE)
		{
			size_t s = sizeof(buffer)-1;
			if (s > SIZE) s = SIZE;
			SIZE -= s;

			memcpy(buffer, ptr, s);
			buffer[s] = 0;
			OutputDebugStringA(buffer);
			ptr += s;
		}
		if (size && data[size-1] != '\n')
			OutputDebugStringA("\n");
#endif //WIN32

		return 0;
	}
}
