/* Copyright (C) 2020 D8DATAWORKS - All Rights Reserved */

#pragma once

#include "common.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <map>
#include <string_view>
#include <memory>

#include "../gsl-lite.hpp"
#include "d8u/buffer.hpp"

namespace mhttp
{
	using namespace std;
	using namespace d8u::buffer;

	class Common
	{
	public:

		template < typename C, typename T > static bool Initialize(C& c, T type) { return true; } //Protocol based on connect details.

		template < typename C, typename M > static bool WriteMessage(C & c, M m,bool & idle)
		{
			while(true)
			{
				if(c.write_buffer.size())
				{
					auto r = c.Send( gsl::span<uint8_t>( c.write_buffer.data() + c.write_offset,c.write_buffer.size() - c.write_offset ) );

					if(!r) break;

					if(r == -1)
						return false;

					idle = false;

					c.write_offset += r;
					if(c.write_offset != c.write_buffer.size())
						return true;

					c.write_bytes += c.write_buffer.size();
					m(c, c.write_count++);

					c.write_buffer = std::vector<uint8_t>();
					c.write_offset = 0;
				}
				
				if(!c.TryWrite(c.write_buffer))
					break;

				uint32_t size = (uint32_t)c.write_buffer.size();
				if (sizeof(uint32_t) != c.Send(gsl::span<uint8_t>((uint8_t*)&size, sizeof(uint32_t))))
					return false;
			}

			return true;
		}

		template < typename C, typename M > static bool WriteRaw(C& c, M m, bool& idle)
		{
			while (true)
			{
				if (c.write_buffer.size())
				{
					auto r = c.Send(gsl::span<uint8_t>(c.write_buffer.data() + c.write_offset, c.write_buffer.size() - c.write_offset));

					if (!r) break;

					if (r == -1)
						return false;

					idle = false;

					c.write_offset += r;
					if (c.write_offset != c.write_buffer.size())
						return true;

					c.write_bytes += c.write_buffer.size();
					m(c, c.write_count++);

					c.write_buffer = std::vector<uint8_t>();
					c.write_offset = 0;
				}

				if (!c.TryWrite(c.write_buffer))
					break;
			}

			return true;
		}
	};

	class Http
	{
	public:

		struct Request
		{
			std::string_view type;
			std::string_view path;
			std::string_view proto;
			std::string_view command;

			std::vector<uint8_t> raw;
			gsl::span<uint8_t> body;

			std::map<std::string_view, std::string_view> headers;
			std::map<std::string_view, std::string_view> parameters;
		};

		static Request ParseRequest(std::vector<uint8_t> && _m, gsl::span<uint8_t> body)
		{
			Request result;

			Helper m(_m);
			auto cm = m.GetLine();
	
			result.type = cm.GetWord(); 
			Helper path = cm.GetWord(), command;
			result.proto = cm.GetWord(); 
			
			std::tie(path,command) = path.Divide('?');

			result.path = path;
			result.command = command;
			result.body = body;

			Helper line = m.GetLine();
			while(line.size())
			{
				auto k = line.GetWord(), v = line.GetWord();
				result.headers[k] = v;
				line = m.GetLine();
			}

			while (command.size())
			{
				auto k = command.GetWord('='), v = command.GetWord('&');
				result.parameters[k] = v;
			}

			result.raw = std::move(_m);

			return result;
		}

		struct Response
		{
			std::string_view proto;
			std::string_view _status;
			std::string_view msg;

			std::vector<uint8_t> raw;
			gsl::span<uint8_t> body;

			int status = 0;

			std::map<std::string_view, std::string_view> headers;
		};

		static Response ParseResponse(vector<uint8_t> && _m, gsl::span<uint8_t> body)
		{
			Response result;

			Helper m(_m);
			Helper cm = m.GetLine();
			Helper proto = cm.GetWord(), status = cm.GetWord(), msg= cm.GetWord();

			result.proto = proto;
			result._status = status;
			result.msg = msg;
			result.body = body;

			result.status = status;

			Helper line = m.GetLine();
			while(line.size())
			{
				auto k = line.GetWord(), v = line.GetWord();
				result.headers[k] = v;
				line = m.GetLine();
			}

			result.raw = std::move(_m);

			return result;
		}

