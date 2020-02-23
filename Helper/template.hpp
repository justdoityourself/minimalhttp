#pragma once

#define ENABLE_IF(_C,_R) typename std::enable_if<_C, _R>::type
#define ENABLE_IF_NOT(_CD,_RD) ENABLE_IF(!_CD,_RD)
#define en_if ENABLE_IF
#define en_nf ENABLE_IF_NOT

#define ENABLE_IF_TYPE_MATCH(_T,_M,_RE) ENABLE_IF((std::is_same<_T, _M>::value), _RE)
#define ENABLE_IF_NOT_TYPE_MATCH(_T,_M,_RE) ENABLE_IF_NOT((std::is_same<_T, _M>::value), _RE)
#define en_if_tm ENABLE_IF_TYPE_MATCH
#define en_nf_tm ENABLE_IF_NOT_TYPE_MATCH

#define cexp constexpr
#define cexp_a cexp auto

#define cexp_if if constexpr

#define ttyp typename
#define self_t (*this)

#define _ttyp(__X) ttyp __X

#define tdef //todu unpack list
#define tdef1 template < typename T >
#define _tdef1(__X) template < typename __X >
#define _tdef1_if(__X,__COND) template < typename __X, __COND = 0 >

#define _ndef1_if(__X,__COND) template < unsigned __X, __COND = 0 >

#define make_static_counter(__N) namespace __N##StaticCounter	\
{\
	template<int N>\
	struct flag {\
		friend constexpr int adl_flag(flag<N>);\
	};\
	template<int N>\
	struct writer {\
		friend constexpr int adl_flag(flag<N>) {\
			return N;\
		}\
		static constexpr int value = N;\
	};\
	template<int N, class = char[noexcept(adl_flag(flag<N>())) ? +1 : -1]>\
	int constexpr reader(int, flag<N>) {\
		return N;\
	}\
	template<int N>\
	int constexpr reader(float, flag<N>, int R = reader(0, flag<N - 1>())) {\
		return R;\
	}\
	int constexpr reader(float, flag<0>) {\
		return 0;\
	}\
}\
	template<int N = 1, int C = __N##StaticCounter::reader(0, __N##StaticCounter::flag<32>())>\
int constexpr __N(int R = __N##StaticCounter::writer<C + N>::value) {\
	return R;\
}

make_static_counter(static_count)
