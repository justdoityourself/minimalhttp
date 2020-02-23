/* Copyright D8Data 2017 */

#pragma once

namespace AtkUtility
{
	template <typename T> std::string bytes_as_string(const T & memory, int delemeter_space = 0, char delemeter = ' ' )
	{
		std::string ret;

		auto data = (uint8_t*)memory.data();

		uint32_t character_count = (uint32_t)memory.size() * 2;
		uint32_t delemeter_count = 0;
		if ( delemeter_space )
		{
			delemeter_space /= 2;
			delemeter_count = character_count / delemeter_space;
		}

		ret.reserve(character_count + delemeter_count + 1);

		for ( uint32_t i = 0; i < memory.size(); i++)
		{
			ret += "0123456789ABCDEF" [ * data / 16 ];
			ret += "0123456789ABCDEF" [ * data ++ % 16 ];

			if(delemeter_space) { if(i % delemeter_space == 0) ret += delemeter; }
		}

		return ret;
	}

	template <typename T, typename L> void string_as_bytes ( T & memory, const L & string )
	{
		char temp[3];
		temp[2] = '\0';

		auto source = string.data();
		uint8_t* destination = memory.data();
		for (uint32_t i = 0; i < memory.size() && i * 2 < string.size(); i++ )
		{
			temp[0] = (char)*source++;
			temp[1] = (char)*source++;

			int32_t result;
			sscanf(temp, "%X", &result);
			*destination++ = (uint8_t)result;
		}
	}
}