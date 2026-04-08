# ══════════════════════════════════════════════════════════════════════
# test_core.flux — exhaustive tests for Flux core (flux.h)
# ══════════════════════════════════════════════════════════════════════

var pass = 0
var fail = 0

func check (name, got, expected) {
    if (str(got) == str(expected)) {
        pass = pass + 1
    } else {
        print "FAIL:" name "— got" str(got) "expected" str(expected)
        fail = fail + 1
    }
}

# ── constants ──────────────────────────────────────────────────────────

check("true",  true, 1)
check("false", false, 0)
check("nil",   str(nil), "nil")
check("pi",    pi > 3.14, 1)
check("e",     e > 2.71,  1)
check("inf",   inf > 999999, 1)

# ── var and assignment ─────────────────────────────────────────────────

var x = 42
check("var", x, 42)
x = 100
check("assign", x, 100)

# ── arithmetic (scalar = vec of size 1) ────────────────────────────────

check("add",  3 + 4, 7)
check("sub",  10 - 3, 7)
check("mul",  6 * 7, 42)
check("div",  20 / 4, 5)
check("mod",  17 % 5, 2)
check("negate", -5, -5)
check("precedence", 2 + 3 * 4, 14)
check("parens", (2 + 3) * 4, 20)
check("float", .5 + .5, 1)

# ── comparison ─────────────────────────────────────────────────────────

check("eq",  5 == 5, 1)
check("neq", 5 != 3, 1)
check("lt",  3 < 5, 1)
check("gt",  5 > 3, 1)
check("le",  3 <= 3, 1)
check("ge",  5 >= 5, 1)

# ── string equality ───────────────────────────────────────────────────

check("str_eq", "hello" == "hello", 1)
check("str_neq", "a" != "b", 1)
check("nil_eq", nil == nil, 1)
check("nil_neq", nil != "x", 1)

# ── logic ──────────────────────────────────────────────────────────────

check("and_tt", 1 and 1, 1)
check("and_tf", 1 and 0, 0)
check("or_tf",  0 or 1, 1)
check("or_ff",  0 or 0, 0)
check("not_t",  not 1, 0)
check("not_f",  not 0, 1)

# ── vectors ────────────────────────────────────────────────────────────

var v = [1, 2, 3, 4, 5]
check("vec_literal", str(v), "[1, 2, 3, 4, 5]")
check("vec_index", v[0], 1)
check("vec_neg_index", v[-1], 5)
check("vec_add", str([1,2] + [3,4]), "[4, 6]")
check("vec_sub", str([5,5] - [1,2]), "[4, 3]")
check("vec_mul", str([2,3] * [4,5]), "[8, 15]")
check("vec_div", str([10,20] / [2,5]), "[5, 4]")
check("vec_broadcast_add", str([1,2,3] + 10), "[11, 12, 13]")
check("vec_broadcast_mul", str(2 * [1,2,3]), "[2, 4, 6]")

# ── vec reductions ─────────────────────────────────────────────────────

check("sum",  sum(v), 15)
check("mean", mean(v), 3)
check("min",  min(v), 1)
check("max",  max(v), 5)

# ── vec element-wise math ──────────────────────────────────────────────

check("sqrt", sqrt(4), 2)
check("abs",  abs(-3), 3)
check("sin0", sin(0), 0)
check("cos0", cos(0), 1)
check("exp0", exp(0), 1)
check("log1", log(1), 0)
check("tan0", tan(0), 0)
check("asin0", asin(0), 0)
check("acos1", acos(1), 0)
check("atan0", atan(0), 0)
check("floor", floor(3.7), 3)
check("ceil",  ceil(3.2), 4)
check("round", round(3.5), 4)
check("pow",   pow(2, 10), 1024)
check("sqrt_vec", str(sqrt([4,9,16])), "[2, 3, 4]")

# ── vec sort ───────────────────────────────────────────────────────────

check("sort", str(sort([3,1,4,1,5])), "[1, 1, 3, 4, 5]")

# ── vec constructors ──────────────────────────────────────────────────

