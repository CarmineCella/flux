# ══════════════════════════════════════════════════════════════════════
# test_stdlib.flux — tests for stdlib.flux
# ══════════════════════════════════════════════════════════════════════

load ("stdlib.flux")

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

# ── list access ────────────────────────────────────────────────────────

var l = list(10, 20, 30, 40, 50)

check("head", head(l), 10)
check("last", last(l), 50)
check("tail", str(tail(l)), "(20, 30, 40, 50)")

var popped = pop(l)
check("pop_list", str(popped[0]), "(10, 20, 30, 40)")
check("pop_elem", popped[1], 50)

check("slice", str(slice(l, 1, 4)), "(20, 30, 40)")
check("slice_start", str(slice(l, 0, 2)), "(10, 20)")

check("insert_mid", str(insert(list(1,2,3), 1, 99)), "(1, 99, 2, 3)")
check("insert_end", str(insert(list(1,2), 5, 99)), "(1, 2, 99)")

check("remove", str(remove(list(1,2,3), 1)), "(1, 3)")
check("remove_first", str(remove(list(1,2,3), 0)), "(2, 3)")

# ── list construction ──────────────────────────────────────────────────

check("flatten", str(flatten(list(list(1,2), 3, list(4,5)))), "(1, 2, 3, 4, 5)")
check("flatten_flat", str(flatten(list(1,2,3))), "(1, 2, 3)")

check("repeat", str(repeat("x", 3)), "(x, x, x)")
check("repeat_0", str(repeat("y", 0)), "()")

check("zip", str(zip(list(1,2,3), list("a","b","c"))), "((1, a), (2, b), (3, c))")
check("zip_uneven", str(zip(list(1,2), list("a","b","c"))), "((1, a), (2, b))")

check("enumerate", str(enumerate(list("x","y"))), "((0, x), (1, y))")

check("range_list", str(range_list(0, 4)), "(0, 1, 2, 3)")
check("range_list_empty", str(range_list(5, 5)), "()")

# ── searching and predicates ───────────────────────────────────────────

check("contains_str_hit", contains("hello world", "world"), true)
check("contains_str_miss", contains("hello", "xyz"), false)
check("contains_list_hit", contains(list(1,2,3), 2), true)
check("contains_list_miss", contains(list(1,2,3), 9), false)

check("index_of_hit", index_of(list("a","b","c"), "b"), 1)
check("index_of_miss", index_of(list("a","b"), "z"), -1)

check("any_true", any(list(1,2,3,4), func (x) { return x > 3 }), true)
check("any_false", any(list(1,2,3), func (x) { return x > 10 }), false)

check("all_true", all(list(1,2,3), func (x) { return x > 0 }), true)
check("all_false", all(list(1,2,3), func (x) { return x > 1 }), false)

check("count", count(list(1,2,3,4,5,6), func (x) { return x % 2 == 0 }), 3)
check("count_0", count(list(1,3,5), func (x) { return x % 2 == 0 }), 0)

# ── string utilities ──────────────────────────────────────────────────

check("starts_with_t", starts_with("hello", "hel"), true)
check("starts_with_f", starts_with("hello", "xyz"), false)
check("starts_with_long", starts_with("hi", "hello"), false)

check("ends_with_t", ends_with("hello", "llo"), true)
check("ends_with_f", ends_with("hello", "xyz"), false)
check("ends_with_long", ends_with("hi", "hello"), false)

check("lpad", lpad("42", 5, "0"), "00042")
check("lpad_noop", lpad("hello", 3, "0"), "hello")

check("rpad", rpad("hi", 6, "."), "hi....")
check("rpad_noop", rpad("hello", 3, "."), "hello")

check("char_at", char_at("abc", 1), "b")

check("chars", str(chars("abc")), "(a, b, c)")
check("chars_empty", str(chars("")), "()")

# ── regex ──────────────────────────────────────────────────────────────

check("match_all", str(match_all("a1 b2 c3", "[a-z][0-9]")), "(a1, b2, c3)")
check("match_all_miss", str(match_all("hello", "[0-9]+")), "()")

# ── math utilities ─────────────────────────────────────────────────────

check("clamp_lo", clamp(-5, 0, 10), 0)
check("clamp_hi", clamp(15, 0, 10), 10)
check("clamp_mid", clamp(5, 0, 10), 5)

check("lerp_0", lerp(0, 100, 0), 0)
check("lerp_1", lerp(0, 100, 1), 100)
check("lerp_mid", lerp(0, 100, 0.5), 50)

check("sign_neg", sign(-5), -1)
check("sign_zero", sign(0), 0)
check("sign_pos", sign(3), 1)

check("deg2rad_180", str(round(deg2rad(180) * 1000) / 1000), str(round(pi * 1000) / 1000))
check("rad2deg_pi", round(rad2deg(pi)), 180)

# ── functional combinators ─────────────────────────────────────────────

func double (x) { return x * 2 }
func add1 (x) { return x + 1 }

var d_then_a = compose(add1, double)
check("compose", d_then_a(5), 11)

var a_then_d = compose(double, add1)
check("compose_order", a_then_d(5), 12)

check("apply_0", apply(func () { return 42 }, list()), 42)
check("apply_2", apply(func (a, b) { return a + b }, list(3, 4)), 7)

var add10 = partial(func (a, b) { return a + b }, 10)
check("partial", add10(7), 17)

var quad = twice(double)
check("twice", quad(3), 12)

# ── sorting ────────────────────────────────────────────────────────────

var unsorted = list(5, 2, 8, 1, 9, 3)
var sorted = sort_list(unsorted, func (a, b) { return a < b })
check("sort_list", str(sorted), "(1, 2, 3, 5, 8, 9)")

var desc = sort_list(unsorted, func (a, b) { return a > b })
check("sort_desc", str(desc), "(9, 8, 5, 3, 2, 1)")

var words = list("banana", "fig", "apple", "cherry")
var by_len = sort_by(words, func (s) { return len(s) })
check("sort_by", str(by_len), "(fig, apple, banana, cherry)")

# ── _list_set helper ───────────────────────────────────────────────────

check("_list_set", str(_list_set(list(1,2,3), 1, 99)), "(1, 99, 3)")

# ── dictionary ─────────────────────────────────────────────────────────

var d = dict_new()
d = dict_set(d, "name", "Flux")
d = dict_set(d, "ver", 1)

check("dict_get", dict_get(d, "name", nil), "Flux")
check("dict_get_miss", str(dict_get(d, "nope", "N/A")), "N/A")
check("dict_has_t", dict_has(d, "name"), true)
check("dict_has_f", dict_has(d, "nope"), false)

d = dict_set(d, "ver", 2)
check("dict_update", dict_get(d, "ver", 0), 2)

check("dict_keys", str(dict_keys(d)), "(name, ver)")
check("dict_values", str(dict_values(d)), "(Flux, 2)")

# ── summary ────────────────────────────────────────────────────────────

print ""
print "═══════════════════════════════════════════"
print " test_stdlib: " pass " passed, " fail " failed"
print "═══════════════════════════════════════════"

if (fail > 0) {
    error("tests failed")
}
