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

namespace http
{
	static std::string getOSVersion()
	{
#if defined WIN32
		OSVERSIONINFO verinfo = { sizeof(OSVERSIONINFO) };
		std::string ret = "Windows";
		GetVersionEx(&verinfo);
		if (verinfo.dwPlatformId == VER_PLATFORM_WIN32_NT) ret += " NT";
		char ver[200];
		sprintf_s(ver, " %d.%d", verinfo.dwMajorVersion, verinfo.dwMinorVersion);
		ret += ver;
		return ret;
#else
		utsname name;
		if (uname(&name))
			return PLATFORM;
		return name.sysname;
#endif
	}

	std::string getUserAgent()
	{
		std::string ret = "reedr/1.0 (";
		ret += getOSVersion();
		ret += ")";
		return ret;
	}

	class HttpHeaderBuilder
	{
		std::list<std::string> lines;
	public:
		void clear() { lines.clear(); }
		bool appendHeaderData(const char* data, size_t len)
		{
			std::string current;

			const char* last = data + len;
			const char* ptr;
			for (; data != last; ++data)
			{
				switch (*data)
				{
				case '\r': break;
				case '\n':
					ptr = current.c_str();
					if (!ptr || !*ptr) return true;
					lines.push_back(current);
					current.clear();
					break;
				default:
					current+=*data;
					break;
				};
			}

			return false;
		}

		void parseHeaders(std::string& reason, int& http_status, Headers& headers) const
		{
			if (lines.size() == 0) return;

			std::string last_key;
			std::string key;
			std::list<std::string>::const_iterator
				_cur = lines.begin(), _end = lines.end();

			const unsigned char* ptr = (const unsigned char*)_cur->c_str();
			while (*ptr && !isspace(*ptr)) ++ptr;
			while (*ptr && isspace(*ptr)) ++ptr;

			http_status = 0;
			while (isdigit(*ptr))
			{
				http_status *= 10;
				http_status += (*ptr++ - '0');
			}
			while (*ptr && !isspace(*ptr)) ++ptr;
			while (*ptr && isspace(*ptr)) ++ptr;

			reason = (const char*)ptr;

			++_cur;
			for (; _cur != _end; ++_cur)
			{
				const char* ptr = _cur->c_str();
				if (isspace((unsigned char)*ptr))
				{
					while (*ptr && isspace((unsigned char)*ptr)) ++ptr;
					std::string& last_value = headers[last_key];
					last_value += " ";
					last_value += (const char*)ptr;
					continue;
				}
				const char* colon = strchr(ptr, ':');
				if (!colon) continue;
				const char* last = colon;
				++colon;

				while (isspace((unsigned char)last[-1])) --last;
				key.clear();
				key.reserve(last - ptr + 1);
				while (ptr < last) key += (char)tolower((unsigned char)*ptr++);

				while (isspace((unsigned char)*colon)) ++colon;
				Headers::iterator _it = headers.find(key);
				if (_it != headers.end())
				{
					_it->second += ", ";
					_it->second += colon;
				}
				else headers[key] = colon;
			}
		};

	};

	struct CurlAsyncJob: http::ConnectionCallback
	{
		typedef HttpCallbackPtr Init;
		Init ref;
		HttpHeaderBuilder resp_builder;
		bool aborting;
		curl_slist* headers;

		CurlAsyncJob(const Init& init): ref(init), aborting(false), headers(nullptr) {}
		~CurlAsyncJob() {}
		void Run();
		void Start(bool async) { Run(); } // sync until job are ported

		size_t OnData(const void * _Str, size_t _Size, size_t _Count);
		size_t OnHeaders(const void * _Str, size_t _Size, size_t _Count);

		void abort() { aborting = true; }
		bool isAborting() const { return aborting; }
		void appendHeader(const std::string& header)
		{
			headers = curl_slist_append(headers, header.c_str());
		}

	};

	void Init()
	{
		curl_global_init(CURL_GLOBAL_ALL);
	}

	void Send(HttpCallbackPtr ref, bool async)
	{
		try {
			auto job = std::make_shared<CurlAsyncJob>(ref);
			job->Start(async);
		} catch(std::bad_alloc) {}
	}

