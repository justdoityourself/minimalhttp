/* Copyright D8Data 2017 */

#pragma once

#include <iostream>

#define self_t (*this)

template < typename T > T& make_singleton() { static T object; return object; }
template < typename T, typename P > T& make_singleton2(const P & p) { static T object(p); return object; }

/*template<class T> auto operator<<(std::ostream& os, const T& t) -> decltype(t.print(os), os) 
{ 
    t.print(os); 
    return os; 
} */

#define let auto

class _console
{
public:
	template<class Head> static void _log(std::ostream& s, Head&& head) 
	{
		s << std::forward<Head>(head);
		s << std::endl;
	}

	template<class Head, class... Tail> static void _log(std::ostream& s, Head&& head, Tail&&... tail) 
	{
		s << std::forward<Head>(head);
		s << " ";
		_log(s, std::forward<Tail>(tail)...);
	}

	template<class... Args> static void log(Args&&... args) 
	{
		_log(std::cout, std::forward<Args>(args)...);
	}
};
_console console;

#pragma warning( disable: 4307 )

uint64_t constexpr __mix(char m, uint64_t s)
{
	return ((s<<7) + ~(s>>3)) + ~m;
}
 
uint64_t constexpr csh(const char * m)
{
	return (*m) ? __mix(*m,csh(m+1)) : 0;
}

uint64_t constexpr csh(const char * m,uint32_t l)
{
	return (l) ? __mix(*m,csh(m+1,l-1)) : 0;
}

template <typename T> uint64_t constexpr csh_t(const T & t)
{
	return csh((const char*)t.data(),(uint32_t)t.size());
}