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
#include <mail.hpp>
#include <crypt.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <future>

#if WIN32
#include <direct.h>
#include <io.h>
#include <fcntl.h>

static inline int pipe(int* fd) { return _pipe(fd, 2048, O_BINARY); }

#else
#include <unistd.h>
#define _getcwd getcwd
#define _read read
#define _close close
#endif

namespace mail
{
	void MimePart::echo()
	{
		if (m_fileName.empty())
			addHeader("Content-Disposition", "inline");
		else
			addHeader("Content-Disposition", "inline; filename=" + m_fileName);

		if (m_buffer)
		{
			echoBody();

			std::ostringstream str;
			str << m_buffer->size();
				
			addHeader("Content-Length", str.str());

			echoHeaders();

			m_downstream->put("\r\n");
			m_buffer->flush();
			m_downstream->put("\r\n");

			return;
		}

		echoHeaders();
		m_downstream->put("\r\n");
		echoBody();
		m_downstream->put("\r\n");
	}

	void MimePart::echoHeaders()
	{
		for (auto& head: m_headers)
		{
			m_downstream->put(head.first);
			m_downstream->put(": ");
			m_downstream->put(head.second);
			m_downstream->put("\r\n");
		}
	}

	static std::string makeCID(size_t part, const std::string& machine)
	{
		static size_t globalPartCounter = 0;
		std::ostringstream out;
		out << part << '.' << ++globalPartCounter << '.' << _getpid() << '.' << tyme::now() << '@' << machine;
		return std::move(out.str());
	}

#ifdef WIN32
#define SEP '\\'
#else
#define SEP '/'
#endif

	ImagePart::ImagePart(size_t part, const std::string& machine, const std::string& mime, const std::string& path)
		: MimePart(mime, "base64")
		, m_cid(std::move(makeCID(part, machine)))
		, m_path(path)
	{
		std::string::size_type pos = path.find_last_of(SEP);
		if (pos != std::string::npos)
			setFileName(path.substr(pos + 1));
		else
			setFileName(path);
		addHeader("Content-ID", "<" + m_cid + ">");
	}

	void ImagePart::echoBody()
	{
		FilterPtr filter = sink();
		if (!filter)
			return;

		char buffer[2048];
		_getcwd(buffer, 1024);
		strcat(strcat(buffer, "\\"), m_path.c_str());
		std::ifstream file(m_path, std::ios::in | std::ios::binary);
		char c;
		while (file.get(c))
			filter->put(c);
	}

	void Multipart::echoBody()
	{
		auto raw = downstream();
		for (auto& part : m_parts)
		{
			raw->put("--");
			raw->put(m_boundary);
			raw->put("\r\n");
			part->echo();
		}

		raw->put("--");
		raw->put(m_boundary);
		raw->put("--\r\n\r\n");
	}

	void Multipart::pipe(const FilterPtr& downstream)
	{
		MimePart::pipe(downstream);
		for (auto part: m_parts)
		{
			part->pipe(downstream);
		}
	}

	template <typename Upstream>
	struct MethodProducer: TextProducer
	{
		std::shared_ptr<Upstream> m_upstream;
		void (Upstream::* m_method)(const FilterPtr&);
		MethodProducer(const std::shared_ptr<Upstream>& upstream, void (Upstream::* method)(const FilterPtr&))
			: m_upstream(upstream)
			, m_method(method)
		{
		}

		void echo(const FilterPtr& downstream) override
		{
			(m_upstream.get()->*m_method)(downstream);
		}
	};

	template <typename Upstream>
	TextProducerPtr make_producer(const std::shared_ptr<Upstream>& upstream, void (Upstream::* method)(const FilterPtr&))
	{
		return std::make_shared<MethodProducer<Upstream>>(upstream, method);
	}

	Message::Message(const std::string& subject, const std::string& boundary, const std::string& machine, const MessageProducerPtr& producer)
		: Multipart("multipart/related", boundary)
		, m_partsSoFar(3)
		, m_subject(subject)
		, m_machine(machine)
		, m_text(std::make_shared<TextPart>("text/plain; charset=utf-8", make_producer(producer, &MessageProducer::echoPlain)))
		, m_html(std::make_shared<TextPart>("text/html; charset=utf-8", make_producer(producer, &MessageProducer::echoHtml)))
		, m_msg(std::make_shared<Multipart>("multipart/alternative", boundary + "_message"))
	{
		char now[100];
		tyme::strftime(now, "%a, %d %b %Y %H:%M:%S GMT", tyme::gmtime(tyme::now()));

		Crypt::md5_t hash;
		Crypt::md5(now, hash);

		std::string msgId;
		msgId.reserve(machine.size() + sizeof(hash) + 1);
		msgId.append(hash, sizeof(hash) - 1);
		msgId.push_back('@');
		msgId += machine;

		addHeader("Date", now);
    	addHeader("Message-ID", msgId);
    	addHeader("X-Mailer", "reedr/1.0");
    	addHeader("MIME-Version", "1.0");
		m_msg->push_back(m_text);
		m_msg->push_back(m_html);
		push_back(m_msg);
	}

