# ════════════════════════════════════════════════════════════════════════
#  reference.flux — a guided tour of every feature in the Flux language.
#  Read top-to-bottom; every section is self-contained and prints output.
# ════════════════════════════════════════════════════════════════════════

# ──────────────────────────────────────────────────────────────────────
# § 1. Comments
# ──────────────────────────────────────────────────────────────────────
# Two comment forms are supported:
#   - line comments start with `#` and run to end of line
#   - block comments use /* ... */ and may span multiple lines (no nesting)

print "══ § 1. Comments ═════════════════════════════════"
var x = 10  # this is a line comment
var y = /* inline block */ 20
var z = /* this comment
           spans
           several lines */ 30
print "  x, y, z =>" x y z


# ──────────────────────────────────────────────────────────────────────
# § 2. Variables and scope
# ──────────────────────────────────────────────────────────────────────
# `var name = expr` declares a new binding in the current scope.
# Plain `name = expr` reassigns an existing binding (looking up the chain).
# Blocks introduced by if/while/for/func get their own nested scope; the
# inner scope can read and update outer bindings.

print ""
print "══ § 2. Variables ════════════════════════════════"
var a = 1
var b = 2
a = a + b               # reassignment, not a new declaration
print "  a after reassign =>" a

# Block-scoped declarations don't leak out:
if (1) {
    var only_inside = 99
    print "  inside if: only_inside =>" only_inside
}
# `only_inside` is gone here — accessing it would error.


# ──────────────────────────────────────────────────────────────────────
# § 3. Numbers
# ──────────────────────────────────────────────────────────────────────
# All numbers are 64-bit floats. There is no separate integer type.
# Internally a "scalar" is a Vec of length 1 — that's why type(42) is
# "scalar" while type([1,2]) is "vec". They participate in the same
# arithmetic by broadcasting (see § 8).

print ""
print "══ § 3. Numbers ══════════════════════════════════"
print "  basic   =>" 1 + 2 * 3
print "  hex-ish =>" 1.5e3 + 0.5
print "  trig    =>" sin(pi / 2)
print "  pow     =>" pow(2, 10)
print "  modulo  =>" 7 % 3
print "  inf?    =>" 1 / 0


# ──────────────────────────────────────────────────────────────────────
# § 4. Strings
# ──────────────────────────────────────────────────────────────────────
# Strings are byte sequences with no built-in Unicode awareness. Indexing
# is byte-based; `for c in s` walks one byte at a time. Standard escape
# sequences: \n \t \r \0 \\ \". There is no string `+` operator — use
# concat() or format() instead (see § 21 format/out).

print ""
print "══ § 4. Strings ══════════════════════════════════"
print "  upper      =>" upper("hello world")
print "  trim       =>" trim("   spaced   ")
print "  substr     =>" substr("hello world", 6, 5)
print "  find       =>" find("greetings", "ting")
print "  replace    =>" replace("a-b-c", "-", "_")
print "  split/join =>" join(split("a,b,c", ","), " | ")
print "  concat     =>" concat("foo", "bar")
print "  asc/char   =>" asc("A") char(65)
print "  index neg  =>" "hello"[-1]


# ──────────────────────────────────────────────────────────────────────
# § 5. nil, booleans, and truthiness
# ──────────────────────────────────────────────────────────────────────
# nil is its own type. true/false are scalar 1/0. Truthiness rules:
#   nil           — false
#   "" / [] / ()  — false
#   {}            — false
#   0             — false
#   any other vec — true iff every element is non-zero (matches MATLAB)
# This makes `if (v == w) { ... }` mean "all elements equal".

print ""
print "══ § 5. nil & truthiness ═════════════════════════"
if (nil) { print "  nil truthy?" } else { print "  nil is falsy" }
if (0)   { print "  0 truthy?"   } else { print "  0 is falsy"   }
if ([])  { print "  [] truthy?"  } else { print "  [] is falsy"  }
if ([1, 2, 3]) { print "  [1,2,3] is truthy" }
if ([1, 0, 3]) { print "  [1,0,3] truthy?" } else { print "  [1,0,3] is falsy (a zero element)" }
print "  type(nil) =>" type(nil)


# ──────────────────────────────────────────────────────────────────────
# § 6. Comparison and equality
# ──────────────────────────────────────────────────────────────────────
# == / != are STRUCTURAL on lists, dicts, strings, and nil. On vec they
# are element-wise (NumPy-style). Cross-type compares are always !=.
# < > <= >= are numeric only.

