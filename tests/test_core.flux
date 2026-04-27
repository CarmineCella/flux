# test_core.flux
# Comprehensive test of every Flux core feature: operators, builtins,
# control flow, closures, dicts, errors. Reports passed/failed counts.

# ── Test framework ────────────────────────────────────────────────────
var passed = 0
var failed = 0
var failures = list()

func ok(name, cond) {
    if (cond) {
        passed = passed + 1
    } else {
        failed = failed + 1
        push(failures, name)
    }
}

# Assert that `thunk` raises an Error (used for negative tests).
func expect_err(name, thunk) {
    try {
        thunk()
        failed = failed + 1
        push(failures, format("{} (expected error, got value)", name))
    } catch (e) {
        passed = passed + 1
    }
}

func section(label) {
    out(format("\n── {} ", label))
    var pad = 50 - len(label)
    for (var i in range(pad)) { out("─") }
    out("\n")
}

# ── Numeric literals & arithmetic ─────────────────────────────────────
section("Numeric literals & arithmetic")
ok("integer literal",     42 == 42)
ok("float literal",       3.14 == 3.14)
ok("negative literal",    -7 == -7)
ok("scientific 1e3",      1e3 == 1000)
ok("scientific 1.5e2",    1.5e2 == 150)
ok("scientific 2.5e-1",   2.5e-1 == 0.25)
ok("addition",            2 + 3 == 5)
ok("subtraction",         10 - 7 == 3)
ok("multiplication",      6 * 7 == 42)
ok("division",            10 / 4 == 2.5)
ok("modulo",              10 % 3 == 1)
ok("unary minus",         -(-5) == 5)
ok("precedence",          2 + 3 * 4 == 14)
ok("parens override",     (2 + 3) * 4 == 20)
ok("nested unary",        --5 == 5)

# ── Comparison ────────────────────────────────────────────────────────
section("Comparison")
ok("eq scalar",           5 == 5)
ok("neq scalar",          5 != 6)
ok("lt",                  3 < 5)
ok("gt",                  5 > 3)
ok("le equal",            5 <= 5)
ok("le less",             4 <= 5)
ok("ge equal",            5 >= 5)
ok("ge greater",          6 >= 5)
ok("cross-type 1==\"1\"", (1 == "1") == 0)
ok("cross-type nil==0",   (nil == 0) == 0)
ok("nil==nil",            (nil == nil) == 1)
ok("string==string",      "foo" == "foo")
ok("string!=string",      "foo" != "bar")

# ── Logical operators ─────────────────────────────────────────────────
section("Logical operators")
ok("and tt",              (1 and 1) == 1)
ok("and tf",              (1 and 0) == 0)
ok("or ft",               (0 or 1) == 1)
ok("or ff",               (0 or 0) == 0)
ok("not true",            (not 1) == 0)
ok("not false",           (not 0) == 1)
ok("not nil",             (not nil) == 1)
ok("not empty str",       (not "") == 1)
ok("not nonempty str",    (not "x") == 0)
ok("not empty list",      (not list()) == 1)
ok("not empty dict",      (not {}) == 1)
ok("short-circuit and",   (0 and undef_xyz) == 0)
ok("short-circuit or",    (1 or undef_xyz) == 1)

# ── Strings ───────────────────────────────────────────────────────────
section("Strings")
ok("len",                 len("hello") == 5)
ok("len empty",           len("") == 0)
ok("index",               "hello"[0] == "h")
ok("neg index",           "hello"[-1] == "o")
ok("upper",               upper("hello") == "HELLO")
ok("lower",               lower("WORLD") == "world")
ok("trim leading",        trim("  hi") == "hi")
ok("trim trailing",       trim("hi  ") == "hi")
ok("trim both",           trim("  hi  ") == "hi")
ok("trim empty",          trim("") == "")
ok("trim all space",      trim("   ") == "")
ok("substr",              substr("hello world", 6, 5) == "world")
ok("find present",        find("hello world", "world") == 6)
ok("find missing",        find("hello", "xyz") == -1)
ok("replace",             replace("a-b-c", "-", "_") == "a_b_c")
ok("replace none",        replace("abc", "x", "_") == "abc")
ok("replace empty from",  replace("abc", "", "x") == "abc")
ok("split",               split("a,b,c", ",") == list("a", "b", "c"))
ok("split no sep",        split("abc", ",") == list("abc"))
ok("join",                join(list("a", "b", "c"), "-") == "a-b-c")
ok("concat strings",      concat("foo", "bar") == "foobar")
ok("reverse string",      reverse("abc") == "cba")
ok("slice string",        slice("hello", 1, 4) == "ell")
ok("slice neg",           slice("hello", -3, 5) == "llo")
ok("char",                char(65) == "A")
ok("char roundtrip",      asc(char(126)) == 126)
ok("escape \\n",          asc("\n") == 10)
ok("escape \\t",          asc("\t") == 9)
ok("escape \\r",          asc("\r") == 13)
ok("escape \\\\",         asc("\\") == 92)
ok("escape \\\"",         asc("\"") == 34)

