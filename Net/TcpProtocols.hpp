/* Copyright D8Data 2017 */

#pragma once

#include "Helper/Crypto.hpp"
#include "Helper/Memory.hpp"
//#include "Json.hpp"
#include "Common.hpp"

#include <sstream>
#include <cstdint>
#include <utility>
#include <map>
#include <string_view>

namespace atk
{
	using namespace std;

	template <typename C> bool ws_init(C & c);
	class Common
	{
	public:
		static constexpr uint8_t ws_text = 129;
		static constexpr uint8_t ws_binary = 130;
		static constexpr uint8_t ws_disconnect = 136;

		static constexpr uint8_t ws_small_message = 126;
		static constexpr uint8_t ws_large_message = 127;

		/*template < typename C, typename ... t_args > static Memory RawJsonMessage(C & c, uint32_t max_message_size, t_args...args)
		{
			uint32_t header_size = 0;
			switch(c.type)
			{
			case ConnectionType::web: 
			case ConnectionType::websz: 
				if ( max_message_size >= ws_small_message )
				{
					if ( max_message_size > 0xffff ) header_size = 10;
					else header_size = 4;
				}
				else
					header_size = 2;
				break;
			case ConnectionType::message: header_size = 4; break;
			default: break;
			}

			Memory base;base.Create(max_message_size + header_size,false);
			Memory stream(base.data()+header_size,base.size()-header_size);

			JsonEncoderStream e(stream);

			JsonObjectVariadic(e, args...);

			uint32_t message_size = e.Finalize().size();
			base.length = header_size + message_size;

			switch(c.type)
			{
			case ConnectionType::web: 
			case ConnectionType::websz: 
				base.data()[0] = (c.type == ConnectionType::web) ? ws_binary : ws_text;
				if ( max_message_size >= ws_small_message )
				{
					if ( max_message_size > 0xffff ) 
					{
						base.data()[1] = ws_large_message;
							
						base.data()[2] = 0;//( write->size() >> 56 ) & 255;
						base.data()[3] = 0;//( write->size() >> 48 ) & 255;
						base.data()[4] = 0;//( write->size() >> 40 ) & 255;
						base.data()[5] = 0;//( write->size() >> 32 ) & 255;
						base.data()[6] = ( message_size >> 24 ) & 255;
						base.data()[7] = ( message_size >> 16 ) & 255;
						base.data()[8] = ( message_size >>  8 ) & 255;
						base.data()[9] = ( message_size       ) & 255;
					}
					else
					{
						base.data()[1] = ws_small_message;
						base.data()[2] = ( message_size >> ( 8 ) ) & 255;
						base.data()[3] = ( message_size ) & 255;
					}
				}
				else
					base.data()[1] = (uint8_t)message_size;
				break;
			case ConnectionType::message: base.rt<uint32_t>() = message_size;
			default: break;
			}

			return base;
		}*/

		template < typename ... t_args > static Memory HttpMessage(HttpCommand command,const string_view path,Memory contents, t_args...headers)
		{
			std::string header = _cat(command," ",path," HTTP/1.1\r\n");
			Memory base;

			auto ln = std::to_string(contents.size());
			auto ex = Memory("Content-Length: ");
			uint32_t size = _MemorySize(header,headers...,ex,ln,Memory("\r\n\r\n"),contents);
			base.Create(size,false);
		
			_JoinMemory(base,0,header,headers...,ex,ln,Memory("\r\n\r\n"),contents);		

			return base;
		}

		template < typename F, typename ... t_args > static Memory HttpMessageStream(uint32_t _size, F f,HttpCommand command, const string_view path, t_args...headers)
		{
			std::string header = _cat(command, " ", path, " HTTP/1.1\r\n");
			Memory base;

			Memory contents(nullptr, _size);
			auto ln = std::to_string(contents.size());
			auto ex = Memory("Content-Length: ");
			uint32_t size = _MemorySize(header, headers..., ex, ln, Memory("\r\n\r\n"), contents);
			base.Create(size, false);

			_JoinMemory2(f,base, 0, header, headers..., ex, ln, Memory("\r\n\r\n"), contents);

			return base;
		}

