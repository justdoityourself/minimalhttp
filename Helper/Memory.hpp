/* Copyright D8Data 2017 */

#pragma once

#include "Helper/Constructs.hpp"
#include "Helper/Hex.hpp"

#include <iostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <algorithm>
#include <cstring>

/*#ifdef _DEBUG

struct ___MemoryRecord
{
	void* pointer = nullptr;
	std::chrono::high_resolution_clock::time_point time;
};

struct ___MemoryForwardGuard
{
	___MemoryForwardGuard(uint64_t _index, uint64_t _size):size(_size),index(_index){}

	uint64_t m1 = 0xABCDEFABCDEFFDBA;
	uint64_t m2 = 0xFEDCBABCDEFECABD;
	uint64_t index;
	uint64_t size;

	bool Valid()
	{
		return m1 == 0xABCDEFABCDEFFDBA && m2 == 0xFEDCBABCDEFECABD;
	}
};

struct ___MemoryRearGuard
{
	___MemoryRearGuard(uint64_t _index, uint64_t _size):size(_size),index(_index){}
	uint64_t index;
	uint64_t size;
	uint64_t m1 = 0xAAAADDDDCCCCFFFF;
	uint64_t m2 = 0xBBBBEEEEFFFFAAAA;

	bool Valid()
	{
		return m1 == 0xAAAADDDDCCCCFFFF && m2 == 0xBBBBEEEEFFFFAAAA;
	}
};

static const int ___memory_bin_count = 200000;
___MemoryRecord ___memory_bins[200000];
std::atomic<int> ___in_use_memory_bins = 0;
std::atomic<int> ___in_use_memory_bytes = 0;
std::atomic<int> ___in_use_memory_overhead = 0;
std::atomic<int> ___memory_skip = 0;
std::atomic<int> ___current_memory_bin = 0;

void AuditMemory()
{
	for(uint32_t i = 0; i < ___memory_bin_count; i++)
	{
		if(!___memory_bins[i].pointer)
			continue;

		void * ptr = (void *) (((char*)___memory_bins[i].pointer) + sizeof(___MemoryForwardGuard));

		___MemoryForwardGuard * pf = (___MemoryForwardGuard*)(((char*)ptr)-sizeof(___MemoryForwardGuard));

		if(!pf->Valid())
		{
			DebugBreak();
			throw "Forward Guard Breach, someone has messed with our memory.";
		}

		___MemoryRearGuard * pr = (___MemoryRearGuard*)(((char*)ptr) + pf->size);

		if(!pr->Valid())
		{
			DebugBreak();
			throw "Rear Guard Breach, this buffer is to blame for the damage.";
		}

		if(pr->index != i)
		{
			DebugBreak();
			throw "Double Delete, this buffer has already been cleaned up.";
		}
	}
}


std::pair<___MemoryRecord*,int> ___GetMemoryRecord()
{
	while(true)
	{
		int idx = (___current_memory_bin++)%___memory_bin_count;

		if(___memory_bins[idx].pointer == nullptr)		
			return std::make_pair(___memory_bins+idx,idx);
		else
			___memory_skip++;
	}
}

void * ___memory_alloc(size_t _size)
{
	size_t actual = _size + sizeof(___MemoryForwardGuard) + sizeof(___MemoryRearGuard);
	void * res = malloc(actual);
	
	___MemoryRecord * bin; int idx;

	std::tie(bin,idx) = ___GetMemoryRecord();

	bin->pointer = res;
	bin->time = std::chrono::high_resolution_clock::now();

	___MemoryForwardGuard * pf = (___MemoryForwardGuard*)res;
	___MemoryRearGuard * pr = (___MemoryRearGuard*)(((char*)res) + _size + sizeof(___MemoryForwardGuard));

	*pf = ___MemoryForwardGuard((uint64_t)idx,(uint64_t)_size);
	*pr = ___MemoryRearGuard((uint64_t)idx,(uint64_t)_size);

	___in_use_memory_bins++;
	___in_use_memory_bytes += (int)actual;
	___in_use_memory_overhead += (sizeof(___MemoryForwardGuard) + sizeof(___MemoryRearGuard));

	return (void*)(((char*)res) + sizeof(___MemoryForwardGuard));
}

void ___memory_delete(void* ptr)
{
	___MemoryForwardGuard * pf = (___MemoryForwardGuard*)(((char*)ptr)-sizeof(___MemoryForwardGuard));

	if(!pf->Valid())
	{
		DebugBreak();
		throw "Forward Guard Breach, someone has messed with our memory.";
	}

	___MemoryRearGuard * pr = (___MemoryRearGuard*)(((char*)ptr) + pf->size);

	if(!pr->Valid())
	{
		DebugBreak();
		throw "Rear Guard Breach, this buffer is to blame for the damage.";
	}

	if(___memory_bins[pf->index].pointer != (void*)pf)
	{
		DebugBreak();
		throw "Double Delete, this buffer has already been cleaned up.";
	}
	else
		___memory_bins[pf->index].pointer = nullptr;

	___in_use_memory_bins--;
	___in_use_memory_bytes -= (pf->size + sizeof(___MemoryForwardGuard) + sizeof(___MemoryRearGuard));
	___in_use_memory_overhead -= (sizeof(___MemoryForwardGuard) + sizeof(___MemoryRearGuard));

	free(pf);
}

void* operator new(size_t _size)
{
	return ___memory_alloc(_size);
}

void* operator new[](size_t _size)
{
	return ___memory_alloc(_size);
}

void operator delete(void* ptr)
{
	___memory_delete(ptr);
}

void  operator delete[](void* ptr)
{
	___memory_delete(ptr);
}

#endif
*/
namespace atk
{
	template < typename T, std::size_t N > constexpr uint32_t __array_elements(const T(&)[N]) { return (uint32_t)N; }
	template < typename T, std::size_t N > constexpr uint32_t __array_element_size(const T(&)[N]) { return (uint32_t)sizeof(T); }
	template < typename T > constexpr uint32_t __array_bytes(T& t) { return __array_elements(t) * __array_element_size(t); }

