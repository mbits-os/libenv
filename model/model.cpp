/*
 * Copyright (C) 2013 Aggregate
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

#include <dbconn.h>
#include <model.h>

#include <time.h>
#include <openssl/crypto.h>
#include <openssl/sha.h>
#include <openssl/md5.h>
#include <openssl/rand.h>

namespace data
{
	namespace sha
	{
		struct SHA512
		{
			bool m_inited;
			SHA512_CTX m_ctx;
		public:
			SHA512()
			{
				m_inited = SHA512_Init(&m_ctx) != 0;
			}
			~SHA512()
			{
				if (m_inited)
					OPENSSL_cleanse(&m_ctx, sizeof(m_ctx));
			}
			bool Update(const void* msg, size_t len)
			{
				if (!m_inited) return false;
				return SHA512_Update(&m_ctx, msg, len) != 0;
			}
			bool Final(SHA512Hash::digest_t& digest)
			{
				if (!m_inited) return false;
				return SHA512_Final(digest, &m_ctx) != 0;
			}
		};
	};

	namespace md
	{
		struct MD5
		{
			bool m_inited;
			MD5_CTX m_ctx;
		public:
			MD5()
			{
				m_inited = MD5_Init(&m_ctx) != 0;
			}
			~MD5()
			{
				if (m_inited)
					OPENSSL_cleanse(&m_ctx, sizeof(m_ctx));
			}
			bool Update(const void* msg, size_t len)
			{
				if (!m_inited) return false;
				return MD5_Update(&m_ctx, msg, len) != 0;
			}
			bool Final(MD5Hash::digest_t& digest)
			{
				if (!m_inited) return false;
				return MD5_Final(digest, &m_ctx) != 0;
			}
		};
	};

	bool SHA512Hash::__crypt(const char* salt, const char* message, size_t len, digest_t &digest)
	{
		sha::SHA512 alg;
		if (!alg.Update(salt, SALT_LENGTH)) return false;
		if (!alg.Update(message, len)) return false;
		return alg.Final(digest);
	}

	bool MD5Hash::__crypt(const char* salt, const char* message, size_t len, digest_t &digest)
	{
		md::MD5 alg;
		if (!alg.Update(salt, SALT_LENGTH)) return false;
		if (!alg.Update(message, len)) return false;
		return alg.Final(digest);
	}

	struct random
	{
		random()
		{
			time_t t;
			srand(time(&t));
		}
	};

	static char alphabet(size_t id)
	{
		static char alph[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-|";
		return alph[id];
	}

	template <class T>
	T sslrand()
	{
		T ret;
		RAND_bytes((unsigned char*)&ret, sizeof(ret));
		return ret;
	}

	static char randomChar()
	{
		return alphabet(sslrand<unsigned char>() >> 2);
	}

	void newSalt(char* salt, size_t len)
	{
		salt[--len] = 0;
		for (size_t i = 0; i < len; ++i)
			salt[i] = randomChar();
	}

	void base64_encode(const void* data, size_t len, char* output)
	{
		const unsigned char* p = (const unsigned char*)data;
		size_t pos = 0;
		unsigned int bits = 0;
		unsigned int accu = 0;
		for (size_t i = 0; i < len; ++i)
		{
			accu = (accu << 8) | (p[i] & 0xFF);
			bits += 8;
			while (bits >= 6)
			{
				bits -= 6;
				output[pos++] = alphabet((accu >> bits) & 0x3F);
			}
		}
		if (bits > 0)
		{
			accu <<= 6 - bits;
			output[pos++] = alphabet(accu & 0x3F);
		}
	}
};