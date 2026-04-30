# stdlib.flux — Flux standard library
#
# Loaded with `load("stdlib.flux")` near the top of a session or program.
# Everything here is written in pure Flux, using only core builtins. The
# point of this file is twofold:
#
#   1. Plug specific gaps in the core (no `pmin`, no vec-aware map, etc.)
#   2. Establish project-wide naming conventions and idioms before the
#      first domain library lands. If anything in here feels off, it's
#      cheaper to fix now than after dsp.flux uses it 200 times.
#
# Conventions used throughout:
#   - Functions that work on lists are named plainly  (`map`, `each`)
#   - Functions that work on vecs use the `v_` prefix (`v_map`, `v_each`)
#   - Predicates return scalar 0/1, not nil/non-nil
#   - "Mutating" stdlib functions are avoided — return a new value instead
#   - Most numeric helpers broadcast: scalars and vecs work the same way


# ════════════════════════════════════════════════════════════════════
#  § 1. Function combinators — pipeline, compose, curry, identity
# ════════════════════════════════════════════════════════════════════

# Identity. Trivial but actually useful as a default in higher-order code:
#   reduce(xs, f, identity(xs[0])) — start fold from a recognisable place
func identity(x) {
    "Return x unchanged."
    return x
}

# Constant function. Often clearer than `func() { return v }`.
func const_fn(v) {
    "Return a function that ignores its argument and returns v."
    return func(_) { return v }
}

# Compose two functions: (compose(f, g))(x) == f(g(x))
func compose(f, g) {
    "Right-to-left composition: compose(f, g)(x) == f(g(x))."
    return func(x) { return f(g(x)) }
}

# Threading: pipe a value through a list of unary functions, left to right.
# Reads naturally even with deep pipelines:
#   pipe(audio, list(normalize, lowpass, gain_db_5))
func pipe(x, fns) {
    "Apply each function in fns to the result of the previous, starting from x."
    return reduce(fns, func(acc, f) { return f(acc) }, x)
}

# Partial application of the first argument.
#   var add5 = partial1(add, 5)
#   add5(2) → 7
func partial1(f, a) {
    "Return a function that calls f with `a` prepended to the given args."
    return func(b) { return f(a, b) }
}

# Reverse the order of a 2-argument function. Useful for partial1 with the
# trailing arg fixed instead of the leading one.
func flip(f) {
    "Reverse argument order: flip(f)(a, b) == f(b, a)."
    return func(a, b) { return f(b, a) }
}


# ════════════════════════════════════════════════════════════════════
#  § 2. List operations the core doesn't ship
# ════════════════════════════════════════════════════════════════════

# Take the first n items. Negative or > len returns the whole list.
func take(xs, n) {
    "Return a list of the first n items (or all of them if n exceeds length)."
    if (n <= 0) { return list() }
    var k = len(xs)
    if (n > k) { n = k }
    return slice(xs, 0, n)
}

# Drop the first n items. Negative drops nothing; n > len returns empty.
func drop(xs, n) {
    "Return the list with the first n items removed."
    if (n <= 0) { return xs }
    var k = len(xs)
    if (n >= k) { return list() }
    return slice(xs, n, k)
}

# Split into (first-n, rest). Useful for windowed iteration.
func split_at(xs, n) {
    "Return a 2-element list (first n items, rest)."
    return list(take(xs, n), drop(xs, n))
}

# Pair each item with its 0-based index.
#   enumerate(list("a","b")) → ((0, "a"), (1, "b"))
func enumerate(xs) {
    "Pair each item with its index. Returns a list of (index, item) pairs."
    var out = list()
    var i = 0
    each(xs, func(x) { push(out, list(i, x))  i = i + 1 })
    return out
}

# Zip two lists into a list of pairs. Stops at the shorter one — no nil filling.
func zip(xs, ys) {
    "Pair items from two lists. Length is min(len(xs), len(ys))."
    var n = len(xs)
    var m = len(ys)
    if (m < n) { n = m }
    var out = list()
    for (var i = 0; i < n; i = i + 1) { push(out, list(xs[i], ys[i])) }
    return out
}