	#define has_function_t(___function___)				\
	template <typename T> class has_function_##___function___	\
	{													\
		typedef char one;								\
		typedef long two;								\
		template <typename C> static one test( decltype(&C:: ___function___) ) ;\
		template <typename C> static two test(...);    \
	public:\
		enum { value = sizeof(test<T>(0)) == sizeof(char) };\
	};

	has_function_t(data);
	has_function_t(size);
	has_function_t(reserve);
	has_function_t(___StringInterface);
	has_function_t(___BinaryInterface);

#pragma pack(push,1)
	template <int N> class StaticString : public std::array<char,N>
	{
	public:
		using std::array<char,N>::data;

		StaticString(const char * s)
		{
			auto l = strlen(s);
			l = (l > (N-1)) ? (N-1) : l;
			std::memcpy(data(),s,l);
			self_t[l] = 0;
		}

		/*template < typename T, typename std::enable_if<std::is_array<T>::value && std::is_same<char, typename std::remove_all_extents<T>::type>::value, void* >::type = nullptr > 
		Memory(const T & t):free(0),string(1),other(0), buffer((uint8_t*)&t[0]),length(__array_bytes(t) - 1) {}
		template < typename T, typename std::enable_if<std::is_array<T>::value && !std::is_same<char, typename std::remove_all_extents<T>::type>::value, void* >::type = nullptr > 
		Memory(const T & t) :free(0),string(1),other(0),buffer((uint8_t*)&t[0]),length(__array_bytes(t)) {}

		template <StaticString(*/

		template <typename T> StaticString(const T & s)
		{
			auto l = s.size();
			l = (l >= N) ? N-1 : l;
			std::memcpy(data(),s.data(),l);
			self_t[l] = 0;
		}

		/*StaticString(const std::string & s)
		{
			auto l = s.size();
			l = (l >= N) ? N-1 : l;
			std::memcpy(data(),s.data(),(l > N) ? N : l);
			self_t[l] = 0;
		}*/

		bool operator==(const StaticString<N> & r) const
		{
			auto l1 = strlen(data());
			auto l2 = strlen(r.data());

			if(l1 != l2)
				 return false;
			return strncmp(data(),r.data(),l1) == 0;
		}

		bool operator !=(const StaticString<N> & r) const
		{
			return !(self_t == r);
		}
		
		uint32_t size() const 
		{ 
			return (uint32_t)strlen(data()); 
		}

		StaticString(){self_t[0]=0;}

		void print(std::ostream& os)const
		{
			os.write((const char*)data(), size());
		}

		operator std::string()
		{
			return std::string(data());
		}
	};

	#pragma warning( disable : 4521 )  

	class Memory
	{
	public:

