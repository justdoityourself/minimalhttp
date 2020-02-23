/* Copyright (C) 2020 D8DATAWORKS - All Rights Reserved */

#pragma once

#include <iostream>
#include <string>

#include "Net/TcpServer.hpp"
#include "Net/HttpClient.hpp"
#include "Helper/CommandLine.hpp"
#include "Helper/Encoding.hpp"

using namespace std;
using namespace atk;

template < typename F > int MicroService(const string_view port,F f)
{
	TcpServer http([&](const auto & c)
	{
		//connect
		return make_pair(true, true);
	}, [&](const auto & c)
	{
		//disconnect
	}, [&](auto & client, const auto & request, const auto & body)
	{
		try
		{
			f(client, request, body);
		}
		catch (...)
		{
			client.Http400();
		}
	}, [&](const auto & c)
	{
		//Error
	}, [&](const auto & mc, const auto & c)
	{
		//Write
	});

	http.Open((uint16_t)stoi(port.data()), "", ConnectionType::http);

	Threads().Wait();

	return 0;
}