print ""
print "══ § 6. Comparison ═══════════════════════════════"
print "  list ==  =>" (list(1,2,3) == list(1,2,3))
print "  dict ==  =>" ({a:1, b:2} == {b:2, a:1})            # order-independent
print "  cross    =>" (1 == "1")                             # 0
print "  vec elem =>" ([1,2,3] == [1,2,4])                   # [1, 1, 0]
# A vec equality used in `if` means "all elements equal" (truthiness rule):
if ([1,2,3] == [1,2,3]) { print "  all-eq?  => yes" }


# ──────────────────────────────────────────────────────────────────────
# § 7. Logical operators
# ──────────────────────────────────────────────────────────────────────
# `and` `or` are short-circuit. `not` is unary. `not` binds tighter than
# `and`/`or`; precedence is the C-family one.

print ""
print "══ § 7. Logical operators ════════════════════════"
print "  not 1 and 0 =>" (not 1 and 0)
print "  1 or 0 and 0 =>" (1 or 0 and 0)
# Short-circuit: side-effecting RHS doesn't fire when LHS settles things
var fired = 0
func tag() { fired = 1  return 1 }
var _ = (1 or tag())
print "  short-circuit fired? =>" fired


# ──────────────────────────────────────────────────────────────────────
# § 8. Vec — numeric arrays with broadcasting
# ──────────────────────────────────────────────────────────────────────
# Vecs are flat arrays of doubles, value-typed (assignment copies). All
# arithmetic, comparison, and the standard math functions broadcast:
# scalar + vec, vec + vec (same size), and elementwise comparison.

print ""
print "══ § 8. Vec ══════════════════════════════════════"
var v = [1, 2, 3, 4]
print "  v + 10      =>" (v + 10)
print "  v * v       =>" (v * v)
print "  sum(v)      =>" sum(v)
print "  range       =>" range(0, 10, 2)
print "  zeros/ones  =>" zeros(3) ones(3)
print "  sqrt        =>" sqrt([1, 4, 9, 16])
print "  pow         =>" pow([1, 2, 3], 2)
print "  sort        =>" sort([3, 1, 2])
print "  slice       =>" slice([10,20,30,40,50], 1, 4)
print "  reverse     =>" reverse([1, 2, 3])
print "  concat      =>" concat([1,2], [3,4])


# ──────────────────────────────────────────────────────────────────────
# § 9. Buffers — first-class audio
# ──────────────────────────────────────────────────────────────────────
# A Buffer holds an interleaved array of doubles plus n_frames,
# n_channels, and sample_rate. It's reference-shared (like list/dict).
# Buffers exist BECAUSE vecs aren't enough for audio:
#
#   - vec is a flat sequence with no shape information; a stereo signal
#     in a vec needs an out-of-band convention (planar? interleaved?)
#   - vec has no sample-rate metadata; resampling, scheduling, and
#     format checks would need parallel side-channels
#   - vec is value-typed (assign copies); for multi-MB audio that's
#     pure waste when the host C++ already owns the bytes
#
# Buffers index two ways:
#   buf[i]      — frame i: scalar (mono) or vec of channels
#   buf[i, c]   — sample at frame i, channel c (always scalar)
# Iteration with `for fr in buf` yields per-frame values.
# Buffers participate in arithmetic the way vec does:
#   buf * 0.5, buf + buf, -buf — all return new buffers
# slice/reverse/concat work by frame.

print ""
print "══ § 9. Buffers ══════════════════════════════════"
var buf = buffer(8, 2, 48000)        # 8 frames, stereo, 48 kHz
print "  shape          =>" frames(buf) "x" channels(buf) "@" sample_rate(buf) "Hz"
print "  buf            =>" buf

# Element write
buf[0, 0] = 0.5  buf[0, 1] = -0.5
buf[1, 0] = 0.7  buf[1, 1] = -0.7
print "  buf[0]         =>" buf[0]            # vec [0.5, -0.5]
print "  buf[1, 0]      =>" buf[1, 0]

# Whole-frame assignment via vec
buf[2] = [0.1, 0.9]
print "  buf[2] = [..]  =>" buf[2]

# Arithmetic — buffer-as-numeric-type
var gained = buf * 0.5
print "  gain shape     =>" frames(gained) "x" channels(gained)
print "  gained[0]      =>" gained[0]

# Reductions over flat samples
print "  sum / max / mean (of buf) =>" sum(buf) max(buf) mean(buf)

