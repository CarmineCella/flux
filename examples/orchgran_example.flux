# orchestral granulator example for Flux
# adjust the database path to your local SOL folder

var db = db_load("/Users/n4/Projects/Media/Datasets/StaticSOL")

var orchestra = "[Fl] [Fl] [Ob] [Ob] [ClBb] [ClBb] [Bn] [Bn] [Hn] [Hn] [TpC] [TpC] [Tbn] [Tbn] [BTb] [Vn] [Vn] [Va] [Va] [Vc] [Vc] [Cb] [Cb]"

var ev = orchgran(db, orchestra, 0, 30,
                  .5, .1, 0, 0,
                  3, 6, 0, 0,
                  3, 5, 0, 0,
                  "[A E C G#] [E #G B D] [A E C G#]",
                  "[p pp] [mf] [f ff] [mf] [pp ppp]",
                  "[ord]")

var snd = orchgransnd(ev, 44100)
wavwrite(snd, 44100, "test.wav", 1)