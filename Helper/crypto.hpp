/* Copyright D8Data 2019 */

#pragma once

#include "Crypto/TinySHA1.hpp" //While it is handy to use libraries, this one sucks :D. Uses non-standard binary format that must be corrected. ( Written for bigendian double words )
#include "Memory.hpp"

namespace atk
{
	template < uint32_t C > class Hash : public std::array<uint8_t,C> {};

	template < typename T > Hash<20> Sha1(const T & m)
	{
		sha1::SHA1 s;
		Hash<20> _result,result;
		s.processBytes((uint8_t*)m.data(),m.size());

		s.getDigest((uint32_t*)_result.data());	

		//Fix stupid library.
		for(uint32_t i = 0; i < 5; i++)
			for(uint32_t j = 0; j < 4; j++)
				result[(i*4) + j] = _result[(i*4) + (3 - j)];

		return result;
	}

	std::string Base64Encode(Memory input) 
	{
		std::string out;

		int val=0, valb=-6;
		for (uint8_t c : input) 
		{
			val = (val<<8) + c;
			valb += 8;
			while (valb>=0) 
			{
				out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(val>>valb)&0x3F]);
				valb-=6;
			}
		}
		if (valb>-6) out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[((val<<8)>>(valb+8))&0x3F]);
		while (out.size()%4) out.push_back('=');
		return out;
	}
}