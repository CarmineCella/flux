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
# arithmetic; the distinction is shown only by `type()`.

print ""
print "══ § 3. Numbers ══════════════════════════════════"
print "  42        =>" 42
print "  3.14      =>" 3.14
print "  -7        =>" (-7)
print "  1e3       =>" 1e3
print "  1.5e-2    =>" 1.5e-2
print "  type(42)  =>" type(42)
print "  type([1,2]) =>" type([1, 2])

# Arithmetic
print "  2 + 3     =>" 2 + 3
print "  10 / 4    =>" 10 / 4
print "  10 % 3    =>" 10 % 3
print "  2 + 3 * 4 =>" 2 + 3 * 4
print "  (2+3)*4   =>" (2 + 3) * 4

# Constants
print "  pi        =>" pi
print "  e         =>" e
print "  inf       =>" inf


# ──────────────────────────────────────────────────────────────────────
# § 4. Strings
# ──────────────────────────────────────────────────────────────────────
# Strings are byte sequences. Iteration and indexing are byte-based, so
# UTF-8 multibyte characters split if iterated as `for c in str`. The
# string operations are: upper, lower, trim, substr, find, replace,
# split, join, concat, reverse, slice, len, char, asc, format.
#
# Escape sequences in literals: \n \t \r \0 \\ \" (any other \x is the
# raw character). Strings can also be indexed with [i] (negative indexes
# count from the end).

print ""
print "══ § 4. Strings ══════════════════════════════════"
var s = "Hello, World"
print "  s              =>" s
print "  len(s)         =>" len(s)
print "  s[0]           =>" s[0]
print "  s[-1]          =>" s[-1]
print "  upper(s)       =>" upper(s)
print "  lower(s)       =>" lower(s)
print "  trim(\"  hi  \") =>" trim("  hi  ")
print "  substr(s,7,5)  =>" substr(s, 7, 5)
print "  find(s,\"World\")=>" find(s, "World")
print "  find(s,\"xyz\")  =>" find(s, "xyz")
print "  replace(s,\"l\",\"L\") =>" replace(s, "l", "L")
print "  split(\"a,b,c\",\",\") =>" split("a,b,c", ",")
print "  join(list(\"a\",\"b\"),\"-\") =>" join(list("a", "b"), "-")
print "  concat(\"foo\",\"bar\")=>" concat("foo", "bar")
print "  reverse(\"abc\") =>" reverse("abc")
print "  slice(s,7,12)  =>" slice(s, 7, 12)
print "  char(65)       =>" char(65)
print "  asc(\"A\")       =>" asc("A")

# Escapes
print "  \"\\n=newline\" => [" "newline-after\nhere" "]"
print "  \"\\t=tab\"     =>" "before\tafter"


# ──────────────────────────────────────────────────────────────────────
# § 5. nil, booleans, and truthiness
# ──────────────────────────────────────────────────────────────────────
# There is no dedicated boolean type. `true` and `false` are bound to 1
# and 0. Truthiness:
#   - nil          → false
#   - 0            → false (any non-zero scalar is truthy)
#   - empty string → false
#   - empty list   → false
#   - empty dict   → false
#   - empty vec    → false
#   - vec with any zero element → false (all-truthy semantics)
#   - everything else → true
#
# The all-truthy rule for vec means `if (v == w) { ... }` correctly
# reads as "all elements equal", since v == w produces an element-wise
# vec of 0s/1s that is truthy iff every element matched.

print ""
print "══ § 5. nil & truthiness ═════════════════════════"
print "  nil           =>" nil
print "  true, false   =>" true false
print "  not 0         =>" (not 0)
print "  not nil       =>" (not nil)
print "  not \"\"        =>" (not "")
print "  not list()    =>" (not list())
print "  not {}        =>" (not {})
print "  not 5         =>" (not 5)
print "  not [1,1,1]   =>" (not [1, 1, 1])      # all non-zero → truthy
print "  not [1,0,1]   =>" (not [1, 0, 1])      # has a zero → falsy
# This is what makes vec equality usable in if/while:
if ([1, 2, 3] == [1, 2, 3]) { print "  if (v==w) entered branch" }


