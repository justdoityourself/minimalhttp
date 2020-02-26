/* Copyright (C) 2020 D8DATAWORKS - All Rights Reserved */

#pragma once

#include <future>
#include <list>
#include <queue>
#include <chrono>

namespace mhttp
{
	class ThreadHub
	{
	public:
		ThreadHub():run(true){}
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

		void Delay(uint32_t d) { std::this_thread::sleep_for(std::chrono::milliseconds(d)); }

		template < typename F > void Async(F f,uint32_t d = 0) { if(d) Delay(d); threads.push_back(std::future<void>(std::async(std::launch::async,f, std::ref(run)))); }

		std::list<std::future<void>> threads;
		bool run;
	};

	template <typename T> class ThreadQueue // MCMP
	{
	public:
		ThreadQueue(){}

		void Push(T e) 
		{
			std::unique_lock<std::mutex> lck(mtx);

			q.push(e);
			cv.notify_one();
		}

		bool Try(T&e)
		{
			std::unique_lock<std::mutex> lck(mtx);

			if(!q.empty()) 
			{
				e = q.front();
				q.pop();
				return true;
			}

			return false;
		}

		T Next(bool * lifetime = nullptr) 
		{
			std::unique_lock<std::mutex> lck(mtx);
			while(true)
			{
				if(lifetime) cv.wait(lck, [&]{return !q.empty() && *lifetime;});
				else cv.wait(lck, [&]{return !q.empty();});

				if(!q.empty()) 
				{
					T r = q.front();
					q.pop();
					return r;
				}
			}

			return T();
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

		EventHandler(uint32_t c, t_handler h, ThreadHub & _threads= Threads()) 
			: handler(h)
			, threads(_threads) 
		{
			Start(c);
		}

		void Start(uint32_t c)
		{
			for(uint32_t i = 0; i< c; i++) 
				threads.Async( [&](bool&run) 
				{ 
					while(run) 
						handler( ThreadQueue<T>::Next(&run) ); 
				});
		}
	};
}