# Apply a 2-arg function to corresponding elements.
#   zip_with(list(1,2,3), list(10,20,30), func(a,b){return a+b})
#   → (11, 22, 33)
func zip_with(xs, ys, f) {
    "Element-wise binary combine; result length is min(len(xs), len(ys))."
    var n = len(xs)
    var m = len(ys)
    if (m < n) { n = m }
    var out = list()
    for (var i = 0; i < n; i = i + 1) { push(out, f(xs[i], ys[i])) }
    return out
}

# Group consecutive items into chunks of size n. Last chunk may be smaller.
#   chunk(list(1,2,3,4,5), 2) → ((1,2), (3,4), (5))
func chunk(xs, n) {
    "Split into consecutive chunks of size n; last chunk may be shorter."
    if (n <= 0) { error("chunk: n must be positive") }
    var out = list()
    var k = len(xs)
    var i = 0
    while (i < k) {
        var stop = i + n
        if (stop > k) { stop = k }
        push(out, slice(xs, i, stop))
        i = stop
    }
    return out
}

# Flatten one level of list-of-lists. Does NOT recurse (use flatten_deep for that).
func flatten(xss) {
    "Concatenate a list of lists into a single list (one level deep)."
    return reduce(xss, func(acc, xs) { return concat(acc, xs) }, list())
}

# Recursively flatten — any nested list becomes part of one flat list.
func flatten_deep(xs) {
    "Recursively flatten any nested list structure into one flat list."
    var out = list()
    each(xs, func(x) {
        if (type(x) == "list") { each(flatten_deep(x), func(y) { push(out, y) }) }
        else                   { push(out, x) }
    })
    return out
}

# First-n by predicate. Returns nil if no item matches.
func find_first(xs, pred) {
    "Return the first item for which pred(x) is truthy, or nil."
    var found = nil
    var done = 0
    each(xs, func(x) { if (not done and pred(x)) { found = x  done = 1 } })
    return found
}

# Position of the first matching item, or -1.
func index_of(xs, pred) {
    "Return the index of the first item for which pred(x) is truthy, or -1."
    var idx = -1
    var i = 0
    each(xs, func(x) { if (idx < 0 and pred(x)) { idx = i }  i = i + 1 })
    return idx
}

# All / any, with short-circuit-like semantics (we still call pred on every
# element — flux doesn't have early `each` exit — but the answer is correct).
func all(xs, pred) {
    "1 if pred is truthy for every item; empty list returns 1 (vacuous truth)."
    var ok = 1
    each(xs, func(x) { if (not pred(x)) { ok = 0 } })
    return ok
}
func any(xs, pred) {
    "1 if pred is truthy for at least one item; empty list returns 0."
    var ok = 0
    each(xs, func(x) { if (pred(x)) { ok = 1 } })
    return ok
}

# Count matching items.
func count(xs, pred) {
    "Number of items in xs for which pred(x) is truthy."
    var n = 0
    each(xs, func(x) { if (pred(x)) { n = n + 1 } })
    return n
}

# Build a list by repeating a value.
func repeat(v, n) {
    "Return a list of n copies of v."
    var out = list()
    for (var i = 0; i < n; i = i + 1) { push(out, v) }
    return out
}

# Sort a list by a key function. Returns a new list (input unchanged).
# Implementation: convert to (key, value) pairs, extract keys to a vec,
# sort vec, then rebuild — leans on the core's `sort` (which is vec-only)
# and so works only for numeric keys. Most realistic use cases are numeric.
func sort_by(xs, key) {
    "Return xs sorted by ascending numeric key. Stable for equal keys."
    var n = len(xs)
    if (n <= 1) { return xs }
    # Build a vec of indices [0..n) and sort it by key(xs[i]).
    # Insertion sort: O(n^2) but fine for stdlib purposes — DSP code that
    # cares about sort speed should use the core's `sort` on a vec directly.
    var idx = list()
    for (var i = 0; i < n; i = i + 1) { push(idx, i) }
    for (var i = 1; i < n; i = i + 1) {
        var j = i
        while (j > 0 and key(xs[idx[j-1]]) > key(xs[idx[j]])) {
            var t = idx[j]
            idx[j] = idx[j-1]
            idx[j-1] = t
            j = j - 1
        }
    }
    var out = list()
    for (var i = 0; i < n; i = i + 1) { push(out, xs[idx[i]]) }
    return out
}