# format
ok("format basic",        format("hi {}", "Ada") == "hi Ada")
ok("format multi",        format("{} + {} = {}", 1, 2, 3) == "1 + 2 = 3")
ok("format escape {{",    format("{{x}}") == "{x}")
ok("format short args",   format("{} {}", 1) == "1 {}")
ok("format no slots",     format("plain") == "plain")

# ── Vec — construction & ops ──────────────────────────────────────────
section("Vec — construction")
ok("literal len",         len([1, 2, 3, 4]) == 4)
ok("index",               [1, 2, 3][0] == 1)
ok("neg index",           [1, 2, 3][-1] == 3)
ok("range stop",          range(5) == [0, 1, 2, 3, 4])
ok("range start stop",    range(2, 5) == [2, 3, 4])
ok("range start stop step", range(0, 10, 2) == [0, 2, 4, 6, 8])
ok("range empty",         len(range(0)) == 0)
ok("zeros",               zeros(3) == [0, 0, 0])
ok("ones",                ones(4) == [1, 1, 1, 1])
ok("vec from list",       vec(list(1, 2, 3)) == [1, 2, 3])

section("Vec — element-wise arithmetic")
ok("scalar broadcast +",  sum([1, 2, 3] + 10) == 36)
ok("element +",           sum([1, 2, 3] + [10, 20, 30]) == 66)
ok("element -",           sum([10, 20, 30] - [1, 2, 3]) == 54)
ok("element *",           sum([1, 2, 3] * [4, 5, 6]) == 32)
ok("element /",           sum([10, 20, 30] / [2, 4, 5]) == 16)
ok("element %",           sum([10, 11, 12] % 3) == 3)
ok("element ==",          sum([1, 2, 3] == [1, 2, 4]) == 2)
ok("element <",           sum([1, 2, 3] < [2, 2, 2]) == 1)
ok("unary minus",         sum(-[1, 2, 3]) == -6)

section("Vec — reductions & math")
ok("sum",                 sum([1, 2, 3, 4]) == 10)
ok("mean",                mean([2, 4, 6, 8]) == 5)
ok("min",                 min([3, 1, 4, 1, 5, 9, 2, 6]) == 1)
ok("max",                 max([3, 1, 4, 1, 5, 9, 2, 6]) == 9)
ok("sort",                sort([3, 1, 2]) == [1, 2, 3])
ok("reverse vec",         reverse([1, 2, 3]) == [3, 2, 1])
ok("slice vec",           slice([1, 2, 3, 4, 5], 1, 4) == [2, 3, 4])
ok("concat vec",          concat([1, 2], [3, 4]) == [1, 2, 3, 4])
ok("sqrt",                sqrt([1, 4, 9])[2] == 3)
ok("abs",                 sum(abs([-1, -2, 3])) == 6)
ok("floor",               floor([1.7, 2.3, -0.5]) == [1, 2, -1])
ok("ceil",                ceil([1.2, 2.8, -0.3]) == [2, 3, 0])
ok("round",               round([1.4, 1.6, 2.5]) == [1, 2, 3])
ok("pow",                 pow([2, 3, 4], 2) == [4, 9, 16])
ok("pow elem 0",          pow([2, 3, 4], 2)[0] == 4)
ok("pow elem 1",          pow([2, 3, 4], 2)[1] == 9)
ok("pow elem 2",          pow([2, 3, 4], 2)[2] == 16)
ok("exp(0)==1",           exp([0])[0] == 1)
ok("log(e)≈1",            round(log([e]) * 1000) == 1000)
ok("sin(0)",              sin([0])[0] == 0)
ok("cos(0)",              cos([0])[0] == 1)
ok("sin(pi)≈0",           round(sin([pi]) * 1e6) == 0)
ok("asin(1)≈pi/2",        round(asin([1])[0] * 1000) == round(pi / 2 * 1000))
ok("acos(0)≈pi/2",        round(acos([0])[0] * 1000) == round(pi / 2 * 1000))
ok("atan(0)",             atan([0])[0] == 0)
ok("tan(0)",              tan([0])[0] == 0)

# Vec mutation
var mv = [10, 20, 30]
mv[1] = 99
ok("vec[i] = …",          mv[1] == 99)
ok("vec mutation isolated", mv[0] == 10 and mv[2] == 30)

# ── Lists ─────────────────────────────────────────────────────────────
section("Lists")
ok("list construction",   len(list(1, "a", [1, 2])) == 3)
ok("list len",            len(list(1, 2, 3)) == 3)
ok("list len empty",      len(list()) == 0)
ok("list index",          list("a", "b", "c")[1] == "b")
ok("list neg index",      list(1, 2, 3)[-1] == 3)
ok("list equality",       list(1, 2, 3) == list(1, 2, 3))
ok("list inequality",     list(1, 2, 3) != list(1, 2, 4))
ok("list nested",         list(1, list(2, 3), 4)[1] == list(2, 3))
ok("list reverse",        reverse(list(1, 2, 3)) == list(3, 2, 1))
ok("list slice",          slice(list(1, 2, 3, 4, 5), 1, 4) == list(2, 3, 4))
ok("list concat",         concat(list(1, 2), list(3, 4)) == list(1, 2, 3, 4))