# ──────────────────────────────────────────────────────────────────────
# § 6. Comparison and equality
# ──────────────────────────────────────────────────────────────────────
# == and != are STRUCTURAL — they recurse into lists and dicts.
# Across different types the result is always false (so 1 != "1").
# < > <= >= require both sides to be numeric.
# For Vec-vs-Vec, == produces an element-wise Vec of 0s/1s (NumPy-style);
# for everything else it produces a scalar 0 or 1.

print ""
print "══ § 6. Comparison ═══════════════════════════════"
print "  5 == 5             =>" (5 == 5)
print "  1 == \"1\"           =>" (1 == "1")        # false: cross-type
print "  nil == nil         =>" (nil == nil)
print "  list(1,2) == list(1,2) =>" (list(1,2) == list(1,2))
print "  {a:1} == {a:1}     =>" ({a:1} == {a:1})
print "  [1,2,3] == [1,2,4] =>" ([1, 2, 3] == [1, 2, 4])  # element-wise
print "  3 < 5              =>" (3 < 5)


# ──────────────────────────────────────────────────────────────────────
# § 7. Logical operators
# ──────────────────────────────────────────────────────────────────────
# and / or / not. `and` and `or` short-circuit: the right operand is
# only evaluated if the result still depends on it.

print ""
print "══ § 7. Logical operators ════════════════════════"
print "  1 and 1   =>" (1 and 1)
print "  1 and 0   =>" (1 and 0)
print "  0 or 1    =>" (0 or 1)
print "  not 1     =>" (not 1)
# Short-circuiting: undef_xyz never evaluated because `0 and ...` is 0.
print "  short-circuit =>" (0 and undef_xyz)


# ──────────────────────────────────────────────────────────────────────
# § 8. Vec — numeric arrays with broadcasting
# ──────────────────────────────────────────────────────────────────────
# A Vec is a contiguous block of doubles, written with [...]. All the
# arithmetic and comparison operators broadcast scalar↔vec and operate
# element-wise on vec↔vec. Vecs assign by VALUE (copying); index
# assignment v[i] = x mutates in place.
#
# Constructors: range, zeros, ones, vec(list), rand
# Reductions:   sum, mean, min, max
# Math:         sqrt, abs, sin, cos, tan, exp, log, asin, acos, atan,
#               floor, ceil, round, pow, sort
# Generic:      len, reverse, slice, concat (must be vec+vec)

print ""
print "══ § 8. Vec ══════════════════════════════════════"
var v = [1, 2, 3, 4]
print "  v               =>" v
print "  len(v)          =>" len(v)
print "  v[0], v[-1]     =>" v[0] v[-1]
print "  v + 10          =>" (v + 10)              # scalar broadcast
print "  v * v           =>" (v * v)               # element-wise
print "  sum(v)          =>" sum(v)
print "  mean(v)         =>" mean(v)
print "  min(v), max(v)  =>" min(v) max(v)
print "  range(5)        =>" range(5)
print "  range(0,10,2)   =>" range(0, 10, 2)
print "  zeros(3)        =>" zeros(3)
print "  ones(3)         =>" ones(3)
print "  sqrt([1,4,9])   =>" sqrt([1, 4, 9])
print "  pow([1,2,3], 2) =>" pow([1, 2, 3], 2)
print "  sort([3,1,2])   =>" sort([3, 1, 2])
print "  reverse([1,2,3])=>" reverse([1, 2, 3])
print "  sin([0, pi/2])  =>" sin([0, pi / 2])

# Vec value semantics: assignment copies.
var v1 = [10, 20, 30]
var v2 = v1
v2[0] = 99
print "  v1, v2 (vec is value-typed) =>" v1 v2


# ──────────────────────────────────────────────────────────────────────
# § 9. Lists — heterogeneous, reference-shared
# ──────────────────────────────────────────────────────────────────────
# Lists hold any mix of types: numbers, strings, vecs, dicts, other
# lists, functions. Built with the `list(...)` function. Lists assign
# by REFERENCE — `b = a` shares the underlying storage; use copy() to
# detach. push/pop/insert/remove all mutate in place.

print ""
print "══ § 9. Lists ════════════════════════════════════"
var l = list(1, "two", [3, 4], {x: 5})
print "  l           =>" l
print "  len(l)      =>" len(l)
print "  l[1]        =>" l[1]
print "  l[-1]       =>" l[-1]

