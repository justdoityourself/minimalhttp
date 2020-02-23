/* Copyright D8Data 2017 */

#pragma once

namespace AtkFile
{
	class FileC
	{
	public:
		FileC() : fd(nullptr) {}

		FileC(const std::string & file, bool read_only_ = true, char* mode="ab+") : fd(nullptr) { Open(file, read_only_, mode); }

		~FileC() { Close(); }

		void Open ( const std::string & file, bool read_only_ = true, char* mode="ab+")
		{
			if(!file.size())
				return;

            fd = fopen ( file.c_str(), (read_only_) ? "rb" : mode );
        }

		void Close ( ) { if ( fd ) { fclose ( fd ); fd = nullptr; } }

		bool Write ( const uint8_t * data, uint32_t length ) const 
		{ 
			if(!fd) return false;

			if(!fwrite( data,1,length, fd))
				return false;
			return true;
		}

		template < typename T > const FileC& Write(const T & t) const
		{
			Write(( const uint8_t*)t.data(), (uint32_t)t.size()); return self_t;
		}

		template < typename T > const FileC& WriteL(const T & t) const
		{
			Write(( const uint8_t*)t.data(), (uint32_t)t.size()); return self_t;
		}

		template < typename T, typename ...args_T > const FileC& WriteL(const T & t, args_T ... args) const
		{
			Write(( const uint8_t*)t.data(), (uint32_t)t.size()); 
			Write(args...);
			
			return self_t;
		}

#ifdef _WIN32

		bool Seek ( uint64_t o ) 
		{ 
			if(_fseeki64(fd,o,SEEK_SET)) 
				return false;

			return true;
		}

		void End() const
		{
			_fseeki64 (fd , 0 , SEEK_END);
		}

		uint64_t size ( )
		{
			if(!fd) return 0;

			_fseeki64 (fd , 0 , SEEK_END);
			uint64_t ret = _ftelli64 (fd);
			rewind(fd);
			return ret;
		}
#else
		void Seek ( uint64_t o )
		{
            fseeko64(fd,o,SEEK_SET);
        }

		void End() const
		{
			fseeko64 (fd , 0 , SEEK_END);
		}

		uint64_t size ( )
		{
			if(!fd) return 0;

			fseeko64 (fd , 0 , SEEK_END);
			uint64_t ret = ftello64 (fd);
			rewind(fd);
			return ret;
		}
#endif

		const FileC& Append(const uint8_t * data, uint32_t length) const { End(); Write(data, length); return self_t; }

		template < typename T > const FileC& Append(const T & t) const
		{
			return Append(( const uint8_t*)t.data(), (uint32_t)t.size());
		}

		template < typename ... args_T > const FileC& AppendL(args_T ... args) const
		{
			End();
			return WriteL(args...);
		}

		bool Read ( uint8_t * data, uint32_t length ) 
		{ 
			if(!fd) return false;

			if(!fread( data,1,length, fd))
				return false;

			return true;
		}

		template < typename T > bool Read ( T & m ) 
		{ 
			if(!fd) return false;

			if(!fread( m.data(),1,m.size(), fd))
				return false;

			return true;
		}

		void Flush() { fflush(fd);}

	private:
		FILE * fd;
	};
}