var l1 = list(1, 2)
push(l1, 3)
ok("push mutates",        l1 == list(1, 2, 3))

var l2 = list(1, 2, 3, 4)
ok("pop returns last",    pop(l2) == 4)
ok("pop shrinks",         l2 == list(1, 2, 3))

var l3 = list(1, 2, 4)
insert(l3, 2, 3)
ok("insert mid",          l3 == list(1, 2, 3, 4))
insert(l3, 0, 0)
ok("insert head",         l3 == list(0, 1, 2, 3, 4))
insert(l3, -1, 99)
ok("insert neg index",    l3 == list(0, 1, 2, 3, 99, 4))

var l4 = list(10, 20, 30, 40)
ok("remove returns elem", remove(l4, 1) == 20)
ok("remove shrinks",      l4 == list(10, 30, 40))

# Reference vs copy semantics for lists
var lo = list(1, 2, 3)
var lr = lo
push(lr, 4)
ok("list shares ref",     lo == list(1, 2, 3, 4))

var lc = copy(lo)
push(lc, 99)
ok("copy is independent", lo == list(1, 2, 3, 4))
ok("copy has new elem",   lc == list(1, 2, 3, 4, 99))

# Indexed assignment
var li = list(10, 20, 30)
li[1] = 99
ok("list[i] = …",         li == list(10, 99, 30))

# ── Higher-order ──────────────────────────────────────────────────────
section("Higher-order functions")
ok("map double",          map(list(1, 2, 3), func(x) { return x * 2 }) == list(2, 4, 6))
ok("map identity",        map(list("a", "b"), func(x) { return x }) == list("a", "b"))
ok("filter gt",           filter(list(1, 2, 3, 4, 5), func(x) { return x > 2 }) == list(3, 4, 5))
ok("filter all",          filter(list(1, 2, 3), func(x) { return 1 }) == list(1, 2, 3))
ok("filter none",         filter(list(1, 2, 3), func(x) { return 0 }) == list())
ok("reduce sum",          reduce(list(1, 2, 3, 4), func(a, b) { return a + b }, 0) == 10)
ok("reduce product",      reduce(list(1, 2, 3, 4), func(a, b) { return a * b }, 1) == 24)
ok("reduce empty",        reduce(list(), func(a, b) { return a + b }, 42) == 42)
ok("apply",               apply(func(a, b, c) { return a + b + c }, list(1, 2, 3)) == 6)

var each_sum = 0
each(list(1, 2, 3, 4), func(x) { each_sum = each_sum + x })
ok("each side effects",   each_sum == 10)

ok("each returns nil",    each(list(1), func(x) { return x }) == nil)

# ── Dicts ─────────────────────────────────────────────────────────────
section("Dicts — construction & access")
ok("empty {}",            len({}) == 0)
ok("literal member",      {a: 1, b: 2}.a == 1)
ok("literal index",       {x: 1, y: 2}["y"] == 2)
ok("string-key literal",  {"with space": 7}["with space"] == 7)
ok("missing → nil",       {a: 1}.b == nil)
ok("missing index → nil", {a: 1}["x"] == nil)
ok("nested deep read",    {x: {y: {z: 42}}}.x.y.z == 42)
ok("len",                 len({a: 1, b: 2, c: 3}) == 3)

section("Dicts — assignment & mutation")
var d = {name: "Ada"}
d.age = 7
ok("member assign new",   d.age == 7)
d.age = 8
ok("member assign upd",   d.age == 8)
d["new key"] = 99
ok("index assign str",    d["new key"] == 99)

var nd = {x: {y: {z: 0}}}
nd.x.y.z = 99
ok("nested member set",   nd.x.y.z == 99)

# Reference semantics
var dr_a = {x: 1}
var dr_b = dr_a
dr_b.x = 99
ok("dict shares ref",     dr_a.x == 99)

var dc = copy(dr_a)
dc.x = 0
ok("dict copy independent", dr_a.x == 99 and dc.x == 0)

section("Dicts — built-in operations")
ok("has present",         has({a: 1, b: 2}, "a") == 1)
ok("has missing",         has({a: 1}, "z") == 0)
ok("get found",           get({a: "X"}, "a", "DEF") == "X")
ok("get default",         get({a: "X"}, "z", "DEF") == "DEF")
ok("get no default",      get({a: 1}, "z") == nil)
ok("keys sorted",         keys({c: 1, a: 2, b: 3}) == list("a", "b", "c"))
ok("values sorted-by-key", values({c: 1, a: 2, b: 3}) == list(2, 3, 1))
ok("equality (any order)", {a: 1, b: 2} == {b: 2, a: 1})
ok("structural eq",       {a: list(1, 2), b: {x: 1}} == {b: {x: 1}, a: list(1, 2)})
ok("inequality",          {a: 1} != {a: 2})
ok("size mismatch",       {a: 1, b: 2} != {a: 1})

