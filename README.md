# Flux

A small, embeddable scripting language in a single C++17 header.

```flux
func make_counter() {
    var n = 0
    return func() { n = n + 1  return n }
}

var c = make_counter()
print c() c() c()           # → 1 2 3

# NumPy-ish vec with broadcasting
print sum(sqrt(range(1, 101)))     # → 671.4629…

# Dicts, closures, try/catch, tail calls — all in ~2300 lines of C++.
```

Flux is a tree-walking interpreter aimed at being **easy to drop into a C++
application** when you need a runtime scripting layer that's a bit more than a
config file: dynamic typing with numeric arrays, dicts, closures, structured
errors, and a cooperative-scheduling hook so a host program can interrupt or
time-slice user code without changing the language.

It is not designed to compete with V8 or LuaJIT on raw speed. It is designed to
be **boring to embed, hard to crash, and pleasant to read**.

---

## Contents

- [Design](#design)
- [Build & run](#build--run)
- [Language tour](#language-tour)
  - [Values and types](#values-and-types)
  - [Operators](#operators)
  - [Strings](#strings)
  - [Vec — numeric arrays](#vec--numeric-arrays)
  - [Lists](#lists)
  - [Dicts](#dicts)
  - [Control flow](#control-flow)
  - [Functions and closures](#functions-and-closures)
  - [Errors](#errors)
  - [Modules](#modules)
  - [Introspection](#introspection)
- [Standard library](#standard-library)
- [Embedding in C++](#embedding-in-c)
- [Cooperative scheduling](#cooperative-scheduling)
- [Gotchas](#gotchas)
- [Project layout](#project-layout)

---

## Design

- **Single header.** Drop `flux.h` into a project, `#include` it, you have a
  language. No build system, no dependencies beyond a C++17 standard library.
- **Tree-walking interpreter.** The implementation is straightforward and
  hackable: ~600 lines of parser, ~600 of evaluator, ~700 of standard library.
- **NumPy-flavored numerics via `std::valarray`.** Scalars are 1-element vecs;
  the same operators broadcast across element-wise math.
- **Lua-flavored composite types.** Lists are mutable, reference-shared. Dicts
  use `{key: value}` syntax, support `.member` and `["key"]` access, and
  iterate keys in sorted order.
- **Real closures + tail-call optimization.** A `return f(args...)` in tail
  position reuses the C++ call frame, so deep tail recursion is unbounded.
- **Structured errors with stack traces.** `try { ... } catch (e) { ... }`
  binds `e` as a dict containing `message`, `file`, `line`, and `trace`.
- **Cooperative scheduling hook.** A host can install a `yield` callback that
  fires at every loop iteration, block step, and function call — useful for
  cancellation, time-slicing, or progress reporting from C++.
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

There are seven value kinds: `nil`, `scalar` (a 1-element vec), `vec`, `string`,
`list`, `dict`, `func`. The type tag for any value is reported by `type(x)`.

```flux
type(nil)        # "nil"
type(42)         # "scalar"        — internally a Vec of size 1
type([1, 2, 3])  # "vec"
type("hi")       # "string"
type(list(1,2))  # "list"
type({a: 1})     # "dict"
type(print)      # "func"
```

Variables are introduced with `var name = expr` and reassigned with `name =
expr` (which walks up the scope chain).

```flux
var x = 1            # declare in current scope
x = x + 1            # reassign — looks up the chain, doesn't shadow
```

Constants: `pi`, `e`, `inf`, `nil`, `true` (= 1), `false` (= 0).

### Operators

```
arithmetic    +  -  *  /  %                  (numeric)
comparison    == != < > <= >=                (== / != are structural)
logical       and  or  not                   (short-circuit and/or)
unary         -  not                         (numeric / boolean)
indexing      x[i]                           (vec, list, string, dict)
member        x.k                            (dict)
call          f(args...)
```

`==` and `!=` are **structural and recursive** — they walk into lists and
dicts and compare element-by-element. Across types they always disagree, so
`1 != "1"` and `nil != 0`.

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

### Control flow

```flux
if (cond) { ... } else if (cond) { ... } else { ... }

while (cond) { ... }

for (var i = 0; i < 10; i = i + 1) { ... }

for (var x in iterable) { ... }      # list, vec, string, dict (keys)

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

### Errors

`error(msg)` raises. `try { ... } catch (e) { ... }` catches user errors
(not `return`/`break`/`continue` — those remain control-flow primitives).
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
vars()       # sorted list of names visible in the current scope
bindings()   # dict {name: value, ...} of visible bindings
eval(src)    # parse + execute src in the current scope; returns last value
```

`eval` runs in the **caller's** scope: it can read and write outer variables.

```flux
var dynamic = 0
eval("dynamic = 7 * 6")
print dynamic             # 42
```

---

## Standard library

| Category   | Functions                                                       |
|------------|------------------------------------------------------------------|
| Polymorphic | `len`, `reverse`, `slice`, `concat`, `copy`                     |
| Reductions | `sum`, `mean`, `min`, `max`                                      |
| Element-wise math | `sqrt`, `abs`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `exp`, `log`, `floor`, `ceil`, `round`, `pow`, `sort` |
| Vec constructors | `range`, `zeros`, `ones`, `vec`, `rand`, `seed`           |
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
| Introspection | `type`, `vars`, `bindings`, `eval`                            |
| Output     | `print` (newline + space-separated), `out` (no separator/newline), `format` (`{}` placeholders) |

See `reference.flux` for one example of every function with expected output.

---

## Embedding in C++

The interpreter is a single class. The minimum to get going:

```cpp
#include "flux.h"

flux::Interpreter interp;
interp.run_file("script.flux");
```

To register a host-side function:

```cpp
interp.register_builtin("greet",
    [](const std::vector<flux::Value>& args, int line, const std::string& file) {
        if (args.size() != 1 || !args[0].is_str())
            flux::err(file, line, "greet: expects one string");
        return flux::Value(flux::Str("hello " + args[0].as_str()));
    });
```

Now `greet("world")` works from Flux code.

To pass values back and forth:

```cpp
auto& global = *interp.global;
global.def("config", flux::Value(flux::Str("/etc/myapp.conf")));

flux::Value result = interp.eval(/* parsed expr */, interp.global);
if (result.is_vec()) {
    for (double x : result.as_vec()) { /* ... */ }
}
```

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

Use cases: stopping runaway scripts, integrating with a real-time audio
callback, gating script execution on a frame budget, or hooking up a progress
bar.

---

## Gotchas

A short list of behaviors worth knowing:

- **Strings are byte sequences.** `len("é")` is 2 (UTF-8). Iteration splits
  multibyte characters. There is no built-in codepoint API.

- **Lists and dicts are reference-typed; vecs and strings are value-typed.**
  `var b = a` shares storage for lists and dicts but copies for vecs and
  strings. Use `copy(a)` to detach.

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

- **`load` is memoized.** Re-loading the same file is a no-op. To genuinely
  re-run a file, use `eval(read("path.flux"))`.

- **`eval` runs in the caller's scope** — it can both read and modify outer
  variables. This is sometimes what you want and sometimes a footgun.

- **Tree-walker, so it's slow.** Roughly an order of magnitude slower than
  Lua. Use a faster language if performance matters; embed Flux if simplicity
  matters more.

- **Stack depth limit is 1000** for non-tail recursion. Tail calls are
  unbounded. Adjust `interp.max_stack` from C++ if needed.

---

## Project layout

```
flux.h            single-header interpreter (~2300 lines)
flux_main.cpp     minimal host: runs a file or starts the REPL
reference.flux    annotated tour of every feature with prints
test_core.flux    301 assertions covering operators, builtins, control flow,
                  closures, dicts, errors — runnable as a regression suite
README.md         this file
```

To verify everything works:

```bash
g++ -std=c++17 -O2 flux_main.cpp -o flux
./flux test_core.flux        # → "Total: 301 passed, 0 failed"
./flux reference.flux        # → guided walkthrough with output
```
