#include <memory>
#include <string>
#include <bit>
#include <cstdint>

namespace nb{

struct slice{
	void* data = 0; size_t size = 0;
	slice(void* a, size_t b) : data(a), size(b){}
	slice(const std::string_view& a) : data((void*)a.data()), size(a.size()){}
	explicit operator std::string() const{return std::string((char*)data,size);}
	operator std::string_view() const{return std::string_view((char*)data,size);}
	template<int N>
	bool operator==(const char (&o)[N]) const{ return size==N-1 && !memcmp(data, o, N-1); }
	bool operator==(const slice&) const = default;
	explicit operator void*() const{return this->data;}
	template<typename T>
	explicit operator T*() const{ return (T*)this->data; }
	void copy_to(void* a) const{
		if(this->size)
			std::memcpy(a, this->data, this->size);
	}
};

class sstring{
	char* _data;
public:
	sstring() : _data(0){}
	sstring(const char* a, size_t len) : _data(){
		if(len){
			_data = (char*)std::malloc(len+sizeof(size_t))+sizeof(size_t);
			((size_t*)_data)[-1] = len;
			std::memcpy(_data, a, len);
		}else _data = 0;
	}
	sstring(const sstring& other){
		if(!other._data){ _data = 0; return; }
		size_t len = ((size_t*)other._data)[-1];
		char* d = (char*) std::malloc(len+sizeof(size_t));
		std::memcpy(_data=d+sizeof(size_t), other._data, *(size_t*)d = len);
	}
	sstring(sstring&& other) : _data(other._data){ other._data = 0; }
	explicit sstring(const std::string& other) : sstring(other.data(), other.size()){}
	explicit sstring(const slice& other) : sstring((char*)other.data, other.size){}
	explicit operator std::string() const{return {_data,_data?((size_t*)_data)[-1]:0};}
	explicit operator char*() const{return _data;}
	operator bool() const{return _data!=0;}
	bool operator!() const{return _data==0;}
	char* data() const{return _data;}
	size_t size() const{return _data?((size_t*)_data)[-1]:0;}
	~sstring(){ if(_data) std::free(_data-sizeof(size_t)); }
	sstring& operator=(const sstring& other){
		this->~sstring();
		return *new (this) sstring(other);
	}
	sstring& operator=(sstring&& other){
		this->~sstring();
		return *new (this) sstring(std::move(other));
	}
};

class BufWriter{
	BufWriter(uint8_t* s, uint8_t* e, uint8_t* h) : start(s), end(e), head(h){}
	uint8_t *start, *end, *head;
public:
	~BufWriter(){ std::free(this->start); }
	size_t size() const{return (size_t)(this->head-this->start);}
	size_t capacity() const{return (size_t)(this->end-this->start);}
	BufWriter(){
		this->start = this->head = (uint8_t*) std::malloc(16);
		this->end = this->start+16;
	}
	// Reinitialize the current object as blank
	void clear(){ this->~BufWriter(); new (this) BufWriter(); }
	BufWriter(BufWriter&& a){
		this->start = a.start; this->end = a.end; this->head = a.head;
		a.start = a.head = a.end = nullptr;
	}
	BufWriter(const BufWriter&) = delete;
	BufWriter& operator=(BufWriter&& a){
		this->~BufWriter();
		this->start = a.start; this->end = a.end; this->head = a.head;
		a.start = a.head = a.end = nullptr;
		return *this;
	}
	BufWriter& operator=(const BufWriter&) = delete;
	// Invalidate the current BufWriter so that it is not destructed. You must std::free this.data() yourself (the buffer must be retrieved before calling invalidate())
	void invalidate(){ this->start = this->head = this->end = nullptr; }
	// Explicitly copy the buffer
	BufWriter copy() const{
		size_t sz = this->head-this->start;
		uint8_t* a = (uint8_t*) std::malloc(sz);
		std::memcpy(a, this->start, sz);
		return {a, a+sz, a+sz};
	}
	// Internal, do not use unless you know what you're doing
	void _expand(){
		size_t sz = (size_t)(this->head-this->start), nsz = (size_t)(this->end-this->start)<<1;
		if(!(this->start = (uint8_t*)realloc(this->start, nsz))){ perror("Out of memory while reallocating Buf"); abort(); }
		this->end = this->start + nsz; this->head = this->start + sz;
	}
	// Skip n bytes in the buffer, leaving them undefined
	void skip(size_t n){
		if(this->head>this->end-n){
			size_t sz = (size_t)(this->head-this->start), nsz = ((size_t)(this->end-this->start)<<1)+n;
			if(!(this->start = (uint8_t*)realloc(this->start, nsz))){ perror("Out of memory while reallocating Buf"); abort(); }
			this->end = this->start + nsz; this->head = (this->start + sz) + n;
		}else this->head += n;
	}
	// T implements static encode(BufWriter&, T&)
	template<typename T>
	void encode(const T& a){ T::encode(*this, a); }
	void b1(size_t& bw, uint8_t bit){
		if(!(bw&7)){
			if(this->head==this->end) this->_expand();
			bw = (this->head-this->start)<<3;
			*this->head++ = 0;
		}
		this->start[bw>>3] |= bit<<(bw&7); bw++;
	}
	void b2(size_t& bw, uint8_t bit){
		uint8_t b = bw&7;
		if(b){
			this->start[bw>>3] |= bit<<b;
			if(b<7){bw+=2;return;}
			bw = b = 1;
		}else bw = 2;
		if(this->head==this->end) this->_expand();
		bw |= (this->head-this->start)<<3;
		*this->head++ = bit>>b;
	}
	void b4(size_t& bw, uint8_t bit){
		uint8_t b = bw&7;
		if(b){
			this->start[bw>>3] |= bit<<b;
			if(b<5){bw+=4;return;}
			bw = b-4; b = 8-b;
		}else bw = 4;
		if(this->head==this->end) this->_expand();
		bw |= (this->head-this->start)<<3;
		*this->head++ = bit>>b;
	}
	// Store a signed int in [0,9223372036854775807] using fewer bytes for smaller numbers
	// 0-63                          -> 00xxxxxx
	// 64-8191                       -> 010xxxxx xxxxxxxx
	// 8192-536870911                -> 011xxxxx xxxxxxxx xxxxxxxx xxxxxxxx
	// 536870912-9223372036854775807 -> 1xxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx
	void v64(uint64_t n){
		if(this->head>this->end-8) this->_expand();
		if(n<64) *this->head++ = n;
		else if(n<8192) *this->head = n>>8|0x40, this->head[1] = n, this->head+=2;
		else if(n<536870912) *this->head = n>>24|0x60, this->head[1] = n>>16, this->head[2] = n>>8, this->head[3] = n, this->head+=4;
		else *this->head = n>>56|0x80, this->head[1] = n>>48, this->head[2] = n>>40, this->head[3] = n>>32, this->head[4] = n>>24, this->head[5] = n>>16, this->head[6] = n>>8, this->head[7] = n, this->head+=8;
	}
	// Store a signed int in [0,2147483647] using fewer bytes for smaller numbers
	// 0-63             -> 00xxxxxx
	// 64-16383         -> 01xxxxxx xxxxxxxx
	// 16384-2147483647 -> 1xxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx
	void v32(uint32_t n){
		if(this->head>this->end-4) this->_expand();
		if(n<64) *this->head++ = n;
		else if(n<16384) *this->head = n>>8|0x40, this->head[1] = n, this->head+=2;
		else *this->head = n>>24|0x80, this->head[1] = n>>16, this->head[2] = n>>8, this->head[3] = n, this->head+=4;
	}
	// Store a signed int in [0,32767] using fewer bytes for smaller numbers
	// 0-127     -> 0xxxxxxx
	// 128-32767 -> 1xxxxxxx xxxxxxxx
	void v16(uint16_t n){
		if(this->head>this->end-2) this->_expand();
		if(n<128) *this->head++ = n;
		else *this->head = n>>8|0x80, this->head[1] = n, this->head+=2;
	}
	void u8(uint8_t n){
		if(this->head==this->end) this->_expand();
		*this->head++ = n;
	}
	void i8(int8_t n){u8(n);}
	void u16(uint16_t n){
		if(this->head>=this->end-1) this->_expand();
		*this->head = n>>8;
		this->head[1] = n;
		this->head += 2;
	}
	void i16(int16_t n){u16(n);}
	void u24(uint32_t n){
		if(this->head>this->end-3) this->_expand();
		*this->head = n>>16; this->head[1] = n>>8;
		this->head[2] = n; this->head += 3;
	}
	void i24(int32_t n){u24(n);}
	void u32(uint32_t n){
		if(this->head>this->end-4) this->_expand();
		*this->head = n>>24; this->head[1] = n>>16;
		this->head[2] = n>>8; this->head[3] = n;
		this->head += 4;
	}
	void i32(int32_t n){u32(n);}
	void u48(uint64_t n){
		if(this->head>this->end-6) this->_expand();
		*this->head = n>>40; this->head[1] = n>>32;
		this->head[2] = n>>24; this->head[3] = n>>16;
		this->head[4] = n>>8; this->head[5] = n;
		this->head += 6;
	}
	void i48(uint64_t n){u48(n);}
	void u64(uint64_t n){
		if(this->head>this->end-8) this->_expand();
		*this->head = n>>56; this->head[1] = n>>48;
		this->head[2] = n>>40; this->head[3] = n>>32;
		this->head[4] = n>>24; this->head[5] = n>>16;
		this->head[6] = n>>8; this->head[7] = n;
		this->head += 8;
	}
	void i64(uint64_t n){u64(n);}
	void f32(float n){u32(std::bit_cast<uint32_t>(n));}
	void f64(double n){u64(std::bit_cast<uint64_t>(n));}
	void str(const void* s, size_t n){
		if(this->head>this->end-n-4){
			size_t sz = (size_t)(this->head-this->start), nsz = ((this->end-this->start)<<1)+n;
			if(!(this->start = (uint8_t*)realloc(this->start, nsz))){ perror("Out of memory while reallocating Buf"); abort(); }
			this->end = this->start + nsz; this->head = this->start + sz;
		}
		if(n<64) *this->head++ = n;
		else if(n<16384) *this->head = n>>8|0x40, this->head[1] = n, this->head+=2;
		else *this->head = n>>24|0x80, this->head[1] = n>>16, this->head[2] = n>>8, this->head[3] = n, this->head+=4;
		if(n){ std::memcpy(this->head, s, n); this->head += n; }
	}
	void u8arr(const void* s, size_t n){
		if(this->head>this->end-n){
			size_t sz = (size_t)(this->head-this->start), nsz = ((this->end-this->start)<<1)+n;
			if(!(this->start = (uint8_t*)realloc(this->start, nsz))){ perror("Out of memory while reallocating Buf"); abort(); }
			this->end = this->start + nsz; this->head = this->start + sz;
		}
		if(n){ std::memcpy(this->head, s, n); this->head += n; }
	}
	void str(const slice& s){ str(s.data,s.size); }
	void str(const sstring& s){ str(s.data(),s.size()); }
	void str(const std::string_view& s){ str(s.data(),s.size()); }
	void str(const char* s){ str(s,strlen(s)); }
	void u8arr(const slice& s){ u8arr(s.data,s.size); }
	void u8arr(const sstring& s){ u8arr(s.data(),s.size()); }
	void u8arr(const std::string_view& s){ u8arr(s.data(),s.size()); }
	operator std::string() const{
		return std::string((char*)this->start, (size_t)(this->head-this->start));
	}
	operator slice() const{
		return {(char*)this->start, (size_t)(this->head-this->start)};
	}
	char* data() const{return (char*)this->start;}
	friend class BufReader;
};

// Buffer reader that gracefully handles out-of-bounds by returning 0
class BufReader{
	uint8_t* head; uint8_t* end;
public:
	BufReader(const void* dat, size_t size){
		this->head = (uint8_t*) dat; this->end = this->head+size;
	}
	BufReader(const BufWriter& b){
		this->head = b.start; this->end = b.end;
	}
	BufReader(const slice& s){
		this->head = (uint8_t*) s.data; this->end = this->head+s.size;
	}
	static BufReader c_str(const char* dat){
		return BufReader(dat, strlen(dat));
	}
	BufReader(const std::string& dat){
		this->head = (uint8_t*) dat.data();
		this->end = this->head + dat.size();
	}
	// Get how many bytes are remaining in the buffer. Running past the end is safe but you can use this for reading rest arrays at the end of a buffer
	size_t remaining() const{
		return (size_t)(this->end-this->head);
	}
	// Skip n bytes
	void skip(size_t n){ this->head += n; }
	// T implements static decode(BufReader&, T&)
	template<typename T>
	auto decode(T& a){ T::decode(*this, a); return a; }
	// T implements static decode(BufReader&, T&)
	template<typename T>
	T decode(){
		T a; T::decode(*this, (T&) a);
		return a;
	}
	uint8_t b1(uint16_t& bw){
		uint16_t a = bw;
		if(a<2) a = ++this->head>this->end ? 256 : this->head[-1]|256;
		bw = a>>1; return a&1;
	}
	uint8_t b2(uint16_t& bw){
		uint16_t a = bw;
		if(a<4){
			bool x = a<2;
			a = (a&x)|(++this->head>this->end ? 256 : this->head[-1]|256)<<x;
		}
		bw = a>>2; return a&3;
	}
	uint8_t b4(uint16_t& bw){
		uint16_t a = bw;
		if(a<16){
			uint8_t x = -21936>>(a<<1)&3;
			a = (a&7>>(3-x))|(++this->head>this->end ? 256 : this->head[-1]|256)<<x;
		}
		bw = a>>4; return a&15;
	}
	// Load a signed int in [0,9223372036854775807] reading fewer bytes for smaller numbers
	// 00xxxxxx -> 0-63
	// 010xxxxx xxxxxxxx -> 0-8191
	// 011xxxxx xxxxxxxx xxxxxxxx xxxxxxxx -> 0-536870911
	// 1xxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx -> 0-9223372036854775807
	uint64_t v64(){
		if(this->head>=this->end) return (this->head++,0);
		uint64_t n = *this->head++;
		if(n<64) return n;
		else if(n<96) return ++this->head>this->end?0:(n&0x1F)<<8|(uint64_t)this->head[-1];
		else if(n<128) return (this->head+=3)>this->end?0:(n&0x1F)<<24|(uint64_t)this->head[-3]<<16|(uint64_t)this->head[-2]<<8|(uint64_t)this->head[-1];
		else return (this->head+=7)>this->end?0:(n&0x7F)<<56|(uint64_t)this->head[-7]<<48|(uint64_t)this->head[-6]<<40|(uint64_t)this->head[-5]<<32|(uint64_t)this->head[-4]<<24|(uint64_t)this->head[-3]<<16|(uint64_t)this->head[-2]<<8|(uint64_t)this->head[-1];
	}
	// Load a signed int in [0,2147483647] reading fewer bytes for smaller numbers
	// 00xxxxxx -> 0-63
	// 01xxxxxx xxxxxxxx -> 0-16383
	// 1xxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx -> 0-2147483647
	uint32_t v32(){
		if(this->head>=this->end) return (this->head++,0);
		uint32_t n = *this->head++;
		if(n<64) return n;
		else if(n<128) return ++this->head>this->end?0:(n&0x3F)<<8|(uint32_t)this->head[-1];
		else return (this->head+=3)>this->end?0:(n&0x7F)<<24|(uint32_t)this->head[-3]<<16|(uint32_t)this->head[-2]<<8|(uint32_t)this->head[-1];
	}
	// Load a signed int in [0,32767] reading fewer bytes for smaller numbers
	// 0xxxxxxx -> 0-127
	// 1xxxxxxx xxxxxxxx -> 0-32767
	uint16_t v16(){
		if(this->head>=this->end) return (this->head++,0);
		uint16_t n = *this->head++;
		if(n<128) return n;
		else return ++this->head>this->end?0:(n&0x7F)<<8|(uint16_t)this->head[-1];
	}
	uint8_t peek(size_t n = 0){ return this->head>=this->end-n ? 0 : this->head[n]; }
	uint8_t u8(){ return ++this->head>this->end ? 0 : this->head[-1]; }
	int8_t i8(){ return u8(); }
	uint16_t u16(){ return (this->head+=2)>this->end ? 0 : this->head[-2]<<8|this->head[-1]; }
	int16_t i16(){return u16();}
	uint32_t u24(){ return (this->head+=3)>this->end ? 0 : this->head[-3]<<16|this->head[-2]<<8|this->head[-1]; }
	int32_t i24(){return u24();}
	uint32_t u32(){ return (this->head+=4)>this->end ? 0 : this->head[-4]<<24|this->head[-3]<<16|this->head[-2]<<8|this->head[-1]; }
	int32_t i32(){ return u32(); }
	uint64_t u48(){
		return (this->head+=6)>this->end ? 0 : uint64_t(this->head[-6]<<24|this->head[-5]<<16|this->head[-4]<<8|this->head[-3])<<16|(this->head[-2]<<8|this->head[-1]);
	}
	int64_t i48(){ return u48(); }
	uint64_t u64(){
		return (this->head+=8)>this->end ? 0 : uint64_t(this->head[-8]<<24|this->head[-7]<<16|this->head[-6]<<8|this->head[-5])<<32|(this->head[-4]<<24|this->head[-3]<<16|this->head[-2]<<8|this->head[-1]);
	}
	int64_t i64(){ return u64(); }
	float f32(){ return std::bit_cast<float>(u32()); }
	double f64(){ return std::bit_cast<double>(u64()); }
	slice str(){
		if(this->head>=this->end) return {++this->head,0};
		uint8_t n = *this->head++;
		if(n>=64){
			if(n<128) n = ++this->head>this->end?0:(n&0x3F)<<8|this->head[-1];
			else n = (this->head+=3)>this->end?0:(n&0x7F)<<24|this->head[-3]<<16|this->head[-2]<<8|this->head[-1];
		}
		uint8_t* s = this->head;
		this->head = s + n;
		return {s, n};
	}
	slice u8arr(size_t n){
		if(this->head>this->end-n) return slice {this->head,0};
		uint8_t* s = this->head;
		this->head = s + n;
		return slice {s, n};
	}
	std::string remainder() const{
		return std::string((char*)this->head, (size_t)(this->end-this->head));
	}
	bool overran() const{return this->head>this->end;}
	char* data() const{ return (char*) head; }
};

inline void hexdump(const void *data, size_t len){
	printf("\x1b[32m=== %p (%zu bytes) ===\x1b[m\n", data, len);
	if(!len) return;
	const uint8_t *p = (const uint8_t *)data;
	char* str = (char*) std::malloc(len*3 + ((len+3)>>2));
	char* str2 = str;
	const char dig[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
	for(int i = 0; i < len; i++){
		uint8_t byte = p[i];
		str2[0] = dig[byte>>4]; str2[1] = dig[byte&15]; str2[2] = ' ';
		str2 += 3;
		if((i&3)==3)
			*str2++ = (i&15)==15 ? '\n' : ' ';
	}
	str2[-1] = '\n';
	fwrite(str, 1, str2-str, stdout);
	free(str);
}
inline void hexdump(const std::string_view& s){ hexdump(s.data(), s.size()); }
inline void hexdump(const slice& s){ hexdump(s.data, s.size); }

}