ok("concat right wins",   concat({a: 1, b: 2}, {b: 99, c: 3}).b == 99)
ok("concat preserves a",  concat({a: 1}, {b: 2}).a == 1)

var dr = {a: 1, b: 2, c: 3}
ok("remove returns val",  remove(dr, "b") == 2)
ok("remove drops key",    has(dr, "b") == 0)
ok("remove missing → nil", remove({}, "x") == nil)

ok("dict() empty",        len(dict()) == 0)
ok("dict() from pairs",   dict(list(list("a", 1), list("b", 2))).a == 1)

# ── Type & conversion ─────────────────────────────────────────────────
section("Type & conversion")
ok("type nil",            type(nil) == "nil")
ok("type scalar",         type(42) == "scalar")
ok("type vec",            type([1, 2, 3]) == "vec")
ok("type string",         type("hi") == "string")
ok("type list",           type(list(1, 2)) == "list")
ok("type dict",           type({a: 1}) == "dict")
ok("type closure",        type(func(x) { return x }) == "func")
ok("type native",         type(len) == "func")
ok("str of num",          str(42) == "42")
ok("str of float",        str(3.5) == "3.5")
ok("str of nil",          str(nil) == "nil")
ok("str of vec",          str([1, 2, 3]) == "[1, 2, 3]")
ok("str of list",         str(list(1, 2)) == "(1, 2)")
ok("num parse int",       num("42") == 42)
ok("num parse float",     num("3.14") == 3.14)
ok("num parse neg",       num("-5") == -5)
ok("num parse sci",       num("1.5e2") == 150)
expect_err("num bad",     func() { return num("abc") })

# ── Regex ─────────────────────────────────────────────────────────────
section("Regex")
ok("match present",       match("hello world", "world") != nil)
ok("match absent",        match("hello", "xyz") == nil)
var m = match("abc123", "([a-z]+)(\\d+)")
ok("match group 0",       m[0] == "abc123")
ok("match group 1",       m[1] == "abc")
ok("match group 2",       m[2] == "123")
expect_err("invalid regex", func() { return match("x", "(unclosed") })

# ── Random (with seed) ────────────────────────────────────────────────
section("Random")
seed(42)
var r1 = rand(5)
seed(42)
var r2 = rand(5)
ok("seed reproducible",   r1 == r2)
seed(1)
var sh1 = shuffle(list(1, 2, 3, 4, 5, 6, 7, 8, 9, 10))
seed(1)
var sh2 = shuffle(list(1, 2, 3, 4, 5, 6, 7, 8, 9, 10))
ok("shuffle reproducible", sh1 == sh2)

seed(7)
var rv = rand(100)
ok("rand range",          min(rv) >= 0 and max(rv) < 1)
ok("rand default size",   len(rand()) == 1)

# ── Constants ─────────────────────────────────────────────────────────
section("Constants")
ok("pi ≈ 3.14159",        round(pi * 100000) == 314159)
ok("e  ≈ 2.71828",        round(e  * 100000) == 271828)
ok("true == 1",           true == 1)
ok("false == 0",          false == 0)
ok("inf > 1e300",         inf > 1e300)
ok("nil is nil",          nil == nil)

# ── Control flow ──────────────────────────────────────────────────────
section("Control flow")
var x = 10
if (x > 5) { x = x + 1 }
ok("if true branch",      x == 11)

if (x < 5) { x = 0 } else { x = x + 1 }
ok("if else branch",      x == 12)

if (x < 0) { x = -1 } else if (x < 100) { x = 50 } else { x = 999 }
ok("else if",             x == 50)

var w = 0
var i = 0
while (i < 5) { w = w + i  i = i + 1 }
ok("while loop",          w == 10)

var fs = 0
for (var j = 0; j < 5; j = j + 1) { fs = fs + j }
ok("c-style for",         fs == 10)

var fis = 0
for (var k in list(10, 20, 30)) { fis = fis + k }
ok("for-in list",         fis == 60)

var visum = 0
for (var v in [1, 2, 3, 4]) { visum = visum + v }
ok("for-in vec",          visum == 10)

var cs = 0
for (var c in "hello") { cs = cs + 1 }
ok("for-in string",       cs == 5)

var keys_seen = list()
for (var dk in {a: 1, b: 2, c: 3}) { push(keys_seen, dk) }
ok("for-in dict (sorted)", keys_seen == list("a", "b", "c"))

var brk = 0
for (var ii = 0; ii < 100; ii = ii + 1) {
    if (ii == 5) { break }
    brk = brk + 1
}
ok("break in for",        brk == 5)

var cnt = 0
for (var ii = 0; ii < 10; ii = ii + 1) {
    if (ii % 2 == 0) { continue }
    cnt = cnt + 1
}
ok("continue in for",     cnt == 5)

