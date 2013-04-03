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

#ifndef __FCGI_THREAD_HPP__
#define __FCGI_THREAD_HPP__

#include <mt.hpp>
#include <fstream>

namespace db
{
	struct Connection;
	typedef std::shared_ptr<Connection> ConnectionPtr;
}

namespace FastCGI
{
	class Thread;
	class Request;
	class Session;
	class Application;
	typedef std::shared_ptr<Thread> ThreadPtr;
	typedef std::shared_ptr<Session> SessionPtr;

	namespace impl
	{
		struct RequestBackend;

		struct ThreadBackend
		{
			virtual void init() {}
			virtual const char * const* envp() const = 0;
			virtual bool accept() = 0;
			virtual void release() = 0;
			virtual std::shared_ptr<RequestBackend> newRequestBackend() = 0;
		};
	};

	class Thread: public mt::Thread, public mt::AsyncData
	{
		friend class Request;

		Application* m_app;
		db::ConnectionPtr m_dbConn;
		std::shared_ptr<impl::ThreadBackend> m_backend;
	public:
		Thread();
		explicit Thread(const char* uri);
		virtual ~Thread();

		bool init();
		void setApplication(Application& app) { m_app = &app; }

		Application* app() { return m_app; }
		db::ConnectionPtr dbConn(Request& request);
		SessionPtr getSession(Request& request, const std::string& sessionId);
		SessionPtr startSession(Request& request, const char* email);
		void endSession(Request& request, const std::string& sessionId);

		const char * const* envp() const { return m_backend->envp(); }
		bool accept();
		void handleRequest();
		virtual void onRequest(Request& request) = 0;

		void run();
	};
}

#endif //__FCGI_THREAD_HPP__