		template < typename ... t_args > static Memory HttpResponse(Memory proto, Memory status, Memory contents, t_args...headers)
		{
			Memory base;

			auto ln = std::to_string(contents.size());
			auto ex = Memory("Content-Length: ");
			uint32_t size = _MemorySize(proto, Memory(" "), status, Memory("\r\n"), headers..., ex, ln, Memory("\r\n\r\n"), contents);
			base.Create(size, false);

			_JoinMemory(base, 0, proto, Memory(" "), status, Memory("\r\n"), headers..., ex, ln, Memory("\r\n\r\n"), contents);

			return base;
		}

		template < typename F, typename ... t_args > static Memory HttpResponseStream(uint32_t _size, F f, Memory proto, Memory status, t_args...headers)
		{
			Memory base;
			Memory contents(nullptr, _size);
			auto ln = std::to_string(contents.size());
			auto ex = Memory("Content-Length: ");
			uint32_t size = _MemorySize(proto, Memory(" "), status, Memory("\r\n"), headers..., ex, ln, Memory("\r\n\r\n"), contents);
			base.Create(size, false);

			_JoinMemory2(f,base, 0, proto, Memory(" "), status, Memory("\r\n"), headers..., ex, ln, Memory("\r\n\r\n"), contents);

			return base;
		}

		template < typename C, typename ... t_args > static Memory StandardMessage(C & c, uint32_t max_buffer_size, t_args...args)
		{
			switch(c.type)
			{
			case ConnectionType::web:
			case ConnectionType::websz:
				break;
			case ConnectionType::message:
				break;
			default:
				break;
			}

			return Memory();
		}

		template < typename C, typename T > static bool Initialize(C & c, T type)
		{
			switch(type)
			{
			case ConnectionType::websz:
			case ConnectionType::web: return ws_init(c);
			default: return true;
			}
		}

		template < typename C, typename M > static bool Write(C & c, M m,bool & idle)
		{
			while(true)
			{
				if(c.write_buffer.size())
				{
					int r = c.Send(Memory(c.write_buffer.data()+c.write_buffer.other,c.write_buffer.size()-c.write_buffer.other));

					if(!r) break;

					if(r == -1)
						return false;

					idle = false;

					c.write_buffer.other += r;
					if(c.write_buffer.other != c.write_buffer.size())
						return true;

					c.write_bytes += c.write_buffer.size();
					m(c,c.current_id);

					c.write_buffer.Cleanup(true);
				}
				
				if(!c.write_queue.Try(c.write_buffer))
					break;

				c.current_id = c.write_buffer.other;
				c.write_buffer.other = 0;
			}

			return true;
		}
	};

	class Http
	{
	public:

		static std::tuple<Memory,Memory,Memory,Memory,std::map<Memory,Memory> > Request(Memory m)
		{
			Memory cm = m.GetLine();
			Memory type = cm.GetWord(), path= cm.GetWord(), proto= cm.GetWord(), command;
			std::tie(path,command) = path.Divide('?');

			std::map<Memory,Memory> headers;

			Memory line = m.GetLine();
			while(line.size())
			{
				auto k = line.GetWord(), v = line.GetWord();
				headers[k] = v;
				line = m.GetLine();
			}

			return std::make_tuple(type,path,proto,command,headers);
		}

		template<std::size_t... I> static auto _Parameters(Memory & cmd, std::index_sequence<I...>)
		{
			return std::make_tuple((I,cmd.GetWord('&').SkipWord('='))...);
		}

		template<std::size_t N, typename Indices = std::make_index_sequence<N>> static auto Parameters(Memory & cmd)
		{
			return _Parameters(cmd, Indices{});
		}