# Slice (frame-level), reverse, concat
var mono = vec_to_buffer([10, 20, 30, 40, 50, 60])
print "  slice 1..4     =>" buffer_to_vec(slice(mono, 1, 4))
print "  reverse        =>" buffer_to_vec(reverse(mono))
print "  concat         =>" buffer_to_vec(concat(slice(mono, 0, 2), slice(mono, 4, 6)))

# Iteration: per-frame values
var energy = 0
for (var fr in buf) { energy = energy + sum(fr * fr) }
print "  energy         =>" energy

# Conversions to/from vec
var v2 = vec_to_buffer([1, 2, 3, 4], 22050)
print "  vec→buffer→vec =>" buffer_to_vec(v2) "@" sample_rate(v2)


# ──────────────────────────────────────────────────────────────────────
# § 10. Lists — heterogeneous, reference-shared
# ──────────────────────────────────────────────────────────────────────
# Lists hold any mix of values. Built with list(...). They are
# reference-shared (assigning a list aliases it); use copy() to detach.

print ""
print "══ § 10. Lists ═══════════════════════════════════"
var l = list(1, "two", [3, 4], {x: 5})
print "  l[2]       =>" l[2]
push(l, 99)
print "  after push =>" l
pop(l)
insert(l, 0, "head")
print "  after ins  =>" l
print "  slice      =>" slice(l, 1, 3)
print "  concat     =>" concat(list(1, 2), list(3, 4))

# Reference vs copy
var la = list(1, 2, 3)
var lb = la                # alias
push(lb, 99)
print "  shared?    =>" la

var lc = copy(la)
push(lc, 0)
print "  detached?  =>" la lc


# ──────────────────────────────────────────────────────────────────────
# § 11. Dicts — string-keyed maps, reference-shared
# ──────────────────────────────────────────────────────────────────────
# Literal {key: value, ...}. Bare-identifier keys are auto-stringified;
# quoted strings allow arbitrary keys. Member access uses .name for
# identifier keys, ["any string"] for the rest. Iteration yields keys in
# sorted order. Reference-shared like list and buffer.

print ""
print "══ § 11. Dicts ═══════════════════════════════════"
var d = {name: "Ada", age: 7, "with space": 42}
print "  d.name      =>" d.name
print "  d[\"with space\"] =>" d["with space"]
d.age = 8
d["new"] = 99
print "  d           =>" d
print "  has(d,\"new\") =>" has(d, "new")
print "  get default =>" get(d, "missing", "fallback")
print "  keys        =>" keys(d)
print "  len         =>" len(d)

# Concat dicts (right wins)
print "  merge       =>" concat({a: 1, b: 2}, {b: 99, c: 3})

# Build from pairs
print "  from pairs  =>" dict(list(list("k1", 1), list("k2", 2)))

# Iteration is sorted-keys deterministic
for (var k in {z: 1, a: 2, m: 3}) { out(concat("  iter ", concat(k, " "))) }
out("\n")

# Dicts compare structurally, regardless of insertion order
print "  dict ==     =>" ({x: 1, y: 2} == {y: 2, x: 1})


# ──────────────────────────────────────────────────────────────────────
# § 12. Opaque — host-side handles
# ──────────────────────────────────────────────────────────────────────
# Opaque values wrap a C++ shared_ptr<void> and a type tag. They are
# created by the host (FFT plans, file/stream handles, ML models,
# audio-device handles) and are intentionally inert from script: you
# can pass them around, store them in dicts, ask their tag, and hand
# them back to natives. You can't introspect their contents.
#
# (No Flux-side constructor — host C++ uses
#    flux::Value(flux::Opaque{"my_tag", std::make_shared<MyData>(...)})
# inside a register_builtin lambda.)

print ""
print "══ § 12. Opaque (host-side) ══════════════════════"
print "  type(x) returns \"opaque\"; opaque_type(x) returns the host tag."
print "  Use a typed signature like \"opaque:fft_plan, buffer\" with"
print "  register_builtin_typed to dispatch on the tag automatically."


# ──────────────────────────────────────────────────────────────────────
# § 13. Indexing and member access summary
# ──────────────────────────────────────────────────────────────────────

print ""
print "══ § 13. Indexing & member access ════════════════"
print "  vec[i]        =>" ([10, 20, 30][1])
print "  vec[-1]       =>" ([10, 20, 30][-1])
print "  list[i]       =>" (list("a","b","c")[1])
print "  string[i]     =>" ("hello"[1])
print "  dict.key      =>" ({a:1, b:2}.b)
print "  dict[\"k\"]     =>" ({a:1, b:2}["a"])
print "  buf[i]        =>" (buffer(3)[0])              # mono → scalar
print "  buf[i, c]     =>" ((buffer(3, 2))[0, 1])       # multi → scalar