check("range1", str(range(5)), "[0, 1, 2, 3, 4]")
check("range2", str(range(2, 5)), "[2, 3, 4]")
check("range3", str(range(0, 1, 0.5)), "[0, 0.5]")
check("zeros", str(zeros(3)), "[0, 0, 0]")
check("ones",  str(ones(3)), "[1, 1, 1]")

var r = rand(100)
check("rand_len", len(r), 100)
check("rand_bounds", min(r) >= 0 and max(r) <= 1, 1)
var r1 = rand()
check("rand_scalar", len(r1), 1)

# ── strings ────────────────────────────────────────────────────────────

var s = "hello world"
check("str_index", s[0], "h")
check("str_neg_index", s[-1], "d")
check("str_len", len(s), 11)
check("upper", upper("hello"), "HELLO")
check("lower", lower("HELLO"), "hello")
check("trim",  trim("  hi  "), "hi")
check("find_hit", find("hello", "ll"), 2)
check("find_miss", find("hello", "zz"), -1)
check("substr", substr("hello world", 6, 5), "world")
check("replace", replace("aabaa", "a", "x"), "xxbxx")
check("split", str(split("a,b,c", ",")), "(a, b, c)")
check("join", join(list("x","y","z"), "-"), "x-y-z")

# ── concat (polymorphic: string, list, vec) ────────────────────────────

check("concat_str", concat("hel", "lo"), "hello")
check("concat_list", str(concat(list(1,2), list(3,4))), "(1, 2, 3, 4)")
check("concat_vec", str(concat([1,2], [3,4])), "[1, 2, 3, 4]")

# ── slice (polymorphic: string, list, vec) ─────────────────────────────

check("slice_vec", str(slice([10,20,30,40,50], 1, 4)), "[20, 30, 40]")
check("slice_vec_neg", str(slice([10,20,30,40,50], -3, -1)), "[30, 40]")
check("slice_vec_full", str(slice([1,2,3], 0, 3)), "[1, 2, 3]")
check("slice_vec_empty", str(slice([1,2,3], 2, 2)), "[]")
check("slice_list", str(slice(list(1,2,3,4), 1, 3)), "(2, 3)")
check("slice_str", slice("hello world", 0, 5), "hello")
check("slice_str_neg", slice("hello", -3, 5), "llo")

# ── regex ──────────────────────────────────────────────────────────────

var m = match("age: 42", "([0-9]+)")
check("match_hit", m[0], "42")
check("match_group", m[1], "42")
check("match_miss", str(match("hello", "[0-9]+")), "nil")

# ── lists ──────────────────────────────────────────────────────────────

var l = list(10, "two", 3.5)
check("list_len", len(l), 3)
check("list_index", l[0], 10)
check("list_neg", l[-1], 3.5)
check("list_push", str(push(list(1,2), 3)), "(1, 2, 3)")

# ── polymorphic: len, reverse ──────────────────────────────────────────

check("len_str", len("abc"), 3)
check("len_vec", len([1,2,3]), 3)
check("len_list", len(list(1,2,3)), 3)
check("reverse_str", reverse("abc"), "cba")
check("reverse_vec", str(reverse([1,2,3])), "[3, 2, 1]")
check("reverse_list", str(reverse(list(1,2,3))), "(3, 2, 1)")

# ── type / conversion ─────────────────────────────────────────────────

check("type_scalar", type(42), "scalar")
check("type_vec", type([1,2]), "vec")
check("type_str", type("hi"), "string")
check("type_list", type(list()), "list")
check("type_nil", type(nil), "nil")
check("type_func", type(check), "func")
check("str_conv", str(42), "42")
check("num_conv", num("3.14"), 3.14)
check("vec_conv", str(vec(list(1,2,3))), "[1, 2, 3]")

# ── functions and closures ─────────────────────────────────────────────

func add (a, b) { return a + b }
check("func_call", add(3, 4), 7)

var f = add
check("first_class", f(10, 20), 30)

func make_adder (n) {
    func adder (x) { return x + n }
    return adder
}
var add5 = make_adder(5)
check("closure", add5(10), 15)

var sq = func (x) { return x * x }
check("anon_func", sq(7), 49)

func iife_test () {
    return func (x) { return x * 2 } (21)
}
check("iife", iife_test(), 42)

