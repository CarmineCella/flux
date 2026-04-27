# Flux

A small, embeddable scripting language in a single C++17 header. Designed for
hosts where the heavy lifting (DSP, audio I/O, numerical kernels, ML) lives in
C++ and the script is the orchestration glue.

```flux
func make_counter() {
    var n = 0
    return func() { n = n + 1  return n }
}

var c = make_counter()
print c() c() c()                       # → 1 2 3

# NumPy-flavored vec with broadcasting
print sum(sqrt(range(1, 101)))          # → 671.4629…

# First-class audio buffer (interleaved doubles, frames × channels)
var buf = buffer(1024, 2, 48000)
buf[0, 0] = 0.5                         # frame 0, left
print frames(buf) channels(buf)         # → 1024 2

# Self-documenting functions
func gain(x, db) {
    "Apply a gain in dB to a sample or vec."
    return x * pow(10, db / 20)
}
print help(gain)                        # → "Apply a gain in dB to a sample or vec."

# Closures, dicts, try/catch/finally, tail calls — ~2700 lines of C++.
print flux_version                      # → "0.2.0"
```

Flux is a tree-walking interpreter aimed at being **easy to drop into a C++
application** when you need a runtime scripting layer that's a bit more than a
config file: dynamic typing with numeric arrays, dicts, closures, structured
errors, an opaque-handle type for host-owned data, a cooperative-scheduling
hook, and a first-class audio buffer.

It is not designed to compete with V8 or LuaJIT on raw speed. It is designed
to be **boring to embed, hard to crash, and pleasant to read**. The intended
shape of a Flux-using program is: heavy lifting in C++, expressed as a few
dozen `register_builtin(...)` calls; everything else in script.

---

## Contents

