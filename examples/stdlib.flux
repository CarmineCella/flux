# ══════════════════════════════════════════════════════════════════════
# stdlib.flux — standard library for Flux
# load ("stdlib.flux")
# ══════════════════════════════════════════════════════════════════════

# ── list access ────────────────────────────────────────────────────────

func head (l) {
    return l[0]
}

func last (l) {
    return l[-1]
}

func tail (l) {
    var out = list()
    var i = 1
    while (i < len(l)) {
        out = push(out, l[i])
        i = i + 1
    }
    return out
}

# pop returns a list: (remaining_list, removed_element)
func pop (l) {
    var elem = l[-1]
    var out = list()
    var i = 0
    while (i < len(l) - 1) {
        out = push(out, l[i])
        i = i + 1
    }
    return list(out, elem)
}

func slice (l, start, stop) {
    var out = list()
    var i = start
    while (i < stop) {
        out = push(out, l[i])
        i = i + 1
    }
    return out
}

# also works for vec via indexing
func slice_vec (v, start, stop) {
    var n = stop - start
    var out = zeros(n)
    var i = 0
    while (i < n) {
        out = out + 0  # force copy (not needed, just clarity)
        i = i + 1
    }
    # rebuild properly
    var r = list()
    var j = start
    while (j < stop) {
        r = push(r, v[j])
        j = j + 1
    }
    return vec(r)
}

func insert (l, idx, val) {
    var out = list()
    var i = 0
    while (i < len(l)) {
        if (i == idx) {
            out = push(out, val)
        }
        out = push(out, l[i])
        i = i + 1
    }
    if (idx >= len(l)) {
        out = push(out, val)
    }
    return out
}

func remove (l, idx) {
    var out = list()
    var i = 0
    while (i < len(l)) {
        if (i != idx) {
            out = push(out, l[i])
        }
        i = i + 1
    }
    return out
}

# ── list construction ──────────────────────────────────────────────────

func flatten (l) {
    var out = list()
    var i = 0
    while (i < len(l)) {
        if (type(l[i]) == "list") {
            var j = 0
            while (j < len(l[i])) {
                out = push(out, l[i][j])
                j = j + 1
            }
        } else {
            out = push(out, l[i])
        }
        i = i + 1
    }
    return out
}

func repeat (val, n) {
    var out = list()
    var i = 0
    while (i < n) {
        out = push(out, val)
        i = i + 1
    }
    return out
}

func zip (a, b) {
    var na = len(a)
    var nb = len(b)
    var n = na
    if (nb < na) { n = nb }
    var out = list()
    var i = 0
    while (i < n) {
        out = push(out, list(a[i], b[i]))
        i = i + 1
    }
    return out
}

func enumerate (l) {
    var out = list()
    var i = 0
    while (i < len(l)) {
        out = push(out, list(i, l[i]))
        i = i + 1
    }
    return out
}

func range_list (start, stop) {
    var out = list()
    var i = start
    while (i < stop) {
        out = push(out, i)
        i = i + 1
    }
    return out
}

# ── searching and predicates ───────────────────────────────────────────

func contains (collection, value) {
    if (type(collection) == "string") {
        return find(collection, value) >= 0
    }
    # list
    var i = 0
    while (i < len(collection)) {
        if (str(collection[i]) == str(value)) {
            return true
        }
        i = i + 1
    }
    return false
}

func index_of (l, value) {
    var i = 0
    while (i < len(l)) {
        if (str(l[i]) == str(value)) {
            return i
        }
        i = i + 1
    }
    return -1
}

func any (l, f) {
    var i = 0
    while (i < len(l)) {
        if (f(l[i])) {
            return true
        }
        i = i + 1
    }
    return false
}

func all (l, f) {
    var i = 0
    while (i < len(l)) {
        if (not f(l[i])) {
            return false
        }
        i = i + 1
    }
    return true
}

func count (l, f) {
    var n = 0
    var i = 0
    while (i < len(l)) {
        if (f(l[i])) {
            n = n + 1
        }
        i = i + 1
    }
    return n
}

# ── string utilities ───────────────────────────────────────────────────

func starts_with (s, prefix) {
    if (len(s) < len(prefix)) {
        return false
    }
    return substr(s, 0, len(prefix)) == prefix
}

func ends_with (s, suffix) {
    if (len(s) < len(suffix)) {
        return false
    }
    return substr(s, len(s) - len(suffix), len(suffix)) == suffix
}

