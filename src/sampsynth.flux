# ══════════════════════════════════════════════════════════════════════
# sampsynth.flux — convenience wrappers for sample synthesis
# load ("sampsynth.flux")
# ══════════════════════════════════════════════════════════════════════

load ("dsp.flux")

# ── db query helpers ──────────────────────────────────────────────────

func db_instruments_query (db, pattern) {
    return db_instruments(db_query(db, pattern))
}

func db_styles_query (db, pattern) {
    return db_styles(db_query(db, pattern))
}

func db_dynamics_query (db, pattern) {
    return db_dynamics(db_query(db, pattern))
}

func db_pitches_query (db, pattern) {
    return db_pitches(db_query(db, pattern))
}

# ── convenience: load first match ─────────────────────────────────────

func db_pick_first (db, pattern, sr) {
    var matches = db_query(db, pattern)
    if (len(matches) == 0) {
        print "db_pick_first: no match for" pattern
        return zeros(1)
    }
    return db_pick(matches[0], sr)
}

# ── orchgran output helpers ───────────────────────────────────────────

func orchgran_normalize (result) {
    return normalize(orchgran_render(result))
}

func orchgran_write (result, sr, path) {
    var sig = normalize(orchgran_render(result))
    wavwrite(sig, sr, path)
    return sig
}