- [Design](#design)
- [Build & run](#build--run)
- [Language tour](#language-tour)
  - [Values and types](#values-and-types)
  - [Operators](#operators)
  - [Strings](#strings)
  - [Vec — numeric arrays](#vec--numeric-arrays)
  - [Buffers — first-class audio](#buffers--first-class-audio)
  - [Lists](#lists)
  - [Dicts](#dicts)
  - [Opaque — host-side handles](#opaque--host-side-handles)
  - [Control flow](#control-flow)
  - [Functions and closures](#functions-and-closures)
  - [Docstrings & help()](#docstrings--help)
  - [Errors](#errors)
  - [Modules](#modules)
  - [Introspection](#introspection)
- [Standard library](#standard-library)
- [Embedding in C++](#embedding-in-c)
- [Cooperative scheduling](#cooperative-scheduling)
- [Gotchas](#gotchas)
- [Project layout](#project-layout)
- [Roadmap](#roadmap)

---

## Design

- **Single header.** Drop `flux.h` into a project, `#include` it, you have a
  language. No build system, no dependencies beyond a C++17 standard library.
- **Tree-walking interpreter.** The implementation is straightforward and
  hackable: ~600 lines of parser, ~700 of evaluator, ~800 of standard library.
- **NumPy-flavored numerics via `std::valarray`.** Scalars are 1-element vecs;
  the same operators broadcast across element-wise math.
- **First-class audio buffer.** A `Buffer` value type with `n_frames`,
  `n_channels`, and `sample_rate`. Indexable as `buf[i]` (returns a vec for
  the frame) and `buf[i, c]` (returns a scalar). Reference-shared, so passing
  to a native C++ DSP kernel is a pointer copy.
- **Opaque handles for host data.** A `shared_ptr<void>` plus a type tag.
  Lets host C++ attach FFT plans, ML model weights, file/stream handles, audio
  device handles — anything that can't be expressed as a Flux value — and
  pass them through script unmolested.
- **Lua-flavored composite types.** Lists are mutable, reference-shared.
  Dicts use `{key: value}` syntax, support `.member` and `["key"]` access,
  and iterate keys in sorted order.
- **Real closures + tail-call optimization.** A `return f(args...)` in tail
  position reuses the C++ call frame, so deep tail recursion is unbounded.
- **Structured errors with stack traces, plus `finally`.**
  `try { ... } catch (e) { ... } finally { ... }` — finally runs even when
  the try block `return`s out of the enclosing function.
- **Self-documenting functions.** A string literal as the first statement of
  a function body becomes its docstring. `help(fn)` retrieves it. Native
  builtins can register docstrings too.
- **Typed signatures for natives.** `register_builtin_typed("fft_apply",
  "opaque:fft_plan, buffer", "...", lambda)` parses the signature, wraps the
  lambda in an arity + type checker, and surfaces the signature through
  `help()`. Errors mention function, argument position, expected type,
  and actual type.
- **Cooperative scheduling hook.** A host can install a `yield` callback that
  fires at every loop iteration, block step, and function call — useful for
  cancellation, time-slicing, or progress reporting from C++.
- **Cycle-safe.** `repr()` and `==` on self-referential lists/dicts no longer
  blow the stack. Cyclic data prints `(...)` / `{...}` for the back-edge.
- **Deterministic shutdown.** Closure↔environment cycles are broken at
  interpreter destruction without leaking.

Flux is intentionally minimal: doubles only (no integers), no bitwise ops, no
async, no class system, no module-as-namespace. If you want all of that, you
probably want a bigger language.

---

## Build & run

Flux needs only a C++17 compiler.

```bash
# Compile a host that runs files or starts a REPL
g++ -std=c++17 -O2 flux_main.cpp -o flux

# Run a script
./flux reference.flux

# Or start the REPL
./flux
>> 1 + 2
3
>> var v = range(10)
>> sum(v * v)
285
>> quit
```

**REPL with line editing.** Build against GNU readline to get a proper line
editor — arrow keys for cursor and history, `~/.flux_history` persistence,
Ctrl-R reverse search, Home/End/Ctrl-A/E, etc. The provided `Makefile`
auto-detects readline; for a manual build:

```bash
g++ -std=c++17 -O2 -DFLUX_USE_READLINE -o flux flux_main.cpp -lreadline
```

To opt out (e.g. on a system without readline) pass `make READLINE=0` or
just compile without the `-DFLUX_USE_READLINE` flag — the REPL falls back
to plain `getline()` and still works, just without history or editing.

**Multi-line input** works in either mode. The REPL keeps reading lines
until parens, brackets, braces, strings, and block comments are all
balanced, then evaluates the whole buffer as one unit:

```
>> func square(x) {
..     return x * x
.. }
>> square(7)
49
```

Multi-line entries are recalled from history as a single item with
newlines flattened to spaces — convenient for re-editing a function you
wrote three commands ago.

A minimal `flux_main.cpp`:

```cpp
#include "flux.h"
int main(int argc, char** argv) {
    flux::Interpreter interp;
    try {
        if (argc >= 2) interp.run_file(argv[1]);
        else           interp.repl();
    } catch (std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
```

---

## Language tour

### Values and types

There are nine value kinds: `nil`, `scalar` (a 1-element vec), `vec`, `string`,
`list`, `dict`, `buffer`, `opaque`, `func`. The type tag for any value is
reported by `type(x)`.

```flux
type(nil)            # "nil"
type(42)             # "scalar"        — internally a Vec of size 1
type([1, 2, 3])      # "vec"
type("hi")           # "string"
type(list(1,2))      # "list"
type({a: 1})         # "dict"
type(buffer(64, 2))  # "buffer"
type(print)          # "func"
# type(x) == "opaque" for host-supplied handles
```

Variables are introduced with `var name = expr` and reassigned with `name =
expr` (which walks up the scope chain).

```flux
var x = 1            # declare in current scope
x = x + 1            # reassign — looks up the chain, doesn't shadow
```

Constants: `pi`, `e`, `inf`, `nil`, `true` (= 1), `false` (= 0), `flux_version`.

### Operators

```
arithmetic    +  -  *  /  %                  (numeric)
comparison    == != < > <= >=                (== / != are structural)
logical       and  or  not                   (short-circuit and/or)
unary         -  not                         (numeric / boolean)
indexing      x[i]      x[i, j]              (i,j only on buffer)
member        x.k                            (dict)
call          f(args...)
```

`==` and `!=` are **structural and recursive** — they walk into lists and
dicts and compare element-by-element. The recursion is cycle-safe
(co-inductive). Across types they always disagree, so `1 != "1"` and
`nil != 0`.

```flux
list(1, 2, 3) == list(1, 2, 3)        # 1
{a: 1, b: 2} == {b: 2, a: 1}          # 1 (order doesn't matter for dicts)
1 == "1"                              # 0 (cross-type)
```

For `vec == vec`, the result is element-wise (NumPy-style):

```flux
[1, 2, 3] == [1, 2, 4]                # [1, 1, 0]
```

A multi-element vec is **truthy iff every element is non-zero**. So
`if (v == w) { ... }` reads as "all elements equal":

```flux
if ([1, 2, 3] == [1, 2, 3]) { print "all match" }     # prints
if ([1, 2, 3] == [1, 2, 4]) { print "all match" }     # doesn't
```

### Strings

Strings are byte sequences. Indexing is byte-based; iteration with `for c in s`
yields one-byte strings (UTF-8 multibyte chars split). Standard escapes:
`\n \t \r \0 \\ \"`. Block comments `/* ... */` and line comments `# ...` are
both supported.

```flux
upper("hello")              # "HELLO"
trim("  hi  ")              # "hi"
substr("hello world", 6, 5) # "world"
find("hello", "lo")         # 3
replace("a-b-c", "-", "_")  # "a_b_c"
split("a,b,c", ",")         # list("a", "b", "c")
join(list("a","b"), "-")    # "a-b"
concat("foo", "bar")        # "foobar"     — there is no `+` for strings
"hello"[1]                  # "e"
"hello"[-1]                 # "o"
char(65)                    # "A"
asc("A")                    # 65
format("hi {}", "Ada")      # "hi Ada"     — `{}` is the placeholder
```

### Vec — numeric arrays

Vecs are written with `[…]`. They support broadcast arithmetic, the standard
math functions, and reductions. Vecs are **value-typed**: `var b = a` makes a
copy.

```flux
var v = [1, 2, 3, 4]

v + 10              # [11, 12, 13, 14]    — scalar broadcast
v * v               # [1, 4, 9, 16]       — element-wise
sum(v)              # 10
mean(v)             # 2.5
range(0, 10, 2)     # [0, 2, 4, 6, 8]
zeros(3)            # [0, 0, 0]
ones(3)             # [1, 1, 1]
sqrt([1, 4, 9])     # [1, 2, 3]
pow([1, 2, 3], 2)   # [1, 4, 9]
sort([3, 1, 2])     # [1, 2, 3]
sin([0, pi/2, pi])  # [0, 1, ~0]
```

Numeric scalars are a Vec of size 1 — that's how `2 + [1, 2, 3]` broadcasts.

### Buffers — first-class audio

A `Buffer` holds an interleaved array of doubles plus `n_frames`,
`n_channels`, and `sample_rate`. It's the bridge between Flux scripts and
host C++ DSP code: hosts allocate buffers, fill them with native I/O
functions, and pass them back and forth as values. Buffers are
**reference-shared** (like list/dict); use `copy(b)` to detach.

```flux
var mono = buffer(1024)              # 1 channel, 44100 Hz default
var st   = buffer(1024, 2, 48000)    # stereo @ 48 kHz

frames(st)                           # 1024
channels(st)                         # 2
sample_rate(st)                      # 48000
len(st)                              # 1024  (== frames)

# Indexing
mono[0] = 0.5                        # mono: scalar in / out
mono[-1]                             # negative wraps from end
st[0, 0] = 0.1                       # frame 0, left
st[0, 1] = 0.2                       # frame 0, right
st[5]                                # vec [L, R] for frame 5
st[3] = [0.3, -0.3]                  # whole-frame assignment via vec

# Iteration: scalar per frame for mono, vec per frame for multichannel
var energy = 0
for (var fr in st) { energy = energy + sum(fr * fr) }

# Conversions
var v   = buffer_to_vec(mono)        # interleaved samples → Vec
var b   = vec_to_buffer([1,2,3], 22050)  # Vec → mono buffer at 22.05 kHz
```

Multi-dimensional indexing `buf[i, j]` is a Buffer-only feature; using it on
a vec/list/dict is an error.

### Lists

Lists hold any mix of values. Built with `list(...)`. **Reference-shared**:
`var b = a` shares storage. Use `copy(a)` to detach.

```flux
var l = list(1, "two", [3, 4], {x: 5})

push(l, 99)             # mutates: appends
pop(l)                  # mutates: removes & returns last
insert(l, 0, "head")    # mutates: insert at index
remove(l, 2)            # mutates: remove at index, return removed
l[0] = "FIRST"          # index assignment

len(l)
reverse(l)
slice(l, 1, 3)
concat(list(1,2), list(3,4))
```

### Dicts

String-keyed maps. Literal syntax: `{key: value, key2: value2}`. Bare
identifiers as keys are auto-stringified; quoted strings allow arbitrary keys.
Member access uses `.k` for identifier keys, `["any string"]` for the rest.
**Reference-shared**, like lists.

```flux
var d = {name: "Ada", age: 7, "with space": 42}

d.name                  # "Ada"
d["age"]                # 7
d.missing               # nil   (missing keys read as nil)
d.age = 8               # member assignment
d["new"] = 99

has(d, "name")          # 1
get(d, "x", "default")  # "default"   — default if key missing
keys(d)                 # sorted list of keys
values(d)               # values in key-sorted order
remove(d, "age")        # removes and returns the value
len(d)

# Iteration yields keys in sorted order (deterministic):
for (var k in d) { print k d[k] }

# Build from pairs:
dict(list(list("a", 1), list("b", 2)))     # {a: 1, b: 2}

# Concat merges (right wins):
concat({a: 1, b: 2}, {b: 99})              # {a: 1, b: 99}
```

Nested writes work as long as the path exists:

```flux
var cfg = {db: {host: "localhost", port: 5432}}
cfg.db.port = 9999      # ok — db exists
cfg.x.y = 1             # error: no such member 'x' (no auto-vivification)
```

**Convention for native functions with many optional arguments:** pass a
trailing options dict.

```flux
# in a host-supplied native:
stft(signal, {win: 1024, hop: 256, type: "hann", normalize: 1})
```

This avoids needing keyword arguments at the language level. Apply it
consistently across a library and the API stays predictable.

### Opaque — host-side handles

Opaque values wrap a `shared_ptr<void>` plus a type tag. They originate in
C++ host code (FFT plans, ML weights, file/stream handles, audio devices) and
are passed around Flux unchanged. From script you can ask the type tag, store
them in dicts, and hand them back to native functions; you can't introspect
their contents.

```flux
# (No Flux-side constructor — host C++ creates them.)
opaque_type(handle)     # → "fft_plan"   (whatever tag the host set)
type(handle)            # → "opaque"
```

The C++ side:

```cpp
auto plan = std::make_shared<MyFFTPlan>(1024);
interp.global->def("plan",
    flux::Value(flux::Opaque{"fft_plan", plan}));
```

### Control flow

```flux
if (cond) { ... } else if (cond) { ... } else { ... }

while (cond) { ... }

for (var i = 0; i < 10; i = i + 1) { ... }

for (var x in iterable) { ... }      # list, vec, string, dict (keys), buffer (frames)

break          # leaves the innermost loop
continue       # next iteration
```

`break` and `continue` are loop-only; using them outside a loop is an error.
A closure called from inside a loop cannot `break` the outer loop — that
becomes an error too.

### Functions and closures

```flux
# Statement-form: requires a name.
func square(x) { return x * x }

# Expression-form: anonymous only.
var f = func(x) { return x * 2 }

# Closures capture lexical scope by reference.
func make_adder(n) { return func(x) { return x + n } }
var add3 = make_adder(3)
add3(5)            # 8
```

`return expr` returns; bare `return` returns `nil`; running off the end also
returns `nil`. Arity is enforced (no varargs in user code).

**Tail-call optimization.** A `return f(args...)` in tail position is converted
into a frame-reuse jump, so any tail call (self or otherwise) runs in O(1) C++
stack. The default non-tail recursion limit is 1000.

```flux
func sum_to(n, acc) {
    if (n == 0) { return acc }
    return sum_to(n - 1, acc + n)        # tail call — unbounded depth
}
sum_to(1000000, 0)
```

Mutual recursion via tail calls also works:

```flux
func is_even(k) { if (k == 0) { return 1 } return is_odd(k - 1) }
func is_odd(k)  { if (k == 0) { return 0 } return is_even(k - 1) }
is_even(100000)        # ok, no stack overflow
```

### Docstrings & help()

A string literal as the first statement of a function body is captured as
that function's docstring. `help(fn)` retrieves it. The string still
evaluates as a no-op statement, so line numbers and AST shape are unaffected.

```flux
func gain(x, db) {
    "Apply a gain in dB to a sample or vec."
    return x * pow(10, db / 20)
}

help(gain)               # "Apply a gain in dB to a sample or vec."
help("gain")             # same — lookup by name
help("buffer")           # native builtins register their own docs
```

`help` accepts a closure or a name string; for native functions, use the
name string (closures wrapping natives lose the underlying name).

### Errors

`error(msg)` raises. `try { ... } catch (e) { ... } finally { ... }` handles
control flow:

- `try { ... } catch (e) { ... }` — catch errors only
- `try { ... } finally { ... }` — cleanup only, errors propagate
- `try { ... } catch (e) { ... } finally { ... }` — both

`finally` runs in **all** exit paths: normal completion, caught errors,
propagated errors, and even when the try block `return`s out of an enclosing
function. Catches do not capture `return`/`break`/`continue` — those are
control-flow primitives, not errors — but `finally` still runs around them.

`e` is bound to a dict:

```flux
{
    message: "something failed",
    file:    "/path/to/script.flux",
    line:    42,
    trace:   list("foo() at script.flux:30",
                  "bar() at script.flux:25", ...)
}
```

```flux
try {
    error("boom")
} catch (e) {
    print "caught:" e.message "at line" e.line
} finally {
    print "always runs"
}
```

Resource cleanup pattern (RAII you can write yourself):

```flux
func process(path) {
    var f = open_file(path)              # host-supplied native returning opaque
    try {
        return analyze(f)
    } finally {
        close_file(f)                    # runs whether analyze returns or throws
    }
}
```

`assert(expr [, msg])` raises an error if `expr` is falsy. The diagnostic
includes a textual rendering of the asserted expression:

```flux
assert(2 + 2 == 5)     # → "assertion failed: 2 + 2 == 5"
assert(0, "uh oh")     # → "assertion failed: 0 — uh oh"
```

### Modules

`load("path.flux")` runs another file in the current scope. Loads are
**memoized by canonical path** — loading the same file twice is a no-op,
which also breaks circular loads.

Search order:
1. relative to the source file doing the `load`
2. each entry in `$FLUX_PATH` (`:` on Unix, `;` on Windows)
3. `~/.flux/` (or `%USERPROFILE%/.flux/` on Windows)

### Introspection

```flux
vars()          # sorted list of names visible in the current scope
bindings()      # dict {name: value, ...} of visible bindings
help(fn|name)   # retrieve a function's docstring
eval(src)       # parse + execute src in the current scope; returns last value
bench(thunk)    # call thunk() and return wall-clock seconds
flux_version    # string constant — "0.2.0"
```

`eval` runs in the **caller's** scope: it can read and write outer variables.

```flux
var dynamic = 0
eval("dynamic = 7 * 6")
print dynamic             # 42
```

`bench` is the easy way to time an algorithm during development:

```flux
var t = bench(func() { sum(sqrt(range(1, 100000))) })
print "took" t "s"
```

---

## Standard library

| Category   | Functions                                                       |
|------------|------------------------------------------------------------------|
| Polymorphic | `len`, `reverse`, `slice`, `concat`, `copy`                     |
| Reductions | `sum`, `mean`, `min`, `max`                                      |
| Element-wise math | `sqrt`, `abs`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `exp`, `log`, `floor`, `ceil`, `round`, `pow`, `sort` |
| Vec constructors | `range`, `zeros`, `ones`, `vec`, `rand`, `seed`           |
| Buffer     | `buffer`, `frames`, `channels`, `sample_rate`, `buffer_to_vec`, `vec_to_buffer` |
| Opaque     | `opaque_type`                                                    |
| List       | `list`, `push`, `pop`, `insert`, `remove`                        |
| Dict       | `dict`, `keys`, `values`, `has`, `get`, `remove`                 |
| Higher-order | `map`, `filter`, `reduce`, `each`, `apply`                     |
| String     | `upper`, `lower`, `trim`, `split`, `join`, `substr`, `find`, `replace`, `format`, `out`, `char`, `asc` |
| Type / cast | `type`, `str`, `num`, `vec`                                     |
| Regex      | `match`                                                          |
| I/O        | `read`, `write`, `append`, `input`                               |
| System     | `clock`, `sleep`, `env`, `exec`, `exit`                          |
| Errors     | `error`, `assert`                                                |
| Random     | `rand`, `seed`, `shuffle`                                        |
| Introspection | `type`, `vars`, `bindings`, `help`, `eval`, `bench`           |
| Output     | `print` (newline + space-separated), `out` (no separator/newline), `format` (`{}` placeholders) |

See `reference.flux` for one example of every feature with expected output.

---

## Embedding in C++

The interpreter is a single class. The minimum to get going:

```cpp
#include "flux.h"

flux::Interpreter interp;
interp.run_file("script.flux");
```

To register a host-side function with a typed signature (recommended — the
wrapper checks arity + per-arg type, surfaces good error messages, and feeds
the signature into `help()`):

```cpp
interp.register_builtin_typed("greet",
    "string",                                // signature
    "Say hello to someone.",                 // doc
    [](const std::vector<flux::Value>& a, int, const std::string&) {
        return flux::Value(flux::Str("hello " + a[0].as_str()));
    });
```

Now `greet("world")` runs from script and `help("greet")` returns
`greet(string) — Say hello to someone.` Calling `greet(42)` from script
raises `greet: arg 1 expected string, got scalar`.

Signature mini-language:

```
type names:    any  nil  scalar  int  number  vec  string  list
               dict  buffer  opaque  opaque:tag  func
optional:      trailing '?'  (must form a contiguous tail)
```

Examples: `"buffer, int, int"`, `"buffer, int, int?"`,
`"opaque:fft_plan, buffer"`, `""` for zero-arg natives.

If you want full manual control (no auto-checking), the untyped form still
works:

```cpp
interp.register_builtin("greet",
    [](const std::vector<flux::Value>& args, int line, const std::string& file) {
        if (args.size() != 1 || !args[0].is_str())
            flux::err(file, line, "greet: expects one string");
        return flux::Value(flux::Str("hello " + args[0].as_str()));
    });
```

(There's also a three-argument form, `register_builtin(name, doc, fn)`, that
attaches a docstring without type-checking.)

To pass values back and forth:

```cpp
auto& global = *interp.global;
global.def("config", flux::Value(flux::Str("/etc/myapp.conf")));

flux::Value result = interp.eval(/* parsed expr */, interp.global);
if (result.is_vec()) {
    for (double x : result.as_vec()) { /* ... */ }
}
```

To return an audio buffer from a native (e.g. `read_wav`):

```cpp
interp.register_builtin("sine_wave",
    "sine_wave(hz, secs, sr) — generate a mono sine.",
    [](const std::vector<flux::Value>& a, int ln, const std::string& f) {
        double hz = a[0].scalar();
        double secs = a[1].scalar();
        double sr = a[2].scalar();
        auto buf = std::make_shared<flux::Buffer>();
        buf->n_frames = (size_t)(secs * sr);
        buf->n_channels = 1;
        buf->sample_rate = sr;
        buf->data.resize(buf->n_frames);
        for (size_t i = 0; i < buf->n_frames; ++i)
            buf->data[i] = std::sin(2 * M_PI * hz * i / sr);
        return flux::Value(buf);
    });
```

To attach an opaque host handle (FFT plan, model, file handle):

```cpp
struct MyFFTPlan { /* ... */ };

interp.register_builtin_typed("make_fft_plan",
    "int",
    "Allocate a reusable FFT plan of the given size.",
    [](const std::vector<flux::Value>& a, int, const std::string&) {
        auto plan = std::make_shared<MyFFTPlan>((int)a[0].scalar());
        return flux::Value(flux::Opaque{"fft_plan", plan});
    });

interp.register_builtin_typed("fft_apply",
    "opaque:fft_plan, buffer",          // tag-checked: rejects other opaques
    "Run the FFT in-place on buf.",
    [](const std::vector<flux::Value>& a, int, const std::string&) {
        auto plan = std::static_pointer_cast<MyFFTPlan>(a[0].as_opaque().ptr);
        // ... use plan and a[1].as_buffer() ...
        return flux::Value(nullptr);
    });
```

The `opaque:fft_plan` tag in the signature means `fft_apply` is statically
guaranteed (at the boundary) to receive an opaque whose `type_tag` is
`"fft_plan"`. Pass any other opaque and the error message tells you exactly
what you got: `fft_apply: arg 1 expected opaque:fft_plan, got opaque:blob`.

Error handling: every error from Flux code throws `flux::Error`, which is a
`std::exception` subclass exposing `.file`, `.line`, `.msg`, and `.trace`.

---

## Cooperative scheduling

A host can install a `yield` callback that fires at every loop iteration, every
block statement, and every function call. This is how to implement
cancellation, time-slicing, or progress reporting without changing the
language:

```cpp
std::atomic<bool> stop_requested{false};

interp.set_yield([&]{
    if (stop_requested.load()) {
        flux::err("<host>", 0, "interrupted");
    }
});

// On another thread:
//   stop_requested = true;
// — the running script halts cleanly with a normal Error at the next yield.
```

**Threading model.** Flux is intended for **offline use or a control thread**,
not the audio thread. The shared-pointer-based allocator and exception-based
control flow are not realtime-safe. The recommended host architecture is:

- C++ audio thread: lock-free, no Flux calls
- C++ / Flux control thread: parameter changes, analysis, scheduling
- Communication via lock-free queues or atomics

Typical use cases: running runaway scripts under a watchdog, integrating
script-driven analysis with a UI, gating execution on a frame budget, hooking
up a progress bar.

---

## Gotchas

A short list of behaviors worth knowing:

- **Strings are byte sequences.** `len("é")` is 2 (UTF-8). Iteration splits
  multibyte characters. There is no built-in codepoint API.

- **Lists, dicts, and buffers are reference-typed; vecs and strings are
  value-typed.** `var b = a` shares storage for lists, dicts, and buffers,
  but copies for vecs and strings. Use `copy(a)` to detach in either case.

- **Vec assignment is by value, but `v[i] = x` mutates in place.** Two
  separate operations.

- **Multi-element vec truthiness is "all non-zero".** This makes
  `if (v == w) { ... }` work intuitively, but it means `if ([1, 0, 1])` is
  false. Use `if (sum(v) > 0)` for "any non-zero" semantics.

- **No string `+`.** Use `concat(a, b)` or `format("{}{}", a, b)`. (This is
  intentional: the binary `+` path stays on the hot vec arithmetic case
  without an extra type dispatch.)

- **Numbers are doubles.** No integer type, no bitwise ops. Indexing
  truncates to int.

- **Multi-index `x[i, j]` is buffer-only.** Using it on vecs or lists
  errors out.

- **Cyclic data does not free.** Self-referential lists/dicts (`push(l, l)`,
  `d.self = d`) are well-supported by the language — `repr` and `==` are
  cycle-safe — but the underlying `shared_ptr` storage forms a reference
  cycle that won't be reclaimed without a garbage collector. Don't build
  cyclic graphs in long-running processes.

- **`load` is memoized.** Re-loading the same file is a no-op. To genuinely
  re-run a file, use `eval(read("path.flux"))`.

- **`eval` runs in the caller's scope** — it can both read and modify outer
  variables. This is sometimes what you want and sometimes a footgun.

- **Tree-walker, so it's slow.** Roughly an order of magnitude slower than
  Lua. The intent is that hot inner loops live in C++ kernels you call from
  Flux, not in Flux itself. Use `bench(thunk)` to measure.

- **Stack depth limit is 1000** for non-tail recursion. Tail calls are
  unbounded. Adjust `interp.max_stack` from C++ if needed.

---

## Project layout

```
flux.h            single-header interpreter (~2700 lines, v0.2.0)
flux_main.cpp     minimal host: runs a file or starts the REPL
Makefile          auto-detects readline; `make` builds, `make test` runs tests
reference.flux    annotated tour of every feature with prints
test_core.flux    355 assertions covering operators, builtins, control flow,
                  closures, dicts, buffers, try/finally, cycles, docstrings —
                  runnable as a regression suite
README.md         this file
```

To verify everything works:

```bash
g++ -std=c++17 -O2 flux_main.cpp -o flux
./flux test_core.flux        # → "Total: 355 passed, 0 failed"
./flux reference.flux        # → guided walkthrough with output
```

---

## Roadmap

Items already in 0.2 are listed under [Design](#design). Items being
considered for future revisions, in rough order of priority:

**Language ergonomics:**
- Symbol or interned-string optimization for hot dict keys / mode strings
  (`mode: "hann"`, `kernel: "rbf"`).
- A few more unary/binary methods on Vec for DSP work
  (`fft`, `ifft`, convolution) — currently expected via host-supplied natives.

**Tooling (separate from `flux.h`):**
- `--check` mode: parse-only validation for CI and editor integration.
- AST cache: serialize parsed modules to disk for faster re-loads.
- Tab-completion in the REPL via readline's `rl_attempted_completion_function`.
- Structured logging hook so a host can capture diagnostic output.
- libFuzzer target on the lexer / parser / evaluator.

**Project hygiene:**
- CHANGELOG.md tracking every release.
- CI matrix: `-O0 / -O2 / -O3` × `asan,ubsan` × clang/gcc.
- Contributor / extension guide showing exactly how to add a builtin and a
  new value type.

**Explicitly not planned:**
- Realtime-safe audio thread support. (See
  [Cooperative scheduling § Threading model](#cooperative-scheduling).)
- Integer types or bitwise operations. (Doubles only is a deliberate
  simplification.)
- Module-as-namespace or class system. (Use dicts.)
