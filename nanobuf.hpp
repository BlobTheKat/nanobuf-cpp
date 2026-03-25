#include <string>

namespace nb{

struct slice{
	void* data = 0; size_t size = 0;
	slice(void* a, size_t b);
	slice(const std::string_view& a);
	explicit operator std::string() const;
	operator std::string_view() const;
	template<int N>
	bool operator==(const char (&o)[N]) const;
	bool operator==(const slice&) const;
	explicit operator void*() const;
	template<typename T>
	explicit operator T*() const;
	void copy_to(void* a) const;
};

class sstring{
	char* _data;
public:
	sstring();
	sstring(const char* a, size_t len);
	sstring(const sstring& other);
	sstring(sstring&& other);
	explicit sstring(const slice& other);
	explicit sstring(const std::string& other);
	explicit operator std::string() const;
	explicit operator char*() const;
	operator bool() const;
	bool operator!() const;
	char* data() const;
	size_t size() const;
	~sstring();
	sstring& operator=(const sstring& other);
	sstring& operator=(sstring&& other);
};

class BufWriter{
	uint8_t *start, *end, *head;
public:
	~BufWriter();
	size_t size() const;
	size_t capacity() const;
	BufWriter();
	// Reinitialize the current object as blank
	void clear();
	BufWriter(BufWriter&& a);
	BufWriter(const BufWriter& a) = delete;
	BufWriter& operator=(BufWriter&& a);
	BufWriter& operator=(const BufWriter& a) = delete;
	// Invalidate the current BufWriter so that it is not destructed. You must std::free this.data() yourself (the buffer must be retrieved before calling invalidate())
	void invalidate();
	// Explicitly copy the buffer
	BufWriter copy() const;
	// Internal, do not use unless you know what you're doing
	void _expand();
	// Skip n bytes in the buffer, leaving them undefined
	void skip(size_t n);
	// T implements static encode(BufWriter&, T&)
	template<typename T>
	void encode(const T& a);
	void b1(size_t& bw, uint8_t bit);
	void b2(size_t& bw, uint8_t bit);
	void b4(size_t& bw, uint8_t bit);
	// Store a signed int in [0,9223372036854775807] using fewer bytes for smaller numbers
	// 0-63                          -> 00xxxxxx
	// 64-8191                       -> 010xxxxx xxxxxxxx
	// 8192-536870911                -> 011xxxxx xxxxxxxx xxxxxxxx xxxxxxxx
	// 536870912-9223372036854775807 -> 1xxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx
	void v64(uint64_t n);
	// Store a signed int in [0,2147483647] using fewer bytes for smaller numbers
	// 0-63             -> 00xxxxxx
	// 64-16383         -> 01xxxxxx xxxxxxxx
	// 16384-2147483647 -> 1xxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx
	void v32(uint32_t n);
	// Store a signed int in [0,32767] using fewer bytes for smaller numbers
	// 0-127     -> 0xxxxxxx
	// 128-32767 -> 1xxxxxxx xxxxxxxx
	void v16(uint16_t n);
	void u8(uint8_t n);
	void i8(int8_t n);
	void u16(uint16_t n);
	void i16(int16_t n);
	void u24(uint32_t n);
	void i24(int32_t n);
	void u32(uint32_t n);
	void i32(int32_t n);
	void u48(uint64_t n);
	void i48(uint64_t n);
	void u64(uint64_t n);
	void i64(uint64_t n);
	void f32(float n);
	void f64(double n);
	void str(const void* s, size_t n);
	void u8arr(const void* s, size_t n);
	void str(const slice& s);
	void str(const sstring& s);
	void str(const std::string_view& s);
	void str(const char* s);
	void u8arr(const slice& s);
	void u8arr(const sstring& s);
	void u8arr(const std::string_view& s);
	operator std::string() const;
	operator slice() const;
	char* data() const;
	friend class BufReader;
};

// Buffer reader that gracefully handles out-of-bounds by returning 0
class BufReader{
	uint8_t* head; uint8_t* end;
public:
	BufReader(const void* dat, size_t size);
	BufReader(const BufWriter& b);
	BufReader(const slice& s);
	static BufReader c_str(const char* dat);
	BufReader(const std::string& dat);
	// Get how many bytes are remaining in the buffer. Running past the end is safe but you can use this for reading rest arrays at the end of a buffer
	size_t remaining() const;
	// Skip n bytes
	void skip(size_t n);
	// T implements static decode(BufReader&, T&)
	template<typename T>
	auto decode(T& a);
	// T implements static decode(BufReader&, T&)
	template<typename T>
	T decode();
	uint8_t b1(uint16_t& bw);
	uint8_t b2(uint16_t& bw);
	uint8_t b4(uint16_t& bw);
	// Load a signed int in [0,9223372036854775807] reading fewer bytes for smaller numbers
	// 00xxxxxx -> 0-63
	// 010xxxxx xxxxxxxx -> 0-8191
	// 011xxxxx xxxxxxxx xxxxxxxx xxxxxxxx -> 0-536870911
	// 1xxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx -> 0-9223372036854775807
	uint64_t v64();
	// Load a signed int in [0,2147483647] reading fewer bytes for smaller numbers
	// 00xxxxxx -> 0-63
	// 01xxxxxx xxxxxxxx -> 0-16383
	// 1xxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx -> 0-2147483647
	uint32_t v32();
	// Load a signed int in [0,32767] reading fewer bytes for smaller numbers
	// 0xxxxxxx -> 0-127
	// 1xxxxxxx xxxxxxxx -> 0-32767
	uint16_t v16();
	inline uint8_t peek(size_t n = 0);
	uint8_t u8();
	inline int8_t i8();
	uint16_t u16();
	inline int16_t i16();
	uint32_t u24();
	inline int32_t i24();
	uint32_t u32();
	inline int32_t i32();
	uint64_t u48();
	inline int64_t i48();
	uint64_t u64();
	inline int64_t i64();
	inline float f32();
	inline double f64();
	slice str();
	slice u8arr(size_t n);
	std::string remainder() const;
	bool overran() const;
	char* data() const;
};

inline void hexdump(const void *data, size_t len);
inline void hexdump(const std::string_view& s);
inline void hexdump(const slice& s);

}