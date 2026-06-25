# C++ Reference: Move Semantics

## C: Copy semantics only

In C, everything is a bitwise copy:

```c
int a = 5;
int b = a;        // copies the value 5

int *p = &a;
int *q = p;       // copies the address (8 bytes on 64-bit)

struct Foo x = y; // copies every byte of the struct
```

If a struct contains a pointer, the pointer (address) is copied — now two structs point to the same memory (shallow copy). C has no way to express "transfer ownership."

## C++: Copy vs Move

C++ adds classes with destructors (RAII). A `std::function` internally heap-allocates a closure (captures variables). If you only copy:

```cpp
std::function<void()> a = [some_captures]{ ... };
std::function<void()> b = a;  // COPY: allocates NEW heap memory, duplicates the closure
```

This is wasteful when `a` is about to be discarded (e.g., a temporary passed to a constructor).

**Move** means: "steal the guts of the source, leave it empty."

```cpp
std::function<void()> b = std::move(a);
// b now owns a's internal heap pointer
// a is left in a valid-but-empty state (like a nullptr)
// NO allocation, NO copy — just pointer reassignment internally
```

## What `std::move` actually does

`std::move` itself does **nothing at runtime**. It's just a cast:

```cpp
// std::move(x) is equivalent to:
static_cast<T&&>(x)    // "treat x as an rvalue reference"
```

This tells the compiler: "I'm done with `x`, you may call the **move constructor** instead of the copy constructor." The move constructor is what actually transfers ownership (swaps internal pointers).

## State of the old variable after move

The old variable is left in a **valid but unspecified state**. In practice, for standard library types:

| Type | After move, old value is: |
|------|--------------------------|
| `std::string` | empty `""` |
| `std::vector` | empty `{}` |
| `std::function` | empty (calling it is undefined) |
| `std::unique_ptr` | `nullptr` |
| `int`, `float`, etc. | unchanged (move = copy for primitives) |

```cpp
std::string a = "hello";
std::string b = std::move(a);
// b == "hello"
// a == ""  (gutted — internal pointer was stolen)
```

The standard only guarantees the old variable is **safe to destroy or reassign**. You shouldn't read from it expecting meaningful data. Think of it like handing someone your house keys — you still exist, but your house is now empty.

## Example: Executor constructor

```cpp
Executor(StepFn step, HaltedFn halted)
    : step_(std::move(step)), halted_(std::move(halted)) {}
```

1. `step` is a parameter (already a local copy in the function)
2. `std::move(step)` casts it to an rvalue → calls `step_`'s move constructor
3. Move constructor: `step_` steals the heap-allocated lambda from `step`
4. `step` is now empty (no deallocation needed when it goes out of scope)

Without `std::move`:
```cpp
: step_(step)  // COPY constructor: allocates new heap memory, copies the lambda
```

## The analogy to C pointers

```c
// "Copy" in C:
char *copy = malloc(strlen(src) + 1);
strcpy(copy, src);   // allocate + duplicate

// "Move" in C (if we had it):
char *moved = src;   // just take the pointer
src = NULL;          // original gives up ownership
```

That's exactly what C++ move does internally for types like `std::function`, `std::vector`, `std::string` — they all hold a heap pointer, and move just transfers that pointer.

## Summary table

| | C | C++ Copy | C++ Move |
|--|--|----------|----------|
| int/float | bitwise copy | same | same (no benefit) |
| struct with pointer | copies the pointer (shallow) | deep copy (allocates) | steals pointer, nulls source |
| std::function | N/A | allocates new closure | transfers closure pointer |

---

# C++ Reference: References (`T&`) vs Pointers (`T*`)

## Core comparison

A reference is essentially a pointer that the compiler manages for you. Under the hood, `T&` compiles to the same machine code as `T*` — it's an address. The differences are all at the language level:

| | `T*` (C pointer) | `T&` (C++ reference) |
|--|--|--|
| Can be null | Yes (`T* p = NULL;`) | No — must always refer to something |
| Can be reassigned | Yes (`p = &other;`) | No — bound at initialization, forever |
| Syntax to access | `*p` or `p->member` | Just `x` or `x.member` (looks like a value) |
| Must initialize | No (`T* p;` is valid, dangerous) | Yes — compiler error if uninitialized |

## Example side-by-side

