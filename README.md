# Pool Allocator (C++)

A fixed-size, typed memory pool allocator written from scratch in C++. It requests
one large chunk of memory directly from the operating system, divides it into
equal blocks, and hands them out and takes them back in O(1) using a free list
embedded inside the free blocks themselves.

Built on Windows with `VirtualAlloc` / `VirtualFree` (the Windows equivalent of
Linux's `mmap` / `munmap`).

---

## Features

- **Typed pools:** `PoolAllocator<T>` serves objects of one type. Block size and
  alignment are computed from `T` at compile time; the caller only chooses how
  many blocks.
- **O(1) allocation and deallocation** via an embedded free list — no extra
  bookkeeping memory per block.
- **In-place construction:** `create(args...)` constructs a `T` directly in a
  pool block using placement new and perfect forwarding, so any constructor
  can be used.
- **Explicit destruction:** `destroy(p)` runs the destructor and returns the
  block to the pool.
- **Exception safe:** if `T`'s constructor throws inside `create`, the block is
  returned to the pool before the exception propagates.
- **Guaranteed alignment:** every block address is a multiple of
  `alignof(T)`, including over-aligned types (e.g. `alignas(32)`).
- **Ownership checks:** `destroy` rejects pointers that don't belong to the
  pool or don't point to the start of a block.
- **Move semantics:** pools can be moved (ownership of the chunk transfers);
  copying is disabled to prevent double frees.

## Usage

```cpp
#include "allocator.h"

struct Person {
    Person(std::string_view name, int age);
    // ...
};

int main() {
    PoolAllocator<Person> pool(4);           // room for 4 Person objects

    Person* a = pool.create("Anthony", 24);  // constructed in place
    Person* b = pool.create("Jeff", 27);

    pool.destroy(a);                         // destructor runs, block reused
    pool.destroy(b);
}
```

## Building

Header-only (`allocator.h`). Requires a C++17 compiler on Windows
(tested with Visual Studio / MSVC). Include the header and build normally.

## How it works

**The pool.** The constructor requests `blockSize * blockCount` bytes with
`VirtualAlloc`. That memory is just raw bytes — "blocks" are an interpretation
layered on top of it.

**The free list.** A free block isn't holding user data, so its first bytes are
reused to store a pointer to the next free block. At startup the constructor
walks the chunk in `blockSize` steps and links every block together, ending
with `nullptr`. The allocator keeps one pointer, `head`, to the first free block.

- **Allocate (pop):** take the block `head` points to, and advance `head` to the
  next free block.
- **Deallocate (push):** write the current `head` into the freed block, and make
  it the new `head`.

When a block is handed out, the caller gets the *entire* block — including the
bytes that held the free-list pointer. Those bytes switch roles: bookkeeping
while free, user data while allocated.

**Block size and alignment** are computed at compile time:

```
alignment = max(alignof(T), alignof(Node))
blockSize = max(sizeof(T), sizeof(Node)) rounded up to a multiple of alignment
```

Each block must be able to hold either a `T` (allocated) or a free-list node
(free), and rounding to the alignment keeps *every* block's address aligned,
not just the first. Both invariants are verified with `static_assert`.

**Memory vs. objects.** Allocating memory and constructing an object are separate
steps. `allocate` (private) only hands out raw memory; `create` constructs a `T`
in it with placement new. Likewise, `destroy` calls the destructor first, then
returns the memory — in that order, because returning a block overwrites its
first bytes with the free-list pointer.

## Safety notes

Allocator bugs are a classic source of security vulnerabilities, and this
design touches several of them directly:

- **Invalid free:** returning a pointer the pool doesn't own would corrupt its
  free list. `destroy` checks that the address lies inside the pool's chunk and
  falls on a block boundary before accepting it.
- **Use-after-free:** while a block is free, its first bytes hold the allocator's
  next pointer. Writing to a block after destroying it overwrites that pointer,
  which in real allocators can be exploited to make a later allocation return an
  attacker-chosen address. (Further hardening is planned — see below.)
- **Double free by copy:** copy operations are deleted, so two pools can never
  own the same chunk.

## Testing

Tested with:
- construction/destruction counting (every created object is destroyed)
- block reuse (a freed block is returned by the next `create`)
- pool exhaustion (allocating past capacity throws `std::bad_alloc`)
- alignment of every block for an `alignas(32)` type
- move construction and move assignment (no leaks, no double frees)
- rejection of pointers from a different pool

## Limitations / future work

- **Fixed-size blocks only.** Each pool serves one type. A general-purpose
  `malloc` / `free` with variable-size blocks, headers, splitting, and coalescing
  is the next milestone.
- **Live objects aren't destroyed automatically.** All objects must be
  `destroy`ed before the pool goes out of scope; the pool releases memory but
  does not run destructors for objects still alive in it.
- **Double-free detection** and header integrity checks are planned as a
  security-hardening milestone.
- **Windows only** (`VirtualAlloc`); a Linux port would use `mmap` / `munmap`.

## What I learned

Built alongside the free-space management chapter of *Operating Systems: Three
Easy Pieces* to understand what happens beneath `malloc` and `new`. Key lessons:
memory is just bytes and types are only an interpretation of them; how pointer
arithmetic and casts actually behave; the difference between allocating memory
and constructing an object; alignment; and modern C++ — class templates,
`constexpr` and `static_assert`, variadic templates with perfect forwarding,
placement new, exception safety, RAII, and move semantics (the Rule of Five).

---
Niangado
