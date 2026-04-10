// sampsynth.h — Flux sample-synthesis library
//
// Call register_sampsynth(interp) from main after constructing the Interpreter.
//
// Primitives: db_load, db_query, db_pick, db_field,
//             db_instruments, db_styles, db_dynamics, db_pitches,
//             orchgran, orchgransnd

#ifndef SAMPSYNTH_H
#define SAMPSYNTH_H

#include "flux.h"
#include "dsp.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <random>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace flux {

// ── internal string helpers (C++ level only) ─────────────────────────
static std::string to_lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}
static bool is_wav(const fs::path& p) {
    return p.has_extension() && to_lower(p.extension().string()) == ".wav";
}
static std::vector<std::string> name_split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) { out.push_back(cur); cur.clear(); }
        else          cur.push_back(c);
    }
    out.push_back(cur);
    return out;
}
static std::string trim_copy(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) ++a;
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}
static std::string join_strings(const std::vector<std::string>& xs,
                                const std::string& sep = " ") {
    std::string out;
    for (size_t i = 0; i < xs.size(); ++i) {
        if (i) out += sep;
        out += xs[i];
    }
    return out;
}
static std::vector<std::string> split_ws(const std::string& s) {
    std::istringstream iss(s);
    std::vector<std::string> out;
    std::string tok;
    while (iss >> tok) out.push_back(tok);
    return out;
}
static std::vector<std::vector<std::string>> parse_groups(const std::string& spec) {
    std::vector<std::vector<std::string>> out;
    std::string cur;
    bool in = false;
    for (char c : spec) {
        if (c == '[') {
            in = true;
            cur.clear();
        } else if (c == ']') {
            if (in) out.push_back(split_ws(cur));
            in = false;
            cur.clear();
        } else if (in) {
            cur.push_back(c);
        }
    }
    return out;
}

// ── pitch helpers ────────────────────────────────────────────────────
static int pitch_class(const std::string& raw) {
    std::string s = to_lower(raw);
    if (s == "c")               return 0;
    if (s == "c#" || s == "db") return 1;
    if (s == "d")               return 2;
    if (s == "d#" || s == "eb") return 3;
    if (s == "e")               return 4;
    if (s == "f")               return 5;
    if (s == "f#" || s == "gb") return 6;
    if (s == "g")               return 7;
    if (s == "g#" || s == "ab") return 8;
    if (s == "a")               return 9;
    if (s == "a#" || s == "bb") return 10;
    if (s == "b" || s == "cb")  return 11;
    return -1;
}
static int pitch_to_midi(const std::string& pitch) {
    if (pitch.empty()) return -1;
    std::string pc;
    pc.push_back(pitch[0]);
    size_t i = 1;
    if (i < pitch.size() && (pitch[i] == '#' || pitch[i] == 'b')) {
        pc.push_back(pitch[i]); ++i;
    }
    std::string oct;
    while (i < pitch.size()) { oct.push_back(pitch[i]); ++i; }
    if (oct.empty()) return -1;
    int pc_val = pitch_class(pc);
    if (pc_val < 0) return -1;
    return 12 * (std::stoi(oct) + 1) + pc_val;
}
static std::string canonical_pc(std::string s) {
    s = trim_copy(s);
    if (s.empty()) return s;
    if (s.size() == 2 && s[0] == '#') s = std::string(1, s[1]) + "#";
    if (s.size() == 2 && s[0] == 'b') s = std::string(1, s[1]) + "b";
    s[0] = (char)std::toupper((unsigned char)s[0]);
    if (s.size() > 1) s[1] = (char)std::tolower((unsigned char)s[1]);
    return s;
}
static std::string pitch_pc_only(const std::string& pitch) {
    if (pitch.empty()) return "";
    std::string pc;
    pc.push_back((char)std::toupper((unsigned char)pitch[0]));
    size_t i = 1;
    if (i < pitch.size() && (pitch[i] == '#' || pitch[i] == 'b')) {
        pc.push_back(pitch[i]);
    }
    return pc;
}
static int pitch_octave_only(const std::string& pitch) {
    if (pitch.empty()) return 0;
    size_t i = 1;
    if (i < pitch.size() && (pitch[i] == '#' || pitch[i] == 'b')) ++i;
    if (i >= pitch.size()) return 0;
    return std::stoi(pitch.substr(i));
}

