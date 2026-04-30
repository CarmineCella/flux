# test_stdlib.flux — regression suite for stdlib.flux
#
# Run after test_core.flux. Loads stdlib once, then exercises every
# function with at least one positive case and at least one edge case
# (empty input, single element, boundary, error path).
#
# Usage:
#   ./flux test_stdlib.flux
#
# This file expects the stdlib at the canonical relative path used by
# the Makefile install target (~/.flux/stdlib.flux), but falls back to
# the local copy for in-tree development.

# ── Locate stdlib ─────────────────────────────────────────────────────
# `load` defines names in the caller's scope, so it must run at the
# top level. We can't try multiple paths via try/catch around a function
# wrapper — that would put the definitions inside the wrapper. Instead,
# require the canonical layout: stdlib.flux is in the same directory
# as this test file. The Makefile install target keeps them together.
load("stdlib.flux")


# ── Test framework (mirrors test_core.flux) ──────────────────────────
var passed = 0
var failed = 0
var failures = list()

func ok(name, cond) {
    if (cond) { passed = passed + 1 }
    else      { failed = failed + 1  push(failures, name) }
}

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


# ════════════════════════════════════════════════════════════════════
#  § 1. Function combinators
# ════════════════════════════════════════════════════════════════════
section("Combinators")

ok("identity scalar",       identity(42) == 42)
ok("identity string",       identity("hi") == "hi")
ok("identity list",         identity(list(1,2)) == list(1,2))

ok("const_fn",              const_fn(7)(99) == 7)
ok("const_fn ignores",      const_fn("x")(list(1,2,3)) == "x")

func inc(x) { return x + 1 }
func dbl(x) { return x * 2 }

ok("compose order",         compose(inc, dbl)(5) == 11)        # dbl first → 10 → inc → 11
ok("compose identity-l",    compose(identity, inc)(5) == 6)
ok("compose identity-r",    compose(inc, identity)(5) == 6)

ok("pipe empty",            pipe(7, list()) == 7)
ok("pipe single",           pipe(5, list(inc)) == 6)
ok("pipe chain",            pipe(5, list(inc, dbl, inc)) == 13)
ok("pipe order",            pipe(5, list(inc, dbl)) == 12)     # (5+1)*2 = 12

func add(a, b) { return a + b }
ok("partial1",              partial1(add, 10)(5) == 15)
ok("partial1 then partial1", partial1(add, 1)(99) == 100)

func sub(a, b) { return a - b }
ok("flip",                  flip(sub)(10, 3) == sub(3, 10))


# ════════════════════════════════════════════════════════════════════
#  § 2. List operations
# ════════════════════════════════════════════════════════════════════
section("List operations")

# take / drop
ok("take 0",                take(list(1,2,3), 0) == list())
ok("take 2",                take(list(1,2,3,4,5), 2) == list(1,2))
ok("take all",              take(list(1,2,3), 3) == list(1,2,3))
ok("take overshoot",        take(list(1,2,3), 99) == list(1,2,3))
ok("take negative",         take(list(1,2,3), -5) == list())

ok("drop 0",                drop(list(1,2,3), 0) == list(1,2,3))
ok("drop 2",                drop(list(1,2,3,4,5), 2) == list(3,4,5))
ok("drop all",              drop(list(1,2,3), 3) == list())
ok("drop overshoot",        drop(list(1,2,3), 99) == list())
ok("drop negative",         drop(list(1,2,3), -5) == list(1,2,3))

# split_at
ok("split_at",              split_at(list(1,2,3,4), 2) == list(list(1,2), list(3,4)))
ok("split_at 0",            split_at(list(1,2,3), 0) == list(list(), list(1,2,3)))

# enumerate
ok("enumerate empty",       enumerate(list()) == list())
ok("enumerate basic",       enumerate(list("a","b","c"))
                              == list(list(0,"a"), list(1,"b"), list(2,"c")))

# zip
ok("zip same len",          zip(list(1,2,3), list("a","b","c"))
                              == list(list(1,"a"), list(2,"b"), list(3,"c")))
ok("zip shorter L",         zip(list(1,2), list("a","b","c","d"))
                              == list(list(1,"a"), list(2,"b")))
ok("zip shorter R",         zip(list(1,2,3,4), list("a","b"))
                              == list(list(1,"a"), list(2,"b")))
ok("zip empty",             zip(list(), list(1,2)) == list())

# zip_with
ok("zip_with sum",          zip_with(list(1,2,3), list(10,20,30), add) == list(11,22,33))
ok("zip_with len min",      len(zip_with(list(1,2,3,4), list(10,20), add)) == 2)

