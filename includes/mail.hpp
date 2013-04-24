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

#ifndef __MAIL_HPP__
#define __MAIL_HPP__

#include <memory>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <filter.hpp>

namespace mail
{
	using filter::Filter;
	using filter::FilterPtr;
	using filter::BufferingFilter;
	using filter::BufferingFilterPtr;
	using filter::Base64;
	using filter::Base64Block;
	using filter::Quoting;
	using filter::QuotedPrintable;

	SHAREABLE(MimePart);
	SHAREABLE(ImagePart);
	SHAREABLE_STRUCT(TextProducer);
	SHAREABLE(TextPart);
	SHAREABLE(Multipart);
	SHAREABLE_STRUCT(MessageProducer);
	SHAREABLE(Message);
	SHAREABLE(PostOffice);

	typedef std::pair<std::string, std::string> Header;
	typedef std::vector<Header> Headers;

	class MimePart
	{
		friend class Message;

		Headers m_headers;
		FilterPtr m_outFilter;
		FilterPtr m_inFilter;
		FilterPtr m_downstream;
		BufferingFilterPtr m_buffer;
		std::string m_fileName;
	protected:
		FilterPtr sink() { return m_inFilter; }
		FilterPtr downstream() { return m_downstream; }
		void setFileName(const std::string& fileName) { m_fileName = fileName; }
	public:
		virtual ~MimePart() {}
		MimePart(const std::string& mime, const std::string& encoding = std::string())
		{
			addHeader("Content-Type", mime);
			if (!encoding.empty())
			{
				addHeader("Content-Transfer-Encoding", encoding);
				if (encoding == "base64")
				{
					m_outFilter = std::make_shared<Base64Block>();
					m_inFilter = std::make_shared<Base64>();
				}
				else
				{
					m_outFilter = std::make_shared<QuotedPrintable>();
					m_inFilter = std::make_shared<Quoting>();
				}
				m_inFilter->pipe(m_outFilter);
			}
		}

		virtual bool addLength() const { return true; }

		virtual void pipe(const FilterPtr& downstream)
		{
			if (!m_buffer && addLength())
			{
				m_buffer = std::make_shared<BufferingFilter>();
				if (m_outFilter)
					m_outFilter->pipe(m_buffer);
				else
					m_inFilter = m_outFilter = m_buffer;
			}

			m_downstream = downstream; // will be used for header
			if (m_buffer)
				m_buffer->pipe(downstream);
			else if (m_outFilter)
				m_outFilter->pipe(downstream);
			else
				m_inFilter = m_outFilter = downstream;
		}

		void addHeader(const std::string& header, const std::string& value)
		{
			std::string head = std::tolower(header);
			for (auto& val : m_headers)
			{
				if (head == std::tolower((const std::string &)val.first))
				{
					val.second = value;
					return;
				}
			}
			m_headers.emplace_back(header, value);
		}

		virtual void echo();
		virtual void echoHeaders();
		virtual void echoBody() = 0;
	};

	class ImagePart: public MimePart
	{
		std::string m_cid;
		std::string m_path;
	public:
		ImagePart(size_t part, const std::string& machine, const std::string& mime, const std::string& path);
		const std::string& getCID() const { return m_cid; }
		void echoBody() override;
	};

	struct TextProducer
	{
		virtual ~TextProducer() {}
		virtual void echo(const FilterPtr& downstream) = 0;
	};

	class TextPart: public MimePart
	{
		TextProducerPtr m_producer;
	public:
		TextPart(const std::string& mime, const TextProducerPtr& producer)
			: MimePart(mime, "Quoted-printable")
			, m_producer(producer)
		{
		}

		void echoBody() override { m_producer->echo(sink()); }
	};

	class Multipart: public MimePart
	{
	protected:
		std::string m_boundary;
		std::vector<MimePartPtr> m_parts;
		static std::string fullMime(const std::string& mime, const std::string& boundary)
		{
			static const char pre[] = "; boundary=\"";
			std::string out;
			out.reserve(mime.size() + boundary.size() + sizeof(pre) + 1);
			out.append(mime);
			out.append(pre, sizeof(pre) - 1);
			out.append(boundary);
			out.append("\"", 1);
			return std::move(out);
		}
	public:
		Multipart(const std::string& mime, const std::string& boundary)
			: MimePart(fullMime(mime, boundary))
			, m_boundary(boundary)
		{
		}
		bool addLength() const override { return false; }

		void echoBody() override;
		void pipe(const FilterPtr& downstream) override;

		void push_back(const MimePartPtr& part) { m_parts.push_back(part); }
	};

	class Address
	{
		std::string m_name;
		std::string m_mail;
	public:
		Address() {}
		Address(const Address& a): m_name(a.m_name), m_mail(a.m_mail) {}
		Address(Address&& a): m_name(std::move(a.m_name)), m_mail(std::move(a.m_mail)) {}
		Address(const std::string& name, const std::string& mail): m_name(name), m_mail(mail) {}
		Address& operator=(const Address& a)
		{
			m_name = a.m_name;
			m_mail = a.m_mail;
			return *this;
		}
		Address& assign(const std::string& name, const std::string& mail)
		{
			m_name = name;
			m_mail = mail;
			return *this;
		}
		std::string format() const
		{
			if (m_name.empty())
				return m_mail;
			return Base64::header(m_name, "<\",") + " <" + m_mail + ">";
		}
		const std::string& name() const { return m_name; }
		const std::string& mail() const { return m_mail; }
	};

	struct MessageProducer
	{
		virtual ~MessageProducer() {}
		virtual void echoPlain(const FilterPtr& downstream) = 0;
		virtual void echoHtml(const FilterPtr& downstream) = 0;
	};

	class Message: public Multipart
	{
		friend class PostOffice;

		typedef std::vector<Address> Addresses;
		size_t m_partsSoFar;
		std::string m_subject;
		std::string m_machine;
		Address m_from;
		Addresses m_to;
		Addresses m_cc;
		Addresses m_bcc;
		MimePartPtr m_text;
		MimePartPtr m_html;
		MultipartPtr m_msg;

		void post();
		std::string getSender() const;
		std::vector<std::string> getRecipients() const;
	public:
		Message(const std::string& subject, const std::string& boundary, const std::string& machine, const MessageProducerPtr& producer);
		void echo() override;
		bool addLength() const override { return !m_html; }

		void setSubject(const std::string& subject) { m_subject = subject; }
		void setFrom(const std::string& name, const std::string& mail) { m_from.assign(name, mail); }
		void addTo(const std::string& name, const std::string& mail) { m_to.push_back(Address(name, mail)); }
		void addCc(const std::string& name, const std::string& mail) { m_cc.push_back(Address(name, mail)); }
		void addBcc(const std::string& name, const std::string& mail) { m_bcc.push_back(Address(name, mail)); }

		ImagePartPtr createInlineImage(const std::string& mime, const std::string& path);
	};

	class PostOffice
	{
		static int send(const std::string& from, const std::vector<std::string>& to, const MessagePtr& ptr);
	public:
		static void post(const MessagePtr& ptr, bool async);
		static MessagePtr newMessage(const std::string& subject, const MessageProducerPtr& producer);
	};
}

using mail::PostOffice;

#endif // __MAIL_HPP__