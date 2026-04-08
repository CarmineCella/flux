# Flux

A small, embeddable scripting language for numerical computation and algorithmic work.
Flux fits in a single C++17 header (~715 lines) with no external dependencies, compiles
in under a second, and runs either as a REPL or as a file interpreter.

## Quick start

```bash
make            # compile
make test       # run 222 tests
make install    # install binary + stdlib to ~/.flux
./flux          # open REPL
./flux script.flux
```

## Command line

```
flux [options] [files...]
```

| Option | Description |
|---|---|
| `--i`, `-i` | Enter the REPL after running all files, keeping the environment intact. Useful for loading libraries and then exploring interactively — the same pattern as `python -i`. |
| `--stack n`, `-s n` | Set the maximum eval recursion depth (default: 1000). A depth of 1000 supports roughly 200 nested function calls. |
| `--args ...` | Everything after `--args` is collected into `__argv`, a list of strings accessible from Flux code. |
| `--help`, `-h` | Print usage information. |

If no files are given, Flux starts the REPL directly. If files are given without `--i`,
they are executed and Flux exits.

### Examples

```bash
# Run a script
flux compute.flux

# Run a script then drop into REPL with all definitions available
flux --i mylib.flux

# Pass arguments to a script
flux process.flux --args input.csv output.csv --verbose

# Limit recursion depth for untrusted code
flux --stack 100 untrusted.flux
```

Accessing command line arguments from Flux:

```python
# __argv is always defined (empty list if no --args)
if (len(__argv) > 0) {
    print "first arg:" __argv[0]
}
```

## The REPL

The Flux REPL supports multi-line input. When you type an incomplete expression —
an unclosed `{`, `(`, `[`, or string — the prompt changes from `>>` to `..` and
keeps reading until everything is balanced.

```
>> func fib (n) {
..     if (n <= 1) { return n }
..     return fib(n - 1) + fib(n - 2)
.. }
>> print fib(10)
55
```

Type `quit` or `exit` to leave the REPL.

## Design

Flux has four value types, all first-class:

- **Scalars and vectors** — backed by `std::valarray<double>`. A scalar is simply a
  vector of size 1. All arithmetic operators (`+ - * / %`) and math functions
  (`sin`, `sqrt`, `exp`, ...) broadcast element-wise, so `[1,2,3] * 10` and
  `sqrt([4,9,16])` work without loops.

- **Strings** — immutable, with indexing, slicing, regex matching, and the usual
  manipulation functions (`split`, `join`, `find`, `replace`, `upper`, `lower`, `trim`).
  String concatenation uses `concat()`, keeping the `+` operator strictly numeric.

- **Lists** — heterogeneous ordered collections, created with `list(...)`. Lists
  are immutable by convention: `push()` returns a new list. Higher-order functions
  (`map`, `filter`, `reduce`, `each`) operate on lists.

- **Functions** — first-class closures with lexical scoping. Functions can be
  anonymous, passed as arguments, returned from other functions, and stored in
  variables. The environment chain uses `std::shared_ptr` for proper static scoping.

Internally, all values are held in a `std::variant<Vec, Str, List, Closure, NativeFn, nullptr_t>`.

## Syntax overview

```python
# comments start with #
var x = 42
var v = [1, 2, 3, 4, 5]
var s = "hello"
var l = list(1, "two", [3])

func add (a, b) {
    return a + b
}

# anonymous functions
var sq = func (x) { return x * x }

# print is a special form — no parentheses, variadic
print "sum:" add(x, 10) "vec:" v

if (x > 10) {
    print "big"
} else {
    print "small"
}

while (x > 0) {
    x = x - 1
}

for (var i = 0; i < 10; i = i + 1) {
    print i
}
```

## Closures and higher-order functions

```python
func make_counter () {
    var n = 0
    func tick () {
        n = n + 1
        return n
    }
    return tick
}

var c = make_counter()
print c() c() c()   # 1 2 3

var nums = list(1, 2, 3, 4, 5)
print map(nums, func (x) { return x * x })        # (1, 4, 9, 16, 25)
print filter(nums, func (x) { return x > 2 })     # (3, 4, 5)
print reduce(nums, func (a, b) { return a + b }, 0) # 15
```

## Vector operations

```python
var a = [1, 2, 3]
var b = [10, 20, 30]

print a + b           # [11, 22, 33]
print a * 10          # [10, 20, 30]
print sqrt([4, 9, 16]) # [2, 3, 4]
print sum(a)          # 6
print mean(a)         # 2

var r = range(0, 1, 0.1)   # [0, 0.1, 0.2, ..., 0.9]
var noise = rand(1000)
print "std dev:" sqrt(mean((noise - mean(noise)) * (noise - mean(noise))))
```

## Polymorphic builtins

These work on vectors, strings, and lists:

| Function | Description |
|---|---|
| `len(x)` | Length / size |
| `reverse(x)` | Reverse order |
| `slice(x, start, stop)` | Sub-range (negative indices supported) |
| `concat(a, b)` | Join two values of the same type |

## String operations

```python
print upper("hello")                      # HELLO
print split("a,b,c", ",")                 # (a, b, c)
print join(list("x","y","z"), "-")        # x-y-z
print replace("foo bar foo", "foo", "baz") # baz bar baz
print match("age: 42", "([0-9]+)")        # (42, 42)
```

## File I/O and path resolution

All file paths are resolved relative to the calling script's directory using
`std::filesystem`, so `read("../data/input.csv")` works correctly regardless of
the working directory.