		Memory substr(uint32_t s, int e = 0)
		{
			if(!e)
				e = size()-s;

			if(e<0)
				e = size()+e-s;

			if(s+e > size())
				return Memory();

			return Memory(data()+s,e);
		}

		operator std::string_view() const
		{
			return { (char*)buffer,(size_t)length };
		}

		std::string s16()
		{
			if(size() < 2) throw;

			uint16_t l = rt<uint16_t>();
			if(size() < (uint32_t)(l+2)) throw;

			char * p = (char*)data()+2;
			buffer = buffer + (2+l);
			length -= (2+l);
			
			return std::string(p,l);
		}

		template < typename T > T* NextT(uint32_t & o, const T& del)
		{
			T* itr = pt<T>(o);
			T* start = itr;
			T* end = plt<T>();

			while(itr<=end)
			{
				o++;
				if(del == *itr)
					break;
				itr++;
			}

			if(!(itr<=end))
				o++;

			return start;
		}

		template < typename T,typename D > T* NextUnalignedT(uint32_t & o, const D& del)
		{
			uint8_t* itr = pt<uint8_t>(o);
			uint8_t* start = itr;
			uint8_t* end = plt<uint8_t>();

			while(itr<=end)
			{
				o++;
				if(del == *((D*)itr))
					break;
				itr++;
			}

			if(!(itr<=end))
				o++;

			return (T*)start;
		}

		uint8_t * Next(uint32_t & o, uint8_t del)
		{
			return NextT<uint8_t>(o,del);
		}

		Memory After(uint32_t n)
		{
			return Memory(data()+size(),n);
		}

		Memory QuoteSegment(uint8_t del = '\"')
		{
			uint32_t itr = 0;
			uint8_t * f = nullptr, * l = nullptr;
			for(;itr<length;itr++) {	if(del == *(data()+itr)) { f = data()+itr; break; } }
			for(;itr<length;itr++) {	if(del == *(data()+itr)) { l = data()+itr; break; } }

			if(!(l && f))
				return Memory();

			return Memory(f+1,(uint32_t) ( l-f-2));
		}

		Memory NextString(uint32_t & o, uint8_t del)
		{
			uint32_t start = o;

			char * str = (char*)NextT<uint8_t>(o,del);

			if(!str)
				return Memory();

			return Memory((const char*) str, o-start-1);
		}

		std::pair<Memory,Memory> Divide(uint8_t c)
		{
			for(uint32_t i = 0; i< length; i++)
			{
				if(buffer[i] == c)
					return std::make_pair(Memory(buffer,i),Memory(buffer+i+1,length-(i+1)));
			}

			return std::make_pair(Memory(buffer,length),Memory());
		}

		Memory NextWord(uint32_t & o)
		{
			return NextString(o,' ');
		}

		Memory NextLine(uint32_t & o)
		{
			uint32_t start = o;
			uint16_t nl = '\n';nl<<=8;nl^='\r';
			char * str = NextUnalignedT<char,uint16_t>(o,nl);

			return Memory((const char*) str, o-start-1);
		}

		Memory GetLine()
		{
			uint32_t o = 0;
			Memory result = NextLine(o);

			Seek(o+1);

			return result;
		}

		Memory GetWord()
		{
			uint32_t o = 0;
			Memory result = NextWord(o);

			Seek(o);

			return result;
		}

		Memory GetWord(uint8_t d)
		{
			uint32_t o = 0;
			Memory result = NextString(o,d);

			Seek(o);

			return result;
		}

		Memory SkipWord(uint8_t d)
		{
			uint32_t o = 0;
			NextString(o, d);

			Seek(o);

			return self_t;
		}

		Memory GetString(uint8_t d)
		{
			uint32_t o = 0;
			Memory result = NextString(o,d);

			Seek(o);

			return result;
		}

		Memory& LowerCase()
		{
			std::transform(begin(), end(), begin(), ::tolower);

			return self_t;
		}

		Memory FindI(Memory m)
		{
			if(m.size() > length)
				return Memory();

			Memory result;

			for(uint32_t i = 0; i< length - m.size() + 1; i++)
			{
				bool match = true;
				for(uint32_t j = 0; j < m.size(); j++)
				{
					if(::tolower(*(m.data() + j)) != ::tolower(*(buffer + i + j)))
					{
						match = false;
						break;
					}
				}
				if(match)
					return Memory(buffer+i,m.size());
			}

			return Memory();
		}

