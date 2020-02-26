/* Copyright (C) 2020 D8DATAWORKS - All Rights Reserved */

#pragma once

#include <thread>

#include "Common.hpp"

namespace mhttp
{
	class UdpConnection
	{
		zed_net_socket_t socket = { 0, 0, 0 };
	public:

		UdpConnection(){}
		UdpConnection(const std::string & s, int timeout = 0){Connect(s,timeout);}
		~UdpConnection() { Close(); }

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

		bool Listen(unsigned short port, int timeout = 0)
		{
			throw "TODO";
		}

		bool Connect(const std::string & s, int timeout = 0)
		{
			if(-1 == zed_net_get_address(s,SOCK_DGRAM,[&](auto s)->bool
			{
				this->Close();
				if (zed_net_udp_socket_open(&socket) == -1) 
					return false;

				if(timeout)
				{
					if (zed_net_timeout(&socket, timeout) < 0) 
						return false;
				}	

				if (zed_net_udp_connect(&socket, s) == -1) 
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
				auto sent = zed_net_tcp_socket_send(&socket,t.data()+offset,t.size()-offset);
				if(!sent)
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				else
					remaining -= sent;
				offset += sent;
			}
		}

		template <typename T> int Receive( T && t, bool*timeout = nullptr )
		{
			return zed_net_tcp_socket_receive(&socket, (char*)t.data(), t.size(),timeout);
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