# Mutation
push(l, "appended")
print "  after push  =>" l
var popped = pop(l)
print "  pop returns =>" popped
insert(l, 0, "head")
print "  after insert=>" l
var rem = remove(l, 1)
print "  remove[1]   =>" rem "  list now:" l
l[0] = "FIRST"          # index assignment
print "  after l[0]= =>" l

# Reference vs copy semantics
var la = list(1, 2, 3)
var lb = la                 # shares
push(lb, 4)
var lc = copy(la)           # detaches
push(lc, 99)
print "  la,lb share =>" la lb
print "  lc is copy  =>" lc

# Whole-list ops
print "  reverse     =>" reverse(list(1, 2, 3))
print "  slice(1,3)  =>" slice(list("a","b","c","d","e"), 1, 3)
print "  concat      =>" concat(list(1, 2), list(3, 4))


# ──────────────────────────────────────────────────────────────────────
# § 10. Dicts — string-keyed maps, reference-shared
# ──────────────────────────────────────────────────────────────────────
# Dicts use {key: value, ...} syntax. Keys are strings (bare identifiers
# in literals are auto-stringified). Member access uses `.` for
# identifier keys, [...] for arbitrary string keys. Missing keys read
# back as nil. Dicts assign by REFERENCE; copy() to detach. Iteration
# order via for-in is sorted by key (deterministic).

print ""
print "══ § 10. Dicts ═══════════════════════════════════"
var d = {name: "Ada", age: 7, tags: list("a", "b")}
print "  d             =>" d
print "  d.name        =>" d.name
print "  d[\"age\"]      =>" d["age"]
print "  d.missing     =>" d.missing                 # nil
print "  has(d,\"name\") =>" has(d, "name")
print "  has(d,\"x\")    =>" has(d, "x")
print "  get(d,\"x\",-1) =>" get(d, "x", -1)
print "  keys(d)       =>" keys(d)
print "  values(d)     =>" values(d)
print "  len(d)        =>" len(d)

# Mutation
d.age = 8
d["new key"] = 99
print "  after writes  =>" d
var removed = remove(d, "tags")
print "  remove tags   =>" removed "  d:" d

# Nested dicts: deep read + deep write
var cfg = {db: {host: "localhost", port: 5432}}
cfg.db.port = 9999
print "  cfg.db.port   =>" cfg.db.port

# Reference vs copy
var d1 = {x: 1}
var d2 = d1                 # shares
d2.x = 99
var d3 = copy(d1)
d3.x = 0
print "  d1,d2 share   =>" d1 d2
print "  d3 is copy    =>" d3

# Equality is structural (and order-independent)
print "  {a:1,b:2} == {b:2,a:1} =>" ({a: 1, b: 2} == {b: 2, a: 1})

# Building a dict from pairs
var built = dict(list(list("a", 1), list("b", 2)))
print "  dict(pairs)   =>" built


# ──────────────────────────────────────────────────────────────────────
# § 11. Indexing and member access summary
# ──────────────────────────────────────────────────────────────────────
# v[i]    — vec, list, string: numeric index (negatives wrap)
# d[k]    — dict: string key, returns nil on miss
# d.k     — dict only: same as d["k"]
# Assignment uses the same forms: x[i] = v, x.k = v.
# For nested writes (a.b.c = v) the intermediate path must already
# exist; only the terminal level can be created by assignment.

print ""
print "══ § 11. Indexing & member access ════════════════"
print "  vec       [1,2,3][1]      =>" ([1, 2, 3][1])
print "  list      list(\"a\",\"b\")[0]=>" list("a", "b")[0]
print "  string    \"hello\"[1]      =>" "hello"[1]
print "  dict      {a:1,b:2}[\"a\"]  =>" {a: 1, b: 2}["a"]
print "  member    {a:1,b:2}.b     =>" {a: 1, b: 2}.b


# ──────────────────────────────────────────────────────────────────────
# § 12. Control flow: if / while / for / for-in
# ──────────────────────────────────────────────────────────────────────
# if (...) { ... } else if (...) { ... } else { ... }
# while (cond) { ... }
# for (init; cond; update) { ... }      — C-style
# for (var x in iterable) { ... }       — list, vec, string, dict (keys)
# break and continue are loop-only; they error if used elsewhere.

print ""
print "══ § 12. Control flow ════════════════════════════"