		static std::tuple<Memory,Memory,Memory,std::map<Memory,Memory> > Response(Memory m)
		{
			Memory cm = m.GetLine();
			Memory proto = cm.GetWord(), status = cm.GetWord(), msg= cm.GetWord();

			std::map<Memory,Memory> headers;

			Memory line = m.GetLine();
			while(line.size())
			{
				auto k = line.GetWord(), v = line.GetWord();
				headers[k] = v;
				line = m.GetLine();
			}

			return std::make_tuple(proto,status,msg,headers);
		}

		static std::pair<Memory,Memory > SSE(Memory r)
		{
			std::map<Memory,Memory> headers;

			while(r.size())
			{
				Memory line = r.GetLine();
				auto k = line.GetWord(':');
				headers[k] = line;
			}

			auto i = headers.find("event");

			if(i == headers.end())
				return  std::make_pair(Memory(),Memory());

			Memory payload = i->second;
			Memory type = payload.GetString('\n');
			Memory id = payload.GetString('\n');
			Memory data = payload.GetString('\n').Divide(':').second;

			return std::make_pair(type,data);
		}

		template <typename C > static std::pair<Memory, Memory> ReadBlocking(C & c)
		{
			std::pair<Memory,Memory> result;
			bool finished = false;
			auto start = std::chrono::high_resolution_clock::now();
			while(!finished)
			{
				bool idle = false;

				auto now = std::chrono::high_resolution_clock::now();

				auto count = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();
				if(5 < count)
					throw "timeout";

				if(!Read(c,[&](auto & c, Memory r, const Memory & m)
				{
					finished = true;
					result = std::make_pair(r,m);
				},idle,true)) return std::make_pair(Memory(),Memory());

				if(idle)
					std::this_thread::sleep_for(std::chrono::milliseconds(300));
			}

			return result;
		}

		static const uint32_t http_buffer_size = 4*1024;
		template < typename C, typename M > static bool Read(C & c, M m,bool & idle,bool talk=false)
		{
			auto grow = [&](uint32_t sz = 0) -> bool
			{
				if(0 == sz)
					sz = c.read_buffer.size() * 2;

				if(sz > 10*1024*1024) //Allocation limit
					return false;

				Memory mnew; mnew.Create(sz);
				//std::cout << "rbuffer " << (void*)mnew.data() << std::endl;
				mnew.CopyInto(c.read_buffer);
				c.read_buffer.Cleanup();
				c.read_buffer = mnew;
				mnew.free=0;

				return true;
			};

			auto reset = [&](uint32_t end)->Memory
			{
				Memory result(c.read_buffer.data(),end);
				//std::cout << "pass " << (void*)c.read_buffer.data() << std::endl;

				if(c.read_buffer.other > end)
				{
					Memory extra(c.read_buffer.data()+end,c.read_buffer.other-end);
					uint32_t alloc = extra.size();
					if(extra.size() < http_buffer_size)
						alloc = http_buffer_size;

					Memory mnew; mnew.Create(alloc);
					//std::cout << "buffer " << (void*)mnew.data() << std::endl;
					mnew.CopyInto(extra);
					c.read_buffer = mnew;
					c.read_buffer.other = extra.size();
					mnew.free=0;
				}
				else
				{
					c.read_buffer.free=0;
					c.read_buffer.buffer=nullptr;
					c.read_buffer.length = 0;
					c.read_buffer.other = 0;
				}

				return result;
			};

			while(true)
			{
CONTINUE:
				if(c.read_buffer.size())
				{
					int32_t remaining = (int32_t)c.read_buffer.size()-(int32_t)c.read_buffer.other;

					if(remaining <= 0)
					{
						if(!grow())
							return false;
						continue;
					}

					int r = c.Receive(Memory(c.read_buffer.data()+c.read_buffer.other,remaining));
					if(!r) break;

					if(r == -1)
						return false;

					//if(talk)console.log("Talk:", std::to_string((uint32_t)c.read_buffer.data()),Memory(c.read_buffer.data()+c.read_buffer.other,r));

					idle = false;

					c.read_buffer.other += r;
					uint32_t end = c.read_buffer.other;

				RETRY:
					uint32_t start = 0;

					uint8_t * ptr = c.read_buffer.data();
					for (; start + 3< end; start++)
					{
						auto t = *ptr++;
						if (  t == '\r' || t == '\n' )
						{
							if((*ptr == '\r' || *ptr == '\n') && (*(ptr+1) == '\r' || *(ptr+1) == '\n') && (*(ptr+2) == '\r' || *(ptr+2) == '\n'))
							{
								uint32_t al = 0;

								Memory _m(c.read_buffer.data(),start);
								Memory h = _m.FindI(Memory("Content-Length: "));

								if(h.size())
									al = Memory(h.data()+h.size(),64).GetLine();

								uint32_t message_size = start + 4 + al;

								if(message_size > end)
								{
									if(message_size>c.read_buffer.size())
										if(!grow(message_size))
											return false;
									goto CONTINUE;
								}
								else
								{
									Memory final = reset(message_size);

									c.read_count++;
									c.read_bytes += final.size();
									m(c,final,Memory(final.data()+start+4,al));

									if (c.read_buffer.size())
										goto RETRY;
									break;
								}
							}
						}
						
					}
				}
				else
				{
					c.read_buffer.Create(http_buffer_size);
					//std::cout << "buffer " << (void*)c.read_buffer.data() << std::endl;
					c.read_buffer.other = 0;
				}
			}

			return true;
		}

