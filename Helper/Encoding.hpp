/* Copyright D8Data 2019 */

#pragma once

#include  "Helper/Memory.hpp"

#include <array>
#include <string>
#include <string_view>

namespace atk
{
	using namespace std;

	string base64_encode(Memory input)
	{
		string out; out.reserve(input.size() * 4 / 3 + 4);

		int val = 0, valb = -6;
		for (uint8_t c : input)
		{
			val = (val << 8) + c;
			valb += 8;
			while (valb >= 0)
			{
				out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(val >> valb) & 0x3F]);
				valb -= 6;
			}
		}
		if (valb > -6) out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[((val << 8) >> (valb + 8)) & 0x3F]);
		while (out.size() % 4) out.push_back('=');
		return out;
	}

	template<typename R> R base64_decode_t(const string_view in)
	{
		R out;
		auto itr = (uint8_t *)&out;

		array<int, 256> T = { -1 };
		for (int i = 0; i < 64; i++) T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;

		int val = 0, dx = 0, valb = -8;
		for (uint8_t c : in) 
		{
			if (T[c] == -1 || dx == sizeof(R)) break;

			val = (val << 6) + T[c];
			valb += 6;

			if (valb >= 0) 
			{
				dx++;
				*itr ++ = (char((val >> valb) & 0xFF));
				valb -= 8;
			}
		}

		return out;
	}

	ManagedMemory base64_decode(const string_view in)
	{
		ManagedMemory out; out.reserve(in.size() * 3 / 4 + 4);

		array<int, 256> T = { -1 };
		for (int i = 0; i < 64; i++) T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;

		int val = 0, valb = -8;
		for (uint8_t c : in) {
			if (T[c] == -1) break;
			val = (val << 6) + T[c];
			valb += 6;
			if (valb >= 0) {
				out.push_back(char((val >> valb) & 0xFF));
				valb -= 8;
			}
		}
		return out;
	}

	string url_encode(Memory input)
	{
		string out; out.reserve(input.size() * 4 / 3 + 4);

		int val = 0, valb = -6;
		for (uint8_t c : input)
		{
			val = (val << 8) + c;
			valb += 8;
			while (valb >= 0)
			{
				out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._"[(val >> valb) & 0x3F]);
				valb -= 6;
			}
		}
		if (valb > -6) out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._"[((val << 8) >> (valb + 8)) & 0x3F]);
		while (out.size() % 4) out.push_back('-');
		return out;
	}

	template<typename R> R url_decode_t(const string_view in)
	{
		R out;
		auto itr = (uint8_t *)&out;

		array<int, 256> T = { -1 };
		for (int i = 0; i < 64; i++) T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._"[i]] = i;

		int val = 0, dx = 0, valb = -8;
		for (uint8_t c : in)
		{
			if (T[c] == -1 || dx == sizeof(R)) break;

			val = (val << 6) + T[c];
			valb += 6;

			if (valb >= 0)
			{
				dx++;
				*itr++ = (char((val >> valb) & 0xFF));
				valb -= 8;
			}
		}

		return out;
	}

	ManagedMemory url_decode(const string_view in)
	{
		ManagedMemory out; out.reserve(in.size() * 3 / 4 + 4);

		array<int, 256> T = { -1 };
		for (int i = 0; i < 64; i++) T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._"[i]] = i;

		int val = 0, valb = -8;
		for (uint8_t c : in) {
			if (T[c] == -1) break;
			val = (val << 6) + T[c];
			valb += 6;
			if (valb >= 0) {
				out.push_back(char((val >> valb) & 0xFF));
				valb -= 8;
			}
		}
		return out;
	}
}