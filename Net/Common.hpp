#pragma once

#include "Platform.hpp"

#include "Helper/StringEnumeration.hpp"
#include <string>
#include <string_view>

namespace atk
{
	using namespace std;
	EnumerateString2(HttpCommand, GET,HEAD,POST,PUT);
	EnumerateString2(ConnectionType, datagram, http,httpb,invalid,newline_pair, message,rest,rtc,stk,text,unknown,web,websz);

	typedef zed_net_address_t TcpAddress;

	class NetworkInstance
	{
	public:
		NetworkInstance(){ zed_net_init(); }
		~NetworkInstance(){ zed_net_shutdown(); }
	};

	void EnableNetworking() { make_singleton<NetworkInstance>(); }

	std::pair<std::string,std::string> address_split(const string_view target,const std::string del=":")
	{
		auto d = target.find(del);
		return std::make_pair(string(target.substr(0, d)),string(target.substr(d+1)));
	}

	std::pair<std::string,int> address_split2(const string_view target,const std::string del=":")
	{
		auto d = target.find(del);
		return std::make_pair(string(target.substr(0, d)),std::stoi(target.substr(d+1).data()));
	}

	/*bool IsIpAddress(const std::string & s)
	{
		for( char c : s )
		{
			if(c >= '0' && c <= '9' || c == '.')
				continue;
			return false;
		}
		return true;
	}*/

	template < typename F, typename M > int zed_net_get_address(const string_view address, M m, F f) 
	{
		std::string host,port; std::tie(host,port) = address_split(address);

		/*if(IsIpAddress(host))
		{
			sockaddr_in base;
			addrinfo ip_address;

			std::memset((char *) &base,0,sizeof(sockaddr_in));
			base.sin_family = AF_INET;
			base.sin_addr.s_addr = inet_addr(host.c_str());
			base.sin_port = htons(std::stoi(port));

			ip_address.ai_addr = (sockaddr*)&base;
			ip_address.ai_addrlen = sizeof(base);
			f(&ip_address);
		}*/

		addrinfo     hints;
		addrinfo     *res = NULL, *r = NULL;

		bool found_host = false;

		std::memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = m;

		if (getaddrinfo(host.c_str(), port.c_str(), &hints, &res) != 0) 
			return zed_net__error("Invalid host name");

		for (r = res; r; r = r->ai_next) 	
		{
			if(f(r)) 
			{
				found_host = true;
				break;
			}
		}

		freeaddrinfo(res);
    
		if(!found_host) return zed_net__error("No Host Accepted");

		return 0;
	}
}