		Memory Find(Memory m)
		{
			if(m.size() > length)
				return Memory();

			Memory result;

			for(uint32_t i = 0; i< length - m.size() + 1; i++)
			{
				if(0 == std::memcmp(m.data(),buffer + i,m.size()))
					return Memory(buffer+i,m.size());
			}

			return Memory();
		}

		void HideRight(int32_t r)
		{
			length -= r;
		}

		void Seek(uint32_t n)
		{
			if(n > length) 
			{
				buffer += length;
				length = 0;
			}
			else
			{
				length -= n;
				buffer += n;
			}
		}

		uint8_t *begin() {return buffer;}
		uint8_t *end() {return buffer+length;}

		template < typename T > void Grow(T g, uint32_t target = 0)
		{
			uint32_t new_size =  (uint32_t)(size()* g);
			while (new_size < target)
				new_size = (uint32_t)(new_size * g);

			Memory n;
			n.Create(new_size);
			std::memcpy(n.data(), data(), size());
			Cleanup();
			buffer = n.buffer;
			length = n.length;
		}

		const char * c_str() { return (const char *)data(); }
		void Zero() const
		{
			std::memset(data(), 0, size());
		}

		uint8_t *data() const { return buffer; }
		uint32_t size() const { return length; }
		uint32_t &edit() { return length; }

		bool Join(const Memory & right)
		{
			if(buffer + length == right.buffer)
			{
				length += right.length;
				return true;
			}

			return false;
		}

		template < typename J > J Integer()
		{
			switch(length)
			{
			default:
			case 1:
				return (J)*((uint8_t*)(data()));
			case 2:
				return (J)*((uint16_t*)(data()));
			case 4:
				return (J)*((uint32_t*)(data()));
			case 8:
				return (J)*((uint64_t*)(data()));
			}
		}

		bool StartsWith(Memory m)
		{
			if(m.size() > size()) return false;

			return 0 == std::memcmp(data(),m.data(),m.size());
		}

		void ZoomIn(uint32_t count = 1){buffer+=count;length-=count*2;}
		void ZoomOut(uint32_t count) { buffer -= count; length += count*2; }
		void ZoomLeft(uint32_t count) { buffer -= count; length += count; }
		void ZoomRight(uint32_t count) { buffer += count; length -= count; }

		bool IsBinary() { return 0 == string; }
		bool IsString() { return 1 == string; }

		void SetBinary() { string = 0; }
		void SetString() { string = 1; }

		Memory( const Memory &m)
		{
			buffer = m.buffer;
			length = m.length;
			free = 0;
			string = m.string;
			other = m.other;
		}

		Memory( Memory &m)
		{
			buffer = m.buffer;
			length = m.length;

			free = m.free;
			m.free = 0;
			string = m.string;
			other = m.other;
		}

		Memory Slice(uint32_t i, uint32_t l=-1) const
		{
			if(-1==l)
				l = length - i;

			return Memory(buffer+i,l);
		}

		enum class DynamicType : uint8_t
		{
			_invalid,
			_int,
			_float,
			_string
		};

		DynamicType GetStringType() const 
		{
			if(!size())
				return DynamicType::_invalid;

			bool decimal = false;
			bool characters = false;

			ForEachT<char>([&](char c)
			{
				if(c == '.')
					decimal = true;
				else if(!(c >= '0' && c <= '9'))
					characters = true;
			});

			if(characters)
				return DynamicType::_string;
			else if(decimal)
				return DynamicType::_float;
			else
				return DynamicType::_int;
		}

		template <typename T,typename F> void ForEachT(F f) const
		{
			T * t =(T*)data();
			for(uint32_t i = 0; i < length; i += sizeof(T))
				f(*t++);
		}

		template <typename F> void ForEachT(F f) const
		{
			uint8_t  * t = data();
			for(uint32_t i = 0; i < length; i++ )
				f(*t++);
		}

		template < typename T > bool FindT(const T & f) const
		{
			T * t =(T*)data();
			for(uint32_t i = 0; i < length; i += sizeof(T))
				if(f == t[i])
					return true;

			return false;
		}

		enum class SplitMode
		{
			first,
			index,
			last
		};

		void Merge(const Memory & m)
		{
			if(!buffer) self_t = m;
			else length = (uint32_t)((m.data()-data()) + m.length);
		}