	ImagePartPtr Message::createInlineImage(const std::string& mime, const std::string& path)
	{
		auto img = std::make_shared<ImagePart>(++m_partsSoFar, m_machine, mime, path);
		push_back(img);
		return img;
	}

	void Message::echo()
	{
		echoHeaders();
		downstream()->put("\r\n");
		echoBody();
	}

	std::string join(const std::vector<Address>& addresses)
	{
		std::string out;
		bool first = true;
		for (auto& address: addresses)
		{
			if (first) first = false;
			else out += ", ";
			out += address.format();
		}
		return std::move(out);
	}

	template <typename Pred>
	std::string join(const std::vector<Address>& addresses, Pred pred)
	{
		std::string out;
		bool first = true;
		for (auto& address: addresses)
		{
			if (first) first = false;
			else out += ", ";
			out += pred(address);
		}
		return std::move(out);
	}

	void Message::send(PostOffice& service)
	{
		std::string to;

		addHeader("Subject", Base64::header(m_subject));
		addHeader("From", m_from.format());
		if (!m_cc.empty()) addHeader("Cc", join(m_cc));
		if (!m_bcc.empty()) addHeader("Bcc", join(m_bcc));

		addHeader("To", join(m_to));
		std::vector<std::string> tos;
		tos.reserve(m_to.size());
		std::transform(
			m_to.begin(), m_to.end(),
			std::back_inserter(tos),
			[](const Address& a) { return a.mail(); }
		);
		std::transform(
			m_cc.begin(), m_cc.end(),
			std::back_inserter(tos),
			[](const Address& a) { return a.mail(); }
		);
		std::transform(
			m_bcc.begin(), m_bcc.end(),
			std::back_inserter(tos),
			[](const Address& a) { return a.mail(); }
		);
		service.send(m_from.mail(), tos, this);
	}

	PostOffice::PostOffice()
	{
	}

	void PostOffice::connect(const std::string& machine, const std::string& user, const std::string& password)
	{
		m_machine = machine;
		m_user = user;
		m_password = password;
	}

	void PostOffice::post(const MessagePtr& ptr)
	{
		ptr->send(*this);
	}

	size_t Message_fread(void *buffer, size_t size, size_t count, void* pfile)
	{
		int file = (int)pfile;
		return _read(file, buffer, size * count) / size;
	}

	void PostOffice::send(const std::string& from, const std::vector<std::string>& to, Message* ptr)
	{
		if (to.empty())
			return;

		auto curl = curl_easy_init();
		struct curl_slist *recipients = nullptr;

		if (!curl)
			return;
		curl_easy_setopt(curl, CURLOPT_URL, ("smtp://" + m_machine).c_str());
		curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
		curl_easy_setopt(curl, CURLOPT_USERNAME, m_user.c_str());
		curl_easy_setopt(curl, CURLOPT_PASSWORD, m_password.c_str());
		curl_easy_setopt(curl, CURLOPT_MAIL_FROM, from.c_str());

		for (auto& addr: to)
			recipients = curl_slist_append(recipients, addr.c_str());
		curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

		int fd[2];
		if (pipe(fd) == -1)
			return;

		ptr->pipe(std::make_shared<filter::FdFilter>(fd[1]));

		auto future = std::async(std::launch::async, [&fd, ptr]()
		{
			ptr->echo();
			_close(fd[1]);
		});

		curl_easy_setopt(curl, CURLOPT_READFUNCTION, Message_fread);
		curl_easy_setopt(curl, CURLOPT_READDATA, (void*)fd[0]);

		auto res = curl_easy_perform(curl);

		_close(fd[0]);

		if(res != CURLE_OK)
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

		/* free the list of recipients and clean up */ 
		curl_slist_free_all(recipients);
		curl_easy_cleanup(curl);

		future.get();
	}

	MessagePtr PostOffice::newMessage(const std::string& subject, const MessageProducerPtr& producer)
	{
		char now[100];
		tyme::strftime(now, "%a, %d %b %Y %H:%M:%S GMT", tyme::gmtime(tyme::now()));

		Crypt::md5_t boundary;
		Crypt::md5(now, boundary);

		return std::make_shared<Message>(subject, boundary, m_machine, producer);
	}

}
