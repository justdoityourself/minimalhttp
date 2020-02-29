/* Copyright (C) 2020 D8DATAWORKS - All Rights Reserved */

#pragma once

#include <thread>
#include <chrono>
#include <string_view>

#include "connection.hpp"

namespace mhttp
{
	template < typename T > class ThreadedClient : public T
	{
		std::thread reader;
		std::thread writer;
		bool run = true;

		using callback_t = std::function<void( std::vector < uint8_t > , gsl::span<uint8_t> )>;

		callback_t read;

	public:
		~ThreadedClient()
		{
			run = false;
			reader.join();
			writer.join();
		}

		ThreadedClient(string_view host, ConnectionType type, callback_t _read, bool writer_thread = true)
			: T(host,type)
			, read(_read)
			, reader([&]()
			{
				while(run)
				{
					bool idle = false;
					if (!DoRead(*this, [&](T & c, std::vector<uint8_t> v, gsl::span<uint8_t> s)
						{
							read(std::move(v), s);

						} , idle))
						break;

					if(idle)
						std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}
			})
			, writer([&]()
			{
				if (!writer_thread)
					return;

				while (run)
				{
					bool idle = false;
					if (!DoWrite(*this, [](auto&, size_t) {}, idle))
						break;

					if (idle)
						std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}
			}) { T::Async(); T::multiplex = true; }
	};

	template < typename T > class EventClient : public T
	{
		std::thread reader;
		std::thread writer;
		bool run = true;

		using callback_t = std::function<void( std::vector < uint8_t > , gsl::span<uint8_t> )>;
		std::queue< callback_t > read_events;

	public:

		void Flush()
		{
			while (read_events.size())
				std::this_thread::sleep_for(std::chrono::milliseconds(300));
		}

		size_t Reads() { return T::read_count; }
		size_t Writes() { return T::write_count; }

		void WaitUntilReads(size_t target) 
		{ 
			while (Reads() < target)
				std::this_thread::sleep_for(std::chrono::milliseconds(300));
		}

		void WaitUntilWrites(size_t target) 
		{ 
			while (Writes() < target)
				std::this_thread::sleep_for(std::chrono::milliseconds(300));
		}

		~EventClient()
		{
			run = false;
			reader.join();
			writer.join();
		}

		EventClient(string_view host, ConnectionType type, bool writer_thread = true)
			: T(host,type)
			, reader([&]()
			{
				while(run)
				{
					bool idle = false;
					if (!DoRead(*this, [&](T & c, std::vector<uint8_t> v, gsl::span<uint8_t> s)
						{
							callback_t cb;

							{
								std::lock_guard<std::mutex> lock(T::ql);
								cb = read_events.front();
								read_events.pop();
							}
							
							cb(std::move(v), s);

						} , idle))
						break;

					if(idle)
						std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}
			})
			, writer([&]()
			{
				if (!writer_thread)
					return;

				while (run)
				{
					bool idle = false;
					if (!DoWrite(*this, [](auto&, size_t) {}, idle))
						break;

					if (idle)
						std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}
			}) { T::Async(); }

		void AsyncWriteCallback(std::vector<uint8_t>&& v, callback_t &&f)
		{
			std::lock_guard<std::mutex> lock(T::ql);

			T::queue.push(std::move(v));

			read_events.emplace(f);
		}

		template < typename TT > void AsyncWriteCallbackT(const TT & t, callback_t &&f)
		{
			std::vector<uint8_t> v(sizeof(TT));
			std::copy(&t, &t + 1, v.begin());

			AsyncWriteCallback(std::move(v), f);
		}

		std::pair<std::vector<uint8_t>,gsl::span<uint8_t>> AsyncWriteWait(std::vector<uint8_t>&& v)
		{
			bool ready = false;
			std::vector<uint8_t> result;
			gsl::span<uint8_t> body;

			{
				std::lock_guard<std::mutex> lock(T::ql);

				T::queue.push(std::move(v));

				read_events.push([&](auto v, auto b) 
				{
					result = std::move(v);
					body = b;
					ready = true;
				});
			}

			while(!ready)
				std::this_thread::sleep_for(std::chrono::milliseconds(100));

			return std::make_pair(std::move(result),body);
		}

		template < typename TT > std::vector<uint8_t> AsyncWriteWaitT(const TT& t)
		{
			std::vector<uint8_t> v(sizeof(TT));
			std::copy(&t, &t + 1, v.begin());

			return AsyncWriteWait(v);
		}
	};
}