		template < typename C, typename M > static bool Write(C & c, M m,bool & idle)
		{
			return Common::Write(c,m,idle);
		}
	};

	class Message
	{
	public:
		template < typename C, typename M > static bool Read(C & c, M m,bool & idle)
		{
			while(true)
			{
				if(c.read_buffer.other)
				{
					int r = c.Receive(Memory(c.read_buffer.data()+c.read_buffer.other,c.read_buffer.size()-c.read_buffer.other));
					if(!r) break;

					if(r == -1)
						return false;

					idle = false;

					c.read_buffer.other += r;
					if(c.read_buffer.other != c.read_buffer.size())
						return true;

					c.read_count++;
					c.read_bytes += c.read_buffer.size();
					m(c,c.read_buffer,Memory(c.read_buffer.data()+sizeof(uint32_t),c.read_buffer.size()-sizeof(uint32_t)));

					c.read_buffer.buffer=nullptr;
					c.read_buffer.length = 0;
					c.read_buffer.other = 0;
				}
				else
				{
					uint32_t ml;
					int header_length = c.Receive(Memory(&ml,sizeof(uint32_t)));
					if(!header_length) break;

					if(header_length != 4)
						return false;

					if(ml > 10*1024*1024) //Max message segment
					{
						std::cout << "Message Exceeded Maximum. (2)" << std::endl;
						return false;
					}

					idle = false;

					c.read_buffer.Create(ml+4);
					c.read_buffer.template rt<uint32_t>() = ml;
					c.read_buffer.other = sizeof(uint32_t);
				}
			}

			return true;
		}

		template < typename C, typename M > static bool Write(C & c, M m,bool & idle)
		{
			return Common::Write(c,m,idle);
		}
	};