		template <typename C > static std::pair<std::vector<uint8_t>, gsl::span<uint8_t> > ReadResponse(C & c)
		{
			std::pair<std::vector<uint8_t>, gsl::span<uint8_t> > result;
			bool finished = false;

			auto start = std::chrono::high_resolution_clock::now();

			while(!finished)
			{
				bool idle = false;

				auto now = std::chrono::high_resolution_clock::now();

				auto count = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();
				if(10 < count)
					throw std::runtime_error("timeout");

				if(!Read(c,[&](auto & c, std::vector<uint8_t> && r, gsl::span<uint8_t> m)
				{
					finished = true;
					result = std::make_pair(std::move(r),m);
				},idle,true)) 
					return result;

				if(idle)
					std::this_thread::sleep_for(std::chrono::milliseconds(300));
			}

			return result;
		}

		template < typename C, typename M > static bool Read(C & c, M m,bool & idle,bool talk=false)
		{
			constexpr auto buffer_size = 16 * 1024;

			auto reset = [&](size_t end)
			{
				if (c.read_offset > end)
				{
					/*
						This condition occurs only when the connection is being used for multiplexing.
					*/

					std::vector<uint8_t> remainder; remainder.reserve(buffer_size);

					remainder.insert(remainder.end(), c.read_buffer.data() + end, c.read_buffer.data() + c.read_offset);

					auto result = std::move(c.read_buffer); 
					result.resize(end);

					c.read_offset = remainder.size();
					c.read_buffer = std::move(remainder);

					return std::move(result);
				}
				else
				{
					c.read_offset = 0;
					c.read_buffer.resize(end);
					return std::move(c.read_buffer);
				}
			};	

			while(true)
			{
				if (!c.read_buffer.size())
					c.read_buffer.resize(buffer_size);

CONTINUE:

				auto rem = (int32_t) c.read_buffer.size() - (int32_t) c.read_offset;

				if(rem <= 0)
				{
					c.read_buffer.resize(c.read_buffer.size() * 2);
					continue;
				}

				int r = c.Receive( gsl::span<uint8_t> ( c.read_buffer.data() + c.read_offset,rem ) );

				if(!r) 
					break;

				if(r == -1)
					return false;

				idle = false;

				c.read_offset += r;
				size_t end = c.read_offset;

RETRY:
				size_t start = 0;

				uint8_t * ptr = c.read_buffer.data();

				for (; start + 3 < end; start++)
				{
					auto t = *ptr++;
					if (  t == '\r' || t == '\n' )
					{
						if( (*ptr == '\r' || *ptr == '\n') && (*(ptr+1) == '\r' || *(ptr+1) == '\n') && (*(ptr+2) == '\r' || *(ptr+2) == '\n') )
						{
							size_t al = 0;

							static std::string_view target = "content-length:";
							auto it = std::search( c.read_buffer.data(), c.read_buffer.data() + start, target.begin(), target.end(), [](char ch1, char ch2) { return std::tolower(ch1) == ch2; } );

							if (it != c.read_buffer.data() + start)
							{
								auto off = std::distance(c.read_buffer.data(), it) + 15;
								al = std::stoi((const char*)(c.read_buffer.data() + off));
								if (al > 1024 * 1024 * 8)
									return false;
							}

							size_t message_size = start + 4 + al;

							if(message_size > end)
							{
								if (message_size > c.read_buffer.size())
									c.read_buffer.resize(message_size);

								goto CONTINUE; //This read is not complete.
							}
							else
							{
								auto single = reset(message_size);
								gsl::span<uint8_t> body(single.data() + start + 4, al);

								c.read_count++;
								c.read_bytes += single.size();

								m(c,std::move(single),body);

								if (c.read_buffer.size())
									goto RETRY; //Another request has been read and is waiting.

								break;
							}
						}
					}
						
				}
			}

			return true;
		}

