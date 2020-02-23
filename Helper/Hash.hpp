/* Copyright D8Data 2019 */

#pragma once

#include <type_traits>
#include <cryptopp/sha.h>

#include "Helper/Template.hpp"

namespace atk
{
	using namespace std;

	template < typename T, typename H > en_if(sizeof(T) == 16, void) HashT(T & t, const H & h)
	{
		Hash<32> dest;
		CryptoPP::SHA256 hh;
		hh.CalculateDigest(dest.data(), h.data(), h.size());

		memcpy(&t, &dest, 16);
	}

	template < typename T, typename H > en_if(sizeof(T) == 24,void) HashT(T & t, const H & h)
	{
		Hash<32> dest;
		CryptoPP::SHA256 hh;
		hh.CalculateDigest(dest.data(), h.data(), h.size());

		memcpy(&t, &dest, 24);
	}

	template < typename T, typename H > en_if(sizeof(T) == 32, void) HashT(T & t, const H & h)
	{
		CryptoPP::SHA256 hh;
		hh.CalculateDigest((uint8_t *)&t, h.data(), h.size());
	}

	template < typename T, typename H > en_if(sizeof(T) == 64, void) HashT(T & t, const H & h)
	{
		CryptoPP::SHA512 hh;
		hh.CalculateDigest((uint8_t *)&t, h.data(), h.size());
	}
}