# ──────────────────────────────────────────────────────────────────────
# § 14. Control flow: if / while / for / for-in
# ──────────────────────────────────────────────────────────────────────

print ""
print "══ § 14. Control flow ════════════════════════════"

# if / else if / else
var grade = 86
if      (grade >= 90) { print "  A" }
else if (grade >= 80) { print "  B" }
else                  { print "  ≤ C" }

# while
var i = 0
var acc = 0
while (i < 5) { acc = acc + i  i = i + 1 }
print "  while sum 0..4 =>" acc

# C-style for
var prod = 1
for (var k = 1; k <= 5; k = k + 1) { prod = prod * k }
print "  5! via c-for   =>" prod

# for-in over each iterable kind
for (var x in [10, 20, 30]) { out("  vec ") out(x) out("") } out("\n")
for (var x in list("a","b","c")) { out("  list ") out(x) out("") } out("\n")
for (var c in "abc") { out("  str ") out(c) out("") } out("\n")
for (var k in {q: 1, m: 2, a: 3}) { out("  dict-key ") out(k) out("") } out("\n")
var fb = vec_to_buffer([1, 2, 3])
for (var fr in fb) { out("  buf ") out(fr) out("") } out("\n")

# break / continue
var found = nil
for (var n in range(20)) {
    if (n == 7) { found = n  break }
}
print "  break at n=7  =>" found

# A closure called from a loop cannot break that loop:
func tries_to_break() { break }
try {
    for (var n in range(3)) { tries_to_break() }
} catch (e) {
    print "  closure break =>" e.message
}


# ──────────────────────────────────────────────────────────────────────
# § 15. Functions
# ──────────────────────────────────────────────────────────────────────
# Statement-form `func name(args) { ... }` declares; expression-form
# `func(args) { ... }` is an anonymous lambda. Arity is enforced.
# `return expr` returns; bare `return` returns nil; falling off the end
# also returns nil.

print ""
print "══ § 15. Functions ═══════════════════════════════"
func square(x) { return x * x }
print "  square(7) =>" square(7)

var f = func(x) { return x + 100 }
print "  anon       =>" f(5)

# Apply takes (fn, list-of-args); map/filter/reduce take (collection, fn).
print "  apply()    =>" apply(square, list(9))
print "  map        =>" map(list(1, 2, 3), func(x) { return x * 10 })
print "  filter     =>" filter(list(1, 2, 3, 4, 5), func(x) { return x > 2 })
print "  reduce     =>" reduce(list(1, 2, 3, 4), func(a, b) { return a + b }, 0)


# ──────────────────────────────────────────────────────────────────────
# § 16. Closures
# ──────────────────────────────────────────────────────────────────────
# Functions capture lexical scope by reference, including across
# returns. This makes counters, accumulators, and decorators easy.

print ""
print "══ § 16. Closures ════════════════════════════════"
func make_adder(n) { return func(x) { return x + n } }
var add3 = make_adder(3)
var add10 = make_adder(10)
print "  add3(5)   =>" add3(5)
print "  add10(5)  =>" add10(5)

# Counter pattern
func make_counter() {
    var n = 0
    return func() { n = n + 1  return n }
}
var c = make_counter()
print "  counter   =>" c() c() c()


# ──────────────────────────────────────────────────────────────────────
# § 17. Recursion and tail-call optimization
# ──────────────────────────────────────────────────────────────────────
# `return f(...)` in tail position reuses the C++ frame, so tail
# recursion (self or mutual) runs in O(1) C++ stack. Non-tail
# recursion is bounded by max_stack (default 1000).

print ""
print "══ § 17. Recursion & TCO ═════════════════════════"
func sum_to(n, acc) {
    if (n == 0) { return acc }
    return sum_to(n - 1, acc + n)        # tail call
}
print "  sum to 1..1000 =>" sum_to(1000, 0)

# Mutual tail recursion
func is_even(k) { if (k == 0) { return 1 } return is_odd(k - 1) }
func is_odd(k)  { if (k == 0) { return 0 } return is_even(k - 1) }
print "  is_even(50000) =>" is_even(50000)


# ──────────────────────────────────────────────────────────────────────
# § 18. Higher-order functions
# ──────────────────────────────────────────────────────────────────────