	class Newline
	{
	public:
		template < typename C, typename M > static bool Read(C & c, M m,bool & idle)
		{
			auto reset = [&](uint32_t end)->Memory
			{
				Memory result(c.read_buffer.data(),end);

				c.read_buffer.free=0;
				c.read_buffer.buffer=nullptr;
				c.read_buffer.length = 0;
				c.read_buffer.other = 0;

				return result;
			};

			while(true)
			{
				if(c.read_buffer.size())
				{
					int32_t remaining = (int32_t)c.read_buffer.size()-(int32_t)c.read_buffer.other;

					if(remaining <= 0)
						return false;

					int r = c.Receive(Memory(c.read_buffer.data()+c.read_buffer.other,remaining));
					if(!r) break;

					if(r == -1)
						return false;

					idle = false;

					uint32_t start = 0;
					c.read_buffer.other += r;
					uint32_t end = c.read_buffer.other;

					uint8_t * ptr = c.read_buffer.data() + start;
					bool second = false;
					uint32_t so = 0;
					uint32_t se = 0;
					for (; start < end; start++)
					{
						auto t = *ptr++;
						if (  t == '\r' || t == '\n' )
						{
							uint32_t ex = 0;
							while((*ptr == '\r' || *ptr == '\n') && start < end) { *ptr ++; start ++; ex ++; }
							if(second)
							{
								Memory _m(c.read_buffer.data(),start);

								uint32_t message_size = start + 1;

								Memory final = reset(message_size);

								c.read_count++;
								c.read_bytes += final.size();
								
								Memory param1(final.data(),se);
								Memory param2(final.data()+so,message_size - so - 1 - ex);
								m(c,param1,param2);
								break;
							}
							else 
							{
								se = start - ex;
								second = true;
								so = start + 1 + ex;
							}
						}
						
					}
				}
				else
				{
					c.read_buffer.Create(2048);
					//std::cout << "buffer " << (void*)c.read_buffer.data() << std::endl;
					c.read_buffer.other = 0;
				}
			}

			return true;
		}

		template < typename C, typename M > static bool Write(C & c, M m,bool & idle)
		{
			return Common::Write(c,m,idle);
		}
	};


	class Websocket
	{
	public:
		static constexpr uint8_t ws_text = 129;
		static constexpr uint8_t ws_binary = 130;
		static constexpr uint8_t ws_disconnect = 136;

		static constexpr uint8_t ws_small_message = 126;
		static constexpr uint8_t ws_large_message = 127;

		template < typename C > static bool Initialize(C & c)
		{
			bool valid = true;
			try
			{
				std::array<uint8_t, 8 * 1024> data;
				uint32_t length=0;

				while (true)
				{
					length += (uint32_t)c.Receive(Memory(data.data()+length, (uint32_t)data.size()-length));
					
					if (length > 4)
					{
						if (*((data.data() + length) - 4) == '\r' && *((data.data() + length) - 3) == '\n' && *((data.data() + length) - 2) == '\r' && *((data.data() + length) - 1) == '\n')
							break;
					}

					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}

				std::string response;

				std::stringstream stream((char*)&(data)[0]);
				std::string line;
				std::map<std::string, std::string> lookup;

				while (std::getline(stream, line, '\n'))
				{
					size_t pos = line.find(":", 0);
					std::string key, value;
					key = line.substr(0, pos);
					value = line.substr(pos + 2, -1);
					lookup[key] = value;
				}

				std::string security = lookup["Sec-WebSocket-Key"];
				security.resize(security.find('\r'));
				security += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

				auto sha1_hash = atk::Sha1(security);

				response +=
					"HTTP/1.1 101 Web Socket Protocol Handshake\r\n" \
					"Upgrade: Websocket\r\n" \
					"Connection: Upgrade\r\n" \
					"Sec-WebSocket-Accept: ";
				response += atk::Base64Encode(sha1_hash);
				response += "\r\n\r\n";

				c.Write(response);
			}
			catch(...)
			{
				valid = false;
			}

			return valid;
		}

