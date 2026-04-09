# db_example.flux — examples for the sample database layer

load ("sampsynth.flux")

var db = db_load("/Users/n4/Projects/Media/Datasets/StaticSOL")

print "number of entries:"
print len(db)
print ""

print "all instruments:"
print db_instruments(db)
print ""

print "all playing styles:"
print db_styles(db)
print ""

print "all dynamics:"
print db_dynamics(db)
print ""

print "all pitches:"
print db_pitches(db)
print ""

print "all oboe entries:"
var q_ob = db_query(db, "Ob")
print len(q_ob)
print ""

print "all oboe ordinario entries:"
var q_ob_ord = db_query(db, "Ob.*ord")
print len(q_ob_ord)
print ""

print "all ff entries:"
var q_ff = db_query(db, "ff")
print len(q_ff)
print ""

print "all pizzicato violin entries:"
var q_vn_pizz = db_query(db, "Vn.*pizz")
print len(q_vn_pizz)
print ""

print "all oboe entries at A#3:"
var q_ob_as3 = db_query(db, "Ob.*A#3")
print q_ob_as3
print ""

print "all clarinet OR bassoon staccato entries:"
var q_reed_stacc = db_query(db, "(Cl|Bn).*stacc")
print len(q_reed_stacc)
print ""

print "all entries matching instrument + style + dynamic:"
var q_combo = db_query(db, "Ob.*ord.*ff")
print q_combo
print ""

print "instruments in the ff subset:"
print db_instruments_query(db, "ff")
print ""

print "playing styles available for oboe:"
print db_styles_query(db, "Ob")
print ""

print "dynamics available for violin pizzicato:"
print db_dynamics_query(db, "Vn.*pizz")
print ""

print "pitches available for clarinet staccato:"
print db_pitches_query(db, "Cl.*stacc")
print ""

if (len(q_ob_ord) > 0) {
    var xs = db_query(db, "Ob.*A4.*ff")
    if (len(xs) > 0) {
        var sig = db_pick(xs[0], 44100)
        print "loaded one oboe ordinario sample, length:"
        print len(sig)
        wavwrite(sig, 44100, "db_pick_example.wav")
    } else {
        print "no oboe ordinario sample found"
    }
} else {
    print "empty oboe ordinario query"
}