	struct curl_f
	{
		static size_t _fwrite(const void * _Str, size_t _Size, size_t _Count, CurlAsyncJob* _this)
		{
			if (_this->isAborting())
				return 0;
			_this->OnData(_Str, _Size, _Count);
			return _Count;
		}
		static size_t _fwrite_header(const char* _Str, size_t _Size, size_t _Count, CurlAsyncJob* _this)
		{
			if (_this->isAborting())
				return 0;
			return _this->OnHeaders(_Str, _Size, _Count);
		}
		static size_t _fread(void * _Str, size_t _Size, size_t _Count, CurlAsyncJob* _this)
		{
		}
		static int _progress(CurlAsyncJob* _this, double, double, double, double)
		{
			return _this->isAborting() ? 1 : 0;
		}
		static int my_trace(CURL *handle, curl_infotype type, char *data, size_t size, void *userp)
		{
			const char *text = nullptr;
			(void)handle; /* prevent compiler warning */

			switch (type)
			{
			case CURLINFO_TEXT:
				fprintf(stderr, "# %s", data);
#ifdef WIN32
				OutputDebugStringA("# ");
				OutputDebugStringA(data);
#endif
			default: /* in case a new one is introduced to shock us */
				return 0;

			case CURLINFO_HEADER_OUT:
			case CURLINFO_DATA_OUT:
				break;
			case CURLINFO_SSL_DATA_OUT:
				text = "=> Send SSL data";
				return 0;
				break;
			case CURLINFO_HEADER_IN:
			case CURLINFO_DATA_IN:
				break;
			case CURLINFO_SSL_DATA_IN:
				text = "<= Recv SSL data";
				return 0;
				break;
			}

			if (text) fprintf(stderr, "%s, %10.10ld bytes (0x%8.8lx)\n", text, (long)size, (long)size); 
			fwrite(data, 1, size, stderr);
			if (size && data[size-1] != '\n')
				fprintf(stderr, "\n");

#ifdef WIN32
			char buffer [8192];
			size_t SIZE = size;
			while (SIZE)
			{
				size_t s = sizeof(buffer)-1;
				if (s > SIZE) s = SIZE;
				SIZE -= s;

				memcpy(buffer, data, s);
				buffer[s] = 0;
				OutputDebugStringA(buffer);
			}
			if (size && data[size-1] != '\n')
				OutputDebugStringA("\n");
#endif //WIN32

			return 0;
		}
	};

	void CurlAsyncJob::Run()
	{
		resp_builder.clear();
		ref->onStart(this);

		CURL* curl = curl_easy_init();
		if (!curl)
		{
			ref->onError();
			return;
		}
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
		curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, curl_f::_progress);
		curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, this);
		curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, curl_f::my_trace);
		if (ref->getDebug()) curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
		//curl_easy_setopt(curl, CURLOPT_CERTINFO, 1L);
		//curl_easy_setopt(curl, CURLOPT_SSL_CTX_FUNCTION, curl_ssl_ctx_callback); 


		{
			ref->appendHeaders();

			curl_easy_setopt(curl, CURLOPT_URL, ref->getUrl().c_str());
			curl_easy_setopt(curl, CURLOPT_USERAGENT, getUserAgent().c_str());
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

			curl_easy_setopt(curl, CURLOPT_WRITEHEADER, this);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_f::_fwrite);
			curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curl_f::_fwrite_header);

			size_t length;
			void* content = ref->getContent(length);

			if (content && length)
			{
				//curl_httppost; HTTPPOST_CALLBACK;
				curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, length);
				curl_easy_setopt(curl, CURLOPT_POSTFIELDS, content);
			}
		}
		//if (req.body != std::string())
		//	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.body.c_str());
		CURLcode ret = curl_easy_perform(curl);

		/* always cleanup */
		curl_easy_cleanup(curl);

		if (ret == CURLE_OK)
			ref->onFinish();
		else
			ref->onError();
	}

	size_t CurlAsyncJob::OnData(const void * _Str, size_t _Size, size_t _Count)
	{
		return ref->onData(_Str, _Size * _Count) / _Size;
	}

	size_t CurlAsyncJob::OnHeaders(const void * _Str, size_t _Size, size_t _Count)
	{
		if (resp_builder.appendHeaderData((const char*)_Str, _Size*_Count))
		{
			std::string reason;
			int http_status;
			Headers headers;
			resp_builder.parseHeaders(reason, http_status, headers);
			ref->onHeaders(reason, http_status, headers);
		}
		return _Count;
	}
}