```python
write("output.txt", "hello")
append("output.txt", " world")
var content = read("output.txt")
```

## Module system

`load("module.flux")` executes another file in the current environment. The search
order is:

1. Relative to the calling script's directory
2. Each directory listed in the `FLUX_PATH` environment variable (`:` separated, `;` on Windows)
3. `~/.flux/` as a fallback

After `make install`, `load("stdlib.flux")` works from any script.

## Introspection and metaprogramming

```python
# type introspection
print type(42)         # scalar
print type([1,2])      # vec
print type("hello")    # string
print type(list())     # list
print type(nil)        # nil
print type(sum)        # func

# vars() — all names visible in the current scope
var names = vars()
print len(names) "symbols visible"

# eval — execute code from strings at runtime
print eval("2 + 3 * 4")           # 14
eval("var x = 42")                 # defines x in current scope
eval("func double(n) { return n * 2 }")

# code generation
var op = "*"
var code = concat(concat("5 ", op), " 10")
print eval(code)                   # 50

# apply — call a function with arguments from a list
func f(a, b, c) { return a + b + c }
print apply(f, list(1, 2, 3))     # 6
```

## Error handling and stack traces

Errors report the source file and line number. When an error occurs inside
nested function calls, Flux prints a full call stack trace:

```
error: lib.flux:2: undefined: bad_var
  in inner(), called from main.flux:6
  in middle(), called from main.flux:10
  in outer(), called from main.flux:13
```

Anonymous functions appear as `<anonymous>()`. Cross-file calls show the correct
source file at each frame.

The `--stack` flag protects against infinite recursion:

```
error: script.flux:1: stack overflow (depth 50)
  in boom(), called from script.flux:1
  in boom(), called from script.flux:2
```

## System interaction

```python
var output = exec("ls -la")    # run shell command, capture stdout
var home = env("HOME")         # read environment variable
var t = clock()                # high-resolution timer (seconds)
assert(1 == 1, "sanity check") # abort with message if false
error("stop here")             # raise an error with file:line
exit(0)                        # terminate
```

## Standard library

The file `stdlib.flux` (installed to `~/.flux/`) provides additional functions
written in Flux itself:

**List**: `head`, `last`, `tail`, `pop`, `slice`, `insert`, `remove`, `flatten`,
`repeat`, `zip`, `enumerate`, `range_list`

**Search**: `contains`, `index_of`, `any`, `all`, `count`

**String**: `starts_with`, `ends_with`, `lpad`, `rpad`, `chars`, `match_all`

**Math**: `clamp`, `lerp`, `sign`, `deg2rad`, `rad2deg`

**Functional**: `compose`, `partial`, `apply`, `twice`

**Sorting**: `sort_list` (custom comparator), `sort_by` (key function)

**Dictionary**: `dict_new`, `dict_set`, `dict_get`, `dict_has`, `dict_keys`, `dict_values`

## Core builtin reference

### Polymorphic
`len`, `reverse`, `slice`, `concat`

### Vector — reductions
`sum`, `mean`, `min`, `max`

### Vector — element-wise
`sqrt`, `abs`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `exp`, `log`,
`floor`, `ceil`, `round`, `pow`, `sort`

### Vector — constructors
`range(stop)`, `range(start, stop)`, `range(start, stop, step)`,
`zeros(n)`, `ones(n)`, `rand()`, `rand(n)`

### String
`upper`, `lower`, `trim`, `split`, `join`, `substr`, `find`, `replace`

### Regex
`match(string, pattern)` — returns list of groups or `nil`

### List
`list(...)`, `push(list, val)`

### Higher-order
`map(list, fn)`, `filter(list, fn)`, `reduce(list, fn, init)`, `each(list, fn)`,
`apply(fn, args_list)`

### Type and conversion
`type(x)`, `str(x)`, `num(string)`, `vec(list)`

### I/O
`read(path)`, `write(path, data)`, `append(path, data)`

### System
`exec(cmd)`, `env(name)`, `exit(code?)`, `clock()`

### Control
`error(msg)`, `assert(cond, msg?)`

### Introspection
`vars()` — list of all names in the current scope
`eval(string)` — parse and execute code at runtime, returns last value

### Constants
`pi`, `e`, `inf`, `nil`, `true`, `false`

### Special variable
`__argv` — list of strings passed via `--args` on the command line

## Architecture

```
flux.cpp          — 45 lines: CLI argument parsing, file loading, REPL entry
flux.h            — ~715 lines: lexer, parser, interpreter, builtins
stdlib.flux       — ~440 lines: standard library in Flux
test_core.flux    — 147 tests for the core
test_stdlib.flux  — 75 tests for the stdlib
reference.flux    — annotated showcase of every feature
```

The interpreter is a recursive-descent parser producing a shared-pointer AST,
evaluated by a tree-walking interpreter. Each AST node carries its source file
and line number for accurate cross-file error reporting. Environments are
linked via `shared_ptr` for proper closure semantics with static scoping.

A stack guard (RAII depth counter) protects against infinite recursion. Errors
during function calls build a stack trace by catching and augmenting
`std::runtime_error` as it unwinds through `call_value` frames.

All file paths are resolved through `std::filesystem` relative to the calling
script's directory, with `FLUX_PATH` and `~/.flux/` fallbacks for module loading.

## Requirements

- C++17 compiler (GCC 8+, Clang 7+, MSVC 19.14+)
- No external libraries
- POSIX `popen` for `exec()` (available on all major platforms)

## License

MIT