# if / else if / else
var n = 7
if (n < 0)        { print "  negative" }
else if (n == 0)  { print "  zero" }
else              { print "  positive:" n }

# while
var i = 0
var sum = 0
while (i < 5) { sum = sum + i  i = i + 1 }
print "  while sum 0..4 =>" sum

# C-style for
var prod = 1
for (var k = 1; k <= 5; k = k + 1) { prod = prod * k }
print "  5! via c-for   =>" prod

# for-in over list, vec, string, dict
var col = list()
for (var x in list("a", "b", "c")) { push(col, x) }
print "  for-in list    =>" col

var s2 = 0
for (var v in [10, 20, 30]) { s2 = s2 + v }
print "  for-in vec     =>" s2

var chars = list()
for (var c in "abc") { push(chars, c) }
print "  for-in string  =>" chars

var seen_keys = list()
for (var k in {b: 2, a: 1, c: 3}) { push(seen_keys, k) }
print "  for-in dict (keys, sorted) =>" seen_keys

# break / continue
var first_even = -1
for (var k in range(1, 100)) {
    if (k % 2 == 0) { first_even = k  break }
}
print "  first_even     =>" first_even

var odds = list()
for (var k in range(10)) {
    if (k % 2 == 0) { continue }
    push(odds, k)
}
print "  odds < 10      =>" odds


# ──────────────────────────────────────────────────────────────────────
# § 13. Functions
# ──────────────────────────────────────────────────────────────────────
# Statement-form named function:   func name(params) { body }
# Expression-form (anonymous only): func(params) { body }
#
# `return expr` returns; bare `return` returns nil; missing return
# falls off the end and returns nil. Arity is enforced.

print ""
print "══ § 13. Functions ═══════════════════════════════"

func square(x) { return x * x }
print "  square(5)     =>" square(5)

# Anonymous function as a value
var triple = func(x) { return x * 3 }
print "  triple(7)     =>" triple(7)

# Multi-arg
func hypot(a, b) { return sqrt([a*a + b*b])[0] }
print "  hypot(3,4)    =>" hypot(3, 4)

# Bare return = nil
func nothing() { return }
print "  nothing()     =>" nothing()

# IIFE — invoke an anonymous function immediately
print "  IIFE          =>" (func(x) { return x + 1 })(41)


# ──────────────────────────────────────────────────────────────────────
# § 14. Closures
# ──────────────────────────────────────────────────────────────────────
# Functions capture their lexical scope by reference. Mutating a
# captured variable from inside the closure updates the outer slot.

print ""
print "══ § 14. Closures ════════════════════════════════"

func make_adder(n) { return func(x) { return x + n } }
var add3 = make_adder(3)
var add10 = make_adder(10)
print "  add3(5), add10(5) =>" add3(5) add10(5)

# Mutable closure state — independent counters
func make_counter() {
    var n = 0
    return func() { n = n + 1  return n }
}
var c1 = make_counter()
var c2 = make_counter()
print "  c1: " c1() c1() c1()       # 1 2 3
print "  c2 independent: " c2() c2() # 1 2


# ──────────────────────────────────────────────────────────────────────
# § 15. Recursion and tail-call optimization
# ──────────────────────────────────────────────────────────────────────
# A `return f(args...)` in tail position reuses the current call frame:
# the C++ stack does NOT grow. This applies to any callee, including
# mutual recursion. The default max_stack of 1000 still bounds non-tail
# recursion, but tail calls are unbounded.

print ""
print "══ § 15. Recursion & TCO ═════════════════════════"

# Naïve (non-tail) recursion is bounded:
func fact(n) { if (n <= 1) { return 1 } return n * fact(n - 1) }
print "  fact(10)      =>" fact(10)

# Tail-recursive accumulator — runs in O(1) stack
func sum_to(n, acc) {
    if (n == 0) { return acc }
    return sum_to(n - 1, acc + n)            # tail call
}
print "  sum_to(50000) =>" sum_to(50000, 0)

# Mutual TCO
func is_even(k) { if (k == 0) { return 1 } return is_odd(k - 1) }
func is_odd(k)  { if (k == 0) { return 0 } return is_even(k - 1) }
print "  is_even(10001) =>" is_even(10001)


