/* Copyright D8Data 2019 */

#pragma once

#include <iostream>
#include <string_view>

namespace atk
{
	using namespace std;

	template < typename ... args_t > auto _CommandLine(int argc, char * argv[], args_t ... args)
	{
		int c = 1;
		return make_tuple(string_view(((c < argc) ? argv[c++] : args))...);
	}

	template < typename ... args_t > auto CommandLine(int argc, char * argv[], args_t ... args)
	{
		auto result = _CommandLine(argc, argv, args...);

		cout << argv[0] << " ";

		std::apply([&](auto& ...x) {(..., (cout << x << " ")); }, result);

		cout << endl;

		return result;
	}
}