# Remove duplicates while preserving first occurrence order.
# Comparison uses ==, which is structural for lists/dicts/vecs.
func unique(xs) {
    "Return a list with duplicates removed, preserving first-occurrence order."
    var out = list()
    each(xs, func(x) {
        if (not any(out, func(y) { return x == y })) { push(out, x) }
    })
    return out
}


# ════════════════════════════════════════════════════════════════════
#  § 3. Vec-flavored higher-order helpers
# ════════════════════════════════════════════════════════════════════
#
# The core's map/filter/reduce work on lists. For numerical work on vecs,
# these v_* variants are the right choice. They keep results as vecs so
# subsequent broadcast arithmetic works without conversions.

func v_map(v, f) {
    "Apply f to each element; return a new vec of the same length."
    var n = len(v)
    var out = zeros(n)
    for (var i = 0; i < n; i = i + 1) { out[i] = f(v[i]) }
    return out
}

func v_each(v, f) {
    "Apply f to each element for side effects; return nil."
    var n = len(v)
    for (var i = 0; i < n; i = i + 1) { f(v[i]) }
    return nil
}

func v_filter(v, pred) {
    "Return a new vec containing only the elements for which pred is truthy."
    # We don't know the result size in advance; build a list and convert.
    var picked = list()
    var n = len(v)
    for (var i = 0; i < n; i = i + 1) {
        if (pred(v[i])) { push(picked, v[i]) }
    }
    return vec(picked)
}

func v_reduce(v, f, init) {
    "Left fold: accumulate with f(acc, x_i) starting from init."
    var acc = init
    var n = len(v)
    for (var i = 0; i < n; i = i + 1) { acc = f(acc, v[i]) }
    return acc
}


# ════════════════════════════════════════════════════════════════════
#  § 4. Numeric helpers — broadcast over scalars and vecs alike
# ════════════════════════════════════════════════════════════════════
#
# These are the building blocks that show up in nearly every signal-
# processing or analysis script. They lean on the core's broadcasting,
# so a single implementation handles scalars, vecs, and (where the core
# supports it) buffers.

# Pairwise minimum and maximum of two values.
# Implementation uses the identity:
#   pmin(a, b) = (a + b - abs(a - b)) / 2
#   pmax(a, b) = (a + b + abs(a - b)) / 2
# This works element-wise on vecs by broadcasting — a real win over a
# Flux-level loop.
func pmin(a, b) {
    "Element-wise minimum of two values (scalar or vec, broadcasts)."
    return (a + b - abs(a - b)) / 2
}
func pmax(a, b) {
    "Element-wise maximum of two values (scalar or vec, broadcasts)."
    return (a + b + abs(a - b)) / 2
}

# Sign function: -1, 0, or +1 (broadcasts).
func sign(x) {
    "Element-wise sign: -1, 0, or +1. Broadcasts."
    return pmax(pmin(x, 1), -1) * (abs(x) > 0)
}

# Clamp a value to [lo, hi]. Broadcasts.
func clamp(x, lo, hi) {
    "Clamp to [lo, hi]. Broadcasts on scalar/vec."
    return pmin(pmax(x, lo), hi)
}

# Linear interpolate between a and b at fraction t (typically in [0,1]).
func lerp(a, b, t) {
    "Linear interpolate: a + (b - a) * t."
    return a + (b - a) * t
}

