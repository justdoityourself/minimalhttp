/* Copyright (C) 2020 D8DATAWORKS - All Rights Reserved */

#pragma once

#include "common.hpp"

#include <string_view>
#include <thread>

/*
	How to use as blocking client:

	Threads().Async([&](bool & run)
	{
		TcpConnection client;

		if(!client.Connect("127.0.0.1:9923"))
			throw "Client failed to connect";

		int sent = client.Write(Memory("\x11" "\x00" "\x00" "\x00" "this is a message"));

		if(sent == -1)
			throw "Client connection failed write";

		std::array<uint8_t,1024> buffer;
		int read = client.Receive(buffer);

		if(read == -1)
			throw "Client connection failed read";
	},2000);

	How to use as Async Server:

	auto onconnect = [](BufferedConnection & c)
	{
		return std::make_pair(true,true);
	};

	auto ondisconnect = [](BufferedConnection & c)
	{
		std::cout << "Connection Closed." << std::endl;
	};

	auto onread = [](BufferedConnection & c,Memory & m)
	{
		std::cout << "Have Read " << m.size() << " bytes." << std::endl;
	};

	auto onwrite = [](BufferedConnection & c, uint32_t id)
	{
		std::cout << "Write Complete." << std::endl;
	};

	auto onerror = [](BufferedConnection & c)
	{
		std::cout << "Connection lost unexpectedly." << std::endl;
	};

	TcpServer<decltype(onconnect),decltype(ondisconnect),decltype(onread),decltype(onerror),decltype(onwrite)> server(onconnect,ondisconnect,onread,onerror,onwrite);

	server.Open(9923,ConnectionType::message);

	Threads().Join([&](const char * exception) { std::cout << exception << std::endl; } );
*/

namespace mhttp
{
	using namespace std;
	class TcpConnection
	{
		zed_net_socket_t socket = { 0, 0, 0 };
	public:

		TcpConnection(){}
		TcpConnection(const std::string & s, int timeout = 0, int buffer = 128*1024){ Connect(s,timeout,buffer);}
		~TcpConnection() { Close(); }

		void Release() { socket = { 0, 0, 0 }; }

		bool Close()
		{
			if(socket.handle)
			{
				zed_net_socket_close(&socket);
				socket.handle = 0;
				return false;
			}
			return true;
		}

		const char * Error() { return zed_net_get_error(); }

		void SetConnection(const TcpConnection & r)
		{
			socket.handle = r.socket.handle;
		}

		bool Listen(uint16_t port, const std::string & options = "", int timeout = 0)
		{
			Close();
			if (zed_net_tcp_socket_open(&socket,port,0,1) == -1) 
				return false;

			if(timeout)
			{
				if (zed_net_timeout(&socket, timeout) < 0) 
					return false;
			}	

			return true;
		}

		bool Accept(TcpConnection & c,zed_net_address_t * address=nullptr)
		{
			return 0 == zed_net_tcp_accept(&socket,&c.socket,address);
		}

		void UseAsync()
		{
			zed_net_async(&socket);
		}

		bool Connect(const string_view s, int timeout = 0,int buffer = 128*1024)
		{
			if(-1 == zed_net_get_address(s,SOCK_STREAM,[&](auto s)->bool
			{
				this->Close();
				if (zed_net_tcp_socket_open(&socket) == -1) 
					return false;

				if(timeout)
				{
					if (zed_net_timeout(&socket, timeout) < 0) 
						return false;
				}	

				if(buffer)
				{
					if (zed_net_buffer(&socket, buffer) < 0) 
						return false;
				}

				if (zed_net_tcp_connect(&socket, s) == -1) 
					return false;

				return true;
			})) return false;

			return true;
		}

		template <typename T> int Send(T && t)
		{
			return zed_net_tcp_socket_send(&socket,t.data(),t.size());
		}

		template <typename T> int Write(T && t)
		{
			auto remaining = t.size();
			uint32_t offset = 0;
			while(remaining)
			{
				auto sent = zed_net_tcp_socket_send(&socket,t.data()+offset,(int) t.size()-offset);
				if(sent == -1)
					return -1;
				if(!sent)
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				else
					remaining -= sent;
				offset += sent;
			}

			return (int)offset;
		}

		template <typename T> int Receive( T && t )
		{
			return zed_net_tcp_socket_receive(&socket, (char*)t.data(), t.size());
		}

		template <typename T> int Read(T && t)
		{
			auto remaining = t.size();
			uint32_t offset = 0;
			while(remaining)
			{
				auto read = zed_net_tcp_socket_receive(&socket,t.data()+offset,t.size()-offset);
				if(read == -1)
					return -1;
				else if(!read)
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				else
					remaining -= read;
				offset += read;
			}

			return offset;
		}
	};
}