```c
// C: pointer
void increment(int *x) {
    *x += 1;         // dereference to access
}
int a = 5;
increment(&a);       // pass address explicitly
// a == 6
```

```cpp
// C++: reference
void increment(int &x) {
    x += 1;          // use directly, no dereference syntax
}
int a = 5;
increment(a);        // no & needed at call site
// a == 6
```

Both generate identical assembly — pass an address, write through it. The reference just hides the pointer syntax.

## Mental model

Think of `T&` as a `T* const` (pointer that can't be repointed) that auto-dereferences:

```cpp
// What you write:         // What the compiler effectively does:
int& ref = a;              int* const ref = &a;
ref = 10;                  *ref = 10;
int b = ref;               int b = *ref;
```

In practice, a reference behaves as if it IS the original value — reads and writes go straight through with no explicit dereference:

```cpp
int a = 3;
int& b = a;    // b is a "T* const" that auto-dereferences

b = 7;         // a is now 7    (pointer equivalent: *ptr = 7)
int c = b;     // c is 7        (pointer equivalent: int c = *ptr)
b += 1;        // a is now 8    (pointer equivalent: *ptr += 1)
```

No `*`, no `->`, no explicit address handling. The compiler inserts the dereference for you everywhere `b` appears. It's syntactic sugar over a pointer, with only two compile-time guarantees: must be initialized, can't be reseated. Everything else (lifetime, validity) is on you.

## The three kinds

```cpp
T&         // lvalue reference: alias to an existing object
T&&        // rvalue reference: alias to a temporary (used for move semantics)
const T&   // read-only reference: can also bind to temporaries
```

## Copying and assigning references

You can never operate on a reference itself — it's always transparent. All operations go through to the underlying value:

```cpp
int a = 3;
int b = 7;
int& ref = a;

int c = ref;     // copies the VALUE of a (3) into c. Not the reference.
ref = b;         // sets a = 7. Does NOT reseat ref to point at b.
```

Assigning a reference to another reference just creates another alias to the same object:

```cpp
int a = 3;
int& ref1 = a;      // ref1 -> a
int& ref2 = ref1;   // ref2 -> a (not "reference to reference" — that doesn't exist)

ref2 = 10;           // a is now 10
// ref1 == 10, ref2 == 10, a == 10 — all the same memory
```

There is no "reference to a reference" in C++. The language collapses it — you always end up with a direct reference to the underlying object.

When a **struct contains a reference member** and you copy the struct, the internal pointer is copied (both refer to the same object):

```cpp
struct Holder {
    int& ref;
};

int x = 5;
Holder h1{x};     // h1.ref -> x
Holder h2 = h1;   // h2.ref -> x (same address copied, like copying a pointer)
```

Neither copy cares about the pointed-to value's lifetime. If `x` dies, both `h1.ref` and `h2.ref` dangle.

## Dangling references — NOT safe from lifetime bugs

References prevent null, but do NOT prevent use-after-free:

```cpp
int& make_dangling() {
    int* p = (int*)malloc(sizeof(int));
    *p = 42;
    int& ref = *p;   // ref points to heap memory
    free(p);          // memory freed
    return ref;       // DANGLING — undefined behavior, same as stale pointer
}
```

Same with stack:

```cpp
int& also_dangling() {
    int local = 10;
    return local;     // local dies when function returns — ref is dangling
}
```

**The "can't be null" guarantee is the ONLY extra safety over pointers.** If the underlying memory is freed, moved, or goes out of scope, a reference is just as broken as a stale `T*`.

This is why C++ relies on RAII (tie memory lifetime to object scope) and why Rust invented the borrow checker (compiler-enforced lifetime tracking). C++ trusts the programmer.

## References compared to other languages

C++ references behave more like Java/Python references than C pointers — you interact with the object directly, no pointer syntax:

| Language | What a "variable" is | Explicit dereference? |
|----------|---------------------|----------------------|
| C (`T*`) | An address you manually dereference | Yes (`*p`, `p->x`) |
| C++ (`T&`) | An alias — looks and feels like the value itself | No |
| Java/Python | Everything is a reference (except primitives in Java) | No |

`int& ref = a` in C++ feels like `ref = a` in Python where both names point to the same mutable object. The difference: C++ also has value types (stack-allocated, copied by default), while Java/Python make everything heap-allocated and GC'd. And Python's GC protects you from lifetime issues — C++ does not.