		template< typename T > uint32_t IndexOfT(const T & m) const
		{
			T * t =(T*)data();
			uint32_t j = 0;
			for(uint32_t i = 0; i < length; i += sizeof(T), t++, j++)
			{
				if(m == *t)
					return j;
			}

			return -1;
		}

		template< typename T > uint32_t LastIndexOfT(const T & m) const
		{
			T * t =(T*)data();
			uint32_t j = 0;
			for(uint32_t i = length - 1; i != (uint32_t)-1; i -= sizeof(T))
			{
				if(m == t[i])
					return i;
			}

			return -1;
		}

		template < typename T, typename F > void SplitT(const T & m, F f) const
		{
			T * t =(T*)data(), * p = (T*)data();
			uint32_t j = 0;
			for(uint32_t i = 0; i < length; i += sizeof(T), t++, j++)
			{
				if(m == *t)
				{
					f(Memory(p,(t/*-1*/)-p),(p==(T*)data())?SplitMode::first : SplitMode::index, j);
					p = t + 1;
				}
			}

			f(Memory((uint8_t*)p,((T*)(data()+size())) - p), SplitMode::last,j);
		}

		//Construction
		Memory():buffer(nullptr),length(0),string(1),free(0),other(0)/*, parameter(-1)*/{}
		Memory(const void * data_, uint32_t length_, uint32_t s = 1):string(s),free(0),other(0),buffer((uint8_t*)data_),length(length_) {};
		template < typename T, typename std::enable_if<std::is_array<T>::value && std::is_same<char, typename std::remove_all_extents<T>::type>::value, void* >::type = nullptr > Memory(const T & t):free(0),string(1),other(0), buffer((uint8_t*)&t[0]),length(__array_bytes(t) - 1) {}
		template < typename T, typename std::enable_if<std::is_array<T>::value && !std::is_same<char, typename std::remove_all_extents<T>::type>::value, void* >::type = nullptr > Memory(const T & t) :free(0),string(1),other(0),buffer((uint8_t*)&t[0]),length(__array_bytes(t)) {}
		template < typename T, typename std::enable_if</*!std::is_pointer<T>::value && !std::is_array<T>::value && !std::is_integral<T>::value &&*/ has_function_data<T>::value || has_function_size<T>::value, void*>::type = nullptr > Memory(const T & t):free(0),string(1),other(0), buffer((uint8_t*)t.data()),length((uint32_t)t.size()){}
		template < typename T, typename std::enable_if</*std::is_pointer<T>::value &&*/ std::is_same<char*, T>::value || std::is_same<const char*, T>::value , void* >::type = nullptr> Memory(const T & t) :free(0),string(1),other(0),buffer((uint8_t*)t),length((uint32_t)strlen(t)) {}
		template < typename T, typename std::enable_if<std::is_integral<T>::value, void* >::type = nullptr> Memory(const T & t):free(0),string(1),other(0),buffer((uint8_t*)&t),length((uint32_t)sizeof(T)) {} 
		template < typename T, typename std::enable_if<std::is_enum<T>::value, int>::type = 0 > Memory(const T & t): Memory(GetEnum(t)) {}
		/*template < typename T, typename std::enable_if<has_function____StringInterface<T>::value, int>::type = 0 > Memory(const T & t) 
		{
			std::cout << "todo";
		}
		template < typename T, typename std::enable_if<has_function____BinaryInterface<T>::value, int>::type = 0 > Memory(const T & t) 
		{
			std::cout << "todo";
		}*/

		~Memory()
		{
			if(free) 
				Cleanup();
		}

		//Conversion
		template < typename P, typename std::enable_if<std::is_enum<P>::value, int>::type = 0 >  operator P () const
		{
			P p;
			MatchEnum(p,self_t);
			return p;
		}

		template < typename P, typename std::enable_if<has_function____StringInterface<P>::value || has_function____BinaryInterface<P>::value, int>::type = 0 >  operator P () const
		{
			P p;
			p.Json(self_t);
			return p;
		}

		template < int P > operator StaticString<P> () const
		{
			StaticString<P> p;
			std::memcpy(p.data(),data(),(p.size() > size()) ? size() : p.size());
			return p;
		}

		template < typename T, size_t N > operator std::array<T,N> () const
		{
			std::array<T,N> p;
			std::memcpy(p.data(),data(),(p.size() > size()) ? size() : p.size());
			return p;
		}

		std::string AsString() const
		{
			if(string)
				return std::string((const char*) data(), size()); 
			else 
				return AtkUtility::bytes_as_string(self_t);
		}