// ── SOL filename parser ──────────────────────────────────────────────
static bool parse_db(const std::string& stem,
                     std::string& instrument, std::string& articulation,
                     std::string& pitch, std::string& dynamic) {
    auto parts = name_split(stem, '-');
    if (parts.size() < 4) return false;
    instrument = parts[0]; articulation = parts[1];
    pitch = parts[2]; dynamic = parts[3];
    return !(instrument.empty() || articulation.empty() ||
             pitch.empty() || dynamic.empty());
}

// ── DB entry: List of 2-element Lists ────────────────────────────────
static Value kv(const std::string& key, Value val) {
    return Value(List{Value(Str(key)), std::move(val)});
}
static Value db_make_entry(const std::string& family,
                           const std::string& instrument,
                           const std::string& articulation,
                           const std::string& pitch, int midi,
                           const std::string& dynamic,
                           const std::string& path) {
    List e;
    e.push_back(kv("family",       Value(Str(family))));
    e.push_back(kv("instrument",   Value(Str(instrument))));
    e.push_back(kv("articulation", Value(Str(articulation))));
    e.push_back(kv("pitch",        Value(Str(pitch))));
    e.push_back(kv("midi",         Value((double)midi)));
    e.push_back(kv("dynamic",      Value(Str(dynamic))));
    e.push_back(kv("path",         Value(Str(path))));
    return Value(std::move(e));
}

static bool db_entry_has(const Value& entry, const std::string& key) {
    if (!entry.is_list()) return false;
    for (auto& item : entry.as_list()) {
        if (!item.is_list()) continue;
        auto& pair = item.as_list();
        if (pair.size() == 2 && pair[0].is_str() && pair[0].as_str() == key)
            return true;
    }
    return false;
}
static const Value& db_entry_get(const Value& entry, const std::string& key,
                                 const std::string& f, int ln) {
    for (auto& item : entry.as_list()) {
        if (!item.is_list()) continue;
        auto& pair = item.as_list();
        if (pair.size() == 2 && pair[0].is_str() && pair[0].as_str() == key)
            return pair[1];
    }
    err(f, ln, "db entry missing key: " + key);
    throw std::logic_error("unreachable");
}
static std::string db_entry_str(const Value& e, const std::string& key,
                                const std::string& f, int ln) {
    auto& v = db_entry_get(e, key, f, ln);
    if (v.is_str()) return v.as_str();
    return std::to_string(v.scalar());
}
static double db_entry_num(const Value& e, const std::string& key,
                           const std::string& f, int ln) {
    auto& v = db_entry_get(e, key, f, ln);
    if (!v.is_vec()) err(f, ln, "db entry field is not numeric: " + key);
    return v.scalar();
}
static std::string db_entry_repr(const Value& entry) {
    if (!entry.is_list()) return entry.repr();
    std::string r;
    for (auto& item : entry.as_list()) {
        if (!item.is_list() || item.as_list().size() != 2) continue;
        auto& p = item.as_list();
        r += p[0].repr() + ":" + p[1].repr() + " ";
    }
    return r;
}