# chunk
ok("chunk even",            chunk(list(1,2,3,4), 2) == list(list(1,2), list(3,4)))
ok("chunk uneven",          chunk(list(1,2,3,4,5), 2) == list(list(1,2), list(3,4), list(5)))
ok("chunk size 1",          chunk(list(1,2,3), 1) == list(list(1), list(2), list(3)))
ok("chunk empty",           chunk(list(), 3) == list())
expect_err("chunk size 0",  func() { return chunk(list(1,2,3), 0) })

# flatten
ok("flatten simple",        flatten(list(list(1,2), list(3,4))) == list(1,2,3,4))
ok("flatten empty",         flatten(list()) == list())
ok("flatten with empty",    flatten(list(list(), list(1), list())) == list(1))

# flatten_deep
ok("flatten_deep flat",     flatten_deep(list(1,2,3)) == list(1,2,3))
ok("flatten_deep nested",   flatten_deep(list(1, list(2, list(3,4)), 5)) == list(1,2,3,4,5))

# find_first
ok("find_first hit",        find_first(list(1,2,3,4), func(x) { return x > 2 }) == 3)
ok("find_first miss",       find_first(list(1,2,3), func(x) { return x > 99 }) == nil)
ok("find_first first match", find_first(list(2,4,6,8), func(x) { return x > 3 }) == 4)

# index_of
ok("index_of hit",          index_of(list(10,20,30,40), func(x) { return x == 30 }) == 2)
ok("index_of miss",         index_of(list(1,2,3), func(x) { return x == 99 }) == -1)

# all / any
func is_pos(x) { return x > 0 }
ok("all true",              all(list(1,2,3), is_pos) == 1)
ok("all false",             all(list(1,-2,3), is_pos) == 0)
ok("all empty",             all(list(), is_pos) == 1)             # vacuous truth
ok("any true",              any(list(-1,-2,3), is_pos) == 1)
ok("any false",             any(list(-1,-2,-3), is_pos) == 0)
ok("any empty",             any(list(), is_pos) == 0)

# count
ok("count zero",            count(list(1,2,3), func(x) { return x > 99 }) == 0)
ok("count some",            count(list(1,2,3,4,5), func(x) { return x > 2 }) == 3)
ok("count all",             count(list(1,2,3), is_pos) == 3)

# repeat
ok("repeat 0",              repeat("x", 0) == list())
ok("repeat 3",              repeat("x", 3) == list("x","x","x"))
ok("repeat scalar",         repeat(7, 4) == list(7,7,7,7))

# sort_by
ok("sort_by ident",         sort_by(list(3,1,2), identity) == list(1,2,3))
ok("sort_by abs",           sort_by(list(-3,1,-2,4), abs) == list(1,-2,-3,4))
ok("sort_by empty",         sort_by(list(), identity) == list())
ok("sort_by single",        sort_by(list(7), identity) == list(7))

# unique
ok("unique no dup",         unique(list(1,2,3)) == list(1,2,3))
ok("unique with dup",       unique(list(1,2,2,3,1)) == list(1,2,3))
ok("unique strings",        unique(list("a","b","a")) == list("a","b"))
ok("unique empty",          unique(list()) == list())


# ════════════════════════════════════════════════════════════════════
#  § 3. Vec-flavored higher-order
# ════════════════════════════════════════════════════════════════════
section("Vec higher-order")

ok("v_map basic",           v_map([1,2,3], func(x) { return x * x }) == [1,4,9])
ok("v_map empty",           len(v_map(zeros(0), func(x) { return x })) == 0)
ok("v_map preserves size",  len(v_map(range(0, 10), inc)) == 10)

# v_each — side effects only; verify it mutates an outer accumulator
var total = 0
v_each([1, 2, 3, 4], func(x) { total = total + x })
ok("v_each side effect",    total == 10)

ok("v_filter basic",        v_filter([1,2,3,4,5], func(x) { return x > 2 }) == [3,4,5])
ok("v_filter all out",      len(v_filter([1,2,3], func(x) { return x > 99 })) == 0)
ok("v_filter all in",       v_filter([1,2,3], func(x) { return x > 0 }) == [1,2,3])

ok("v_reduce sum",          v_reduce([1,2,3,4], add, 0) == 10)
ok("v_reduce empty",        v_reduce(zeros(0), add, 99) == 99)


# ════════════════════════════════════════════════════════════════════
#  § 4. Numeric helpers
# ════════════════════════════════════════════════════════════════════
section("Numeric helpers")

# pmin / pmax — scalars and broadcast
ok("pmin scalars",          pmin(3, 7) == 3)
ok("pmin negative",         pmin(-3, -7) == -7)
ok("pmin equal",            pmin(5, 5) == 5)
ok("pmin vec broadcast",    pmin([1,5,3], 4) == [1,4,3])
ok("pmin elementwise",      pmin([1,5,3], [2,4,5]) == [1,4,3])

