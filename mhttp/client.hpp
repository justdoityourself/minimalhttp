/* Copyright (C) 2020 D8DATAWORKS - All Rights Reserved */

#pragma once

#include <thread>
#include <chrono>
#include <string_view>

#include "connection.hpp"

namespace mhttp
{
	template < typename T > class ThreadedClientT : public T
	{
		std::thread reader;
		std::thread writer;
		bool run = true;

		using callback_t = std::function<void( std::vector < uint8_t > , gsl::span<uint8_t> )>;

		callback_t read;

	public:
		~ThreadedClientT()
		{
			run = false;
			reader.join();
			writer.join();

			T::Disconnect();
		}

		ThreadedClientT(std::string_view host, ConnectionType type, callback_t _read, bool writer_thread = true)
			: T(host,type)
			, read(_read)
			, reader([&]()
			{
				while (!T::connection_state)
					std::this_thread::sleep_for(std::chrono::milliseconds(50));

				T::Async(); T::multiplex = true;

				while(run)
				{
					bool idle = true;
					if (!DoRead(*this, [&](T & c, std::vector<uint8_t> v, gsl::span<uint8_t> s)
						{
							read(std::move(v), s);

						} , idle))
						break;

					if(idle)
						std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}

				//Todo clean up reads, todo reconnect?

				if (run) std::cout << "ThreadedClientT Reader EOT" << std::endl;
			})
			, writer([&]()
			{
				while (!T::connection_state)
					std::this_thread::sleep_for(std::chrono::milliseconds(50));

				T::Async(); T::multiplex = true;

				if (!writer_thread)
					return;

				while (run)
				{
					bool idle = true;
					if (!DoWrite(*this, [](auto&, size_t) {}, idle))
						break;

					if (idle)
						std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}

				//Todo clean up write, todo reconnect?

				if (run) std::cout << "ThreadedClientT Writer EOT" << std::endl;
			}) {  }
	};

	template < typename T > class EventClientT : public T
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

		~EventClientT()
		{
			Flush();

			run = false;
			reader.join();
			writer.join();

			T::Disconnect();
		}

		EventClientT(std::string_view host, ConnectionType type, bool writer_thread = true)
			: T(host,type)
			, reader([&, v = host.size() != 0]()
			{
				if (!v) return;

				while(!T::connection_state)
					std::this_thread::sleep_for(std::chrono::milliseconds(50));

				if (!T::Valid())
				{
					std::cout << "EventClientT Reader Failed to Connect" << std::endl;
					return;
				}

				T::Async();

				while(run)
				{
					bool idle = true;
					if (!DoRead(*this, [&](T & c, std::vector<uint8_t> v, gsl::span<uint8_t> s)
						{
							callback_t cb;

							{
								std::lock_guard<std::mutex> lock(T::ql);

								if (!v.size())
								{
									//Ping to validate connection is alive. For Long polling...
									return;
								}

								cb = read_events.front();
								read_events.pop();
							}
							
							cb(std::move(v), s);

						} , idle))
						break;

					if(idle)
						std::this_thread::sleep_for(std::chrono::milliseconds(10));
				}

				//Todo clean up reads, todo reconnect?

				if(run) 
					std::cout << "EventClientT Reader EOT" << std::endl;
			})
			, writer([&, v = host.size() != 0]()
			{
				if (!v) return;

				while (!T::connection_state)
					std::this_thread::sleep_for(std::chrono::milliseconds(50));

				if (!T::Valid())
				{
					std::cout << "EventClientT Writer Failed to Connect" << std::endl;
					return;
				}

				T::Async();

				if (!writer_thread)
					return;

				while (run)
				{
					bool idle = true;
					if (!DoWrite(*this, [](auto&, size_t) {}, idle))
						break;

					if (idle)
						std::this_thread::sleep_for(std::chrono::milliseconds(10));
				}

				//Todo clean up writes, todo reconnect?

				if (run) 
					std::cout << "EventClientT Writer EOT" << std::endl;
			}) {  }

		void AsyncWriteCallback(std::vector<uint8_t>&& v, callback_t &&f)
		{
			std::lock_guard<std::mutex> lock(T::ql);

			T::queue.push({ true,std::move(v) });

			read_events.emplace(f);
		}

		template < typename TT > void AsyncWriteCallbackT(const TT & t, callback_t &&f)
		{
			std::vector<uint8_t> v(sizeof(TT));
			std::copy((uint8_t*)&t, (uint8_t *)(&t + 1), v.begin());

			AsyncWriteCallback(std::move(v), std::move(f));
		}

		std::pair<std::vector<uint8_t>,gsl::span<uint8_t>> AsyncWriteWait(std::vector<uint8_t>&& v)
		{
			bool ready = false;
			std::vector<uint8_t> result;
			gsl::span<uint8_t> body;

			{
				std::lock_guard<std::mutex> lock(T::ql);

				T::queue.push({ true,std::move(v) });

				read_events.push([&](auto v, auto b) 
				{
					result = std::move(v);
					body = b;
					ready = true;
				});
			}

			while(!ready)
				std::this_thread::sleep_for(std::chrono::milliseconds(10));

			return std::make_pair(std::move(result),body);
		}

		template < typename TT > auto AsyncWriteWaitT(const TT& t)
		{
			std::vector<uint8_t> v(sizeof(TT));
			std::copy((uint8_t*)&t, (uint8_t*)(&t + 1), v.begin());

			return AsyncWriteWait(std::move(v));
		}
	};

	using ThreadedClient = ThreadedClientT< sock_t >;
	using EventClient = EventClientT< sock_t >;
}