# break/continue inside while
var wb = 0
while (wb < 100) {
    if (wb >= 7) { break }
    wb = wb + 1
}
ok("break in while",      wb == 7)

# Nested loops with break (only innermost breaks)
var inner_breaks = 0
for (var a in range(3)) {
    for (var b in range(3)) {
        if (b == 1) { break }
        inner_breaks = inner_breaks + 1
    }
}
ok("break inner only",    inner_breaks == 3)

# ── Functions & closures ──────────────────────────────────────────────
section("Functions & closures")
func double_fn(x) { return x * 2 }
ok("named func",          double_fn(7) == 14)

var lam = func(x) { return x + 1 }
ok("anon func",           lam(41) == 42)

func multi(a, b, c) { return a + b * c }
ok("multi args",          multi(1, 2, 3) == 7)

func no_return() { var z = 5 }
ok("no return = nil",     no_return() == nil)

func bare_return() { return }
ok("bare return = nil",   bare_return() == nil)

# IIFE
ok("IIFE",                (func(x) { return x + 1 })(41) == 42)

# Closure capture
func make_adder(n) { return func(x) { return x + n } }
var add3 = make_adder(3)
var add10 = make_adder(10)
ok("closure 1",           add3(5) == 8)
ok("closure 2",           add10(5) == 15)
ok("closures independent", add3(5) == 8 and add10(5) == 15)

# Closure with mutable state
func make_counter() {
    var n = 0
    return func() { n = n + 1  return n }
}
var c1 = make_counter()
ok("counter 1",           c1() == 1)
ok("counter 2",           c1() == 2)
ok("counter 3",           c1() == 3)

var c2 = make_counter()
ok("independent counters", c2() == 1 and c1() == 4)

# Recursion + TCO
func count_down(n) { if (n == 0) { return "ok" }  return count_down(n - 1) }
ok("TCO 100k",            count_down(100000) == "ok")

func sum_to(n, acc) { if (n == 0) { return acc }  return sum_to(n - 1, acc + n) }
ok("TCO sum_to 10k",      sum_to(10000, 0) == 50005000)

# Mutual TCO
func is_even(n) { if (n == 0) { return 1 }  return is_odd(n - 1) }
func is_odd(n)  { if (n == 0) { return 0 }  return is_even(n - 1) }
ok("mutual TCO even",     is_even(10000) == 1)
ok("mutual TCO odd",      is_odd(10001) == 1)

# ── Errors & try/catch ────────────────────────────────────────────────
section("Errors & try/catch")
expect_err("undefined var",    func() { return undef_xyz })
expect_err("call non-func",    func() { var n = 5  return n(1) })
expect_err("list out of range", func() { return list(1, 2, 3)[10] })
expect_err("vec size mismatch", func() { return [1, 2] + [1, 2, 3] })
expect_err("string + num",     func() { return "a" + 1 })
expect_err("neg of string",    func() { return -"hi" })
expect_err("break at top",     func() { eval("break") })
expect_err("continue at top",  func() { eval("continue") })
expect_err("named func expr",  func() { eval("var f = func g() { return 1 }") })
expect_err("multi dots",       func() { eval("var x = 1.2.3") })
expect_err("dict key not str", func() { var d = {a: 1}  return d[5] })
expect_err("nonexistent key write", func() { var d = {}  eval("d.a.b = 1") })

# `return` at top-level cannot be tested via a thunk: the thunk's call frame
# raises function_depth so eval'd `return` would be valid. Test at top level.
var return_top_caught = 0
try {
    eval("return 1")
} catch (e) {
    if (find(e.message, "return outside") >= 0) { return_top_caught = 1 }
}
ok("return at top",        return_top_caught == 1)

# error() and the catch dict
var ce = nil
try { error("custom") } catch (e) { ce = e }
ok("error() raises",      ce != nil)
ok("error.message",       ce.message == "custom")
ok("error.file string",   type(ce.file) == "string")
ok("error.line scalar",   type(ce.line) == "scalar")
ok("error.trace list",    type(ce.trace) == "list")

# Error with stack trace
func deep1() { error("from deep") }
func deep2() { deep1() }
func deep3() { deep2() }
var de = nil
try { deep3() } catch (e) { de = e }
ok("traced error",        de.message == "from deep")
ok("trace populated",     len(de.trace) >= 3)

# Try doesn't disturb function return
func tries_then_returns() {
    try { error("x") } catch (e) {}
    return 99
}
ok("try preserves return", tries_then_returns() == 99)

# Return inside try
func returns_in_try() {
    try { return 7 } catch (e) { return -1 }
    return 0
}
ok("return inside try",   returns_in_try() == 7)

# Assert message includes expression
var amsg = nil
try { assert(2 + 2 == 5) } catch (e) { amsg = e.message }
ok("assert echoes expr",  find(amsg, "2 + 2 == 5") >= 0)