ok("pmax scalars",          pmax(3, 7) == 7)
ok("pmax vec broadcast",    pmax([1,5,3], 4) == [4,5,4])
ok("pmax elementwise",      pmax([1,5,3], [2,4,5]) == [2,5,5])

# sign — note: with current core, abs(0) == 0 so sign(0) returns scalar 0.
# But on a vec of zeros, vec*vec where one is the zero-comparison still works.
ok("sign positive",         sign(5) == 1)
ok("sign negative",         sign(-5) == -1)
ok("sign zero",             sign(0) == 0)

# clamp — scalar and vec
ok("clamp under",           clamp(-5, 0, 10) == 0)
ok("clamp over",            clamp(15, 0, 10) == 10)
ok("clamp inside",          clamp(5, 0, 10) == 5)
ok("clamp vec",             clamp([1, 5, 10, 50], 3, 8) == [3, 5, 8, 8])

# lerp / unlerp / remap
ok("lerp midpoint",         lerp(10, 20, 0.5) == 15)
ok("lerp 0",                lerp(10, 20, 0) == 10)
ok("lerp 1",                lerp(10, 20, 1) == 20)
ok("lerp negative t",       lerp(0, 10, -0.5) == -5)
ok("unlerp midpoint",       unlerp(10, 20, 15) == 0.5)
ok("unlerp endpoints",      unlerp(0, 100, 25) == 0.25)
ok("remap basic",           remap(5, 0, 10, 100, 200) == 150)
ok("remap inverted",        remap(0, 0, 10, 100, 0) == 100)

# mod_pos
ok("mod_pos positive",      mod_pos(7, 3) == 1)
ok("mod_pos negative",      mod_pos(-1, 3) == 2)
ok("mod_pos zero",          mod_pos(0, 3) == 0)
ok("mod_pos exact",         mod_pos(6, 3) == 0)


# ════════════════════════════════════════════════════════════════════
#  § 5. Statistics
# ════════════════════════════════════════════════════════════════════
section("Statistics")

# variance / stddev — population (n-divisor)
ok("variance single",       variance([5]) == 0)
ok("variance constant",     variance([3,3,3,3]) == 0)
ok("variance basic",        variance([1,2,3,4,5]) == 2)         # ((4+1+0+1+4)/5)
ok("stddev basic",          abs(stddev([1,2,3,4,5]) - sqrt(2)) < 1e-10)
ok("variance empty",        variance(zeros(0)) == 0)

# standardise — should give zero mean and unit stddev (within tolerance)
var s_vals = [3, 1, 4, 1, 5, 9, 2, 6]
var s_norm = standardise(s_vals)
ok("standardise mean",      abs(mean(s_norm)) < 1e-10)
ok("standardise stddev",    abs(stddev(s_norm) - 1) < 1e-10)

# Constant input → standardise returns zero-centered (no division by zero)
var s_const = [5, 5, 5]
ok("standardise const",     standardise(s_const) == [0, 0, 0])

# normalise to [0,1]
ok("normalise 0..1",        normalise([0, 5, 10], 0, 1) == [0, 0.5, 1])
ok("normalise -1..1",       normalise([0, 5, 10], -1, 1) == [-1, 0, 1])
ok("normalise const",       normalise([7,7,7], 0, 1) == [0,0,0])

# cumsum
ok("cumsum basic",          cumsum([1,1,1,1]) == [1,2,3,4])
ok("cumsum empty",          len(cumsum(zeros(0))) == 0)
ok("cumsum single",         cumsum([5]) == [5])
ok("cumsum negative",       cumsum([1,-1,1,-1]) == [1,0,1,0])

# diff
ok("diff basic",            diff([1, 3, 7, 15]) == [2, 4, 8])
ok("diff length",           len(diff([1,2,3])) == 2)
ok("diff single",           len(diff([5])) == 0)
ok("diff empty",            len(diff(zeros(0))) == 0)

# dot
ok("dot basic",             dot([1,2,3], [4,5,6]) == 32)
ok("dot zero",              dot([0,0], [1,1]) == 0)
ok("dot self",              dot([3,4], [3,4]) == 25)
expect_err("dot mismatch",  func() { return dot([1,2], [1,2,3]) })


# ════════════════════════════════════════════════════════════════════
#  § 6. Dict helpers
# ════════════════════════════════════════════════════════════════════
section("Dict helpers")

ok("dict_map basic",        dict_map({a:1, b:2}, func(v) { return v * 10 })
                              == {a:10, b:20})
ok("dict_map empty",        dict_map({}, func(v) { return v }) == {})

ok("dict_filter keep",      dict_filter({a:1, b:2, c:3}, func(k, v) { return v > 1 })
                              == {b:2, c:3})
ok("dict_filter by key",    dict_filter({a:1, b:2}, func(k, v) { return k == "a" })
                              == {a:1})

