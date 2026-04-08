# ══════════════════════════════════════════════════════════════════════
# reference.flux — Flux language reference and showcase
# ══════════════════════════════════════════════════════════════════════
# Run: ./flux reference.flux
# ══════════════════════════════════════════════════════════════════════

load ("stdlib.flux")

print "╔══════════════════════════════════════════╗"
print "║         Flux Language Reference          ║"
print "╚══════════════════════════════════════════╝"
print ""

# ┌────────────────────────────────────────────────┐
# │  1. COMMENTS                                   │
# └────────────────────────────────────────────────┘
# Lines starting with # are comments.
var x = 42  # inline comment


# ┌────────────────────────────────────────────────┐
# │  2. VARIABLES AND TYPES                        │
# └────────────────────────────────────────────────┘

print "── 2. Variables and Types ──"

var n = 42
var f = 3.14
var h = .5
var sci = 1.5e3
print "scalar:" n f h sci

var greeting = "hello world"
var escaped = "line1\nline2\ttab"
print "string:" greeting

var v = [1, 2, 3, 4, 5]
print "vector:" v

var stuff = list(42, "text", [1,2,3], nil)
print "list:" stuff

print "nil:" nil
print "types:" type(42) type([1,2]) type("hi") type(list()) type(nil) type(sum)


# ┌────────────────────────────────────────────────┐
# │  3. OPERATORS                                  │
# └────────────────────────────────────────────────┘

print ""
print "── 3. Operators ──"

print "  2 + 3 =" 2 + 3
print "  10 % 3 =" 10 % 3
print "  5 == 5:" 5 == 5
print "  \"a\" == \"a\":" "a" == "a"
print "  nil == nil:" nil == nil
print "  true and false:" true and false
print "  not false:" not false
var neg_vec = -[1,2,3]
print "  -[1,2,3]:" neg_vec


# ┌────────────────────────────────────────────────┐
# │  4. VECTOR ARITHMETIC AND BROADCAST            │
# └────────────────────────────────────────────────┘

print ""
print "── 4. Vector Arithmetic ──"

var a = [1, 2, 3]
var b = [10, 20, 30]

print "a + b:" a + b
print "a * b:" a * b
print "a * 10:" a * 10
print "100 / a:" 100 / a
var cmp_test = [1,5,3] > 2
print "[1,5,3] > 2:" cmp_test


# ┌────────────────────────────────────────────────┐
# │  5. CONTROL FLOW                               │
# └────────────────────────────────────────────────┘

print ""
print "── 5. Control Flow ──"

var temp = 25
if (temp > 30) {
    print "hot"
} else if (temp > 20) {
    print "warm (temp =" temp ")"
} else {
    print "cold"
}

var counter = 0
while (counter < 5) { counter = counter + 1 }
print "while result:" counter

var i = 0
while (true) {
    if (i >= 3) { break }
    i = i + 1
}
print "break at:" i

var total = 0
for (var k = 1; k <= 10; k = k + 1) { total = total + k }
print "for 1..10:" total


# ┌────────────────────────────────────────────────┐
# │  6. FUNCTIONS                                  │
# └────────────────────────────────────────────────┘

print ""
print "── 6. Functions ──"

func greet (name) { return concat("hello, ", name) }
print greet("world")

var mygreet = greet
print mygreet("flux")

var square = func (x) { return x * x }
print "square(7):" square(7)

func make_counter () {
    var n = 0
    func tick () {
        n = n + 1
        return n
    }
    return tick
}
var c1 = make_counter()
var c2 = make_counter()
print "c1:" c1() c1() c1()
print "c2:" c2()

print "IIFE:" func (x) { return x * x } (6)

func safe_div (a, b) {
    if (b == 0) { return inf }
    return a / b
}
print "safe_div:" safe_div(10, 3) safe_div(1, 0)


# ┌────────────────────────────────────────────────┐
# │  7. HIGHER-ORDER FUNCTIONS                     │
# └────────────────────────────────────────────────┘

print ""
print "── 7. Higher-Order Functions ──"

var data = list(1, 2, 3, 4, 5, 6, 7, 8)

print "squares:" map(data, func (x) { return x * x })
print "evens:" filter(data, func (x) { return x % 2 == 0 })
print "sum:" reduce(data, func (acc, x) { return acc + x }, 0)