		template < typename C, typename M > static bool Read(C & c, M m,bool & idle)
		{
			while(true)
			{
				if(c.read_buffer.other)
				{
					int r = c.Receive(Memory(c.read_buffer.data()+c.read_buffer.other,c.read_buffer.size()-c.read_buffer.other));
					if(!r) break;

					if(r == -1)
					{
						std::cout << "Socket Fault. (6)" << std::endl;
						return false;
					}

					idle = false;

					c.read_buffer.other += r;
					if(c.read_buffer.other != c.read_buffer.size())
						return true;

					uint8_t control_code = c.read_buffer.data()[0];
					bool final_frame = ((control_code >> 7) == 0x1);
					uint8_t opcode = control_code & 0xf;

					int ml = (int) c.read_buffer.data()[1] & ws_large_message;

					uint8_t * mask;
					uint32_t message_offset;

					if (ml == ws_small_message)
					{
						mask = c.read_buffer.template pt<uint8_t>(0,4);
						message_offset = 8;
					}

					else if (ml == ws_large_message)
					{
						mask = c.read_buffer.template pt<uint8_t>(0,10);
						message_offset = 14;
					}
					else
					{
						mask = c.read_buffer.template pt<uint8_t>(0,2);
						message_offset = 6;
					}

					ml = c.read_buffer.size()-message_offset;

					if(*mask)
						for ( int j = 0; j < ml; j ++ )
							c.read_buffer.data()[ j + message_offset ] ^= mask[ j % 4 ];

					c.read_count++;
					c.read_bytes += c.read_buffer.size();
					m(c,c.read_buffer,Memory(c.read_buffer.data()+message_offset,c.read_buffer.size()-message_offset));

					c.read_buffer.buffer=nullptr;
					c.read_buffer.length = 0;
					c.read_buffer.other = 0;
				}
				else
				{
					uint16_t header;
					uint16_t sm;
					uint64_t lg;

					int control_length = c.Receive(Memory(&header,sizeof(uint16_t)));
					if(!control_length) break;

					if(control_length != 2)
					{
						std::cout << "Header Fault. (5)" << std::endl;
						return false;
					}

					uint8_t control_code = ((uint8_t*)(&header))[0];
					bool final_frame = ((control_code >> 7) == 0x1);
					uint8_t opcode = control_code & 0xf;

					if (8 == opcode) 
					{
						std::cout << "Client Side Termination. (4)" << std::endl;
						return false;
					}

					//if (9 == opcode); //Unsupported websocket ping

					int ml = (int) ((uint8_t*)(&header))[1] & ws_large_message;

					if (ml == ws_small_message)
					{
						control_length += c.Receive(Memory(&sm,sizeof(uint16_t)));

						if(control_length != 4)
						{
							std::cout << "Header Fragmented. (1)" << std::endl;
							return false;
						}

						uint8_t * i = (uint8_t*)&sm;
						ml = 0;

						for (uint32_t j = 0; j < 2; j++)
							ml += i[j] << (8 * (1 - j));
					}

					else if (ml == ws_large_message)
					{
						control_length += c.Receive(Memory(&lg,sizeof(uint64_t)));

						if(control_length != 10)
						{
							std::cout << "Header Fragmented. (2)" << std::endl;
							return false;
						}

						uint8_t * i = (uint8_t*)&lg;
						ml = 0;

						for (uint32_t j = 0; j < 8; j++)
							ml += i[j] << (8 * (7 - j));
					}

					if(ml > 10*1024*1024) //Max message segment
					{
						std::cout << "Message Exceeded Maximum. (3)" << std::endl;
						return false;
					}

					idle = false;

					c.read_buffer.Create(ml+control_length+4);
					c.read_buffer.template rt<uint16_t>() = header;
					if(control_length == 4) c.read_buffer.template rt<uint16_t>(1) = sm;
					else if(control_length == 10) c.read_buffer.template rt<uint64_t>(0,2) = lg;
					c.read_buffer.other = control_length;
				}
			}

			return true;
		}

		template < typename C, typename M > static bool Write(C & c, M m,bool & idle)
		{
			return Common::Write(c,m,idle);
		}
	};
	template < typename C > bool ws_init(C & c)
	{	
		return Websocket::Initialize(c);
	}
};