var amsg2 = nil
try { assert(0, "custom") } catch (e) { amsg2 = e.message }
ok("assert custom msg",   find(amsg2, "custom") >= 0)

# Stray break in closure called from a loop must not escape
func bad_break() { break }
var stray_caught = 0
try {
    for (var x in list(1)) { bad_break() }
} catch (e) {
    if (find(e.message, "break outside") >= 0) { stray_caught = 1 }
}
ok("stray break contained", stray_caught == 1)

# ── I/O ───────────────────────────────────────────────────────────────
section("I/O")
write("test_io.tmp", "hello")
ok("write→read",          read("test_io.tmp") == "hello")
append("test_io.tmp", " world")
ok("append",              read("test_io.tmp") == "hello world")
write("test_io.tmp", 42)
ok("write num as repr",   read("test_io.tmp") == "42")
expect_err("read missing", func() { return read("nonexistent_xyz_42.tmp") })

# ── System ────────────────────────────────────────────────────────────
section("System")
ok("clock scalar",        type(clock()) == "scalar")
ok("clock positive",      clock() > 0)
ok("clock monotonic",     clock() <= clock())
sleep(0)
ok("sleep returns nil",   sleep(0) == nil)
ok("env returns str/nil", type(env("PATH")) == "string" or type(env("PATH")) == "nil")
ok("env missing → nil",   env("DEFINITELY_NOT_SET_XYZ_42") == nil)
ok("exec echo",           trim(exec("echo flux_test_42")) == "flux_test_42")

# ── Introspection ─────────────────────────────────────────────────────
section("Introspection")
var bindings_snapshot = passed
var b = bindings()
ok("bindings is dict",    type(b) == "dict")
ok("bindings has passed", has(b, "passed") == 1)
ok("bindings snapshots",  b.passed == bindings_snapshot)

var vs = vars()
ok("vars is list",        type(vs) == "list")
ok("vars has names",      len(vs) > 0)

ok("eval simple",         eval("1 + 2") == 3)
ok("eval multi-stmt",     eval("var ev_x = 99  ev_x") == 99)
ok("eval defines in scope", ev_x == 99)