print ""
print "══ § 18. Higher-order ════════════════════════════"
print "  map      =>" map(list(1, 2, 3), func(x) { return x + 1 })
print "  filter   =>" filter(list(0, 1, 2, 3, 4, 5), func(x) { return x % 2 == 0 })
print "  reduce   =>" reduce(list(1, 2, 3, 4), func(a, b) { return a * b }, 1)
each(list(1, 2, 3), func(x) { out("  each ") out(x) out("") }) out("\n")


# ──────────────────────────────────────────────────────────────────────
# § 19. Errors: try / catch / finally / error / assert
# ──────────────────────────────────────────────────────────────────────
# Errors are first-class. The catch-bound variable is a dict with
#   { message, file, line, trace }   — trace is innermost-first.
# `finally` runs in every exit path: normal completion, caught error,
# uncaught error, and even when the try block returns from a function.
# Either `catch` or `finally` (or both) must follow `try`.

print ""
print "══ § 19. Errors ══════════════════════════════════"

# Catching error()
try {
    error("something failed")
} catch (e) {
    print "  caught e.message =>" e.message
    print "  caught e.line    =>" e.line
}

# Stacked error → trace populated
func deep1() { error("from deep1") }
func deep2() { deep1() }
func deep3() { deep2() }
try {
    deep3()
} catch (e) {
    print "  trace depth      =>" len(e.trace)
    for (var t in e.trace) { print "    -" t }
}

# finally runs after caught errors AND after returns out of try
func process(should_fail) {
    var log = list()
    try {
        push(log, "open")
        if (should_fail) { error("nope") }
        push(log, "work")
        return list("ok", log)
    } catch (e) {
        push(log, concat("caught:", e.message))
        return list("err", log)
    } finally {
        push(log, "close")
    }
}
print "  process(0)    =>" process(0)
print "  process(1)    =>" process(1)

# assert echoes expression text
try { assert(2 + 2 == 5) }
catch (e) { print "  assert echo  =>" e.message }

# Error messages on type mismatches now name the operator AND the types:
func msg(thunk) { try { thunk() } catch (e) { return e.message } }
print "  scalar+string =>" msg(func() { return 1 + "x" })
print "  buf+vec       =>" msg(func() { return buffer(2) + [1, 2] })
print "  vec mismatch  =>" msg(func() { return [1,2] + [1,2,3] })


# ──────────────────────────────────────────────────────────────────────
# § 20. Docstrings & help()
# ──────────────────────────────────────────────────────────────────────
# A string literal as the first statement of a function body is captured
# as its docstring. help(fn) returns it; help("name") looks up native
# builtins by name. Use this to keep libraries self-documenting.

print ""
print "══ § 20. Docstrings & help() ═════════════════════"
func gain(x, db) {
    "Apply a gain in dB to a sample, vec, or buffer."
    return x * pow(10, db / 20)
}
print "  help(gain)        =>" help(gain)
print "  help(\"buffer\")    =>" help("buffer")
print "  help(\"sin\")       =>" help("sin")          # no doc registered for sin


# ──────────────────────────────────────────────────────────────────────
# § 21. format and out — formatting and raw stdout
# ──────────────────────────────────────────────────────────────────────
# format(fmt, args...) — `{}` placeholders, `{{` and `}}` for literals
# out(args...)         — write reprs to stdout, no separator, no newline
# print args...        — space-separated, trailing newline

print ""
print "══ § 21. format / out ════════════════════════════"
print "  basic     =>" format("Hello, {}!", "Ada")
print "  multi     =>" format("{} + {} = {}", 2, 3, 5)
print "  literals  =>" format("{{x}} and {}", 7)
out("  out chain : ") out("part-1 ") out("part-2") out("\n")


# ──────────────────────────────────────────────────────────────────────
# § 22. Type and conversion
# ──────────────────────────────────────────────────────────────────────

print ""
print "══ § 22. Types & conversion ══════════════════════"
print "  type(42)        =>" type(42)
print "  type([1,2])     =>" type([1, 2])
print "  type(\"hi\")      =>" type("hi")
print "  type(buffer(3)) =>" type(buffer(3))
print "  num(\"3.14\")     =>" num("3.14")
print "  str(42)         =>" str(42)
print "  vec(list(1,2,3))=>" vec(list(1, 2, 3))