// ── WAV I/O (internal — reads mono, resamples) ──────────────────────
static Vec read_wav_mono(const std::string& path, int target_sr,
                         const std::string& f, int ln) {
    std::ifstream fi(path, std::ios::binary);
    if (!fi) err(f, ln, "cannot open wav: " + path);
    char id[4]; fi.read(id, 4);
    if (std::memcmp(id, "RIFF", 4)) err(f, ln, "not RIFF: " + path);
    auto r32 = [&]() -> uint32_t {
        uint8_t a=fi.get(),b=fi.get(),c=fi.get(),d=fi.get();
        return (uint32_t)(a|(b<<8)|(c<<16)|(d<<24));
    };
    auto r16 = [&]() -> uint16_t {
        uint8_t a=fi.get(),b=fi.get();
        return (uint16_t)(a|(b<<8));
    };
    r32(); fi.read(id,4);
    if (std::memcmp(id,"WAVE",4)) err(f,ln,"not WAVE: "+path);
    int sr=44100,nch=1,bits=16;
    Vec sig; bool got_data=false;
    while (fi) {
        char chunk[4]; fi.read(chunk,4);
        if (!fi) break;
        uint32_t sz=r32();
        if (!std::memcmp(chunk,"fmt ",4)) {
            r16(); nch=(int)r16(); sr=(int)r32();
            r32(); r16(); bits=(int)r16();
            if (sz>16) fi.seekg(sz-16,std::ios::cur);
        } else if (!std::memcmp(chunk,"data",4)) {
            got_data=true;
            int bps=bits/8, ns=(int)(sz/(uint32_t)bps);
            sig.resize(ns);
            if (bits==16) {
                for (int i=0;i<ns;++i){
                    uint8_t lo=fi.get(),hi=fi.get();
                    sig[i]=(int16_t)(lo|(hi<<8))/32768.0;
                }
            } else if (bits==24) {
                for (int i=0;i<ns;++i){
                    uint8_t a=fi.get(),b=fi.get(),c=fi.get();
                    int32_t s=(int32_t)(a|(b<<8)|(c<<16));
                    if(s&0x800000) s|=(int32_t)0xFF000000;
                    sig[i]=s/8388608.0;
                }
            } else if (bits==8) {
                for (int i=0;i<ns;++i) sig[i]=((uint8_t)fi.get()-128)/128.0;
            } else err(f,ln,"unsupported bit depth in "+path);
            if (sz&1u) fi.get();
            break;
        } else fi.seekg(sz+(sz&1u),std::ios::cur);
    }
    if (!got_data) err(f,ln,"no data chunk in "+path);
    if (nch>1) {
        size_t frames=sig.size()/(size_t)nch;
        Vec mono(frames);
        for (size_t i=0;i<frames;++i){
            double acc=0;
            for (int ch=0;ch<nch;++ch) acc+=sig[i*nch+ch];
            mono[i]=acc/nch;
        }
        sig=std::move(mono);
    }
    if (target_sr>0 && sr>0 && sr!=target_sr)
        sig=resample_fd(sig,(double)target_sr/(double)sr);
    return sig;
}
static Vec extract_signal(const Value& v, int target_sr,
                          const std::string& f, int ln) {
    if (v.is_str()) return read_wav_mono(v.as_str(), target_sr, f, ln);
    if (v.is_list() && db_entry_has(v, "path"))
        return read_wav_mono(db_entry_str(v,"path",f,ln), target_sr, f, ln);
    err(f,ln,"db_pick: expected db entry or path string");
    return {};
}

// ── unique field values ──────────────────────────────────────────────
static List unique_field(const List& db, const std::string& key) {
    std::vector<std::string> names;
    for (auto& e : db) {
        if (!e.is_list()) continue;
        for (auto& item : e.as_list()) {
            if (!item.is_list()) continue;
            auto& pair = item.as_list();
            if (pair.size()==2 && pair[0].is_str() &&
                pair[0].as_str()==key && pair[1].is_str()) {
                names.push_back(pair[1].as_str());
                break;
            }
        }
    }
    std::sort(names.begin(),names.end());
    names.erase(std::unique(names.begin(),names.end()),names.end());
    List out;
    for (auto& s : names) out.push_back(Value(Str(s)));
    return out;
}