# ── Block comments & escapes (round-trip via eval) ───────────────────
section("Block comments")
ok("inline /* */",        eval("var bc1 = /* tail */ 7  bc1") == 7)
ok("multi-line /* */",    eval("var bc2 = /* a
b
c */ 3  bc2") == 3)
ok("# comment",           eval("var bc3 = 5  # tail
bc3") == 5)

# ── Buffers ───────────────────────────────────────────────────────────
section("Buffers")
var b1 = buffer(100)
ok("mono buffer len",     len(b1) == 100)
ok("mono buffer chans",   channels(b1) == 1)
ok("default sr",          sample_rate(b1) == 44100)
ok("mono buf zero init",  b1[0] == 0)
ok("mono buf zero last",  b1[-1] == 0)

# Mono indexing & assignment
b1[0] = 0.5
b1[1] = -0.5
b1[-1] = 1
ok("mono write",          b1[0] == 0.5)
ok("mono neg write",      b1[-1] == 1)
ok("mono mid",            b1[1] == -0.5)

# Multi-channel buffer
var b2 = buffer(10, 2, 48000)
ok("stereo frames",       frames(b2) == 10)
ok("stereo channels",     channels(b2) == 2)
ok("stereo sr",           sample_rate(b2) == 48000)
ok("stereo type",         type(b2) == "buffer")

# Multi-channel indexing
b2[0, 0] = 0.1
b2[0, 1] = 0.2
b2[1, 0] = 0.3
b2[1, 1] = 0.4
ok("stereo [0,0]",        b2[0, 0] == 0.1)
ok("stereo [0,1]",        b2[0, 1] == 0.2)
ok("stereo [1,0]",        b2[1, 0] == 0.3)
ok("stereo [1,1]",        b2[1, 1] == 0.4)

# Single-index on multichannel returns vec of all channels for that frame
var frame0 = b2[0]
ok("frame is vec",        type(frame0) == "vec")
ok("frame size = chans",  len(frame0) == 2)
ok("frame contents",      frame0 == [0.1, 0.2])

# Frame-vec assignment
b2[2] = [0.7, 0.8]
ok("frame vec assign 0",  b2[2, 0] == 0.7)
ok("frame vec assign 1",  b2[2, 1] == 0.8)

# Iteration
var b3 = buffer(4)
b3[0] = 1
b3[1] = 2
b3[2] = 3
b3[3] = 4
var s = 0
for (var x in b3) { s = s + x }
ok("for-in mono buf",     s == 10)

# Conversions
var v_round = buffer_to_vec(b3)
ok("buffer_to_vec",       v_round == [1, 2, 3, 4])

var b4 = vec_to_buffer([10, 20, 30], 96000)
ok("vec_to_buffer chan",  channels(b4) == 1)
ok("vec_to_buffer sr",    sample_rate(b4) == 96000)
ok("vec_to_buffer data",  b4[0] == 10 and b4[1] == 20 and b4[2] == 30)

# Reference vs copy
var ba = buffer(5)
var bb = ba          # shares
bb[0] = 99
ok("buffer shares ref",   ba[0] == 99)
var bc = copy(ba)
bc[0] = 0
ok("buffer copy detached", ba[0] == 99 and bc[0] == 0)

# Errors
expect_err("buf out of range", func() { var b = buffer(5)  return b[10] })
expect_err("buf chan oob",     func() { var b = buffer(5, 2)  return b[0, 5] })
expect_err("multi-idx on vec", func() { return [1, 2, 3][0, 1] })
expect_err("scalar to multich", func() { var b = buffer(5, 2)  b[0] = 1.0  return 0 })
expect_err("buf neg frames",   func() { return buffer(-5) })
expect_err("buf 0 channels",   func() { return buffer(10, 0) })
expect_err("buf neg sr",       func() { return buffer(10, 1, -1) })

# ── Buffer arithmetic ─────────────────────────────────────────────────
section("Buffer arithmetic")
var ab = vec_to_buffer([1, 2, 3, 4])

# scalar broadcast
ok("buf * scalar type",   type(ab * 0.5) == "buffer")
ok("buf * scalar values", buffer_to_vec(ab * 0.5) == [0.5, 1, 1.5, 2])
ok("scalar - buf",        buffer_to_vec(10 - ab) == [9, 8, 7, 6])
ok("buf + scalar",        buffer_to_vec(ab + 1) == [2, 3, 4, 5])

# buffer + buffer
ok("buf + buf",           buffer_to_vec(ab + ab) == [2, 4, 6, 8])
ok("buf * buf",           buffer_to_vec(ab * ab) == [1, 4, 9, 16])

# unary minus
ok("-buffer",             buffer_to_vec(-ab) == [-1, -2, -3, -4])
ok("-buffer preserves type", type(-ab) == "buffer")

# stereo
var stereo = buffer(2, 2)
stereo[0] = [1, 2]
stereo[1] = [3, 4]
ok("stereo * 2",          buffer_to_vec(stereo * 2) == [2, 4, 6, 8])
ok("stereo result chans", channels(stereo * 2) == 2)

# preserves sample_rate
var sr_buf = vec_to_buffer([1, 2, 3], 48000)
ok("sr preserved",        sample_rate(sr_buf * 2) == 48000)

# Shape-mismatch errors (new)
expect_err("buf shape diff",       func() { return buffer(2,2) + buffer(3,2) })
expect_err("buf chan diff",        func() { return buffer(2,2) + buffer(2,1) })
expect_err("buf sr diff",          func() { return buffer(2,1,44100) + buffer(2,1,48000) })
expect_err("buf + vec rejected",   func() { return buffer(2) + [1, 2] })

# Reductions on buffer
ok("sum(buffer)",         sum(ab) == 10)
ok("max(buffer)",         max(ab) == 4)
ok("min(buffer)",         min(ab) == 1)
ok("mean(buffer)",        mean(ab) == 2.5)

# ── Buffer slice / reverse / concat ───────────────────────────────────
section("Buffer slice/reverse/concat")
var big = vec_to_buffer([10, 20, 30, 40, 50, 60, 70, 80])

var win = slice(big, 2, 5)
ok("slice frames",        frames(win) == 3)
ok("slice values",        buffer_to_vec(win) == [30, 40, 50])
ok("slice preserves sr",  sample_rate(win) == sample_rate(big))

# negative indices
var tail = slice(big, -3, -1)
ok("slice negative",      buffer_to_vec(tail) == [60, 70])

# slice does not alias
win[0] = 999
ok("slice detached",      big[2] == 30)

# stereo slice keeps channels
var st = buffer(4, 2)
st[0] = [1,1]  st[1] = [2,2]  st[2] = [3,3]  st[3] = [4,4]
var stslice = slice(st, 1, 3)
ok("stereo slice frames", frames(stslice) == 2)
ok("stereo slice chans",  channels(stslice) == 2)
ok("stereo slice values", buffer_to_vec(stslice) == [2, 2, 3, 3])

# reverse
ok("reverse buffer",      buffer_to_vec(reverse(big)) == [80, 70, 60, 50, 40, 30, 20, 10])
var rev_st = reverse(st)
ok("reverse stereo frame0", buffer_to_vec(slice(rev_st, 0, 1)) == [4, 4])

# concat
var aa = vec_to_buffer([1, 2])
var bb_ = vec_to_buffer([3, 4, 5])
ok("concat buffers",      buffer_to_vec(concat(aa, bb_)) == [1, 2, 3, 4, 5])
ok("concat preserves sr", sample_rate(concat(aa, bb_)) == 44100)

expect_err("concat chan diff", func() { return concat(buffer(2,1), buffer(2,2)) })
expect_err("concat sr diff",   func() { return concat(buffer(2,1,44100), buffer(2,1,48000)) })

# ── Informative type errors ───────────────────────────────────────────
section("Type-error message quality")
func msg_of(thunk) {
    try { thunk() } catch (e) { return e.message }
    return "<no error>"
}

# Each message should mention the operator AND the actual operand types.
ok("scalar+string says both", find(msg_of(func() { return 1 + "x" }), "scalar") >= 0
                              and find(msg_of(func() { return 1 + "x" }), "string") >= 0)
ok("scalar+nil names types",  find(msg_of(func() { return 1 + nil }), "scalar") >= 0
                              and find(msg_of(func() { return 1 + nil }), "nil") >= 0)
ok("unary minus names type",  find(msg_of(func() { return -"hi" }), "string") >= 0)
ok("vec mismatch shows sizes", find(msg_of(func() { return [1,2] + [1,2,3] }), "2 vs 3") >= 0)
ok("buf shape shows shapes",   find(msg_of(func() { return buffer(2,2) + buffer(3,2) }), "2x2") >= 0)

# ── try/finally ───────────────────────────────────────────────────────
section("try/finally")

# finally runs after normal completion of try
var tf1 = list()
try { push(tf1, "try") } finally { push(tf1, "finally") }
ok("finally after try",   tf1 == list("try", "finally"))

# finally runs after caught error
var tf2 = list()
try {
    push(tf2, "try")
    error("oops")
} catch (e) {
    push(tf2, "catch")
} finally {
    push(tf2, "finally")
}
ok("finally after catch", tf2 == list("try", "catch", "finally"))

# finally runs even when there's no catch and the error propagates
var tf3 = list()
var caught_outer = nil
try {
    try {
        push(tf3, "inner-try")
        error("propagate")
    } finally {
        push(tf3, "inner-finally")
    }
} catch (e) {
    caught_outer = e.message
}
ok("finally w/o catch",   tf3 == list("inner-try", "inner-finally"))
ok("error reaches outer", caught_outer == "propagate")

# finally runs even when try returns from a function
func tries_return() {
    var log = list()
    try {
        push(log, "before")
        return log
    } finally {
        push(log, "finally")
    }
}
var ret_log = tries_return()
ok("finally after return", ret_log == list("before", "finally"))

# try with only finally (no catch)
var tf4 = 0
try { tf4 = 1 } finally { tf4 = tf4 + 10 }
ok("try-finally only",    tf4 == 11)

# try with neither catch nor finally is a parse error
expect_err("try no clauses", func() { eval("try { var x = 1 }") })

# ── Cycle protection ──────────────────────────────────────────────────
section("Cycle protection")
var cl = list(1, 2)
push(cl, cl)            # self-referential list
# repr must not stack-overflow
ok("repr cyclic list",    find(str(cl), "...") >= 0)

var cd = {a: 1}
cd.self = cd            # self-referential dict
ok("repr cyclic dict",    find(str(cd), "...") >= 0)

# Length still works
ok("len cyclic list",     len(cl) == 3)

# Mutual cycle through dicts
var ca = {tag: "a"}
var cb = {tag: "b"}
ca.peer = cb
cb.peer = ca
ok("repr mutual cycle",   find(str(ca), "...") >= 0)

# Equality on cyclic data does not loop forever (co-inductive)
var ce1 = list(1)
var ce2 = list(1)
push(ce1, ce1)
push(ce2, ce2)
ok("eq on cyclic lists",  ce1 == ce2)

# ── Docstrings & help() ───────────────────────────────────────────────
section("Docstrings & help()")
func documented(x) {
    "Square the input."
    return x * x
}
func undocumented(x) { return x + 1 }

ok("help on doc'd",       help(documented) == "Square the input.")
ok("help on plain",       help(undocumented) == "<no documentation>")
ok("help by name str",    help("documented") == "Square the input.")
ok("help on native",      find(help("buffer"), "audio buffer") >= 0)
ok("help missing native", find(help("not_a_real_fn"), "no documentation") >= 0)

# Function still runs the docstring as a no-op stmt
ok("doc'd fn still works", documented(7) == 49)

# ── Versioning ────────────────────────────────────────────────────────
section("Versioning")
ok("flux_version is str", type(flux_version) == "string")
ok("flux_version shape",  find(flux_version, ".") > 0)

# ── bench ─────────────────────────────────────────────────────────────
section("bench")
var t = bench(func() { sum_to(1000, 0) })
ok("bench is scalar",     type(t) == "scalar")
ok("bench is non-neg",    t >= 0)

# ── Cleanup ───────────────────────────────────────────────────────────
exec("rm -f test_io.tmp")

# ── Summary ───────────────────────────────────────────────────────────
print ""
print "════════════════════════════════════════════════════"
print format("Total: {} passed, {} failed", passed, failed)
print "════════════════════════════════════════════════════"
if (failed > 0) {
    print "Failures:"
    for (var msg in failures) {
        out("  • ")
        out(msg)
        out("\n")
    }
    exit(1)
}
print "All tests passed."