func lpad (s, n, ch) {
    var out = s
    while (len(out) < n) {
        out = concat(ch, out)
    }
    return out
}

func rpad (s, n, ch) {
    var out = s
    while (len(out) < n) {
        out = concat(out, ch)
    }
    return out
}

func char_at (s, i) {
    return s[i]
}

func chars (s) {
    var out = list()
    var i = 0
    while (i < len(s)) {
        out = push(out, s[i])
        i = i + 1
    }
    return out
}

# ── regex ──────────────────────────────────────────────────────────────

func match_all (s, pattern) {
    var results = list()
    var remaining = s
    var m = match(remaining, pattern)
    while (m != nil) {
        var matched = m[0]
        results = push(results, matched)
        var pos = find(remaining, matched)
        var next_start = pos + len(matched)
        if (next_start >= len(remaining)) {
            break
        }
        remaining = substr(remaining, next_start, len(remaining) - next_start)
        m = match(remaining, pattern)
    }
    return results
}

# ── math utilities ─────────────────────────────────────────────────────

func clamp (x, lo, hi) {
    if (x < lo) { return lo }
    if (x > hi) { return hi }
    return x
}

func lerp (a, b, t) {
    return a + (b - a) * t
}

func sign (x) {
    if (x > 0) { return 1 }
    if (x < 0) { return -1 }
    return 0
}

func deg2rad (d) {
    return d * pi / 180
}

func rad2deg (r) {
    return r * 180 / pi
}

# ── functional combinators ─────────────────────────────────────────────

func compose (f, g) {
    func composed (x) {
        return f(g(x))
    }
    return composed
}

func apply (f, args) {
    # apply a function to a list of arguments
    # works for 0-4 args (flux has no splat operator)
    var n = len(args)
    if (n == 0) { return f() }
    if (n == 1) { return f(args[0]) }
    if (n == 2) { return f(args[0], args[1]) }
    if (n == 3) { return f(args[0], args[1], args[2]) }
    if (n == 4) { return f(args[0], args[1], args[2], args[3]) }
    error("apply: too many arguments (max 4)")
}

func partial (f, first) {
    func bound (x) {
        return f(first, x)
    }
    return bound
}

func twice (f) {
    func applied (x) {
        return f(f(x))
    }
    return applied
}

# ── sorting (insertion sort for lists) ─────────────────────────────────

func sort_list (l, cmp) {
    # cmp(a, b) should return true if a < b
    var out = list()
    var i = 0
    while (i < len(l)) {
        out = push(out, l[i])
        var j = len(out) - 1
        while (j > 0) {
            if (cmp(out[j], out[j - 1])) {
                # swap
                var tmp = out[j - 1]
                out = _list_set(out, j - 1, out[j])
                out = _list_set(out, j, tmp)
            }
            j = j - 1
        }
        i = i + 1
    }
    return out
}

# helper: return a new list with l[i] = val
func _list_set (l, idx, val) {
    var out = list()
    var i = 0
    while (i < len(l)) {
        if (i == idx) {
            out = push(out, val)
        } else {
            out = push(out, l[i])
        }
        i = i + 1
    }
    return out
}

func sort_by (l, key) {
    return sort_list(l, func (a, b) { return key(a) < key(b) })
}

# ── dictionary-like (list of pairs) ───────────────────────────────────

func dict_new () {
    return list()
}

func dict_set (d, k, v) {
    var out = list()
    var found = false
    var i = 0
    while (i < len(d)) {
        if (str(d[i][0]) == str(k)) {
            out = push(out, list(k, v))
            found = true
        } else {
            out = push(out, d[i])
        }
        i = i + 1
    }
    if (not found) {
        out = push(out, list(k, v))
    }
    return out
}

func dict_get (d, k, default_val) {
    var i = 0
    while (i < len(d)) {
        if (str(d[i][0]) == str(k)) {
            return d[i][1]
        }
        i = i + 1
    }
    return default_val
}

func dict_has (d, k) {
    var i = 0
    while (i < len(d)) {
        if (str(d[i][0]) == str(k)) {
            return true
        }
        i = i + 1
    }
    return false
}

func dict_keys (d) {
    return map(d, func (pair) { return pair[0] })
}

func dict_values (d) {
    return map(d, func (pair) { return pair[1] })
}