ok("merge basic",           merge({a:1}, {b:2}) == {a:1, b:2})
ok("merge right wins",      merge({a:1, b:2}, {b:99}) == {a:1, b:99})

ok("from_pairs basic",      from_pairs(list(list("a", 1), list("b", 2))) == {a:1, b:2})
ok("to_pairs sorted",       to_pairs({z:1, a:2}) == list(list("a", 2), list("z", 1)))
ok("from_pairs roundtrip",  from_pairs(to_pairs({x:1, y:2})) == {x:1, y:2})


# ════════════════════════════════════════════════════════════════════
#  § 7. String helpers
# ════════════════════════════════════════════════════════════════════
section("String helpers")

ok("str_repeat 0",          str_repeat("ab", 0) == "")
ok("str_repeat 3",          str_repeat("ab", 3) == "ababab")
ok("str_repeat 1",          str_repeat("x", 1) == "x")

ok("pad_left short",        pad_left("42", 5, "0") == "00042")
ok("pad_left exact",        pad_left("hello", 5, " ") == "hello")
ok("pad_left longer",       pad_left("hello", 3, " ") == "hello")
ok("pad_right short",       pad_right("42", 5, "0") == "42000")
ok("pad_right exact",       pad_right("hi", 2, "x") == "hi")

ok("starts_with yes",       starts_with("hello world", "hello") == 1)
ok("starts_with no",        starts_with("hello", "world") == 0)
ok("starts_with empty",     starts_with("hi", "") == 1)
ok("starts_with overflow",  starts_with("ab", "abc") == 0)

ok("ends_with yes",         ends_with("hello world", "world") == 1)
ok("ends_with no",          ends_with("hello", "world") == 0)
ok("ends_with empty",       ends_with("hi", "") == 1)
ok("ends_with overflow",    ends_with("ab", "abc") == 0)

ok("contains hit",          contains("hello world", "lo wor") == 1)
ok("contains miss",         contains("hello", "xyz") == 0)
ok("contains empty",        contains("hello", "") == 1)


# ════════════════════════════════════════════════════════════════════
#  § 8. Predicates
# ════════════════════════════════════════════════════════════════════
section("Predicates")

ok("is_nil yes",            is_nil(nil) == 1)
ok("is_nil no scalar",      is_nil(0) == 0)
ok("is_nil no string",      is_nil("") == 0)

ok("is_scalar yes",         is_scalar(42) == 1)
ok("is_scalar no vec",      is_scalar([1,2]) == 0)
ok("is_scalar nil",         is_scalar(nil) == 0)

ok("is_vec yes",            is_vec([1,2,3]) == 1)
ok("is_vec no scalar",      is_vec(42) == 0)             # scalar is its own type-tag

ok("is_string yes",         is_string("hi") == 1)
ok("is_string no",          is_string(42) == 0)

ok("is_list yes",           is_list(list()) == 1)
ok("is_list no",            is_list({}) == 0)

ok("is_dict yes",           is_dict({}) == 1)
ok("is_dict no",            is_dict(list()) == 0)

ok("is_buffer yes",         is_buffer(buffer(4)) == 1)
ok("is_buffer no",          is_buffer([0,0,0,0]) == 0)

ok("is_func yes",           is_func(func(){return 0}) == 1)
ok("is_func native",        is_func(sin) == 1)
ok("is_func no",            is_func(42) == 0)

ok("is_number scalar",      is_number(42) == 1)
ok("is_number vec",         is_number([1,2,3]) == 1)
ok("is_number string",      is_number("42") == 0)
ok("is_number nil",         is_number(nil) == 0)


# ════════════════════════════════════════════════════════════════════
#  § 9. Misc
# ════════════════════════════════════════════════════════════════════
section("Misc")

ok("first list",            first(list(10,20,30)) == 10)
ok("first vec",             first([5,6,7]) == 5)
ok("first string",          first("hello") == "h")
expect_err("first empty",   func() { return first(list()) })

ok("last list",             last(list(10,20,30)) == 30)
ok("last vec",              last([5,6,7]) == 7)
ok("last string",           last("hello") == "o")
expect_err("last empty",    func() { return last(list()) })

# unless_then — runs action only when cond is falsy
var hit = 0
unless_then(0, func() { hit = 1 })
ok("unless_then runs",      hit == 1)
unless_then(1, func() { hit = 99 })
ok("unless_then skips",     hit == 1)               # unchanged

# defaulting
ok("defaulting nil",        defaulting(nil, 42) == 42)
ok("defaulting missing",    defaulting(stdlib_missing, "fallback") == "fallback")
ok("defaulting present",    defaulting(7, 99) == 7)
ok("defaulting falsy",      defaulting(0, 99) == 0)        # 0 is a real value, kept


# ════════════════════════════════════════════════════════════════════
# Summary
# ════════════════════════════════════════════════════════════════════
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