# ──────────────────────────────────────────────────────────────────────
# § 16. Higher-order functions
# ──────────────────────────────────────────────────────────────────────
# map(list, fn)         — apply fn to each element, return new list
# filter(list, pred)    — keep elements where pred returns truthy
# reduce(list, fn, seed) — left-fold
# each(list, fn)        — for side effects, returns nil
# apply(fn, list)       — call fn with the list as positional args

print ""
print "══ § 16. Higher-order ════════════════════════════"

print "  map double    =>" map(list(1, 2, 3), func(x) { return x * 2 })
print "  filter > 2    =>" filter(list(1, 2, 3, 4), func(x) { return x > 2 })
print "  reduce +      =>" reduce(list(1, 2, 3, 4), func(a, b) { return a + b }, 0)
print "  reduce *      =>" reduce(list(1, 2, 3, 4), func(a, b) { return a * b }, 1)
print "  apply         =>" apply(func(a, b, c) { return a + b + c }, list(1, 2, 3))


# ──────────────────────────────────────────────────────────────────────
# § 17. Errors: try / catch / error / assert
# ──────────────────────────────────────────────────────────────────────
# Errors are first-class. The catch-bound variable is a dict with:
#   { message, file, line, trace }   — trace is innermost-first.
# error(msg) raises; assert(expr [, msg]) raises if expr is falsy and
# the diagnostic includes the source-form of the asserted expression.

print ""
print "══ § 17. Errors ══════════════════════════════════"

# Catching error()
try {
    error("something failed")
} catch (e) {
    print "  caught e.message =>" e.message
    print "  caught e.line    =>" e.line
}

# Catching a stacked error — trace is populated
func deep1() { error("from deep1") }
func deep2() { deep1() }
func deep3() { deep2() }
try {
    deep3()
} catch (e) {
    print "  trace depth      =>" len(e.trace)
    for (var t in e.trace) { print "    -" t }
}

# Assert echoes the expression text
try {
    assert(2 + 2 == 5)
} catch (e) {
    print "  assert failure   =>" e.message
}

# Assert with custom message
try {
    assert(0, "intentional")
} catch (e) {
    print "  assert custom    =>" e.message
}


# ──────────────────────────────────────────────────────────────────────
# § 18. format and out — formatting and raw stdout
# ──────────────────────────────────────────────────────────────────────
# format(fmt, args...) — `{}` placeholders, `{{` and `}}` for literal braces
# out(args...)         — write args' reprs to stdout, no separator, no
#                        newline (use this for progress dots, prompts, etc.)
# print args...        — space-separated, trailing newline
# Combine with format for printf-style:  out(format("...", ...))

print ""
print "══ § 18. format / out ════════════════════════════"
print "  basic     =>" format("Hello, {}!", "Ada")
print "  multi     =>" format("{} + {} = {}", 2, 3, 5)
print "  literal { =>" format("{{x}}")
print "  short args=>" format("{} {}", 1)               # leaves second {}
out("  out (no \\n): ")
out("part-1 ") out("part-2") out("\n")


# ──────────────────────────────────────────────────────────────────────
# § 19. Type and conversion
# ──────────────────────────────────────────────────────────────────────
# type(x) returns one of: nil, scalar, vec, string, list, dict, func.
# str, num, vec convert between types.

print ""
print "══ § 19. Types & conversion ══════════════════════"
print "  type(nil)     =>" type(nil)
print "  type(42)      =>" type(42)
print "  type([1,2])   =>" type([1, 2])
print "  type(\"hi\")    =>" type("hi")
print "  type(list(1)) =>" type(list(1))
print "  type({a:1})   =>" type({a: 1})
print "  type(square)  =>" type(square)
print "  str(42)       =>" str(42)
print "  num(\"3.14\")   =>" num("3.14")
print "  vec(list(1,2,3)) =>" vec(list(1, 2, 3))


# ──────────────────────────────────────────────────────────────────────
# § 20. Regex
# ──────────────────────────────────────────────────────────────────────
# match(text, pattern) — std::regex (ECMAScript flavor). Returns a list
# of [whole_match, group1, group2, ...] or nil if no match.

print ""
print "══ § 20. Regex ═══════════════════════════════════"
var m = match("year 2026 month 04", "(\\d{4}) month (\\d{2})")
print "  matched groups =>" m
print "  no match       =>" match("hello", "\\d+")


