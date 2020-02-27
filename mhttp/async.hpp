/* Copyright (C) 2020 D8DATAWORKS - All Rights Reserved */

#pragma once

#include <future>
#include <list>
#include <queue>
#include <chrono>
#include <condition_variable>

namespace mhttp
{
	class ThreadHub
	{
	public:
		ThreadHub() { Start(); }
		~ThreadHub()
		{
			Stop();
		}

		void Stop()
		{
			if(!run) return;

			run = false;
			Wait();
		}

		void Signal(){ run = false; }
		void Start(){ run = true; }

		void Wait()
		{
			for(auto & i : threads)
				i.get();

			threads.clear();

			Start();
		}

		void Join()
		{
			Wait();
		}

		template <typename F> void Join(F f)
		{	
			bool active = true;
			while(active)
			{
				try 
				{ 
					Wait(); 
					active = false;
				}
				catch(const char * m) { f( m ); }
				catch(...) { f( "Unknown Exception." ); }
			}
		}

		void Delay(uint32_t d) 
		{ 
			std::this_thread::sleep_for(std::chrono::milliseconds(d)); 
		}

		template < typename F > void Async(F f,uint32_t d = 0) 
		{ 
			if(d) 
				Delay(d); 
			
			threads.push_back( std::future<void>( std::async(std::launch::async,f, std::ref(run)) ) ); 
		}

		std::list<std::future<void>> threads;
		bool run=false;
	};

	template <typename T> class ThreadQueue // MCMP
	{
	public:
		ThreadQueue(){}

		void Push(T && e) 
		{
			std::unique_lock<std::mutex> lck(mtx);

			q.push(std::move(e));
			cv.notify_one();
		}

		bool Try(T&e)
		{
			std::unique_lock<std::mutex> lck(mtx);

			if(!q.empty()) 
			{
				e = std::move(q.front());
				q.pop();
				return true;
			}

			return false;
		}

		bool Next(bool & run, T & r) 
		{
			std::unique_lock<std::mutex> lck(mtx);
			while(run)
			{
				if ( cv.wait_for( lck, std::chrono::milliseconds(1000), [&] {return !q.empty(); } ) )
				{
					r = std::move(q.front());
					q.pop();
					return true;
				}
			}

			return false;
		}
	private:
		std::queue<T> q;

		std::mutex mtx;
		std::condition_variable cv;
	};

	ThreadHub& Threads() { static ThreadHub hub; return hub; }

	template < typename T, typename t_handler > class EventHandler : public ThreadQueue<T>
	{
		ThreadHub & threads;
		t_handler handler;
	public:
		EventHandler(t_handler h,ThreadHub & _threads= Threads()) 
			: handler(h)
			, threads(_threads) {}

		EventHandler(size_t c, t_handler h, ThreadHub & _threads= Threads()) 
			: handler(h)
			, threads(_threads) 
		{
			Start(c);
		}

		void Start(size_t c)
		{
			for(size_t i = 0; i< c; i++)
				threads.Async( [&](bool&run) 
				{ 
					while(run) 
					{
						T evt;
						if(ThreadQueue<T>::Next(run, evt))
							handler(std::move(evt));
					}
				});
		}
	};
}