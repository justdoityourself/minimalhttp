#pragma once

#include <boost/preprocessor.hpp>

/*
	Notes on how to use:
	EnumerateString2([Type Name], [lowercase alphabetically ordered list of elements])
	EnumerateString2(ConnectionTypes,http,raw,tcp,udp,websocket);

	Warning: the list must be properly order a-z and also lowercase;

	Now: MatchConnectionTypes("http") == ConnectionTypes::http
	And: GetConnectionTypes(ConnectionTypes::http) == "http"
*/

template < typename E, typename L, typename C > E EnumStringMatch(const char * s, uint32_t l, L elements, C count)
{
	auto lt = [] ( const char * l, uint32_t ll, const char * r, uint32_t rl ) -> bool 
	{ 
		auto cmp = strncmp ( l , r, (ll > rl) ? rl : ll );

		if(0 == cmp)
			return ll < rl;

		auto re = cmp < 0;
		return re;
	};
	auto cmpl = [&](auto o) -> bool { return lt(o, (uint32_t)strlen(o), s, l); };
	auto cmpr = [&](auto o) -> bool { return lt(s, l, o, (uint32_t)strlen(o)); };

	int low = 0;
	int high = (int)count-1;

	while (low <= high)
	{
		int middle = (low + high) >> 1;

		if (cmpl(*(elements+middle)))
			low = middle + 1;
		else if (cmpr(*(elements+middle)))
			high = middle - 1;
		else { return (E)middle; }
	}

	return (E)-1;
}

template < typename E, typename U, typename L, typename C > E EnumStringMatch(const U & u, L elements, C count) { return EnumStringMatch<E,L,C>((const char*)u.data(), (uint32_t)u.size(), elements, count); }

#define BindEnumString(___ENUM___,___STR___) ___ENUM___ Match##___ENUM___(const char * s, uint32_t l) { return EnumStringMatch<___ENUM___>(s,l, ___STR___, ___ENUM___::Count);}template <typename U > ___ENUM___ Match##___ENUM___(const U & u) { return ((uint32_t)u.size()) ? EnumStringMatch<___ENUM___>((const char*)u.data(), (uint32_t) u.size(), ___STR___, ___ENUM___::Count) : (___ENUM___)0; }const char * Get##___ENUM___(___ENUM___ c) { return ___STR___[(uint32_t)c]; }

#define BindClassString(___ENUM___,___STR___) static ___ENUM___ Match##___ENUM___(const char * s, uint32_t l) { return EnumStringMatch<___ENUM___>(s,l, ___STR___, ___ENUM___::Count);}template <typename U > static ___ENUM___ Match##___ENUM___(const U & u) { return ((uint32_t)u.size()) ? EnumStringMatch<___ENUM___>((const char*)u.data(), (uint32_t) u.size(), ___STR___, ___ENUM___::Count) : (___ENUM___)0; }static const char * Get##___ENUM___(___ENUM___ c) { return ___STR___[(uint32_t)c]; }

#define EnumerateString2(___ENUM___, ...) EnumerateString(___ENUM___,uint8_t,__VA_ARGS__)
#define EnumerateString(___ENUM___, ___TYPE___, ...)											\
enum class ___ENUM___ : ___TYPE___															\
{																							\
	BOOST_PP_SEQ_FOR_EACH_I(__reflect_enum, data, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))	\
	Count																					\
};																							\
const char * ___ENUM___##s[] =																\
{																							\
	BOOST_PP_SEQ_FOR_EACH_I(__reflect_string, data, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))	\
};																							\
BindEnumString(___ENUM___,___ENUM___##s)													\
template < typename P >  typename std::enable_if<std::is_same<P, ___ENUM___>::value, const char *>::type GetEnum(const P & t) {return Get##___ENUM___(t);}\
template < typename P,typename U, typename std::enable_if<std::is_same<P, ___ENUM___>::value, int>::type = 0 > void MatchEnum(P & t, const U & u) {t = Match##___ENUM___(u);}

#define ClassString(___ENUM___, ...)											\
enum class ___ENUM___ : uint8_t															\
{																							\
	BOOST_PP_SEQ_FOR_EACH_I(__reflect_enum, data, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))	\
	Count																					\
};																							\
static constexpr const char * const ___ENUM___##s[] =																\
{																							\
	BOOST_PP_SEQ_FOR_EACH_I(__reflect_string, data, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))	\
};																							\
BindClassString(___ENUM___,___ENUM___##s)		

#define __reflect_enum(r, data, __i, __x)		__x,
#define __reflect_string(r, data, __i, __x)		BOOST_PP_STRINGIZE( __x ),