var result = reduce(
    map(filter(data, func (x) { return x % 2 == 0 }), func (x) { return x * x }),
    func (a, b) { return a + b }, 0)
print "sum of squares of evens:" result


# ┌────────────────────────────────────────────────┐
# │  8. PRINT, INDEXING, SLICING                   │
# └────────────────────────────────────────────────┘

print ""
print "── 8. Print / Indexing / Slicing ──"

print "pi =" pi "e =" e "tau =" 2 * pi

print "string[0]:" "hello"[0]
print "string[-1]:" "hello"[-1]
var idx_demo = [10, 20, 30]
print "vec[2]:" idx_demo[2]
print "list[1]:" list("a", "b", "c")[1]

# slice(collection, start, stop) — polymorphic, supports negative indices
print "slice vec:" slice([10,20,30,40,50], 1, 4)
print "slice str:" slice("hello world", 6, 11)
print "slice list:" slice(list(1,2,3,4,5), -3, -1)
print "slice neg:" slice("hello", -3, 5)


# ┌────────────────────────────────────────────────┐
# │  9. VECTOR BUILTINS                            │
# └────────────────────────────────────────────────┘

print ""
print "── 9. Vector Builtins ──"

var w = [4, 1, 9, 2, 7]

print "sum:" sum(w) "mean:" mean(w) "min:" min(w) "max:" max(w)
print "sqrt:" sqrt([4, 9, 16])
print "abs:" abs([-1, -2, 3])
print "sin/cos:" sin(0) cos(0)
print "floor/ceil/round:" floor(3.7) ceil(3.2) round(3.5)
print "pow:" pow(2, [1, 2, 3, 4])
print "sort:" sort(w)

print "range:" range(5)
print "range(2,8):" range(2, 8)
print "range(0,1,.2):" range(0, 1, 0.2)
print "zeros:" zeros(4)
print "ones:" ones(4)
print "rand(5):" rand(5)


# ┌────────────────────────────────────────────────┐
# │  10. STRING BUILTINS                           │
# └────────────────────────────────────────────────┘

print ""
print "── 10. String Builtins ──"

print "upper:" upper("hello flux")
print "lower:" lower("HELLO FLUX")
print "trim:" trim("   spaces   ")
print "find:" find("hello world", "world")
print "substr:" substr("hello world", 0, 5)
print "replace:" replace("foo bar foo", "foo", "baz")
print "split:" split("one,two,three", ",")
print "join:" join(list("a", "b", "c"), " + ")


# ┌────────────────────────────────────────────────┐
# │  11. POLYMORPHIC BUILTINS                      │
# └────────────────────────────────────────────────┘

print ""
print "── 11. Polymorphic Builtins ──"

print "len:" len("abc") len([1,2,3]) len(list(1,2))
print "reverse:" reverse("abc") reverse([1,2,3]) str(reverse(list(1,2,3)))
print "concat str:" concat("hel", "lo")
print "concat vec:" concat([1,2], [3,4])
print "concat list:" concat(list(1,2), list(3,4))
print "slice vec:" slice([10,20,30,40], 1, 3)
print "slice str:" slice("abcdef", 2, 5)


# ┌────────────────────────────────────────────────┐
# │  12. REGEX                                     │
# └────────────────────────────────────────────────┘

print ""
print "── 12. Regex ──"

var m = match("score: 42 points", "([0-9]+)")
print "match:" m
print "no match:" match("hello", "[0-9]+")
print "match_all:" match_all("x1 y2 z3", "[a-z][0-9]")


# ┌────────────────────────────────────────────────┐
# │  13. INTROSPECTION AND METAPROGRAMMING         │
# └────────────────────────────────────────────────┘

print ""
print "── 13. Introspection and Metaprogramming ──"

# vars() returns a sorted list of all names visible in the current scope
var my_var = 42
func my_func () { return 1 }
var all_names = vars()
print "total visible names:" len(all_names)

func show_locals () {
    var local_a = 1
    var local_b = 2
    var names = vars()
    return filter(names, func (n) {
        return starts_with(n, "local_")
    })
}
print "locals in function:" show_locals()

# eval(string) — parse and execute code at runtime
print "eval expr:" eval("2 + 3 * 4")

# eval can define variables and functions in the current scope
eval("var dynamic = 100")
print "eval defined:" dynamic

eval("func cube (x) { return x * x * x }")
print "eval func:" cube(4)

