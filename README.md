# Nanobuf (C++ implementation)

C++ implementation of the same interface as npm's [nanobuf](https://github.com/BlobTheKat/nanobuf). Requires C++20

- Usable as both a statically linked library and header-only library (include the .cpp file for the latter)
	- If you are compiling separately (i.e library), `-c -std=c++20 -O3 -Wall -fno-exceptions -flto` as a minimum

This library also exposes
- `class sstring`: a butterfly-pointer string class. `sizeof(sstring) == sizeof(char*)`
- `struct slice`: type which is analogous to `string_view` and implicitly casts between the two
- A `hexdump(void*,size_t)` utility to dump a region of memory to stdout


```cpp
#include "nanobuf.cpp"
#include <cassert>

nb::BufWriter test_write(){
	nb::BufWriter a{};

	a.u32(1);
	a.f32(1.5);
	a.str(std::string("Hello"));
	// ...
	
	return a;
}

void test_read(nb::BufReader b){
	assert(b.u32() == 1);
	assert(b.f32() == 1.5);
	assert(b.str() == "Hello");

	assert(!b.remaining());
}

int main(){
	nb::BufWriter buf = test_write();
	//some_socket.write((nb::slice) buf);
	test_read((nb::BufReader) (nb::slice) buf);
}
```