# Inverse of lerp: where in [a, b] does x sit? Returns 0 if x==a, 1 if x==b.
# Will return values outside [0, 1] for x outside [a, b].
func unlerp(a, b, x) {
    "Inverse lerp: returns t such that lerp(a, b, t) == x."
    return (x - a) / (b - a)
}

# Remap x from [a1,b1] into [a2,b2]. Composition of unlerp + lerp.
func remap(x, a1, b1, a2, b2) {
    "Map x from range [a1,b1] linearly into [a2,b2]."
    return lerp(a2, b2, unlerp(a1, b1, x))
}

# Modulo that always returns a non-negative result (mathematical mod).
# The core's % follows C semantics where -1 % 3 is -1.
func mod_pos(x, m) {
    "Mathematical modulo: result is in [0, m) for positive m."
    var r = x % m
    return r + m * (r < 0)
}


# ════════════════════════════════════════════════════════════════════
#  § 5. Statistics on vecs
# ════════════════════════════════════════════════════════════════════

func variance(v) {
    "Population variance (n-divisor, not n-1)."
    var n = len(v)
    if (n == 0) { return 0 }
    var mu = mean(v)
    var d = v - mu
    return sum(d * d) / n
}

func stddev(v) {
    "Population standard deviation (n-divisor)."
    return sqrt(variance(v))
}

# Z-score normalise: zero mean, unit variance.
func standardise(v) {
    "Subtract mean, divide by stddev. Falls back to subtracted-mean if stddev=0."
    var s = stddev(v)
    if (s == 0) { return v - mean(v) }
    return (v - mean(v)) / s
}

# Min-max normalise to a target range (default [0, 1]).
func normalise(v, lo, hi) {
    "Linearly rescale v so that its min maps to lo and its max to hi."
    var vmin = min(v)
    var vmax = max(v)
    if (vmax == vmin) { return zeros(len(v)) + lo }
    return lerp(lo, hi, (v - vmin) / (vmax - vmin))
}

# Cumulative sum.
func cumsum(v) {
    "Running sum: out[i] = v[0] + v[1] + ... + v[i]."
    var n = len(v)
    var out = zeros(n)
    if (n == 0) { return out }
    out[0] = v[0]
    for (var i = 1; i < n; i = i + 1) { out[i] = out[i-1] + v[i] }
    return out
}

# Pairwise differences: out[i] = v[i+1] - v[i]. Length is n - 1.
func diff(v) {
    "Pairwise differences. Output length is len(v) - 1."
    var n = len(v)
    if (n <= 1) { return zeros(0) }
    var out = zeros(n - 1)
    for (var i = 0; i < n - 1; i = i + 1) { out[i] = v[i+1] - v[i] }
    return out
}

# Dot product. Errors if lengths differ.
func dot(a, b) {
    "Inner product of two same-sized vecs."
    if (len(a) != len(b)) { error("dot: size mismatch") }
    return sum(a * b)
}


# ════════════════════════════════════════════════════════════════════
#  § 6. Dict helpers
# ════════════════════════════════════════════════════════════════════

# Apply a function to every value, keeping keys.
func dict_map(d, f) {
    "Apply f to every value; return a new dict with the same keys."
    var out = {}
    each(keys(d), func(k) { out[k] = f(d[k]) })
    return out
}

# Filter to only those (k,v) pairs for which pred(k, v) is truthy.
func dict_filter(d, pred) {
    "Return a dict with only the (k, v) pairs satisfying pred(k, v)."
    var out = {}
    each(keys(d), func(k) {
        if (pred(k, d[k])) { out[k] = d[k] }
    })
    return out
}

# Merge two dicts. Right wins on key conflicts (same as core `concat`).
# Provided as a name that reads more clearly in this context.
func merge(a, b) {
    "Merge two dicts; right wins on key collisions."
    return concat(a, b)
}

# Build a dict from a list of [key, value] pair-lists.
# Equivalent to the core's `dict(pairs)` with input validation hint.
func from_pairs(pairs) {
    "Build a dict from a list of [key, value] 2-element lists."
    return dict(pairs)
}

