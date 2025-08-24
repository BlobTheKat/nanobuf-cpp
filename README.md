# Nanobuf (C++ implementation)

C++ implementation of the same interface as npm's [nanobuf](https://github.com/BlobTheKat/nanobuf)

Requires C++20. Recommended flags: `-Wall -fno-exceptions`

> This library also exposes the `sstring` class: a `string` type of 4/8 bytes instead of 12/24 (the length is embedded in the backing memory block)
> And the `slice` type which is analogous to `string_view` and implicitly casts
> And a `hexdump()` utility to dump a region of memory to stdout

```cpp
#include "nanobuf.cpp"
using namespace nb;

BufWriter a{};

a.u32(1);
a.f32(1.5);
a.str(std::string("Hello"));
// ...
some_socket.write((slice) a);

BufReader b{a};

assert(b.u32() == 1);
assert(b.f32() == 1.5);
assert(b.str() == "Hello");
assert(!b.remaining());
```