		operator std::string() const 
		{ 
			return AsString();
		}

		void operator=(const Memory & r) { string = r.string; free = r.free; other = r.other; buffer = r.data(); length = r.size(); /*parameter = r.parameter;*/ }

		uint8_t & operator[](int32_t index) const { return *(buffer + index); }

		bool operator==(const Memory & r) { if (size() == r.size()) return 0 == std::memcmp(data(), r.data(), size()); return false; }

		bool operator<(const Memory & r) const
		{ 
			if (size() == r.size()) 
				return 1 == std::memcmp(data(), r.data(), size()); 
			else
				return size() < r.size();
		}

		template < typename T, typename std::enable_if<!std::is_array<T>::value, void* >::type = nullptr> bool Compare(const T & r) const
		{
			if (size() == r.size())
				return 0 == std::memcmp(data(), r.data(), size());

			return false;
		}

		template < typename T, typename std::enable_if< std::is_array<T>::value, void* >::type = nullptr > bool Compare(const T & r) const
		{
			if (size() == __array_bytes(r) - 1)
				return 0 == std::memcmp(data(), r, size());

			return false;
		}

		template < typename T > bool CompareFront(const T & r) const
		{
			return 0 == std::memcmp(data(), r.data(), size());
		}

		void operator++(int) {buffer++; length--;}

		template < typename T> bool Trim(T t) {while (length && *buffer == t) { length--; buffer++; } }
		template < typename T> bool ScanTo(T t) { while (length--) if (*buffer++ == t) return true; return false; }
		template < typename T > uint32_t SegmentSize(T * t) { return (uint32_t)(buffer - ((uint8_t*)t)); }

		template <typename T, typename ...t_args> T * AllocateT(t_args&&...args) { return new(Allocate(sizeof(T))) T(std::forward<t_args>(args)...); }
		template <typename T> T * FetchT() { return (T*)Allocate(sizeof(T)); }
		uint8_t * Allocate(uint32_t count, uint32_t*offset = nullptr)
		{
			if (count > length)
				throw "Memory Exception, Stream Buffer Full.";

			if (offset)
				*offset += count;

			uint8_t *result = buffer;
			buffer += count;
			length -= count;

			return result;
		}

		Memory AllocateSegment(uint32_t count)
		{
			if (count > length)
				throw "Memory Exception, Stream Buffer Full.";

			uint8_t *result = buffer;
			buffer += count;
			length -= count;

			return Memory(result,count);
		}

		void Insert(void* data, uint32_t length, uint32_t*offset = nullptr) { memcpy(Allocate(length, offset), data, length); }
		void Insert(const Memory &m, uint32_t*offset = nullptr) { memcpy(Allocate(m.size(), offset), m.data(), m.size()); }
		uint32_t string_uint32 ()
		{
			char* end;
			uint32_t result;

			while (*buffer == ' ') buffer++;

			if(*buffer =='0' && ( (*buffer+1) == 'x' || (*buffer+1) == 'X'))
				result = strtoul((const char*)buffer, &end, 16);
			else
				result = strtoul((const char*)buffer, &end, 10);

			uint32_t m = (uint32_t)(end - (char*)buffer);
			buffer += m;
			length -= m;

			return result;
		}

		void Create(uint32_t size,bool cleanup=true)
		{
			length = size;
			buffer = new uint8_t[length];
			if(cleanup)free = 1;
		}

		template <typename T> void CopyInto(const T & t) { std::memcpy(data(), t.data(), (t.size() > size()) ? size() : t.size()); }

		template <typename T> void CopyOffset(const T & t, uint32_t &o) { std::memcpy(data()+o, t.data(), t.size()); o += (uint32_t)t.size();}

		template < typename Y > Y * pt(uint32_t index=0, uint32_t g_index = 0) const
		{
			return ((Y*)(data()+g_index)) + index;
		}

		template < typename Y > Y & rt(uint32_t index=0, uint32_t g_index = 0) const
		{
			return *pt<Y>(index,g_index);
		}

		template < typename T > T * plt(uint32_t index = 0)
		{
			return (((T*)(data() + size())) - (1 + index));
		}

		template < typename Y > Y & rlt(uint32_t index=0)
		{
			return *plt<Y>(index);
		}

		void Copy(const Memory &r)
		{
			length = r.length;
			buffer = new uint8_t[length];
			free=1;
			string = r.string;
			std::memcpy(buffer, r.buffer, length);
		}

