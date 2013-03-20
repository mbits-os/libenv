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

#ifndef __LIBENV_CRYPT_HPP__
#define __LIBENV_CRYPT_HPP__

namespace crypt
{
	void newSalt(char* salt, size_t len);

	template <size_t len>
	void newSalt(char (&salt)[len])
	{
		newSalt(salt, len);
	}

	// the output should be at least (len * 8 + 5) DIV 6 long
	void base64_encode(const void* data, size_t len, char* output);

	template <typename T, size_t salt_length, size_t digest_size, char alg_id>
	struct HashBase
	{
		enum {
			SALT_LENGTH = salt_length,
			SALT_SIZE = SALT_LENGTH + 1,
			DIGEST_SIZE = digest_size,
			PAYLOAD_LENGTH = (DIGEST_SIZE * 8 + 5) / 6,
			HASH_SIZE = PAYLOAD_LENGTH + SALT_LENGTH + 2,
			ALG_ID = alg_id
		};
		typedef char salt_t[SALT_SIZE];
		typedef char hash_t[HASH_SIZE];
		typedef unsigned char digest_t[DIGEST_SIZE];
		static bool verify(const char* message, const char* hash)
		{
			if (!message || !*message || !hash || !*hash)
				return false;

			size_t msg_len = strlen(message);
			size_t hash_len = strlen(hash);

			// is there enough data in the hash to hold the salt and the id?
			if (hash_len != HASH_SIZE - 1)
				return false;

			const char* salt = hash + hash_len - 1;

			if (*salt != ALG_ID)
				return false;

			salt -= SALT_LENGTH;
			digest_t digest;

			if (!T::__crypt(salt, message, msg_len, digest))
				return false;

			hash_t new_hash;
			pack(digest, (salt_t&)salt, new_hash);

			return strcmp(new_hash, hash) == 0;
		}

		static bool crypt(const char* message, hash_t& hash)
		{
			if (!message || !*message)
				return false;

			salt_t salt;
			digest_t digest;
			newSalt(salt);
			if (!T::__crypt(salt, message, strlen(message), digest))
				return false;

			pack(digest, salt, hash);			
			return true;
		}

		static void pack(const digest_t& digest, const salt_t& salt, hash_t& hash)
		{
			base64_encode(digest, DIGEST_SIZE, hash);
			strcpy(hash + PAYLOAD_LENGTH, salt);
			hash[HASH_SIZE - 2] = ALG_ID;
			hash[HASH_SIZE - 1] = 0;
		}
	};

	struct SHA512Hash: HashBase<SHA512Hash, 16, 64, '5'>
	{
		static bool __crypt(const char* salt, const char* message, size_t len, digest_t &digest);
	};

	struct MD5Hash: HashBase<MD5Hash, 12, 16, 'M'>
	{
		static bool __crypt(const char* salt, const char* message, size_t len, digest_t &digest);
	};

	typedef SHA512Hash PasswordHash;
	typedef MD5Hash SessionHash;

	typedef PasswordHash::hash_t password_t;
	typedef SessionHash::hash_t session_t;

	inline bool password(const char* message, password_t& hash)
	{
		return PasswordHash::crypt(message, hash);
	}

	inline bool session(const char* message, session_t& hash)
	{
		return SessionHash::crypt(message, hash);
	}

	bool verify(const char* message, const char* hash);
};

#endif //__LIBENV_CRYPT_HPP__