// ── granulator helpers ───────────────────────────────────────────────
static double lerp_num(double a, double b, double t) {
    return a + (b - a) * t;
}
static size_t phase_index(size_t n, double phase) {
    if (n == 0) return 0;
    long i = (long)std::floor(phase * (double)n);
    if (i < 0) i = 0;
    if ((size_t)i >= n) i = (long)n - 1;
    return (size_t)i;
}
static bool contains_str(const std::vector<std::string>& xs, const std::string& x) {
    for (auto& s : xs) if (s == x) return true;
    return false;
}
static std::vector<std::string> normalize_pitch_classes(const std::vector<std::string>& xs) {
    std::vector<std::string> out;
    for (auto s : xs) {
        s = canonical_pc(s);
        if (!s.empty()) out.push_back(s);
    }
    return out;
}
static void fade_tail(Vec& sig, int ns) {
    if (ns <= 0 || sig.size() == 0) return;
    if ((size_t)ns > sig.size()) ns = (int)sig.size();
    int start = (int)sig.size() - ns;
    for (int i = start; i < (int)sig.size(); ++i) {
        double g = (double)((int)sig.size() - i) / (double)ns;
        sig[(size_t)i] *= g;
    }
}
static void mix_at(Vec& dst, const Vec& src, int offset) {
    if (offset < 0) return;
    int need = offset + (int)src.size();
    if (need > (int)dst.size()) dst.resize((size_t)need, 0.0);
    for (size_t i = 0; i < src.size(); ++i) dst[(size_t)offset + i] += src[i];
}
static std::string event_str(const Value& ev, const std::string& key,
                             const std::string& f, int ln) {
    return db_entry_str(ev, key, f, ln);
}
static double event_num(const Value& ev, const std::string& key,
                        const std::string& f, int ln) {
    return db_entry_num(ev, key, f, ln);
}
static bool event_match(const Value& entry,
                        const std::string& instrument,
                        const std::vector<std::string>& chord_pcs,
                        int oct_lo, int oct_hi,
                        const std::vector<std::string>& dyns,
                        const std::vector<std::string>& styles,
                        const std::string& f, int ln) {
    if (!entry.is_list()) return false;
    if (db_entry_str(entry, "instrument", f, ln) != instrument) return false;
    std::string art = db_entry_str(entry, "articulation", f, ln);
    std::string dyn = db_entry_str(entry, "dynamic", f, ln);
    std::string pit = db_entry_str(entry, "pitch", f, ln);
    std::string pc = canonical_pc(pitch_pc_only(pit));
    int oct = pitch_octave_only(pit);
    if (!styles.empty() && !contains_str(styles, art)) return false;
    if (!dyns.empty() && !contains_str(dyns, dyn)) return false;
    if (!chord_pcs.empty() && !contains_str(chord_pcs, pc)) return false;
    if (oct < oct_lo || oct > oct_hi) return false;
    return true;
}
static Value make_event(const Value& entry,
                        int group_index,
                        const std::string& group_name,
                        double onset,
                        double requested_dur) {
    List e;
    e.push_back(kv("group_index", Value((double)group_index)));
    e.push_back(kv("group", Value(Str(group_name))));
    e.push_back(kv("onset", Value(onset)));
    e.push_back(kv("duration", Value(requested_dur)));
    e.push_back(kv("instrument", Value(Str(db_entry_str(entry, "instrument", "<orch>", 0)))));
    e.push_back(kv("articulation", Value(Str(db_entry_str(entry, "articulation", "<orch>", 0)))));
    e.push_back(kv("pitch", Value(Str(db_entry_str(entry, "pitch", "<orch>", 0)))));
    e.push_back(kv("midi", Value(db_entry_num(entry, "midi", "<orch>", 0))));
    e.push_back(kv("dynamic", Value(Str(db_entry_str(entry, "dynamic", "<orch>", 0)))));
    e.push_back(kv("path", Value(Str(db_entry_str(entry, "path", "<orch>", 0)))));
    return Value(std::move(e));
}
static Vec orch_mixdown(const List& events, int sr,
                        const std::string& f, int ln) {
    if (sr <= 0) err(f, ln, "orchgransnd: sr must be > 0");
    Vec out;
    int fade_ns = std::max(1, sr / 20); // 50 ms
    for (auto& ev : events) {
        if (!ev.is_list()) continue;
        double onset = event_num(ev, "onset", f, ln);
        double dur = event_num(ev, "duration", f, ln);
        std::string path = event_str(ev, "path", f, ln);
        Vec sig = read_wav_mono(path, sr, f, ln);
        int max_ns = std::max(1, (int)std::llround(dur * (double)sr));
        if ((int)sig.size() > max_ns) {
            sig.resize((size_t)max_ns);
            fade_tail(sig, std::min(fade_ns, max_ns));
        }
        int off = std::max(0, (int)std::llround(onset * (double)sr));
        mix_at(out, sig, off);
    }
    double mx = 0.0;
    for (size_t i = 0; i < out.size(); ++i) mx = std::max(mx, std::abs(out[i]));
    if (mx > 1.0) out /= mx;
    return out;
}