# ── higher-order ───────────────────────────────────────────────────────

var nums = list(1, 2, 3, 4, 5)
check("map", str(map(nums, func (x) { return x * x })), "(1, 4, 9, 16, 25)")
check("filter", str(filter(nums, func (x) { return x > 3 })), "(4, 5)")
check("reduce", reduce(nums, func (a, b) { return a + b }, 0), 15)

var side = 0
each(list(1, 2, 3), func (x) { side = side + x })
check("each", side, 6)

# ── eval ───────────────────────────────────────────────────────────────

check("eval_expr", eval("2 + 3"), 5)
check("eval_last", eval("1\n2\n3 + 4"), 7)
eval("var eval_var = 42")
check("eval_def", eval_var, 42)
eval("func eval_fn (x) { return x * 10 }")
check("eval_func", eval_fn(5), 50)

# ── apply ──────────────────────────────────────────────────────────────

func add3 (a, b, c) { return a + b + c }
check("apply", apply(add3, list(10, 20, 30)), 60)
check("apply_anon", apply(func (x, y) { return x * y }, list(6, 7)), 42)
check("apply_empty", apply(func () { return 99 }, list()), 99)

# ── if / else / else if ───────────────────────────────────────────────

var branch = ""
if (1 > 2) {
    branch = "a"
} else if (2 > 3) {
    branch = "b"
} else {
    branch = "c"
}
check("if_else", branch, "c")

# ── while / break / continue ──────────────────────────────────────────

var sum_w = 0
var i = 0
while (i < 10) {
    i = i + 1
    if (i == 5) { break }
    sum_w = sum_w + i
}
check("while_break", sum_w, 10)

var sum_c = 0
var j = 0
while (j < 6) {
    j = j + 1
    if (j % 2 == 0) { continue }
    sum_c = sum_c + j
}
check("while_continue", sum_c, 9)

# ── for ────────────────────────────────────────────────────────────────

var sum_f = 0
for (var k = 1; k <= 5; k = k + 1) {
    sum_f = sum_f + k
}
check("for", sum_f, 15)

# ── return ─────────────────────────────────────────────────────────────

func early (n) {
    if (n < 0) { return -1 }
    return 1
}
check("return_early", early(-5), -1)
check("return_normal", early(5), 1)

# ── nested scopes ─────────────────────────────────────────────────────

var outer = 10
func scope_test () {
    var inner = 20
    return outer + inner
}
check("nested_scope", scope_test(), 30)

# ── vars() introspection ──────────────────────────────────────────────

# helper for vars tests
func contains (lst, val) {
    var idx = 0
    while (idx < len(lst)) {
        if (lst[idx] == val) { return true }
        idx = idx + 1
    }
    return false
}

var all_names = vars()
check("vars_has_pi", contains(all_names, "pi"), true)
check("vars_has_check", contains(all_names, "check"), true)
check("vars_has_x", contains(all_names, "x"), true)

func scoped_vars () {
    var local_only = 99
    var names = vars()
    return contains(names, "local_only")
}
check("vars_local", scoped_vars(), true)

# ── I/O ────────────────────────────────────────────────────────────────

write("/tmp/flux_test_io.txt", "hello flux")
check("write_read", read("/tmp/flux_test_io.txt"), "hello flux")
append("/tmp/flux_test_io.txt", "!")
check("append", read("/tmp/flux_test_io.txt"), "hello flux!")

# ── system ─────────────────────────────────────────────────────────────

check("exec", trim(exec("echo ok")), "ok")
check("env", type(env("HOME")), "string")
var t = clock()
check("clock", t > 0, 1)

# ── load ───────────────────────────────────────────────────────────────

write("/tmp/flux_loaded_module.flux", "var loaded_val = 999\n")
load ("/tmp/flux_loaded_module.flux")
check("load", loaded_val, 999)

# ── assert ─────────────────────────────────────────────────────────────

assert(1 == 1, "basic assert")
assert(true)

# ── summary ────────────────────────────────────────────────────────────

print ""
print "═══════════════════════════════════════════"
print " test_core: " pass " passed, " fail " failed"
print "═══════════════════════════════════════════"

if (fail > 0) {
    error("tests failed")
}