# eval returns the value of the last expression
print "eval last:" eval("1 + 1\n2 + 2\n3 + 3")

# code generation: build flux code as strings, then eval
var op = "*"
var code = concat(concat("5 ", op), " 10")
print "codegen:" eval(code)

# apply(fn, args_list) — call any function with arguments from a list
func sum3 (a, b, c) { return a + b + c }
print "apply:" apply(sum3, list(10, 20, 30))

# apply works with any callable
print "apply lambda:" apply(func (x, y) { return x * y }, list(7, 6))


# ┌────────────────────────────────────────────────┐
# │  14. I/O AND SYSTEM                            │
# └────────────────────────────────────────────────┘

print ""
print "── 14. I/O and System ──"

write("/tmp/flux_ref_demo.txt", "first line")
append("/tmp/flux_ref_demo.txt", "\nsecond line")
print "file:" read("/tmp/flux_ref_demo.txt")

print "exec:" trim(exec("echo hello from shell"))
print "HOME:" env("HOME")

var t0 = clock()
var dummy = sum(rand(10000))
print "clock:" (clock() - t0) "seconds for sum(rand(10000))"

assert(2 + 2 == 4, "math works")


# ┌────────────────────────────────────────────────┐
# │  15. STDLIB HIGHLIGHTS                         │
# └────────────────────────────────────────────────┘

print ""
print "── 15. Stdlib Highlights ──"

print "head:" head(list(1,2,3))
print "tail:" tail(list(1,2,3))
print "pop:" pop(list(1,2,3))
print "flatten:" flatten(list(list(1,2), 3, list(4,5)))
print "zip:" zip(list(1,2), list("a","b"))
print "enumerate:" enumerate(list("x","y","z"))
print "contains:" contains("hello", "ell") contains(list(1,2,3), 2)
print "any:" any(list(1,2,3), func (x) { return x > 2 })
print "all:" all(list(1,2,3), func (x) { return x > 0 })
print "starts_with:" starts_with("hello", "hel")
print "clamp:" clamp(15, 0, 10)
print "lerp:" lerp(0, 100, 0.25)

func dbl (x) { return x * 2 }
print "compose:" compose(func (x) { return x + 1 }, dbl) (5)
print "sort_list:" sort_list(list(3,1,2), func (a, b) { return a < b })

var db = dict_new()
db = dict_set(db, "lang", "Flux")
db = dict_set(db, "year", 2025)
print "dict:" db
print "dict_get:" dict_get(db, "lang", nil)


# ┌────────────────────────────────────────────────┐
# │  16. EXAMPLES                                  │
# └────────────────────────────────────────────────┘

print ""
print "── 16. FizzBuzz ──"
var lines = list()
for (var n = 1; n <= 15; n = n + 1) {
    if (n % 15 == 0) { lines = push(lines, "FizzBuzz") }
    else if (n % 3 == 0) { lines = push(lines, "Fizz") }
    else if (n % 5 == 0) { lines = push(lines, "Buzz") }
    else { lines = push(lines, str(n)) }
}
print join(lines, " ")

print ""
print "── 16. Fibonacci ──"
func fib (n) {
    if (n <= 1) { return n }
    var a = 0
    var b = 1
    var idx = 2
    while (idx <= n) {
        var tmp = b
        b = a + b
        a = tmp
        idx = idx + 1
    }
    return b
}
var fibs = list()
for (var idx = 0; idx <= 12; idx = idx + 1) { fibs = push(fibs, fib(idx)) }
print "fib(0..12):" fibs

print ""
print "── 16. Newton's Method ──"
func newton_sqrt (x) {
    var guess = x / 2
    var idx = 0
    while (idx < 20) {
        guess = (guess + x / guess) / 2
        idx = idx + 1
    }
    return guess
}
print "newton_sqrt(2):" newton_sqrt(2) "(actual:" sqrt(2) ")"

print ""
print "── 16. Statistics ──"
var samples = rand(1000)
print "n:" len(samples) "mean:" mean(samples)
print "min:" min(samples) "max:" max(samples)
var mu = mean(samples)
print "std dev:" sqrt(mean((samples - mu) * (samples - mu)))

print ""
print "╔══════════════════════════════════════════╗"
print "║            Reference complete            ║"
print "╚══════════════════════════════════════════╝"
