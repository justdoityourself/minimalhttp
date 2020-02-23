/* Copyright D8Data 2017 */

#pragma once

#include <type_traits>
#include "Memory.hpp"

namespace AtkDataStructures
{
	using namespace AtkMemory;

	template < typename T > void ExpandT ( T* root, int count, int at_index )
	{
		for (int i = count - 1; i >= at_index; i--)
			*(root + i + 1) = *(root + i);
	}

	template <typename T, typename P, typename P2, typename std::enable_if<std::is_pointer<T>::value>::type* = nullptr > T* FindTSM(T* root, int count, const Memory& s, P param, P2 param2)
	{
		if (!count)
			return nullptr;

		auto lt = [] ( const Memory& l, const Memory & r ) -> bool 
		{ 
			auto cmp = strncmp ( (const char*)l.data() , (const char *) r.data(), (l.size() > r.size()) ? r.size() : l.size() );

			if(0 == cmp)
				return l.size() < r.size();

			auto re = cmp < 0;
			return re;
		};
		auto cmpl = [&](auto & o) -> bool { return lt(o->Key(param,param2), s); };
		auto cmpr = [&](auto & o) -> bool { return lt(s, o->Key(param,param2)); };

		int low = 0;
		int high = count-1;

		while (low <= high)
		{
			int middle = (low + high) >> 1;

			if (cmpl(*(root+middle)))
				low = middle + 1;
			else if (cmpr(*(root+middle)))
				high = middle - 1;
			else { return root + middle; }
		}

		return nullptr;
	}

	template <typename T, typename P, typename P2, typename std::enable_if<!std::is_pointer<T>::value>::type* = nullptr > T* InsertTSM(T* root, int count, const T & element, bool multimap, bool*pexpand, P param, P2 param2 )
	{
		if (!count)
		{
			root[0] = element;
			if(pexpand)*pexpand = true;
			return root;
		}

		auto lt = [] ( Memory&& l, Memory && r ) -> bool 
		{ 
			auto cmp = strncmp ( (const char*)l.data() , (const char *) r.data(), (l.size() > r.size()) ? r.size() : l.size() );

			if(0 == cmp)
				return l.size() < r.size();

			auto re = cmp < 0;
			return re; 
		};
		auto cmp = [&] ( auto & l, auto & r ) -> bool { return lt ( l.Key(param,param2), r.Key(param,param2) ); };

		bool e = false;
		int low = 0;
		int high = count-1;

		while (low <= high)
		{
			int middle = (low + high) >> 1;

			if (cmp(*(root+middle), element))
				low = middle + 1;
			else if (cmp(element, *(root+middle)))
				high = middle - 1;
			else { low = middle;  e = true; break; }
		}

		if( e && multimap || !e) ExpandT(root, count, low);
		else if ( e&& !multimap )  { root[low].~T(); *pexpand = false; }
		else *pexpand = false;

		root[low] = element;

		return root + low;
	}

	template <typename T, typename P > T* InsertTSM2(T* root, int count, Memory element, bool multimap, bool&e, P param )
	{
		e = true;

		if (!count)
			return root;

		auto lt = [] ( const Memory& l, const Memory & r ) -> bool 
		{ 
			auto cmp = strncmp ( (const char*)l.data() , (const char *) r.data(), (l.size() > r.size()) ? r.size() : l.size() );

			if(0 == cmp)
				return l.size() < r.size();

			auto re = cmp < 0;
			return re;
		};

		int low = 0;
		int high = count-1;

		while (low <= high)
		{
			int middle = (low + high) >> 1;

			if (lt((root+middle)->Key(param), element))
				low = middle + 1;
			else if (lt(element, (root+middle)->Key(param)))
				high = middle - 1;
			else { low = middle;  e = false; break; }
		}

		if( !e && multimap || e) 
			ExpandT(root, count, low);
		else if ( !e && !multimap )
			root[low].~T(); 

		return root + low;
	}

	template <typename T, typename P, typename P2, typename std::enable_if<!std::is_pointer<T>::value>::type* = nullptr > T* FindTSM(T* root, int count, const Memory& s, P param, P2 param2)
	{
		if (!count)
			return nullptr;

		auto lt = [] ( const Memory& l, const Memory & r ) -> bool 
		{ 
			auto cmp = strncmp ( (const char*)l.data() , (const char *) r.data(), (l.size() > r.size()) ? r.size() : l.size() );

			if(0 == cmp)
				return l.size() < r.size();

			auto re = cmp < 0;
			return re; 
		};
		auto cmpl = [&](auto & o) -> bool { return lt(o.Key(param,param2), s); };
		auto cmpr = [&](auto & o) -> bool { return lt(s, o.Key(param,param2)); };

		int low = 0;
		int high = count-1;

		while (low <= high)
		{
			int middle = (low + high) >> 1;

			if (cmpl(*(root+middle)))
				low = middle + 1;
			else if (cmpr(*(root+middle)))
				high = middle - 1;
			else { return root + middle; }
		}

		return nullptr;
	}

	template <typename T, typename P > T* FindTSM2(T* root, int count, const Memory& s, P param)
	{
		if (!count)
			return nullptr;

		auto lt = [] ( const Memory& l, const Memory & r ) -> bool 
		{ 
			auto cmp = strncmp ( (const char*)l.data() , (const char *) r.data(), (l.size() > r.size()) ? r.size() : l.size() );

			if(0 == cmp)
				return l.size() < r.size();

			auto re = cmp < 0;
			return re; 
		};
		auto cmpl = [&](auto & o) -> bool { return lt(o.Key(param), s); };
		auto cmpr = [&](auto & o) -> bool { return lt(s, o.Key(param)); };

		int low = 0;
		int high = count-1;

		while (low <= high)
		{
			int middle = (low + high) >> 1;

			if (cmpl(*(root+middle)))
				low = middle + 1;
			else if (cmpr(*(root+middle)))
				high = middle - 1;
			else { return root + middle; }
		}

		return nullptr;
	}
}