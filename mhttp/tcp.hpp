/* Copyright (C) 2020 D8DATAWORKS - All Rights Reserved */

#pragma once

#include "common.hpp"

#include <string_view>
#include <thread>

#include "d8u/memory.hpp"

#include "../gsl-lite.hpp"


namespace mhttp
{
	class TcpConnection
	{
		zed_net_socket_t socket = { 0, 0, 0 };
	public:

		TcpConnection(){}
		TcpConnection(const std::string & s, int timeout = 0, int buffer = 128*1024){ Connect(s,timeout,buffer);}
		~TcpConnection() { Close(); }

		void Release() { socket = { 0, 0, 0 }; }

		bool Valid() { return socket.handle != 0; }

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

		void Async(u_long val = 1)
		{
			zed_net_async(&socket, val);
		}

		bool ReadBuffer(int size)
		{
			return zed_net_read_buffer(&socket, size);
		}

		bool WriteBuffer(int size)
		{
			return zed_net_write_buffer(&socket, size);
		}

		bool Connect(const std::string_view s, int timeout = 0,int buffer = 128*1024)
		{
			if (!s.size())
				return false;

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
			return zed_net_tcp_socket_send(&socket,t.data(),(int)t.size());
		}

		template <typename T> int Write(const T & t)
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

		template <typename T> int WriteIf(const T& t)
		{
			auto remaining = t.size();
			uint32_t offset = 0;
			while (remaining)
			{
				auto sent = zed_net_tcp_socket_send(&socket, t.data() + offset, (int)t.size() - offset);

				if (!send && !offset)
					return 0;

				if (sent == -1)
					return -1;
				if (!sent)
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				else
					remaining -= sent;
				offset += sent;
			}

			return (int)offset;
		}

		template <typename T> int Receive( T && t )
		{
			return zed_net_tcp_socket_receive(&socket, (char*)t.data(), (int)t.size());
		}

		template <typename T> int Read(T && t)
		{
			auto remaining = t.size();
			uint32_t offset = 0;
			while(remaining)
			{
				auto read = zed_net_tcp_socket_receive(&socket,t.data()+offset,(int)t.size()-offset);
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

		template <typename T> int ReadIf(T&& t)
		{
			auto remaining = t.size();
			uint32_t offset = 0;
			while (remaining)
			{
				auto read = zed_net_tcp_socket_receive(&socket, t.data() + offset, (int)t.size() - offset);

				if (!read && !offset)
					return 0;

				if (read == -1)
					return -1;
				else if (!read)
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				else
					remaining -= read;
				offset += read;
			}

			return offset;
		}
	};

	class MsgConnection : public TcpConnection
	{
	public:
		MsgConnection() {}

		MsgConnection(const std::string_view s) { Connect(s); }

		void Disconnect()
		{
			std::vector<uint8_t> _null = { 0,0,0,0 };
			Write(_null);
		}

		~MsgConnection()
		{
			Disconnect();
		}

		template < typename T > void SendT(const T& t)
		{
			uint32_t size = (uint32_t)sizeof(T);
			if(sizeof(uint32_t) != Write(gsl::span<uint8_t>((uint8_t*)&size, sizeof(uint32_t))) || sizeof(T) != Write(gsl::span<uint8_t>((uint8_t*)&t, sizeof(T))))
				throw std::runtime_error("Socket disconnected");
		}

		template < typename T > T ReceiveT()
		{
			uint32_t size;
			Read(gsl::span<uint8_t>((uint8_t*)&size, sizeof(uint32_t)));

			if (sizeof(T) != size)
				throw std::runtime_error("Expected Struct not Sent");

			T result;
			if(sizeof(T) != Read(gsl::span<uint8_t>((uint8_t*)&result, sizeof(T))))
				throw std::runtime_error("Socket disconnected");

			return result;
		}

		template < typename T > void SendMessage( const T & t)
		{
			uint32_t size = (uint32_t)t.size();

			if(sizeof(uint32_t) != Write(gsl::span<uint8_t>((uint8_t*)&size, sizeof(uint32_t))) || size != Write(t))
				throw std::runtime_error("Socket disconnected");
		}

		d8u::sse_vector ReceiveMessage()
		{
			uint32_t size;
			if(sizeof(uint32_t) != Read(gsl::span<uint8_t>((uint8_t*)&size, sizeof(uint32_t))))
				throw std::runtime_error("Socket disconnected");

			d8u::sse_vector result(size);
			if(size != Read(result))
				throw std::runtime_error("Socket disconnected");

			return result;
		}

		template < typename T > d8u::sse_vector Transact(const T& t)
		{
			SendMessage(t);
			return ReceiveMessage();
		}

		template < typename ID, typename T > d8u::sse_vector Transact32(const ID& id, const T& t)
		{
			uint32_t size = (uint32_t)t.size();
			if(sizeof(uint32_t) != Write(gsl::span<uint8_t>((uint8_t*)&size, sizeof(uint32_t))) || 32 != Write(gsl::span<uint8_t>((uint8_t*)id.data(), 32)) || size != Write(t))
				throw std::runtime_error("Socket disconnected");

			return ReceiveMessage();
		}
	};
}