# ──────────────────────────────────────────────────────────────────────
# § 23. Regex
# ──────────────────────────────────────────────────────────────────────
# match(pattern, str) returns a list of (full_match, capture1, …) for the
# first match, or nil. ECMAScript regex syntax (std::regex default).

print ""
print "══ § 23. Regex ═══════════════════════════════════"
print "  match     =>" match("(\\d+)-(\\w+)", "build 42-final ok")


# ──────────────────────────────────────────────────────────────────────
# § 24. Random
# ──────────────────────────────────────────────────────────────────────

print ""
print "══ § 24. Random ══════════════════════════════════"
seed(0)
print "  rand()    =>" rand()
print "  rand(10)  =>" rand(10)
print "  rand(2,5) =>" rand(2, 5)
print "  shuffle   =>" shuffle([1, 2, 3, 4, 5])


# ──────────────────────────────────────────────────────────────────────
# § 25. I/O
# ──────────────────────────────────────────────────────────────────────

print ""
print "══ § 25. I/O ═════════════════════════════════════"
write("/tmp/_flux_ref.txt", "hello flux\n")
append("/tmp/_flux_ref.txt", "more\n")
print "  read      =>" read("/tmp/_flux_ref.txt")
exec("rm -f /tmp/_flux_ref.txt")


# ──────────────────────────────────────────────────────────────────────
# § 26. System
# ──────────────────────────────────────────────────────────────────────

print ""
print "══ § 26. System ══════════════════════════════════"
print "  clock     =>" type(clock())
print "  HOME?     =>" (env("HOME") != nil)
# sleep(0.001) — uncomment to see it work; we skip for fast test runs


# ──────────────────────────────────────────────────────────────────────
# § 27. Introspection — vars / bindings / eval / bench / version
# ──────────────────────────────────────────────────────────────────────
# vars()      — sorted list of names visible in the current scope
# bindings()  — dict {name: value} of visible bindings
# eval(src)   — parse + execute src in the CURRENT scope (not isolated)
# bench(thunk)— call thunk() and return wall-clock seconds taken
# flux_version— string constant

print ""
print "══ § 27. Introspection ═══════════════════════════"
if (1) {
    var local_demo = 7
    print "  vars sample      =>" len(vars())
    eval("local_demo = local_demo * 6")
    print "  eval mutates outer =>" local_demo
}

var elapsed = bench(func() {
    var s = 0
    for (var i in range(10000)) { s = s + i }
})
print "  10k loop took    =>" elapsed "s"
print "  flux_version     =>" flux_version


# ──────────────────────────────────────────────────────────────────────
# § 28. Loading other files
# ──────────────────────────────────────────────────────────────────────
# load("path") executes another file in the current scope. Loads are
# memoized by canonical path — re-loading is a no-op, which also breaks
# circular load chains.

print ""
print "══ § 28. load ════════════════════════════════════"
print "  (this section just describes load — examples"
print "   require an external file. Search order: relative to the"
print "   loading file, then \\$FLUX_PATH, then ~/.flux/.)"


# ──────────────────────────────────────────────────────────────────────
# § 29. Cooperative scheduling hook (host-side feature)
# ──────────────────────────────────────────────────────────────────────
# A C++ host can install a yield callback that fires at every loop
# iteration, every block step, and every function call. Use it for
# cancellation, time-slicing, or progress reporting:
#
#   interp.set_yield([&]{
#       if (stop_requested) flux::err("<host>", 0, "interrupted");
#   });

print ""
print "══ § 29. Cooperative scheduling ══════════════════"
print "  (host-side: see Interpreter::set_yield in flux.h)"


# ──────────────────────────────────────────────────────────────────────
# § 30. Cycle protection
# ──────────────────────────────────────────────────────────────────────
# Self-referential lists/dicts have well-defined repr() and equality —
# no stack overflow on legitimate-looking data. Equality is co-inductive:
# revisiting an in-progress pair returns true.

print ""
print "══ § 30. Cycle protection ════════════════════════"
var cyc = list(1, 2)
push(cyc, cyc)
print "  cyclic list    =>" cyc                     # (1, 2, (...))

var cdict = {tag: "self"}
cdict.me = cdict
print "  cyclic dict    =>" cdict                   # {me: {...}, tag: self}

# Equality on cyclic structures terminates
var p = list(1)  push(p, p)
var q = list(1)  push(q, q)
print "  cyclic ==      =>" (p == q)


# ════════════════════════════════════════════════════════════════════════
print ""
print "════════════════════════════════════════════════════"
print "  reference.flux completed."
print "════════════════════════════════════════════════════"