# Inverse of from_pairs — list of [key, value] pairs in sorted-key order.
func to_pairs(d) {
    "Return [key, value] pairs in sorted-key order."
    var out = list()
    each(keys(d), func(k) { push(out, list(k, d[k])) })
    return out
}


# ════════════════════════════════════════════════════════════════════
#  § 7. String helpers
# ════════════════════════════════════════════════════════════════════

# Repeat a string n times.
func str_repeat(s, n) {
    "Concatenate n copies of s."
    var out = ""
    for (var i = 0; i < n; i = i + 1) { out = concat(out, s) }
    return out
}

# Pad a string on the left to a target length.
func pad_left(s, target_len, pad_char) {
    "Left-pad s with pad_char until its length reaches target_len."
    var k = len(s)
    if (k >= target_len) { return s }
    return concat(str_repeat(pad_char, target_len - k), s)
}

func pad_right(s, target_len, pad_char) {
    "Right-pad s with pad_char until its length reaches target_len."
    var k = len(s)
    if (k >= target_len) { return s }
    return concat(s, str_repeat(pad_char, target_len - k))
}

# Does s start / end with prefix / suffix? Predicates returning 0/1.
func starts_with(s, prefix) {
    "1 if s begins with prefix, 0 otherwise."
    if (len(prefix) > len(s)) { return 0 }
    return substr(s, 0, len(prefix)) == prefix
}
func ends_with(s, suffix) {
    "1 if s ends with suffix, 0 otherwise."
    var ks = len(s)
    var kp = len(suffix)
    if (kp > ks) { return 0 }
    return substr(s, ks - kp, kp) == suffix
}

# Does s contain needle anywhere? `find` returns -1 on miss.
func contains(s, needle) {
    "1 if needle appears anywhere in s, 0 otherwise."
    return find(s, needle) >= 0
}


# ════════════════════════════════════════════════════════════════════
#  § 8. Predicates (small but used everywhere)
# ════════════════════════════════════════════════════════════════════

func is_nil(x)    { "1 if x is nil."     return type(x) == "nil"    }
func is_scalar(x) { "1 if x is scalar."  return type(x) == "scalar" }
func is_vec(x)    { "1 if x is a vec."   return type(x) == "vec"    }
func is_string(x) { "1 if x is string."  return type(x) == "string" }
func is_list(x)   { "1 if x is a list."  return type(x) == "list"   }
func is_dict(x)   { "1 if x is a dict."  return type(x) == "dict"   }
func is_buffer(x) { "1 if x is buffer."  return type(x) == "buffer" }
func is_func(x)   { "1 if x is callable." return type(x) == "func"  }
func is_number(x) {
    "1 if x is scalar or vec (anything you can do arithmetic on)."
    var t = type(x)
    return t == "scalar" or t == "vec"
}


# ════════════════════════════════════════════════════════════════════
#  § 9. Misc utilities — last, first, etc.
# ════════════════════════════════════════════════════════════════════

func first(xs) {
    "First element of a list, vec, or string. Errors if empty."
    if (len(xs) == 0) { error("first: empty") }
    return xs[0]
}

func last(xs) {
    "Last element of a list, vec, or string. Errors if empty."
    if (len(xs) == 0) { error("last: empty") }
    return xs[-1]
}

# A more readable test alternative when you want the predicate-as-statement.
# `unless(cond) { ... }` doesn't exist as syntax; this is a helper.
func unless_then(cond, action) {
    "Run action() if cond is falsy. Returns action's result, or nil."
    if (not cond) { return action() }
    return nil
}

# Sentinel for missing-or-default argument patterns. Compares with ==.
var stdlib_missing = "__stdlib_missing__"

func defaulting(value, fallback) {
    "Return value, unless it equals stdlib_missing or nil, in which case fallback."
    if (value == stdlib_missing or value == nil) { return fallback }
    return value
}