# ──────────────────────────────────────────────────────────────────────
# § 21. Random
# ──────────────────────────────────────────────────────────────────────
# Backed by mt19937_64. seed(n) makes runs reproducible.
# rand([n])     — vec of n uniform [0,1) values (default 1)
# shuffle(list-or-vec) — returns a shuffled COPY (not in place)

print ""
print "══ § 21. Random ══════════════════════════════════"
seed(42)
print "  rand(3)        =>" rand(3)
seed(42)
print "  rand(3) again  =>" rand(3)             # identical: seeded
seed(1)
print "  shuffle list   =>" shuffle(list(1, 2, 3, 4, 5))


# ──────────────────────────────────────────────────────────────────────
# § 22. I/O
# ──────────────────────────────────────────────────────────────────────
# read(path)         — returns whole file as string
# write(path, val)   — writes repr(val), overwriting
# append(path, val)  — appends repr(val)
# Paths are relative to the current working directory.

print ""
print "══ § 22. I/O ═════════════════════════════════════"
write("flux_demo.tmp", "first line")
append("flux_demo.tmp", " + appended")
print "  file contents  =>" read("flux_demo.tmp")
exec("rm -f flux_demo.tmp")


# ──────────────────────────────────────────────────────────────────────
# § 23. System
# ──────────────────────────────────────────────────────────────────────
# clock()         — seconds since some epoch (high-resolution monotonic)
# sleep(seconds)  — block this thread (real-valued)
# env(name)       — environment variable string or nil
# exec(cmd)       — run shell command, capture stdout as string
# exit(code)      — terminate immediately (not run here)

print ""
print "══ § 23. System ══════════════════════════════════"
var t0 = clock()
sleep(0.001)
var t1 = clock()
print "  elapsed (s)    =>" (t1 - t0)
print "  env(\"HOME\")    =>" env("HOME")
print "  exec echo      =>" trim(exec("echo from-shell"))


# ──────────────────────────────────────────────────────────────────────
# § 24. Introspection
# ──────────────────────────────────────────────────────────────────────
# vars()      — sorted list of names visible in the current scope
# bindings()  — same, but as a dict {name: value, ...}
# eval(src)   — parse + execute string in the CURRENT scope; returns
#               the value of the last expression.

print ""
print "══ § 24. Introspection ═══════════════════════════"

func demo_introspection() {
    var local_var = 99
    return list(len(vars()), bindings().local_var)
}
print "  inside func    =>" demo_introspection()

# eval runs in the caller's scope, so it can both read and write outer vars
var dynamic = 0
eval("dynamic = 7 * 6")
print "  eval set var   =>" dynamic
print "  eval expr      =>" eval("1 + 2 + 3")


# ──────────────────────────────────────────────────────────────────────
# § 25. Loading other files
# ──────────────────────────────────────────────────────────────────────
# load("path") — runs another .flux file in the current scope. Loads
# are memoized by canonical path (loading the same file twice is a
# no-op), and circular loads are safely broken. Search order:
#   1. relative to the current source file
#   2. each entry in $FLUX_PATH (colon-separated on Unix, ; on Windows)
#   3. ~/.flux/ (or %USERPROFILE%/.flux/ on Windows)

print ""
print "══ § 25. load ════════════════════════════════════"
write("greet.flux", "func greet(name) { return concat(\"hello \", name) }")
load("greet.flux")
print "  loaded greet   =>" greet("Ada")
load("greet.flux")                      # second load is a no-op (memoized)
print "  load is memoized — no re-execution"
exec("rm -f greet.flux")


# ──────────────────────────────────────────────────────────────────────
# § 26. Cooperative scheduling hook (host-side feature)
# ──────────────────────────────────────────────────────────────────────
# Hosts embedding flux can call `interp.set_yield(fn)` from C++ to
# install a callback invoked at every loop iteration, every block
# statement, and every function call. This lets the host implement
# cancellation, time-slicing, or progress reporting without changing
# the language. There's no flux-level API for it; mentioned here for
# completeness.

print ""
print "══ § 26. Cooperative scheduling ══════════════════"
print "  (host-side: see Interpreter::set_yield in flux.h)"


# ════════════════════════════════════════════════════════════════════════
print ""
print "════════════════════════════════════════════════════"
print "  reference.flux completed."
print "════════════════════════════════════════════════════"
