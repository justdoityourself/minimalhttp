/* Copyright D8Data 2019 */

#pragma once

#include <chrono>
#include <atomic>
#include <list>

#include "Common.hpp"
#include "TcpProtocols.hpp"
#include "Helper/Async.hpp"

namespace atk
{
	template < typename T > class BasicConnection : public T
	{
	public:
		BasicConnection():pending(0){}
		std::atomic<uint32_t> pending;// = 0;

		ConnectionType type;
		TcpAddress address;

		bool reader_fault = false;
		bool writer_fault = false;

		uint32_t uid = 0;
		uint32_t priority = 0;

		std::chrono::high_resolution_clock::time_point connection_time = std::chrono::high_resolution_clock::now();
		std::chrono::high_resolution_clock::time_point last_message  = std::chrono::high_resolution_clock::now();
	};

	template < typename T > class BufferedConnection : public BasicConnection<T>
	{
	public:
		BufferedConnection(T &c, TcpAddress &a, ConnectionType t,uint32_t _uid)
		{
			BasicConnection<T>::address = a;
			BasicConnection<T>::SetConnection(c);
			BasicConnection<T>::uid = _uid;
			BasicConnection<T>::type = t;
			read_buffer.other = 0;
		}

		Memory read_buffer;
		uint32_t read_count = 0;
		uint64_t read_bytes = 0;

		Memory write_buffer;
		uint32_t current_id;
		ThreadQueue2<Memory> write_queue;
		uint32_t write_count = 0;
		uint64_t write_bytes = 0;

		uint32_t QueueWrite(Memory m)
		{
			uint32_t write_id = write_count ++;
			m.other = write_id;
			write_queue.Push(m);
			return write_id;
		}

		template < typename ... t_args > uint32_t WriteJson2(uint32_t max_message_size, t_args...args)
		{
			return QueueWrite(Common::RawJsonMessage(self_t,max_message_size,args...));
		}

		template < typename ... t_args > uint32_t WriteJsonHttp(HttpCommand command,const std::string & path,Memory contents,t_args...args)
		{
			return QueueWrite(Common::HttpJsonMessage(command,path,contents,args...));
		}

		template < typename ... t_args > uint32_t HttpResponse(Memory proto, Memory status, Memory contents, t_args...args)
		{
			return QueueWrite(Common::HttpResponse(proto,status, contents, args..., Memory("Access-Control-Allow-Origin: *\r\n")));
		}

		template < typename ... t_args > uint32_t Http200F(Memory proto, Memory contents, t_args...args)
		{
			return QueueWrite(Common::HttpResponse(proto, Memory("200 OK"), contents, Memory("Access-Control-Allow-Origin: *\r\n"),args...));
		}

		template < typename ... t_args > uint32_t Http200(t_args...args)
		{
			return QueueWrite(Common::HttpResponse(Memory("HTTP/1.1"), Memory("200 OK"), Memory(),Memory("Access-Control-Allow-Origin: *\r\n"), args...));
		}

		template <typename F, typename ... t_args> uint32_t HttpStream200(uint32_t size, F f, t_args...args)
		{
			return QueueWrite(Common::HttpResponseStream(size,f,Memory("HTTP/1.1"), Memory("200 OK"), Memory("Access-Control-Allow-Origin: *\r\n"), args...));
		}

		uint32_t Http400(Memory proto = Memory("HTTP/1.1"))
		{
			return QueueWrite(Common::HttpResponse(proto, Memory("400 Bad Request"),Memory(), Memory("Access-Control-Allow-Origin: *\r\n")));
		}

		uint32_t Http401(Memory proto = Memory("HTTP/1.1"))
		{
			return QueueWrite(Common::HttpResponse(proto, Memory("401 Unauthorized"), Memory(), Memory("Access-Control-Allow-Origin: *\r\n")));
		}

		template < typename ... t_args > uint32_t WriteAsync(const std::string & s)
		{
			Memory m;m.Create(s.size());
			m.free = 0;
			std::memcpy(m.data(),s.data(),s.size());
			return QueueWrite(m);
		}
	};
}