		void Clear() { buffer = nullptr; length = 0; }
		void Cleanup(bool force = false) { if (buffer&&(free||force)) { delete [] buffer; buffer = nullptr; free=0;} length = 0; other = 0; }

		operator uint8_t() const { return (uint8_t)std::stol(std::string(*this)); }

		operator uint64_t() const
		{ 
			/*try
			{
				return (length&&buffer) ? std::stoull(std::string(*this)) : 0; 
			}
			catch(...)
			{
				throw "Memory Exception, failed to read string from memory.";
			}*/

			if(string)
				return (length&&buffer) ? std::stoull(std::string(*this)) : 0; 
			else
			{
				switch(length)
				{
				case 1:
					return (uint64_t)rt<uint8_t>();
				case 2:
					return (uint64_t)rt<uint16_t>();
				case 4:
					return (uint64_t)rt<uint32_t>();
				default:
					return rt<uint64_t>();
				}
			}
		}

		operator int64_t() const
		{ 
			if(string)
				return (length&&buffer) ? std::stoll(std::string(*this)) : 0; 
			else
			{
				switch(length)
				{
				case 1:
					return (int64_t)rt<int8_t>();
				case 2:
					return (int64_t)rt<int16_t>();
				case 4:
					return (int64_t)rt<int32_t>();
				default:
					return rt<int64_t>();
				}
			}
		}

		operator long double() const
		{ 
			if(string)
				return (length&&buffer) ? std::stold(std::string(*this)) : 0; 
			else
				return rt<long double>();
		}

		operator float() const
		{ 
			if(string)
				return std::stof(std::string(*this));
			else
				return rt<float>();
		}

		operator uint32_t() const
		{ 
			if(string)
				return (length&&buffer) ? std::stoul(std::string(*this)) : 0; 
			else
			{
				switch(length)
				{
				case 1:
					return (uint32_t)rt<uint8_t>();
				case 2:
					return (uint32_t)rt<uint16_t>();
				default:
					return rt<uint32_t>();
				}
			}
		}

		operator int32_t() const
		{ 
			if(string)
				return (length&&buffer) ? std::stol(std::string(*this)) : 0;
			else
			{
				switch(length)
				{
				case 1:
					return (int32_t)rt<int8_t>();
				case 2:
					return (int32_t)rt<int16_t>();
				default:
					return rt<int32_t>();
				}
			}
		}

		operator uint16_t() const
		{ 
			if(string)
				return (length&&buffer) ? (uint16_t) std::stoul(std::string(*this)) : 0; 
			else
			{
				switch(length)
				{
				case 1:
					return (uint16_t)rt<uint8_t>();
				default:
					return rt<uint16_t>();
				}
			}
		}
		operator bool() const 
		{ 
			if(!string)
				return buffer[0]>0;

			if(!length)
				return false;

			if(length >= 5 && 0 == std::memcmp(buffer,"false", 5))
				return false;

			if(length >= 4 && 0 == std::memcmp(buffer,"true", 4))
				return true;

			return 0 != std::stol(std::string(*this)); 
		}
		operator void*() const { return (void*)data(); }

		void print(std::ostream& s)const{s.write((const char*)data(), size());/*s<<std::endl;*/}

		void cout() const { std::cout.write((const char*)data(), size()); std::cout << std::endl;  }

		uint8_t * buffer;
		uint32_t length;
	
		uint32_t free:1;
		uint32_t string:1;
		uint32_t other:30;
	};

	#pragma pack(pop)

	class RawBytes
	{
	public:
		RawBytes(const Memory & m)
		{
			buffer = (void*)m.data();
			length = m.size();
		}

		void* data() const {return buffer;}
		size_t size() const {return length;}

		void* buffer;
		size_t length;
	};

	template <typename T> uint32_t _MemorySize(T & t)
	{
		return (uint32_t) t.size();
	}

	template <typename T, typename ... t_args> uint32_t _MemorySize(T & t, t_args ... args)
	{
		return (uint32_t)t.size() + _MemorySize(args...);
	}

	template <typename T> void _JoinMemory(Memory m, uint32_t offset, T & t)
	{
		if (t.data()) std::memcpy(m.data()+offset,t.data(),t.size());
	}

	template <typename T, typename ... t_args> void _JoinMemory(Memory m, uint32_t offset, T & t, t_args ... args)
	{
		if(t.data()) std::memcpy(m.data()+offset,t.data(),t.size());
		_JoinMemory(m,offset+(uint32_t)t.size(),args...);
	}

