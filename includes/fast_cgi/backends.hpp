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

#ifndef __BACKENDS_HPP__
#define __BACKENDS_HPP__

namespace FastCGI
{
	namespace impl
	{
		class LibFCGIRequest: public RequestBackend
		{
			fcgi_streambuf m_streambufCin;
			fcgi_streambuf m_streambufCout;
			fcgi_streambuf m_streambufCerr;
			std::ostream m_cout;
			std::ostream m_cerr;
			std::istream m_cin;
		public:
			LibFCGIRequest(FCGX_Request& request);
			std::ostream& cout() { return m_cout; }
			std::ostream& cerr() { return m_cerr; }
			std::istream& cin() { return m_cin; }
		};

		class LibFCGIThread: public ThreadBackend
		{
			FCGX_Request m_request;
		public:
			void init() { FCGX_InitRequest(&m_request, 0, 0); }
			const char * const* envp() const { return m_request.envp; }
			bool accept() { return FCGX_Accept_r(&m_request) == 0; }
			void release() { FCGX_Finish_r(&m_request); }
			std::shared_ptr<RequestBackend> newRequestBackend() { return std::shared_ptr<RequestBackend>(new (std::nothrow) LibFCGIRequest(m_request)); }
		};

		class STLRequest: public RequestBackend
		{
		public:
			std::ostream& cout() { return std::cout; }
			std::ostream& cerr() { return std::cerr; }
			std::istream& cin() { return std::cin; }
		};

		class STLThread: public ThreadBackend
		{
			char* REQUEST_URI;
			char* QUERY_STRING;
			const char* environment[8];
		public:
			STLThread(const char* uri);
			~STLThread();
			void init() {}
			const char * const* envp() const { return environment; }
			bool accept() { return false; }
			void release() { }
			std::shared_ptr<RequestBackend> newRequestBackend() { return std::shared_ptr<RequestBackend>(new (std::nothrow) STLRequest()); }
		};
	};
}

#endif //__BACKENDS_HPP__