// ═════════════════════════════════════════════════════════════════════
// registration
// ═════════════════════════════════════════════════════════════════════
inline void register_sampsynth(Interpreter& interp) {
    auto D = [&](const char* nm, NativeFn fn) {
        interp.global->def(nm, Value(std::move(fn)));
    };

    // ── db_load(root) ────────────────────────────────────────────────
    D("db_load",[](auto& a,int ln,auto& f)->Value{
        if (a.size()!=1||!a[0].is_str()) err(f,ln,"db_load expects: root_path");
        fs::path root=normalize_path(a[0].as_str(),f);
        if (!fs::exists(root)) err(f,ln,"db_load: path not found: "+root.string());
        List db;
        for (auto& de : fs::recursive_directory_iterator(root)) {
            if (!de.is_regular_file()) continue;
            auto& p=de.path();
            if (!is_wav(p)) continue;
            std::string inst,art,pitch,dyn;
            if (!parse_db(p.stem().string(),inst,art,pitch,dyn)) continue;
            int midi=pitch_to_midi(pitch);
            if (midi<0) continue;
            std::string family;
            try {
                auto rel=fs::relative(p,root);
                auto it=rel.begin();
                if (it!=rel.end()) family=it->string();
            } catch(...) {}
            db.push_back(db_make_entry(family,inst,art,pitch,midi,dyn,fs::absolute(p).string()));
        }
        return Value(std::move(db));
    });

    D("db_query",[](auto& a,int ln,auto& f)->Value{
        if (a.size()!=2||!a[0].is_list()||!a[1].is_str()) err(f,ln,"db_query expects: db regex");
        std::regex re(a[1].as_str());
        List out;
        for (auto& e : a[0].as_list()) if (std::regex_search(db_entry_repr(e),re)) out.push_back(e);
        return Value(std::move(out));
    });

    D("db_pick",[](auto& a,int ln,auto& f)->Value{
        if (a.empty()||a.size()>2) err(f,ln,"db_pick expects: entry_or_path [sr]");
        int sr=(a.size()==2&&a[1].is_vec())?(int)a[1].scalar():0;
        return Value(extract_signal(a[0],sr,f,ln));
    });

    D("db_field",[](auto& a,int ln,auto& f)->Value{
        if (a.size()!=2||!a[0].is_list()||!a[1].is_str()) err(f,ln,"db_field expects: entry key");
        return db_entry_get(a[0],a[1].as_str(),f,ln);
    });

    D("db_instruments",[](auto& a,int ln,auto& f)->Value{
        if (a.size()!=1||!a[0].is_list()) err(f,ln,"db_instruments expects: db");
        return Value(unique_field(a[0].as_list(),"instrument"));
    });
    D("db_styles",[](auto& a,int ln,auto& f)->Value{
        if (a.size()!=1||!a[0].is_list()) err(f,ln,"db_styles expects: db");
        return Value(unique_field(a[0].as_list(),"articulation"));
    });
    D("db_dynamics",[](auto& a,int ln,auto& f)->Value{
        if (a.size()!=1||!a[0].is_list()) err(f,ln,"db_dynamics expects: db");
        return Value(unique_field(a[0].as_list(),"dynamic"));
    });
    D("db_pitches",[](auto& a,int ln,auto& f)->Value{
        if (a.size()!=1||!a[0].is_list()) err(f,ln,"db_pitches expects: db");
        return Value(unique_field(a[0].as_list(),"pitch"));
    });

    // orchgran(db, orchestra, pos, dur,
    //          idens, edens, irand_dens, erand_dens,
    //          ioct, eoct, irand_oct, erand_oct,
    //          ilen, elen, irand_len, erand_len,
    //          chords, dynamics, styles, [seed]) -> events
    D("orchgran",[](auto& a,int ln,auto& f)->Value{
        if (a.size()!=19 && a.size()!=20) {
            err(f,ln,"orchgran expects: db orchestra pos dur idens edens irand_dens erand_dens ioct eoct irand_oct erand_oct ilen elen irand_len erand_len chords dynamics styles [seed]");
        }
        if (!a[0].is_list() || !a[1].is_str() || !a[2].is_vec() || !a[3].is_vec() ||
            !a[4].is_vec() || !a[5].is_vec() || !a[6].is_vec() || !a[7].is_vec() ||
            !a[8].is_vec() || !a[9].is_vec() || !a[10].is_vec() || !a[11].is_vec() ||
            !a[12].is_vec() || !a[13].is_vec() || !a[14].is_vec() || !a[15].is_vec() ||
            !a[16].is_str() || !a[17].is_str() || !a[18].is_str()) {
            err(f,ln,"orchgran: invalid argument types");
        }

        const List& db = a[0].as_list();
        std::vector<std::vector<std::string>> orchestra = parse_groups(a[1].as_str());
        std::vector<std::vector<std::string>> chords = parse_groups(a[16].as_str());
        std::vector<std::vector<std::string>> dyns = parse_groups(a[17].as_str());
        std::vector<std::vector<std::string>> styles = parse_groups(a[18].as_str());
        if (orchestra.empty()) err(f,ln,"orchgran: orchestra is empty");
        if (chords.empty()) err(f,ln,"orchgran: chords are empty");
        if (dyns.empty()) dyns.push_back({});
        if (styles.empty()) styles.push_back({});

        double pos = a[2].scalar();
        double dur = a[3].scalar();
        if (dur <= 0.0) err(f,ln,"orchgran: dur must be > 0");

        double idens=a[4].scalar(),  edens=a[5].scalar();
        double ird =a[6].scalar(),   erd  =a[7].scalar();
        double ioct=a[8].scalar(),   eoct =a[9].scalar();
        double iro =a[10].scalar(),  ero  =a[11].scalar();
        double ilen=a[12].scalar(),  elen =a[13].scalar();
        double irl =a[14].scalar(),  erl  =a[15].scalar();

        std::mt19937 rng;
        if (a.size()==20) rng.seed((uint32_t)a[19].scalar());
        else rng.seed((uint32_t)std::random_device{}());
        std::uniform_real_distribution<double> U(-1.0, 1.0);

        std::vector<double> free_at(orchestra.size(), 0.0);
        List events;
        double t = 0.0;
        while (t < dur) {
            double phase = (dur > 0.0) ? (t / dur) : 0.0;
            phase = std::max(0.0, std::min(1.0, phase));

            double dens = std::max(1e-6, lerp_num(idens, edens, phase));
            double dens_j = std::max(0.0, lerp_num(ird, erd, phase));
            double interval = 1.0 / dens;
            interval *= std::max(0.05, 1.0 + dens_j * U(rng));

            double len = std::max(0.01, lerp_num(ilen, elen, phase));
            double len_j = std::max(0.0, lerp_num(irl, erl, phase));
            len *= std::max(0.05, 1.0 + len_j * U(rng));

            double oct = lerp_num(ioct, eoct, phase);
            double oct_j = std::max(0.0, lerp_num(iro, ero, phase));
            int oct_lo = (int)std::floor(oct - oct_j);
            int oct_hi = (int)std::ceil(oct + oct_j);
            if (oct_hi < oct_lo) std::swap(oct_lo, oct_hi);

            std::vector<std::string> chord_pcs = normalize_pitch_classes(chords[phase_index(chords.size(), phase)]);
            std::vector<std::string> cur_dyns  = dyns[phase_index(dyns.size(), phase)];
            std::vector<std::string> cur_styles= styles[phase_index(styles.size(), phase)];

            bool placed = false;
            for (size_t gi = 0; gi < orchestra.size() && !placed; ++gi) {
                if (free_at[gi] > t) continue;
                std::vector<Value> chosen;
                bool ok = true;
                for (auto& inst : orchestra[gi]) {
                    std::vector<size_t> matches;
                    for (size_t di = 0; di < db.size(); ++di) {
                        if (event_match(db[di], inst, chord_pcs, oct_lo, oct_hi,
                                        cur_dyns, cur_styles, f, ln)) {
                            matches.push_back(di);
                        }
                    }
                    if (matches.empty()) { ok = false; break; }
                    std::uniform_int_distribution<size_t> pick(0, matches.size() - 1);
                    chosen.push_back(db[matches[pick(rng)]]);
                }
                if (!ok) continue;
                double onset = pos + t;
                std::string gname = join_strings(orchestra[gi], "+");
                for (auto& entry : chosen) {
                    events.push_back(make_event(entry, (int)gi, gname, onset, len));
                }
                free_at[gi] = t + len;
                placed = true;
            }
            t += interval;
        }
        return Value(std::move(events));
    });
    // orchgransnd(events, sr) -> signal
    D("orchgransnd",[](auto& a,int ln,auto& f)->Value{
        if (a.size()!=2 || !a[0].is_list() || !a[1].is_vec()) {
            err(f,ln,"orchgransnd expects: events sr");
        }
        int sr = (int)a[1].scalar();
        Vec sig = orch_mixdown(a[0].as_list(), sr, f, ln);
        return Value(sig);
    });
}

} // namespace flux

#endif // SAMPSYNTH_H
