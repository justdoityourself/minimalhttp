/* Copyright (C) 2020 D8DATAWORKS - All Rights Reserved */

#pragma once

#include <chrono>
#include <atomic>
#include <list>

#include "common.hpp"
#include "protocols.hpp"

namespace mhttp
{
	template < typename T > class BasicConnection : public T
	{
	public:
		BasicConnection() : pending(0) {}

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
		BufferedConnection() {}

		BufferedConnection(T &c, TcpAddress &a, ConnectionType t,uint32_t _uid)
		{
			Init(c, a, t, _uid);
		}

		void Init(T& c, TcpAddress& a, ConnectionType t, uint32_t _uid)
		{
			BasicConnection<T>::address = a;
			BasicConnection<T>::SetConnection(c);
			BasicConnection<T>::uid = _uid;
			BasicConnection<T>::type = t;
		}

		size_t read_offset = 0;
		std::vector<uint8_t> read_buffer;

		size_t read_count = 0;
		size_t read_bytes = 0;

		size_t write_offset = 0;
		std::vector<uint8_t> write_buffer;

		size_t write_count = 0;
		size_t write_bytes = 0;

		std::mutex ql;
		std::queue<std::vector<uint8_t>> queue;

		bool TryWrite(std::vector<uint8_t>& v)
		{
			std::lock_guard<std::mutex> lock(ql);

			if (!queue.size())
				return false;

			v = std::move(queue.front());
			queue.pop();

			return true;
		}

		void AsyncWrite(std::vector<uint8_t> && v)
		{
			std::lock_guard<std::mutex> lock(ql);

			queue.push(std::move(v));
		}
	};
}