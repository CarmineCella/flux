// sampsynth.h — Flux sample-synthesis library
//
// Call register_sampsynth(interp) from main after constructing the Interpreter.
//
// Primitives: db_load, db_query, db_pick, db_field,
//             db_instruments, db_styles, db_dynamics, db_pitches,
//             orchgran, orchgran_render, orchgran_score, orchgran_print

#ifndef SAMPSYNTH_H
#define SAMPSYNTH_H

#include "flux.h"
#include "dsp.h"       // resample_fd, next_pow2

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

// ── internal string helpers (C++ level, not exposed to flux) ─────────
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

// ── pitch helpers ────────────────────────────────────────────────────
static int pitch_class(const std::string& raw) {
    std::string s = to_lower(raw);
    if (s == "c")              return 0;
    if (s == "c#" || s == "db") return 1;
    if (s == "d")              return 2;
    if (s == "d#" || s == "eb") return 3;
    if (s == "e")              return 4;
    if (s == "f")              return 5;
    if (s == "f#" || s == "gb") return 6;
    if (s == "g")              return 7;
    if (s == "g#" || s == "ab") return 8;
    if (s == "a")              return 9;
    if (s == "a#" || s == "bb") return 10;
    if (s == "b" || s == "cb") return 11;
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
//   list(list("family","Strings"), list("instrument","Vn"), ...)

static Value kv(const std::string& key, Value val) {
    return Value(List{Value(Str(key)), std::move(val)});
}

static Value ss_make_entry(const std::string& family,
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

// entry → compact string for regex matching
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
                for (int i=0;i<ns;++i)
                    sig[i]=((uint8_t)fi.get()-128)/128.0;
            } else err(f,ln,"unsupported bit depth in "+path);
            if (sz&1u) fi.get();
            break;
        } else fi.seekg(sz+(sz&1u),std::ios::cur);
    }
    if (!got_data) err(f,ln,"no data chunk in "+path);

    // fold to mono
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
    // resample
    if (target_sr>0 && sr>0 && sr!=target_sr)
        sig=resample_fd(sig,(double)target_sr/(double)sr);
    return sig;
}

static Vec extract_signal(const Value& v, int target_sr,
                             const std::string& f, int ln) {
    if (v.is_str())
        return read_wav_mono(v.as_str(), target_sr, f, ln);
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

// ═════════════════════════════════════════════════════════════════════
// registration
// ═════════════════════════════════════════════════════════════════════

inline void register_sampsynth(Interpreter& interp) {
    auto D = [&](const char* nm, NativeFn fn) {
        interp.global->def(nm, Value(std::move(fn)));
    };

    // ── db_load(root) ────────────────────────────────────────────────
    D("db_load",[](auto& a,int ln,auto& f)->Value{
        if (a.size()!=1||!a[0].is_str())
            err(f,ln,"db_load expects: root_path");
        fs::path root=normalize_path(a[0].as_str(),f);
        if (!fs::exists(root))
            err(f,ln,"db_load: path not found: "+root.string());
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
            db.push_back(ss_make_entry(family,inst,art,pitch,midi,dyn,
                                       fs::absolute(p).string()));
        }
        return Value(std::move(db));
    });

    // ── db_query(db, regex) ──────────────────────────────────────────
    D("db_query",[](auto& a,int ln,auto& f)->Value{
        if (a.size()!=2||!a[0].is_list()||!a[1].is_str())
            err(f,ln,"db_query expects: db regex");
        std::regex re(a[1].as_str());
        List out;
        for (auto& e : a[0].as_list())
            if (std::regex_search(db_entry_repr(e),re)) out.push_back(e);
        return Value(std::move(out));
    });

    // ── db_pick(entry_or_path, [sr]) ─────────────────────────────────
    D("db_pick",[](auto& a,int ln,auto& f)->Value{
        if (a.empty()||a.size()>2)
            err(f,ln,"db_pick expects: entry_or_path [sr]");
        int sr=(a.size()==2&&a[1].is_vec())?(int)a[1].scalar():0;
        return Value(extract_signal(a[0],sr,f,ln));
    });

    // ── db_field(entry, key) ─────────────────────────────────────────
    D("db_field",[](auto& a,int ln,auto& f)->Value{
        if (a.size()!=2||!a[0].is_list()||!a[1].is_str())
            err(f,ln,"db_field expects: entry key");
        return db_entry_get(a[0],a[1].as_str(),f,ln);
    });

    // ── db_instruments / db_styles / db_dynamics / db_pitches ─────────
    D("db_instruments",[](auto& a,int ln,auto& f)->Value{
        if (a.size()!=1||!a[0].is_list()) err(f,ln,"db_instruments expects: db");
        return Value(unique_field(a[0].as_list(),"instrument"));});
    D("db_styles",[](auto& a,int ln,auto& f)->Value{
        if (a.size()!=1||!a[0].is_list()) err(f,ln,"db_styles expects: db");
        return Value(unique_field(a[0].as_list(),"articulation"));});
    D("db_dynamics",[](auto& a,int ln,auto& f)->Value{
        if (a.size()!=1||!a[0].is_list()) err(f,ln,"db_dynamics expects: db");
        return Value(unique_field(a[0].as_list(),"dynamic"));});
    D("db_pitches",[](auto& a,int ln,auto& f)->Value{
        if (a.size()!=1||!a[0].is_list()) err(f,ln,"db_pitches expects: db");
        return Value(unique_field(a[0].as_list(),"pitch"));});
}

} // namespace flux

#endif // SAMPSYNTH_H
