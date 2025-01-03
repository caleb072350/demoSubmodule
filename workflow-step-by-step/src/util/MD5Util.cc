#include <openssl/evp.h>
#include <stdio.h>
#include <string.h>
#include <stdexcept>
#include "MD5Util.h"

static inline void __md5(const std::string& str, unsigned char *md)
{
	EVP_MD_CTX *ctx = EVP_MD_CTX_new();
	if (!ctx)
		throw std::runtime_error("Failed to create EVP_MD_CTX");
	if (EVP_DigestInit_ex(ctx, EVP_md5(), nullptr) != 1)
	{
		EVP_MD_CTX_free(ctx);
		throw std::runtime_error("EVP_DigestInit_ex failed");
	}
	if (EVP_DigestUpdate(ctx, str.c_str(), str.size()) != 1)
	{
		EVP_MD_CTX_free(ctx);
		throw std::runtime_error("EVP_DigestUpdate failed");
	}
	if (EVP_DigestFinal_ex(ctx, md, nullptr) != 1)
    {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("EVP_DigestFinal_ex failed");
    }

    EVP_MD_CTX_free(ctx);
}

std::string MD5Util::md5_bin(const std::string& str)
{
	unsigned char md[16];

	__md5(str, md);
	return std::string((const char *)md, 16);
}

static inline char __hex_char(int v)
{
	return v < 10 ? '0' + v : 'a' + v - 10;
}

static inline void __plain_hex(char *s, int ch)
{
	*s = __hex_char(ch / 16);
	*(s + 1) = __hex_char(ch % 16);
}

std::string MD5Util::md5_string_32(const std::string& str)
{
	unsigned char md[16];
	char out[32];

	__md5(str, md);
	for (int i = 0; i < 16; i++)
		__plain_hex(out + (i * 2), md[i]);

	return std::string((const char *)out, 32);
}

std::string MD5Util::md5_string_16(const std::string& str)
{
	unsigned char md[16];
	char out[16];

	__md5(str, md);
	for (int i = 0; i < 8; i++)
		__plain_hex(out + (i * 2), md[i + 4]);

	return std::string((const char *)out, 16);
}

std::pair<uint64_t, uint64_t> MD5Util::md5_integer_32(const std::string& str)
{
	unsigned char md[16];
	std::pair<uint64_t, uint64_t> res;

	__md5(str, md);
	memcpy(&res.first, md, sizeof (uint64_t));
	memcpy(&res.second, md + 8, sizeof (uint64_t));
	return res;
}

uint64_t MD5Util::md5_integer_16(const std::string& str)
{
	unsigned char md[16];
	uint64_t res;

	__md5(str, md);
	memcpy(&res, md + 4, sizeof (uint64_t));
	return res;
}