	template <typename F, typename T> void _JoinMemory2(F f, Memory m, uint32_t offset, T & t)
	{
		if (t.data()) std::memcpy(m.data() + offset, t.data(), t.size()); else f(Memory(m.data() + offset, (uint32_t)t.size()));
	}

	template <typename F, typename T, typename ... t_args> void _JoinMemory2(F f, Memory m, uint32_t offset, T & t, t_args ... args)
	{
		if (t.data()) std::memcpy(m.data() + offset, t.data(), t.size()); else f(Memory(m.data()+offset,(uint32_t)t.size()));
		_JoinMemory2(f, m, offset + (uint32_t)t.size(), args...);
	}

	template < typename P >  typename std::enable_if<std::is_same<P, Memory>::value, void>::type StrPut(std::string & s,const P & _t)
	{
		s += std::string(_t);
	}

	template < typename P >  typename std::enable_if<std::is_same<P, string_view>::value, void>::type StrPut(std::string & s, const P & _t)
	{
		s += std::string(_t);
	}
		
	template < typename P > typename std::enable_if<std::is_integral<P>::value, void>::type StrPut(std::string & s,const P & _t)
	{
		s += std::to_string(_t);
	}

	template < typename P >  typename std::enable_if<std::is_same<P, std::string>::value, void>::type StrPut(std::string & s,const P & t)
	{
		s += t;
	}

	/*template < typename P, int N >  typename std::enable_if<std::is_same<P, StaticString<N> >::value, void>::type StrPut(std::string & s,const P & t)
	{
		s += t;
	}*/

	template < typename P > typename std::enable_if<std::is_same<P, char>::value, void>::type StrPut(std::string & s,const P * t)
	{
		s += t;
	}
		
	template < typename P > typename std::enable_if<std::is_enum<P>::value, void>::type StrPut(std::string & s,const P & _t)
	{
		s += GetEnum(_t);
	}

	template <typename V> void _cat_internal( std::string & s,const V & v)
	{
		StrPut(s, v);
	}

	template <typename V, typename ...t_args> void _cat_internal( std::string & s, const V & v, t_args &&...args)
	{
		StrPut(s,v);
		_cat_internal(s, args...);
	}

	template < typename ...t_args > std::string _cat( t_args &&...args )
	{
		std::string result;

		_cat_internal(result,args...);

		return result;
	}

	class ManagedMemory : public std::vector<uint8_t>
	{
	public:
		ManagedMemory(){}
		ManagedMemory(const Memory & m )
		{
			resize(m.size());
			std::memcpy(data(), m.data(), m.size());
		}
		ManagedMemory(uint32_t s) { resize(s); }

		void Zero() const
		{
			std::memset((void*)data(), 0, size());
		}

		template < typename Y > Y & rt(uint32_t index=0, uint32_t g_index = 0)
		{
			return *pt<Y>(index,g_index);
		}

		template < typename Y > Y & rlt(uint32_t index=0)
		{
			return *plt<Y>(index);
		}

		template < typename T > T * pt(uint32_t index=0,uint32_t offset=0)
		{
			return (((T*)(data()+offset)) + index);
		}

		template < typename T > T * plt(uint32_t index=0)
		{
			return (((T*)(data()+size()))- (1+ index));
		}

		template <typename T, typename ... t_args> T * Allocate(t_args ... args)
		{
			uint32_t s = (uint32_t)size();
			resize(s+sizeof(T));

			new((data()+s)) T(args...);

			return (T*)(data()+s);
		}

		template <typename T, typename ... t_args> uint32_t AllocateOffset(t_args ... args)
		{
			uint32_t s = (uint32_t)size();
			resize(s+sizeof(T));

			new((data()+s)) T(args...);

			return s;
		}

		template <typename T > T* Offset(uint32_t o)
		{
			return (T*)(data()+o);
		}

		template <typename T>void append(const T &t)
		{
			auto os = size();
			resize(os + t.size());
			std::memcpy(data() + os, t.data(), t.size());
		}

		void Stream(uint32_t s)
		{
			resize(size() + sizeof(uint32_t));
			*plt<uint32_t>() = s;
		}

		void Stream(Memory m)
		{
			uint32_t offset = (uint32_t)size();
			resize(offset + m.size());
			std::memcpy(data()+offset,m.data(),m.size());
		}
	};
}