		template < typename C, typename M > static bool Write(C & c, M m,bool & idle)
		{
			return Common::WriteRaw(c,m,idle);
		}
	};

	class Message
	{
	public:
		template < typename C, typename M > static bool Read(C & c, M m,bool & idle)
		{
			while(true)
			{
				if(c.read_buffer.size())
				{
					auto r = c.Receive( gsl::span<uint8_t>(c.read_buffer.data() + c.read_offset, c.read_buffer.size() - c.read_offset) );

					if(!r) 
						break;

					if(r == -1)
						return false; //error

					idle = false;

					c.read_offset += r;

					if(c.read_offset != c.read_buffer.size())
						return true;

					c.read_count++;
					c.read_bytes += c.read_buffer.size();

					m( c, std::move(c.read_buffer) , gsl::span<uint8_t>(c.read_buffer) );

					c.read_offset = 0;

					if(!c.Multiplex()) break;
				}
				else
				{
					uint32_t ml;
					int header_length = c.Receive( gsl::span<uint8_t>( (uint8_t*) &ml, sizeof(uint32_t) ) );

					if(!header_length) 
						break;

					if(header_length != 4)
						return false;

					if(ml > 8*1024*1024)
						return false;

					idle = false;

					c.read_buffer.resize(ml);
				}
			}

			return true;
		}

		template < typename C, typename M > static bool Write(C & c, M m,bool & idle)
		{
			return Common::WriteMessage(c,m,idle);
		}
	};

	class Map32
	{
	public:

		template < typename T > static std::pair<uint32_t, gsl::span<uint8_t>> DecodeHeader(const T& t)
		{
			return std::make_pair(*((uint32_t*)t.data()), gsl::span<uint8_t>(((uint8_t*)t.data()) + sizeof(uint32_t), t.size() - sizeof(uint32_t)));
		}

		/*template < typename T > static std::vector<uint8_t> MakeHeader(const T& t, size_t sz)
		{
			return std::make_pair(*((uint32_t*)t.data()), gsl::span<uint8_t>(((uint8_t*)t.data()) + sizeof(uint32_t), t.size() - sizeof(uint32_t)));
		}*/

		template < typename C, typename M > static bool Read(C& c, M m, bool& idle)
		{
			while (true)
			{
				if (c.read_buffer.size())
				{
					auto r = c.Receive(gsl::span<uint8_t>(c.read_buffer.data() + c.read_offset, c.read_buffer.size() - c.read_offset));

					if (!r)
						break;

					if (r == -1)
						return false; //error

					idle = false;

					c.read_offset += r;

					if (c.read_offset != c.read_buffer.size())
						return true;

					c.read_count++;
					c.read_bytes += c.read_buffer.size();

					m(c, std::move(c.read_buffer), gsl::span<uint8_t>(c.read_buffer));

					c.read_offset = 0;

					if (!c.Multiplex()) break;
				}
				else
					c.read_buffer.resize(32 + sizeof(uint32_t));
			}

			return true;
		}

		template < typename C, typename M > static bool Write(C& c, M m, bool& idle)
		{
			while (true)
			{
				if (c.map.size())
				{
					auto r = c.Send(gsl::span<uint8_t>(c.map.data() + c.write_offset, c.map.size() - c.write_offset));

					if (!r) break;

					if (r == -1)
						return false;

					idle = false;

					c.write_offset += r;
					if (c.write_offset != c.map.size())
						return true;

					c.write_bytes += c.map.size();
					m(c, c.write_count++);

					c.map = gsl::span<uint8_t>();
					c.write_offset = 0;
				}

				if (!c.TryMap(c.map))
					break;

				uint32_t size = (uint32_t)c.map.size();
				if (sizeof(uint32_t) != c.Send(gsl::span<uint8_t>((uint8_t*)&size, sizeof(uint32_t))))
					return false;
			}

			return true;
		}
	};
};