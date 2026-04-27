//
// flux.h — Flux interpreter, single-header.
//

#ifndef FLUX_H
#define FLUX_H

// ── Version ───────────────────────────────────────────────────────────
#define FLUX_VERSION_MAJOR 0
#define FLUX_VERSION_MINOR 2
#define FLUX_VERSION_PATCH 0
#define FLUX_VERSION       "0.2.0"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <valarray>
#include <variant>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <regex>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <thread>
#include <random>
#include <cerrno>
#include <cctype>
#include <array>
#include <filesystem>

namespace flux {

namespace fs = std::filesystem;

struct Env;
using EnvPtr = std::shared_ptr<Env>;
struct Value;
using Vec     = std::valarray<double>;
using List    = std::vector<Value>;
using ListPtr = std::shared_ptr<List>;
using DictMap = std::unordered_map<std::string, Value>;
using DictPtr = std::shared_ptr<DictMap>;
using Str     = std::string;

// ── Buffer — first-class audio/sample buffer ──────────────────────────
// Frames × channels, interleaved (data[frame * n_channels + channel]).
// Doubles for now; convert at C++ boundaries if your engine uses floats.
struct Buffer {
    std::vector<double> data;
    size_t n_frames = 0;
    size_t n_channels = 1;
    double sample_rate = 44100.0;
};
using BufferPtr = std::shared_ptr<Buffer>;

// ── Opaque — shared_ptr<void> handle for arbitrary C++ data ───────────
// The host attaches things that can't be expressed as Flux values:
// FFT plans, model weights, file/stream handles, audio device handles.
// Type tag is for diagnostics and host-side dispatch.
struct Opaque {
    std::string type_tag;
    std::shared_ptr<void> ptr;
};

struct Stmt;
struct Expr;
struct Closure {
    std::string name;
    std::vector<std::string> params;
    std::vector<Stmt> body;
    EnvPtr env;
    const Expr* origin = nullptr;   // shared pointer to the FuncDecl AST node
    std::string doc;                // optional docstring (first-stmt string literal)
};
using NativeFn = std::function<Value(const std::vector<Value>&, int, const std::string&)>;
using YieldFn  = std::function<void()>;

// ── Structured error type with call-stack trace ───────────────────────
struct Error : std::exception {
    std::string file;
    int line = 0;
    std::string msg;
    std::vector<std::string> trace;          // innermost-first
    mutable std::string cached;
    Error() = default;
    Error(std::string f, int ln, std::string m)
        : file(std::move(f)), line(ln), msg(std::move(m)) {}
    const char* what() const noexcept override {
        if (cached.empty()) {
            cached = file + ":" + std::to_string(line) + ": " + msg;
            if (!trace.empty()) {
                cached += "\n  call stack:";
                for (size_t i = 0; i < trace.size(); ++i)
                    cached += "\n    " + std::to_string(i + 1) + "> " + trace[i];
            }
        }
        return cached.c_str();
    }
};

[[noreturn]] inline void err(const std::string& f, int ln, const std::string& msg) {
    throw Error{f, ln, msg};
}

// ── Value — scalars are Vec of size 1 ─────────────────────────────────
struct Value {
    std::variant<Vec, Str, ListPtr, DictPtr, BufferPtr, Opaque,
                 Closure, NativeFn, std::nullptr_t> data;
    Value() : data(nullptr) {}
    Value(double d) : data(Vec{d}) {}
    Value(Vec v)    : data(std::move(v)) {}
    Value(Str s)    : data(std::move(s)) {}
    Value(List l)   : data(std::make_shared<List>(std::move(l))) {}
    Value(ListPtr p): data(std::move(p)) {}
    Value(DictPtr p): data(std::move(p)) {}
    Value(BufferPtr p): data(std::move(p)) {}
    Value(Opaque o) : data(std::move(o)) {}
    Value(Closure c): data(std::move(c)) {}
    Value(NativeFn f): data(std::move(f)) {}
    Value(std::nullptr_t) : data(nullptr) {}

    bool is_vec()     const { return std::holds_alternative<Vec>(data); }
    bool is_str()     const { return std::holds_alternative<Str>(data); }
    bool is_list()    const { return std::holds_alternative<ListPtr>(data); }
    bool is_dict()    const { return std::holds_alternative<DictPtr>(data); }
    bool is_buffer()  const { return std::holds_alternative<BufferPtr>(data); }
    bool is_opaque()  const { return std::holds_alternative<Opaque>(data); }
    bool is_closure() const { return std::holds_alternative<Closure>(data); }
    bool is_native()  const { return std::holds_alternative<NativeFn>(data); }
    bool is_nil()     const { return std::holds_alternative<std::nullptr_t>(data); }

    const Vec&     as_vec()      const { return std::get<Vec>(data); }
    const Str&     as_str()      const { return std::get<Str>(data); }
    const List&    as_list()     const { return *std::get<ListPtr>(data); }
    // Mutable view through the shared pointer. Const-qualified because the
    // shared_ptr itself is not modified — only the pointee.
    List&          as_list_mut() const { return *std::get<ListPtr>(data); }
    const ListPtr& as_list_ptr() const { return std::get<ListPtr>(data); }
    const DictMap& as_dict()     const { return *std::get<DictPtr>(data); }
    DictMap&       as_dict_mut() const { return *std::get<DictPtr>(data); }
    const DictPtr& as_dict_ptr() const { return std::get<DictPtr>(data); }
    const Buffer&  as_buffer()        const { return *std::get<BufferPtr>(data); }
    Buffer&        as_buffer_mut()    const { return *std::get<BufferPtr>(data); }
    const BufferPtr& as_buffer_ptr()  const { return std::get<BufferPtr>(data); }
    const Opaque&  as_opaque()   const { return std::get<Opaque>(data); }
    const Closure& as_closure()  const { return std::get<Closure>(data); }
    const NativeFn& as_native()  const { return std::get<NativeFn>(data); }
    double scalar() const { return as_vec()[0]; }

    bool truthy() const {
        if (is_nil()) return false;
        if (is_vec()) {
            // Multi-element vec is truthy iff every element is non-zero.
            // This makes `if (v == w) { ... }` mean "all elements equal",
            // matching MATLAB/Octave semantics. Empty vec is falsy.
            auto& v = as_vec();
            if (v.size() == 0) return false;
            for (size_t i = 0; i < v.size(); ++i)
                if (v[i] == 0.0) return false;
            return true;
        }
        if (is_str()) return !as_str().empty();
        if (is_list()) return !as_list().empty();
        if (is_dict()) return !as_dict().empty();
        if (is_buffer()) return as_buffer().n_frames > 0;
        return true;          // opaque, closure, native
    }
    static std::string fmt(double d) {
        auto s = std::to_string(d);
        s.erase(s.find_last_not_of('0') + 1);
        if (s.back() == '.') s.pop_back();
        return s;
    }

    // Cycle-aware repr. The visited set is per-call so simple repr() pays nothing.
    std::string repr() const {
        std::unordered_set<const void*> seen;
        return repr_with(seen);
    }
    std::string repr_with(std::unordered_set<const void*>& seen) const {
        if (is_nil()) return "nil";
        if (is_vec()) {
            auto& v = as_vec();
            if (v.size() == 1) return fmt(v[0]);
            std::string r = "[";
            for (size_t i = 0; i < v.size(); ++i) { if (i) r += ", "; r += fmt(v[i]); }
            return r + "]";
        }
        if (is_str()) return as_str();
        if (is_list()) {
            const void* key = as_list_ptr().get();
            if (seen.count(key)) return "(...)";
            seen.insert(key);
            std::string r = "(";
            auto& l = as_list();
            for (size_t i = 0; i < l.size(); ++i) {
                if (i) r += ", ";
                r += l[i].repr_with(seen);
            }
            seen.erase(key);
            return r + ")";
        }
        if (is_dict()) {
            const void* key = as_dict_ptr().get();
            if (seen.count(key)) return "{...}";
            seen.insert(key);
            auto& d = as_dict();
            std::vector<std::string> keys;
            keys.reserve(d.size());
            for (auto& kv : d) keys.push_back(kv.first);
            std::sort(keys.begin(), keys.end());
            std::string r = "{";
            for (size_t i = 0; i < keys.size(); ++i) {
                if (i) r += ", ";
                r += keys[i] + ": " + d.at(keys[i]).repr_with(seen);
            }
            seen.erase(key);
            return r + "}";
        }
        if (is_buffer()) {
            auto& b = as_buffer();
            return "<buffer " + std::to_string(b.n_frames) + "x" +
                   std::to_string(b.n_channels) + " @ " + fmt(b.sample_rate) + "Hz>";
        }
        if (is_opaque()) return "<opaque:" + as_opaque().type_tag + ">";
        if (is_closure()) {
            auto& c = as_closure();
            return c.name.empty() ? "<func>" : ("<func " + c.name + ">");
        }
        if (is_native())  return "<native>";
        return "?";
    }

    // Structural equality — recurses into lists and dicts. Buffers, opaques,
    // closures, natives compare by identity (or are never-equal). Cycle-safe:
    // revisiting an in-progress (a, b) pair returns true (Tarjan-style).
    static bool deep_eq(const Value& a, const Value& b) {
        std::unordered_set<size_t> on_stack;
        return deep_eq_with(a, b, on_stack);
    }
    static bool deep_eq_with(const Value& a, const Value& b,
                             std::unordered_set<size_t>& on_stack) {
        if (a.is_nil()) return b.is_nil();
        if (b.is_nil()) return false;
        if (a.is_str()) return b.is_str() && a.as_str() == b.as_str();
        if (b.is_str()) return false;
        if (a.is_vec()) {
            if (!b.is_vec()) return false;
            auto& va = a.as_vec(); auto& vb = b.as_vec();
            if (va.size() != vb.size()) return false;
            for (size_t i = 0; i < va.size(); ++i) if (va[i] != vb[i]) return false;
            return true;
        }
        if (b.is_vec()) return false;
        if (a.is_list()) {
            if (!b.is_list()) return false;
            size_t key = std::hash<const void*>{}(a.as_list_ptr().get())
                       ^ (std::hash<const void*>{}(b.as_list_ptr().get()) << 1);
            if (on_stack.count(key)) return true;        // co-inductive
            on_stack.insert(key);
            auto& la = a.as_list(); auto& lb = b.as_list();
            bool ok = la.size() == lb.size();
            for (size_t i = 0; ok && i < la.size(); ++i)
                ok = deep_eq_with(la[i], lb[i], on_stack);
            on_stack.erase(key);
            return ok;
        }
        if (b.is_list()) return false;
        if (a.is_dict()) {
            if (!b.is_dict()) return false;
            size_t key = std::hash<const void*>{}(a.as_dict_ptr().get())
                       ^ (std::hash<const void*>{}(b.as_dict_ptr().get()) << 1);
            if (on_stack.count(key)) return true;
            on_stack.insert(key);
            auto& da = a.as_dict(); auto& db = b.as_dict();
            bool ok = da.size() == db.size();
            for (auto it = da.begin(); ok && it != da.end(); ++it) {
                auto jt = db.find(it->first);
                if (jt == db.end()) { ok = false; break; }
                ok = deep_eq_with(it->second, jt->second, on_stack);
            }
            on_stack.erase(key);
            return ok;
        }
        if (b.is_dict()) return false;
        if (a.is_buffer()) {
            return b.is_buffer() && a.as_buffer_ptr().get() == b.as_buffer_ptr().get();
        }
        if (b.is_buffer()) return false;
        if (a.is_opaque()) {
            return b.is_opaque()
                && a.as_opaque().ptr.get() == b.as_opaque().ptr.get()
                && a.as_opaque().type_tag == b.as_opaque().type_tag;
        }
        if (b.is_opaque()) return false;
        return false;        // closures/natives never compare equal here
    }
};

// ── Environment ───────────────────────────────────────────────────────
struct Env {
    std::unordered_map<std::string, Value> vars;
    EnvPtr parent;
    Env() = default;
    explicit Env(EnvPtr p) : parent(std::move(p)) {}
    Value* find(const std::string& n) {
        auto it = vars.find(n);
        if (it != vars.end()) return &it->second;
        return parent ? parent->find(n) : nullptr;
    }
    void set(const std::string& n, Value v) {
        auto* p = find(n);
        if (p) { *p = std::move(v); return; }
        vars[n] = std::move(v);
    }
    void def(const std::string& n, Value v) { vars[n] = std::move(v); }
};

// ── Tokens ────────────────────────────────────────────────────────────
enum class Tk {
    Num, Str, Id, LPar, RPar, LBrace, RBrace, LBrack, RBrack,
    Plus, Minus, Star, Slash, Percent, Eq, EqEq, Neq, Lt, Gt, Le, Ge,
    And, Or, Not, Comma, Semi, Dot, Colon, Eof,
    Var, Func, If, Else, While, For, In, Return, Break, Continue,
    Print, Load, Try, Catch, Finally, Assert
};
inline const char* tk_name(Tk t) {
    switch (t) {
    case Tk::Num:      return "number";
    case Tk::Str:      return "string";
    case Tk::Id:       return "identifier";
    case Tk::LPar:     return "'('";
    case Tk::RPar:     return "')'";
    case Tk::LBrace:   return "'{'";
    case Tk::RBrace:   return "'}'";
    case Tk::LBrack:   return "'['";
    case Tk::RBrack:   return "']'";
    case Tk::Plus:     return "'+'";
    case Tk::Minus:    return "'-'";
    case Tk::Star:     return "'*'";
    case Tk::Slash:    return "'/'";
    case Tk::Percent:  return "'%'";
    case Tk::Eq:       return "'='";
    case Tk::EqEq:     return "'=='";
    case Tk::Neq:      return "'!='";
    case Tk::Lt:       return "'<'";
    case Tk::Gt:       return "'>'";
    case Tk::Le:       return "'<='";
    case Tk::Ge:       return "'>='";
    case Tk::And:      return "'and'";
    case Tk::Or:       return "'or'";
    case Tk::Not:      return "'not'";
    case Tk::Comma:    return "','";
    case Tk::Semi:     return "';'";
    case Tk::Dot:      return "'.'";
    case Tk::Colon:    return "':'";
    case Tk::Eof:      return "end of input";
    case Tk::Var:      return "'var'";
    case Tk::Func:     return "'func'";
    case Tk::If:       return "'if'";
    case Tk::Else:     return "'else'";
    case Tk::While:    return "'while'";
    case Tk::For:      return "'for'";
    case Tk::In:       return "'in'";
    case Tk::Return:   return "'return'";
    case Tk::Break:    return "'break'";
    case Tk::Continue: return "'continue'";
    case Tk::Print:    return "'print'";
    case Tk::Load:     return "'load'";
    case Tk::Try:      return "'try'";
    case Tk::Catch:    return "'catch'";
    case Tk::Finally:  return "'finally'";
    case Tk::Assert:   return "'assert'";
    }
    return "?";
}
struct Token { Tk type; std::string text; int line; };

// ── Path resolution ───────────────────────────────────────────────────
inline fs::path normalize_path(const std::string& raw, const std::string& src_file) {
    fs::path p(raw);
    if (p.is_absolute()) return fs::weakly_canonical(p);
    fs::path base = fs::path(src_file).parent_path();
    if (base.empty()) base = fs::current_path();
    return fs::weakly_canonical(base / p);
}
inline fs::path resolve_load(const std::string& raw, const std::string& src_file) {
    auto p = normalize_path(raw, src_file);
    if (fs::exists(p)) return p;
    if (auto* env = std::getenv("FLUX_PATH")) {
        std::string paths(env);
#ifdef _WIN32
        char sep = ';';
#else
        char sep = ':';
#endif
        size_t start = 0, pos;
        while ((pos = paths.find(sep, start)) != std::string::npos) {
            auto candidate = fs::path(paths.substr(start, pos - start)) / raw;
            if (fs::exists(candidate)) return fs::weakly_canonical(candidate);
            start = pos + 1;
        }
        auto candidate = fs::path(paths.substr(start)) / raw;
        if (fs::exists(candidate)) return fs::weakly_canonical(candidate);
    }
    if (auto* home = std::getenv("HOME")) {
        auto candidate = fs::path(home) / ".flux" / raw;
        if (fs::exists(candidate)) return fs::weakly_canonical(candidate);
    }
#ifdef _WIN32
    if (auto* home = std::getenv("USERPROFILE")) {
        auto candidate = fs::path(home) / ".flux" / raw;
        if (fs::exists(candidate)) return fs::weakly_canonical(candidate);
    }
#endif
    return p;
}

// ── Lexer ─────────────────────────────────────────────────────────────
struct Lexer {
    std::string src;
    size_t pos = 0;
    int line = 1;
    std::string file;
    Lexer(std::string s, std::string f = "<repl>") : src(std::move(s)), file(std::move(f)) {}
    char peek(size_t off = 0) const { return pos + off < src.size() ? src[pos + off] : '\0'; }
    char advance() { char c = peek(); if (c == '\n') ++line; ++pos; return c; }
    void skip() {
        while (pos < src.size()) {
            char c = src[pos];
            if (std::isspace((unsigned char)c)) { advance(); continue; }
            if (c == '#') {
                while (pos < src.size() && src[pos] != '\n') ++pos;
                continue;
            }
            // Block comments: /* ... */ (non-nesting).
            if (c == '/' && pos + 1 < src.size() && src[pos + 1] == '*') {
                int start_line = line;
                advance(); advance();   // consume /*
                while (pos < src.size() &&
                       !(src[pos] == '*' && pos + 1 < src.size() && src[pos + 1] == '/'))
                    advance();
                if (pos >= src.size()) err(file, start_line, "unterminated /* ... */ comment");
                advance(); advance();   // consume */
                continue;
            }
            break;
        }
    }
    Token next() {
        skip();
        int ln = line;
        if (pos >= src.size()) return {Tk::Eof, "", ln};
        char c = peek();
        if (c == '"') {
            advance();
            std::string s;
            int str_start = ln;
            while (pos < src.size() && peek() != '"') {
                if (peek() == '\\') {
                    advance();
                    if (pos >= src.size()) break;
                    char e = advance();
                    switch (e) {
                    case 'n':  s += '\n'; break;
                    case 't':  s += '\t'; break;
                    case 'r':  s += '\r'; break;
                    case '0':  s += '\0'; break;
                    case '\\': s += '\\'; break;
                    case '"':  s += '"';  break;
                    default:   s += e;    break;
                    }
                } else s += advance();
            }
            if (peek() != '"') err(file, str_start, "unterminated string literal");
            advance();
            return {Tk::Str, s, ln};
        }
        if (std::isdigit((unsigned char)c) ||
            (c == '.' && pos + 1 < src.size() && std::isdigit((unsigned char)src[pos + 1]))) {
            std::string n;
            bool seen_dot = false;
            while (std::isdigit((unsigned char)peek()) || peek() == '.') {
                if (peek() == '.') {
                    if (seen_dot) err(file, ln, "invalid numeric literal: multiple decimal points");
                    seen_dot = true;
                }
                n += advance();
            }
            if (peek() == 'e' || peek() == 'E') {
                n += advance();
                if (peek() == '+' || peek() == '-') n += advance();
                if (!std::isdigit((unsigned char)peek()))
                    err(file, ln, "invalid numeric literal: missing exponent digits");
                while (std::isdigit((unsigned char)peek())) n += advance();
            }
            return {Tk::Num, n, ln};
        }
        if (std::isalpha((unsigned char)c) || c == '_') {
            std::string id;
            while (std::isalnum((unsigned char)peek()) || peek() == '_') id += advance();
            static const std::unordered_map<std::string, Tk> kw = {
                {"var", Tk::Var}, {"func", Tk::Func}, {"if", Tk::If}, {"else", Tk::Else},
                {"while", Tk::While}, {"for", Tk::For}, {"in", Tk::In},
                {"return", Tk::Return}, {"break", Tk::Break}, {"continue", Tk::Continue},
                {"print", Tk::Print}, {"load", Tk::Load},
                {"and", Tk::And}, {"or", Tk::Or}, {"not", Tk::Not},
                {"try", Tk::Try}, {"catch", Tk::Catch}, {"finally", Tk::Finally},
                {"assert", Tk::Assert}
            };
            auto it = kw.find(id);
            return {it != kw.end() ? it->second : Tk::Id, id, ln};
        }
        advance();
        switch (c) {
        case '(': return {Tk::LPar, "(", ln};
        case ')': return {Tk::RPar, ")", ln};
        case '{': return {Tk::LBrace, "{", ln};
        case '}': return {Tk::RBrace, "}", ln};
        case '[': return {Tk::LBrack, "[", ln};
        case ']': return {Tk::RBrack, "]", ln};
        case '+': return {Tk::Plus, "+", ln};
        case '-': return {Tk::Minus, "-", ln};
        case '*': return {Tk::Star, "*", ln};
        case '/': return {Tk::Slash, "/", ln};
        case '%': return {Tk::Percent, "%", ln};
        case ',': return {Tk::Comma, ",", ln};
        case ';': return {Tk::Semi, ";", ln};
        case '.': return {Tk::Dot, ".", ln};
        case ':': return {Tk::Colon, ":", ln};
        case '=': if (peek() == '=') { advance(); return {Tk::EqEq, "==", ln}; } return {Tk::Eq, "=", ln};
        case '!': if (peek() == '=') { advance(); return {Tk::Neq, "!=", ln}; } return {Tk::Not, "!", ln};
        case '<': if (peek() == '=') { advance(); return {Tk::Le, "<=", ln}; }  return {Tk::Lt, "<", ln};
        case '>': if (peek() == '=') { advance(); return {Tk::Ge, ">=", ln}; }  return {Tk::Gt, ">", ln};
        default: err(file, ln, std::string("unexpected char '") + c + "'");
        }
    }
    std::vector<Token> tokenize() {
        std::vector<Token> ts;
        Token t;
        do { t = next(); ts.push_back(t); } while (t.type != Tk::Eof);
        return ts;
    }
};

// ── AST — each node carries file + line ───────────────────────────────
enum class NodeT {
    Num, Str, Id, BinOp, UnaryOp, Call, Index, Member, VecLit, DictLit,
    VarDecl, Assign, IndexAssign, FuncDecl, IfStmt, WhileStmt, ForStmt, ForIn,
    ReturnStmt, BreakStmt, ContStmt, PrintStmt, Block, LoadStmt,
    TryCatch, AssertStmt
};
struct Expr;
using ExprPtr = std::shared_ptr<Expr>;
struct Expr {
    NodeT type;
    int line = 0;
    std::shared_ptr<std::string> file;
    double num_val = 0;
    std::string str_val, op;
    ExprPtr left, right;
    std::vector<ExprPtr> args;
    std::vector<std::string> params;   // also dict-literal keys for DictLit
    std::vector<Stmt> body;
    const std::string& src_file() const {
        static std::string u = "<?>";
        return file ? *file : u;
    }
};
struct Stmt { ExprPtr expr; };
struct ReturnSignal   { Value val; };
struct BreakSignal    {};
struct ContinueSignal {};
struct TailCall       { Value fn; std::vector<Value> args; };

// ── Best-effort source reconstruction (used by 'assert') ──────────────
inline std::string expr_to_string(const ExprPtr& e) {
    if (!e) return "";
    switch (e->type) {
    case NodeT::Num: return Value::fmt(e->num_val);
    case NodeT::Str: {
        std::string r = "\"";
        for (char c : e->str_val) {
            if      (c == '\n') r += "\\n";
            else if (c == '\t') r += "\\t";
            else if (c == '"')  r += "\\\"";
            else if (c == '\\') r += "\\\\";
            else r += c;
        }
        return r + "\"";
    }
    case NodeT::Id: return e->str_val;
    case NodeT::BinOp:
        return expr_to_string(e->left) + " " + e->op + " " + expr_to_string(e->right);
    case NodeT::UnaryOp:
        return e->op + (e->op == "not" ? " " : "") + expr_to_string(e->left);
    case NodeT::Call: {
        std::string r = expr_to_string(e->left) + "(";
        for (size_t i = 0; i < e->args.size(); ++i) {
            if (i) r += ", ";
            r += expr_to_string(e->args[i]);
        }
        return r + ")";
    }
    case NodeT::Index:
        return expr_to_string(e->left) + "[" + expr_to_string(e->right) + "]";
    case NodeT::Member:
        return expr_to_string(e->left) + "." + e->str_val;
    case NodeT::VecLit: {
        std::string r = "[";
        for (size_t i = 0; i < e->args.size(); ++i) {
            if (i) r += ", ";
            r += expr_to_string(e->args[i]);
        }
        return r + "]";
    }
    case NodeT::DictLit: {
        std::string r = "{";
        for (size_t i = 0; i < e->args.size(); ++i) {
            if (i) r += ", ";
            r += e->params[i] + ": " + expr_to_string(e->args[i]);
        }
        return r + "}";
    }
    default: return "<expr>";
    }
}

// ── Parser ────────────────────────────────────────────────────────────
struct Parser {
    std::vector<Token> tokens;
    size_t pos = 0;
    std::shared_ptr<std::string> file;
    Parser(std::vector<Token> t, const std::string& f)
        : tokens(std::move(t)), file(std::make_shared<std::string>(f)) {}
    Token& cur() { return tokens[pos]; }
    Token eat(Tk t) {
        if (cur().type != t)
            err(*file, cur().line,
                std::string("expected ") + tk_name(t) + ", got '" + cur().text + "'");
        return tokens[pos++];
    }
    bool match(Tk t) { if (cur().type == t) { ++pos; return true; } return false; }
    bool check(Tk t) { return cur().type == t; }
    ExprPtr make(NodeT t, int ln) {
        auto e = std::make_shared<Expr>();
        e->type = t; e->line = ln; e->file = file;
        return e;
    }

    std::vector<Stmt> parse_program() {
        std::vector<Stmt> s;
        while (!check(Tk::Eof)) s.push_back({parse_stmt()});
        return s;
    }
    ExprPtr parse_stmt() {
        int ln = cur().line; (void)ln;
        if (check(Tk::Var))    return parse_var();
        if (check(Tk::Func))   return parse_func(/*allow_name=*/true, /*require_name=*/true);
        if (check(Tk::If))     return parse_if();
        if (check(Tk::While))  return parse_while();
        if (check(Tk::For))    return parse_for();
        if (check(Tk::Try))    return parse_try();
        if (check(Tk::Return)) {
            int rln = cur().line; ++pos;
            auto e = make(NodeT::ReturnStmt, rln);
            if (!check(Tk::RBrace) && !check(Tk::Eof) && !check(Tk::Semi))
                e->left = parse_expr();
            return e;
        }
        if (check(Tk::Break))    { int bln = cur().line; ++pos; return make(NodeT::BreakStmt, bln); }
        if (check(Tk::Continue)) { int cln = cur().line; ++pos; return make(NodeT::ContStmt, cln); }
        if (check(Tk::Print))    return parse_print();
        if (check(Tk::Assert))   return parse_assert();
        if (check(Tk::Load)) {
            int lln = cur().line; ++pos;
            eat(Tk::LPar);
            auto e = make(NodeT::LoadStmt, lln);
            e->left = parse_expr();
            eat(Tk::RPar);
            return e;
        }
        return parse_assign();
    }
    ExprPtr parse_var() {
        int ln = cur().line;
        eat(Tk::Var);
        auto e = make(NodeT::VarDecl, ln);
        e->str_val = eat(Tk::Id).text;
        eat(Tk::Eq);
        e->left = parse_expr();
        return e;
    }
    // Unified function parser. Statement-form (top-level / inside a block as a
    // statement) requires a name; expression-form (inside a primary) forbids one
    // — that prevents `var f = func g(){}` from also leaking `g` into scope.
    ExprPtr parse_func(bool allow_name, bool require_name) {
        int ln = cur().line;
        eat(Tk::Func);
        auto e = make(NodeT::FuncDecl, ln);
        if (check(Tk::Id)) {
            if (!allow_name)
                err(*file, ln, "function expression cannot be named here");
            e->str_val = eat(Tk::Id).text;
        } else {
            if (require_name)
                err(*file, ln, "function statement requires a name");
            e->str_val = "";
        }
        eat(Tk::LPar);
        while (!check(Tk::RPar)) {
            e->params.push_back(eat(Tk::Id).text);
            if (!check(Tk::RPar)) eat(Tk::Comma);
        }
        eat(Tk::RPar);
        e->body = parse_block();
        return e;
    }
    std::vector<Stmt> parse_block() {
        eat(Tk::LBrace);
        std::vector<Stmt> s;
        while (!check(Tk::RBrace) && !check(Tk::Eof)) s.push_back({parse_stmt()});
        eat(Tk::RBrace);
        return s;
    }
    ExprPtr parse_if() {
        int ln = cur().line;
        eat(Tk::If);
        eat(Tk::LPar);
        auto e = make(NodeT::IfStmt, ln);
        e->left = parse_expr();
        eat(Tk::RPar);
        e->body = parse_block();
        if (match(Tk::Else)) {
            if (check(Tk::If)) {
                e->args.push_back(parse_if());
            } else {
                auto bl = make(NodeT::Block, cur().line);
                bl->body = parse_block();
                e->right = bl;
            }
        }
        return e;
    }
    ExprPtr parse_while() {
        int ln = cur().line;
        eat(Tk::While);
        eat(Tk::LPar);
        auto e = make(NodeT::WhileStmt, ln);
        e->left = parse_expr();
        eat(Tk::RPar);
        e->body = parse_block();
        return e;
    }
    ExprPtr parse_for() {
        int ln = cur().line;
        eat(Tk::For);
        eat(Tk::LPar);
        // Detect for-in: `var IDENT in EXPR` — lookahead without partial commit.
        if (check(Tk::Var)) {
            size_t save = pos;
            ++pos;                      // consume 'var'
            if (check(Tk::Id)) {
                std::string vn = cur().text;
                ++pos;                  // consume IDENT
                if (check(Tk::In)) {
                    ++pos;              // consume 'in'
                    auto e = make(NodeT::ForIn, ln);
                    e->str_val = vn;
                    e->left = parse_expr();
                    eat(Tk::RPar);
                    e->body = parse_block();
                    return e;
                }
            }
            pos = save;                 // not for-in: rewind cleanly
        }
        // C-style for(init; cond; update)
        auto e = make(NodeT::ForStmt, ln);
        e->args.push_back(parse_stmt());
        eat(Tk::Semi);
        e->args.push_back(parse_expr());
        eat(Tk::Semi);
        e->args.push_back(parse_stmt());
        eat(Tk::RPar);
        e->body = parse_block();
        return e;
    }
    ExprPtr parse_try() {
        int ln = cur().line;
        eat(Tk::Try);
        auto e = make(NodeT::TryCatch, ln);
        e->body = parse_block();
        // catch (var) { ... } and finally { ... } are both optional, but
        // at least one of them must follow `try { ... }`.
        if (check(Tk::Catch)) {
            ++pos;
            eat(Tk::LPar);
            e->str_val = eat(Tk::Id).text;     // catch variable name
            eat(Tk::RPar);
            auto bl = make(NodeT::Block, cur().line);
            bl->body = parse_block();
            e->right = bl;                     // catch block
        }
        if (check(Tk::Finally)) {
            ++pos;
            auto fl = make(NodeT::Block, cur().line);
            fl->body = parse_block();
            e->args.push_back(fl);             // finally block in args[0]
        }
        if (!e->right && e->args.empty())
            err(*file, ln, "try requires a 'catch' or 'finally' clause");
        return e;
    }
    ExprPtr parse_print() {
        int ln = cur().line;
        eat(Tk::Print);
        auto e = make(NodeT::PrintStmt, ln);
        while (!check(Tk::Eof) && !check(Tk::RBrace) && cur().line == ln)
            e->args.push_back(parse_expr());
        return e;
    }
    ExprPtr parse_assert() {
        int ln = cur().line;
        eat(Tk::Assert);
        eat(Tk::LPar);
        auto e = make(NodeT::AssertStmt, ln);
        e->left = parse_expr();
        // Capture a textual rendering of the asserted expression for diagnostics.
        e->str_val = expr_to_string(e->left);
        if (match(Tk::Comma)) e->args.push_back(parse_expr());
        eat(Tk::RPar);
        return e;
    }
    ExprPtr parse_assign() {
        auto e = parse_expr();
        if (check(Tk::Eq)) {
            int ln = cur().line;
            if (e->type == NodeT::Id) {
                eat(Tk::Eq);
                auto a = make(NodeT::Assign, ln);
                a->str_val = e->str_val;
                a->left = parse_expr();
                return a;
            }
            if (e->type == NodeT::Index) {
                eat(Tk::Eq);
                auto a = make(NodeT::IndexAssign, ln);
                a->left  = e->left;                 // target
                a->right = e->right;                // first index
                // Carry through any extra (multi-dim) indices, then the value.
                // Layout in a->args: [extra_indices..., value]
                for (auto& extra : e->args) a->args.push_back(extra);
                a->args.push_back(parse_expr());
                return a;
            }
            if (e->type == NodeT::Member) {
                // a.b = v   →   IndexAssign(a, "b", v)
                eat(Tk::Eq);
                auto a = make(NodeT::IndexAssign, ln);
                a->left = e->left;
                auto key = make(NodeT::Str, ln);
                key->str_val = e->str_val;
                a->right = key;
                a->args.push_back(parse_expr());
                return a;
            }
        }
        return e;
    }
    ExprPtr parse_expr() { return parse_or(); }
    ExprPtr parse_or() {
        auto l = parse_and();
        while (check(Tk::Or)) {
            int ln = cur().line; ++pos;
            auto e = make(NodeT::BinOp, ln);
            e->op = "or"; e->left = l; e->right = parse_and();
            l = e;
        }
        return l;
    }
    ExprPtr parse_and() {
        auto l = parse_eq();
        while (check(Tk::And)) {
            int ln = cur().line; ++pos;
            auto e = make(NodeT::BinOp, ln);
            e->op = "and"; e->left = l; e->right = parse_eq();
            l = e;
        }
        return l;
    }
    ExprPtr parse_eq() {
        auto l = parse_cmp();
        while (check(Tk::EqEq) || check(Tk::Neq)) {
            int ln = cur().line;
            auto op = eat(cur().type).text;
            auto e = make(NodeT::BinOp, ln);
            e->op = op; e->left = l; e->right = parse_cmp();
            l = e;
        }
        return l;
    }
    ExprPtr parse_cmp() {
        auto l = parse_add();
        while (check(Tk::Lt) || check(Tk::Gt) || check(Tk::Le) || check(Tk::Ge)) {
            int ln = cur().line;
            auto op = eat(cur().type).text;
            auto e = make(NodeT::BinOp, ln);
            e->op = op; e->left = l; e->right = parse_add();
            l = e;
        }
        return l;
    }
    ExprPtr parse_add() {
        auto l = parse_mul();
        while (check(Tk::Plus) || check(Tk::Minus)) {
            int ln = cur().line;
            auto op = eat(cur().type).text;
            auto e = make(NodeT::BinOp, ln);
            e->op = op; e->left = l; e->right = parse_mul();
            l = e;
        }
        return l;
    }
    ExprPtr parse_mul() {
        auto l = parse_unary();
        while (check(Tk::Star) || check(Tk::Slash) || check(Tk::Percent)) {
            int ln = cur().line;
            auto op = eat(cur().type).text;
            auto e = make(NodeT::BinOp, ln);
            e->op = op; e->left = l; e->right = parse_unary();
            l = e;
        }
        return l;
    }
    ExprPtr parse_unary() {
        if (check(Tk::Minus)) {
            int ln = cur().line; ++pos;
            auto e = make(NodeT::UnaryOp, ln);
            e->op = "-"; e->left = parse_unary();
            return e;
        }
        if (check(Tk::Not)) {
            int ln = cur().line; ++pos;
            auto e = make(NodeT::UnaryOp, ln);
            e->op = "not"; e->left = parse_unary();
            return e;
        }
        return parse_postfix();
    }
    static bool callable_node(const ExprPtr& e) {
        return e->type == NodeT::Id || e->type == NodeT::Call ||
               e->type == NodeT::Index || e->type == NodeT::Member ||
               e->type == NodeT::FuncDecl;
    }
    ExprPtr parse_postfix() {
        auto l = parse_primary();
        while (true) {
            if (check(Tk::LPar) && callable_node(l)) {
                int ln = cur().line;
                eat(Tk::LPar);
                auto c = make(NodeT::Call, ln);
                c->left = l;
                while (!check(Tk::RPar)) {
                    c->args.push_back(parse_expr());
                    if (!check(Tk::RPar)) eat(Tk::Comma);
                }
                eat(Tk::RPar);
                l = c;
            } else if (check(Tk::LBrack)) {
                int ln = cur().line;
                eat(Tk::LBrack);
                auto idx = make(NodeT::Index, ln);
                idx->left = l;
                idx->right = parse_expr();         // first index
                while (match(Tk::Comma))           // optional extra indices
                    idx->args.push_back(parse_expr());
                eat(Tk::RBrack);
                l = idx;
            } else if (check(Tk::Dot)) {
                int ln = cur().line;
                eat(Tk::Dot);
                auto m = make(NodeT::Member, ln);
                m->left = l;
                m->str_val = eat(Tk::Id).text;
                l = m;
            } else break;
        }
        return l;
    }
    ExprPtr parse_primary() {
        int ln = cur().line;
        if (check(Tk::Num)) {
            auto e = make(NodeT::Num, ln);
            e->num_val = std::stod(eat(Tk::Num).text);
            return e;
        }
        if (check(Tk::Str)) {
            auto e = make(NodeT::Str, ln);
            e->str_val = eat(Tk::Str).text;
            return e;
        }
        if (check(Tk::Id)) {
            auto e = make(NodeT::Id, ln);
            e->str_val = eat(Tk::Id).text;
            return e;
        }
        if (check(Tk::LPar)) {
            eat(Tk::LPar);
            auto e = parse_expr();
            eat(Tk::RPar);
            return e;
        }
        if (check(Tk::LBrack)) {
            eat(Tk::LBrack);
            auto e = make(NodeT::VecLit, ln);
            while (!check(Tk::RBrack)) {
                e->args.push_back(parse_expr());
                if (!check(Tk::RBrack)) eat(Tk::Comma);
            }
            eat(Tk::RBrack);
            return e;
        }
        if (check(Tk::LBrace)) {
            // Dict literal: {} or {key: value, ...}
            eat(Tk::LBrace);
            auto e = make(NodeT::DictLit, ln);
            while (!check(Tk::RBrace)) {
                std::string key;
                if (check(Tk::Id))       key = eat(Tk::Id).text;
                else if (check(Tk::Str)) key = eat(Tk::Str).text;
                else err(*file, cur().line,
                         std::string("expected dict key (identifier or string), got '")
                         + cur().text + "'");
                eat(Tk::Colon);
                e->params.push_back(key);
                e->args.push_back(parse_expr());
                if (!check(Tk::RBrace)) eat(Tk::Comma);
            }
            eat(Tk::RBrace);
            return e;
        }
        if (check(Tk::Func)) {
            // Anonymous-only in expression position.
            return parse_func(/*allow_name=*/false, /*require_name=*/false);
        }
        err(*file, ln, "unexpected '" + cur().text + "'");
    }
};

// ── Typed argument signatures (for reg_typed) ─────────────────────────
// A tiny mini-language for declaring native function signatures, so the
// per-argument arity/type checking boilerplate can be folded into the
// registration. The signature is also surfaced through help().
//
// Token grammar:    type ('?' if optional)
//                   types separated by ','
//                   trailing 'opaque:tag' for tag-checked opaques
// Type names: any, nil, scalar, int, number, vec, string, list, dict,
//             buffer, opaque[:tag], func
// Optional args (with trailing ?) must be at the end.
//
// Examples:
//   ""                          — zero args
//   "string"                    — exactly one string
//   "buffer, int, int"          — three required
//   "buffer, int, int?"         — last is optional
//   "opaque:fft_plan, buffer"   — first must be opaque tagged "fft_plan"
struct ArgSpec {
    enum Kind { Any, Nil, Scalar, Vec, Str, List, Dict,
                Buffer, Opaque, Func };
    Kind kind = Any;
    std::string tag;        // for opaque:tag
    bool optional = false;
};

inline const char* arg_kind_name(ArgSpec::Kind k) {
    switch (k) {
        case ArgSpec::Any:    return "any";
        case ArgSpec::Nil:    return "nil";
        case ArgSpec::Scalar: return "scalar";
        case ArgSpec::Vec:    return "vec";
        case ArgSpec::Str:    return "string";
        case ArgSpec::List:   return "list";
        case ArgSpec::Dict:   return "dict";
        case ArgSpec::Buffer: return "buffer";
        case ArgSpec::Opaque: return "opaque";
        case ArgSpec::Func:   return "func";
    }
    return "?";
}

inline std::string value_kind_name(const Value& v) {
    if (v.is_nil()) return "nil";
    if (v.is_vec()) return v.as_vec().size() == 1 ? "scalar" : "vec";
    if (v.is_str()) return "string";
    if (v.is_list()) return "list";
    if (v.is_dict()) return "dict";
    if (v.is_buffer()) return "buffer";
    if (v.is_opaque()) return "opaque:" + v.as_opaque().type_tag;
    if (v.is_closure() || v.is_native()) return "func";
    return "?";
}

inline bool match_arg(const Value& v, const ArgSpec& s) {
    switch (s.kind) {
        case ArgSpec::Any:    return true;
        case ArgSpec::Nil:    return v.is_nil();
        case ArgSpec::Scalar: return v.is_vec() && v.as_vec().size() == 1;
        case ArgSpec::Vec:    return v.is_vec();
        case ArgSpec::Str:    return v.is_str();
        case ArgSpec::List:   return v.is_list();
        case ArgSpec::Dict:   return v.is_dict();
        case ArgSpec::Buffer: return v.is_buffer();
        case ArgSpec::Opaque:
            if (!v.is_opaque()) return false;
            return s.tag.empty() || v.as_opaque().type_tag == s.tag;
        case ArgSpec::Func:   return v.is_closure() || v.is_native();
    }
    return false;
}

inline std::vector<ArgSpec> parse_signature(const std::string& sig) {
    std::vector<ArgSpec> out;
    auto skip_ws = [&](size_t& i) {
        while (i < sig.size() && (sig[i] == ' ' || sig[i] == '\t')) ++i;
    };
    auto read_ident = [&](size_t& i) {
        size_t s = i;
        while (i < sig.size() &&
               (std::isalnum((unsigned char)sig[i]) || sig[i] == '_'))
            ++i;
        return sig.substr(s, i - s);
    };

    size_t i = 0;
    skip_ws(i);
    if (i >= sig.size()) return out;

    while (i < sig.size()) {
        skip_ws(i);
        std::string tok = read_ident(i);
        if (tok.empty())
            throw std::runtime_error("bad signature: '" + sig + "'");
        ArgSpec spec;
        if      (tok == "any")                        spec.kind = ArgSpec::Any;
        else if (tok == "nil")                        spec.kind = ArgSpec::Nil;
        else if (tok == "scalar" || tok == "number" ||
                 tok == "int")                        spec.kind = ArgSpec::Scalar;
        else if (tok == "vec")                        spec.kind = ArgSpec::Vec;
        else if (tok == "string")                     spec.kind = ArgSpec::Str;
        else if (tok == "list")                       spec.kind = ArgSpec::List;
        else if (tok == "dict")                       spec.kind = ArgSpec::Dict;
        else if (tok == "buffer")                     spec.kind = ArgSpec::Buffer;
        else if (tok == "opaque")                     spec.kind = ArgSpec::Opaque;
        else if (tok == "func")                       spec.kind = ArgSpec::Func;
        else
            throw std::runtime_error("bad signature type '" + tok + "' in '" + sig + "'");

        // opaque:tag refinement
        if (spec.kind == ArgSpec::Opaque &&
            i < sig.size() && sig[i] == ':') {
            ++i;
            spec.tag = read_ident(i);
            if (spec.tag.empty())
                throw std::runtime_error("bad signature: opaque: missing tag");
        }

        // optional ?
        if (i < sig.size() && sig[i] == '?') {
            spec.optional = true;
            ++i;
        }
        out.push_back(spec);

        skip_ws(i);
        if (i >= sig.size()) break;
        if (sig[i] == ',') { ++i; continue; }
        throw std::runtime_error("bad signature near '" +
                                 std::string(1, sig[i]) + "' in '" + sig + "'");
    }

    // Validate: optional args must form a trailing run.
    bool seen_opt = false;
    for (auto& s : out) {
        if (s.optional) seen_opt = true;
        else if (seen_opt)
            throw std::runtime_error(
                "required arg follows optional in '" + sig + "'");
    }
    return out;
}

// ── Interpreter ───────────────────────────────────────────────────────
struct Interpreter {
    EnvPtr global;
    int stack_depth = 0;
    int max_stack = 1000;
    int function_depth = 0;                          // for return-outside-function
    int loop_depth = 0;                              // for break/continue-outside-loop
    std::vector<std::string> call_stack;             // for Error::trace
    YieldFn yield_fn;                                // cooperative-scheduling hook
    std::vector<std::weak_ptr<Env>> tracked_envs;    // for cycle breaking at shutdown
    std::unordered_set<std::string> loaded_files;    // load() memoization & cycle break
    std::unordered_map<std::string, std::string> native_docs;   // for help()
    std::mt19937_64 rng;                             // single source of randomness

    Interpreter() {
        rng.seed((uint64_t)std::chrono::high_resolution_clock::now()
                              .time_since_epoch().count());
        global = make_env();
        register_builtins();
    }

    // Closure↔Env reference cycles are unavoidable in a tree-walking interpreter
    // that captures lexical scope by shared_ptr (a function defined in env E with
    // FuncDecl ends up as E.vars[name] = Closure{env: shared_ptr_to(E)}). Rather
    // than threading weak_ptrs through Closure (which breaks the case where a
    // closure escapes its defining scope), we track every Env we create and clear
    // each one's vars at shutdown. That breaks every cycle deterministically; any
    // remaining shared_ptrs unwind normally.
    ~Interpreter() {
        for (auto& we : tracked_envs)
            if (auto e = we.lock()) e->vars.clear();
    }

    EnvPtr make_env() {
        auto e = std::make_shared<Env>();
        track_env(e);
        return e;
    }
    EnvPtr make_env(EnvPtr parent) {
        auto e = std::make_shared<Env>(std::move(parent));
        track_env(e);
        return e;
    }
    void track_env(const EnvPtr& e) {
        tracked_envs.push_back(e);
        // Periodic compaction to keep memory use bounded for long-running scripts.
        if (tracked_envs.size() >= 1024 && (tracked_envs.size() & 1023) == 0) {
            tracked_envs.erase(
                std::remove_if(tracked_envs.begin(), tracked_envs.end(),
                    [](const std::weak_ptr<Env>& w) { return w.expired(); }),
                tracked_envs.end());
        }
    }

    struct StackGuard {
        int& d;
        StackGuard(int& d) : d(d) { ++d; }
        ~StackGuard() { --d; }
    };

    void yield() { if (yield_fn) yield_fn(); }
    void set_yield(YieldFn fn) { yield_fn = std::move(fn); }

    // ── guards ────────────────────────────────────────────────────────
    static void ck(const char* nm, const std::vector<Value>& a, size_t n, int ln, const std::string& f) {
        if (a.size() != n) err(f, ln, std::string(nm) + " expects " + std::to_string(n) +
                                       " arg(s), got " + std::to_string(a.size()));
    }
    static void nv(const char* nm, const Value& v, int ln, const std::string& f) {
        if (!v.is_vec()) err(f, ln, std::string(nm) + " expects numeric");
    }
    static void ns(const char* nm, const Value& v, int ln, const std::string& f) {
        if (!v.is_str()) err(f, ln, std::string(nm) + " expects string");
    }
    static void nf(const char* nm, const Value& v, int ln, const std::string& f) {
        if (!v.is_closure() && !v.is_native())
            err(f, ln, std::string(nm) + " expects function");
    }
    static void nd(const char* nm, const Value& v, int ln, const std::string& f) {
        if (!v.is_dict()) err(f, ln, std::string(nm) + " expects dict");
    }

    void reg(const char* name, NativeFn fn) {
        global->def(name, Value(std::move(fn)));
    }
    // Register a native with an attached docstring (retrievable via help()).
    void reg_doc(const char* name, const std::string& doc, NativeFn fn) {
        native_docs[name] = doc;
        global->def(name, Value(std::move(fn)));
    }

    // Register a native with a typed signature. The signature string is
    // parsed (see parse_signature) and the supplied lambda is wrapped in a
    // checker that validates arity and per-argument type before dispatch.
    // Errors mention the function name, argument position, expected type,
    // and actual received type. The signature is also stored in native_docs
    // so help() reports it. Pass an empty doc string to use only the signature.
    void reg_typed(const char* name,
                   const std::string& sig_str,
                   const std::string& doc,
                   NativeFn fn) {
        auto specs = parse_signature(sig_str);
        size_t n_required = 0;
        for (auto& s : specs) if (!s.optional) ++n_required;
        size_t n_total = specs.size();

        std::string nm(name);
        std::string sig_show = nm + "(" + sig_str + ")";
        native_docs[nm] = doc.empty() ? sig_show : (sig_show + " — " + doc);

        NativeFn wrapped =
          [nm, specs = std::move(specs), n_required, n_total, fn = std::move(fn)]
          (const std::vector<Value>& a, int ln, const std::string& f) -> Value {
            if (a.size() < n_required || a.size() > n_total) {
                std::string msg = nm + ": expected ";
                if (n_required == n_total) msg += std::to_string(n_required);
                else msg += std::to_string(n_required) + "-" + std::to_string(n_total);
                msg += " arg" + std::string(n_total == 1 ? "" : "s")
                     + ", got " + std::to_string(a.size());
                err(f, ln, msg);
            }
            for (size_t i = 0; i < a.size(); ++i) {
                if (!match_arg(a[i], specs[i])) {
                    std::string want = arg_kind_name(specs[i].kind);
                    if (!specs[i].tag.empty()) want += ":" + specs[i].tag;
                    err(f, ln, nm + ": arg " + std::to_string(i + 1)
                             + " expected " + want
                             + ", got " + value_kind_name(a[i]));
                }
            }
            return fn(a, ln, f);
        };
        global->def(nm, Value(std::move(wrapped)));
    }

    // Public hook for hosts to register additional builtins after construction.
    void register_builtin(const std::string& name, NativeFn fn) {
        global->def(name, Value(std::move(fn)));
    }
    void register_builtin(const std::string& name, const std::string& doc, NativeFn fn) {
        native_docs[name] = doc;
        global->def(name, Value(std::move(fn)));
    }
    // Public counterpart of reg_typed for host code.
    void register_builtin_typed(const std::string& name,
                                const std::string& sig,
                                const std::string& doc,
                                NativeFn fn) {
        reg_typed(name.c_str(), sig, doc, std::move(fn));
    }

    // ── call (with tail-call trampoline + signal containment) ────────
    // The while loop both invokes a call and reuses the same C++ frame for
    // tail calls (`return f(...)`) thrown from inside the body. Stray
    // break/continue inside a closure body become errors instead of leaking
    // into the caller's loops; loop_depth is reset to 0 on entry so a closure
    // body cannot affect any enclosing loop's break/continue counts.
    Value call_value(const Value& fn_in, const std::vector<Value>& args_in,
                     int ln, const std::string& f) {
        yield();
        Value fn = fn_in;
        std::vector<Value> args = args_in;
        while (true) {
            if (fn.is_native()) return fn.as_native()(args, ln, f);
            if (!fn.is_closure()) err(f, ln, "not callable");
            const Closure& cl = fn.as_closure();
            if (args.size() != cl.params.size())
                err(f, ln, (cl.name.empty() ? "<anonymous>" : cl.name)
                    + " expects " + std::to_string(cl.params.size())
                    + " arg(s), got " + std::to_string(args.size()));

            std::string label = (cl.name.empty() ? std::string("<anonymous>") : cl.name)
                              + "() at " + f + ":" + std::to_string(ln);
            call_stack.push_back(label);
            ++function_depth;
            int saved_loop = loop_depth;
            loop_depth = 0;

            auto local = make_env(cl.env);
            for (size_t i = 0; i < args.size(); ++i) local->def(cl.params[i], args[i]);

            bool tail = false;
            Value out = Value(nullptr);
            bool returned = false;
            try {
                exec_block(cl.body, local);
            } catch (ReturnSignal& rs) {
                out = rs.val; returned = true;
            } catch (TailCall& tc) {
                fn = std::move(tc.fn);
                args = std::move(tc.args);
                tail = true;
            } catch (BreakSignal&) {
                call_stack.pop_back();
                --function_depth;
                loop_depth = saved_loop;
                err(f, ln, "break outside loop");
            } catch (ContinueSignal&) {
                call_stack.pop_back();
                --function_depth;
                loop_depth = saved_loop;
                err(f, ln, "continue outside loop");
            } catch (Error& e) {
                if (e.trace.empty())
                    e.trace.assign(call_stack.rbegin(), call_stack.rend());
                call_stack.pop_back();
                --function_depth;
                loop_depth = saved_loop;
                throw;
            } catch (...) {
                call_stack.pop_back();
                --function_depth;
                loop_depth = saved_loop;
                throw;
            }

            call_stack.pop_back();
            --function_depth;
            loop_depth = saved_loop;
            if (tail) continue;                 // reuse the C++ frame
            return returned ? out : Value(nullptr);
        }
    }

    void register_builtins() {
        // ── polymorphic: len, reverse ─────────────────────────────────
        reg("len", [](auto& a, int ln, auto& f) -> Value {
            ck("len", a, 1, ln, f);
            if (a[0].is_vec())    return Value((double)a[0].as_vec().size());
            if (a[0].is_str())    return Value((double)a[0].as_str().size());
            if (a[0].is_list())   return Value((double)a[0].as_list().size());
            if (a[0].is_dict())   return Value((double)a[0].as_dict().size());
            if (a[0].is_buffer()) return Value((double)a[0].as_buffer().n_frames);
            err(f, ln, "len: unsupported type");
        });

        reg("reverse", [](auto& a, int ln, auto& f) -> Value {
            ck("reverse", a, 1, ln, f);
            if (a[0].is_str()) {
                auto s = a[0].as_str();
                std::reverse(s.begin(), s.end());
                return Value(Str(s));
            }
            if (a[0].is_list()) {
                List l = a[0].as_list();
                std::reverse(l.begin(), l.end());
                return Value(std::move(l));
            }
            if (a[0].is_vec()) {
                auto& v = a[0].as_vec();
                Vec r(v.size());
                for (size_t i = 0; i < v.size(); ++i) r[i] = v[v.size() - 1 - i];
                return Value(r);
            }
            err(f, ln, "reverse: unsupported type");
        });

        reg("slice", [](auto& a, int ln, auto& f) -> Value {
            ck("slice", a, 3, ln, f);
            nv("slice", a[1], ln, f); nv("slice", a[2], ln, f);
            int start = (int)a[1].scalar(), stop = (int)a[2].scalar();
            if (a[0].is_vec()) {
                auto& v = a[0].as_vec();
                if (start < 0) start += (int)v.size();
                if (stop  < 0) stop  += (int)v.size();
                if (start < 0) start = 0;
                if (stop > (int)v.size()) stop = (int)v.size();
                int n = std::max(0, stop - start);
                Vec r(n);
                for (int i = 0; i < n; ++i) r[i] = v[start + i];
                return Value(r);
            }
            if (a[0].is_list()) {
                auto& l = a[0].as_list();
                if (start < 0) start += (int)l.size();
                if (stop  < 0) stop  += (int)l.size();
                if (start < 0) start = 0;
                if (stop > (int)l.size()) stop = (int)l.size();
                if (start > stop) start = stop;
                return Value(List(l.begin() + start, l.begin() + stop));
            }
            if (a[0].is_str()) {
                auto& s = a[0].as_str();
                if (start < 0) start += (int)s.size();
                if (stop  < 0) stop  += (int)s.size();
                if (start < 0) start = 0;
                if (stop > (int)s.size()) stop = (int)s.size();
                return Value(Str(s.substr(start, stop - start)));
            }
            err(f, ln, "slice: unsupported type");
        });

        reg("concat", [](auto& a, int ln, auto& f) -> Value {
            ck("concat", a, 2, ln, f);
            if (a[0].is_str() && a[1].is_str())
                return Value(Str(a[0].as_str() + a[1].as_str()));
            if (a[0].is_list() && a[1].is_list()) {
                List l = a[0].as_list();
                auto& r = a[1].as_list();
                l.insert(l.end(), r.begin(), r.end());
                return Value(std::move(l));
            }
            if (a[0].is_vec() && a[1].is_vec()) {
                auto& va = a[0].as_vec();
                auto& vb = a[1].as_vec();
                Vec r(va.size() + vb.size());
                for (size_t i = 0; i < va.size(); ++i) r[i] = va[i];
                for (size_t i = 0; i < vb.size(); ++i) r[va.size() + i] = vb[i];
                return Value(r);
            }
            if (a[0].is_dict() && a[1].is_dict()) {
                auto d = std::make_shared<DictMap>(a[0].as_dict());
                for (auto& kv : a[1].as_dict()) (*d)[kv.first] = kv.second;
                return Value(d);
            }
            err(f, ln, "concat expects two values of the same type (string, list, vec, or dict)");
        });

        // ── vec: reductions ───────────────────────────────────────────
        reg("sum",  [](auto& a, int ln, auto& f) -> Value { ck("sum",  a, 1, ln, f); nv("sum",  a[0], ln, f); return Value(a[0].as_vec().sum()); });
        reg("mean", [](auto& a, int ln, auto& f) -> Value { ck("mean", a, 1, ln, f); nv("mean", a[0], ln, f);
            auto& v = a[0].as_vec(); return Value(v.sum() / (double)v.size()); });
        reg("min",  [](auto& a, int ln, auto& f) -> Value { ck("min",  a, 1, ln, f); nv("min",  a[0], ln, f); return Value(a[0].as_vec().min()); });
        reg("max",  [](auto& a, int ln, auto& f) -> Value { ck("max",  a, 1, ln, f); nv("max",  a[0], ln, f); return Value(a[0].as_vec().max()); });

        // ── vec: element-wise math ────────────────────────────────────
        auto m1 = [this](const char* nm, Vec(*op)(const Vec&)) {
            reg(nm, [nm, op](auto& a, int ln, auto& f) -> Value {
                ck(nm, a, 1, ln, f); nv(nm, a[0], ln, f);
                return Value(op(a[0].as_vec()));
            });
        };
        m1("sqrt", [](const Vec& v) -> Vec { return std::sqrt(v); });
        m1("abs",  [](const Vec& v) -> Vec { return std::abs(v); });
        m1("sin",  [](const Vec& v) -> Vec { return std::sin(v); });
        m1("cos",  [](const Vec& v) -> Vec { return std::cos(v); });
        m1("tan",  [](const Vec& v) -> Vec { return std::tan(v); });
        m1("exp",  [](const Vec& v) -> Vec { return std::exp(v); });
        m1("log",  [](const Vec& v) -> Vec { return std::log(v); });
        m1("asin", [](const Vec& v) -> Vec { return std::asin(v); });
        m1("acos", [](const Vec& v) -> Vec { return std::acos(v); });
        m1("atan", [](const Vec& v) -> Vec { return std::atan(v); });

        reg("floor", [](auto& a, int ln, auto& f) -> Value {
            ck("floor", a, 1, ln, f); nv("floor", a[0], ln, f);
            auto& v = a[0].as_vec(); Vec r(v.size());
            for (size_t i = 0; i < v.size(); ++i) r[i] = std::floor(v[i]);
            return Value(r);
        });
        reg("ceil", [](auto& a, int ln, auto& f) -> Value {
            ck("ceil", a, 1, ln, f); nv("ceil", a[0], ln, f);
            auto& v = a[0].as_vec(); Vec r(v.size());
            for (size_t i = 0; i < v.size(); ++i) r[i] = std::ceil(v[i]);
            return Value(r);
        });
        reg("round", [](auto& a, int ln, auto& f) -> Value {
            ck("round", a, 1, ln, f); nv("round", a[0], ln, f);
            auto& v = a[0].as_vec(); Vec r(v.size());
            for (size_t i = 0; i < v.size(); ++i) r[i] = std::round(v[i]);
            return Value(r);
        });
        reg("pow", [](auto& a, int ln, auto& f) -> Value {
            ck("pow", a, 2, ln, f); nv("pow", a[0], ln, f); nv("pow", a[1], ln, f);
            // Broadcast explicitly: std::pow on valarrays of mismatched
            // sizes is undefined behavior in libstdc++.
            Vec va = a[0].as_vec(), vb = a[1].as_vec();
            if (va.size() == 1 && vb.size() > 1) { Vec t(vb.size()); t = va[0]; va = t; }
            if (vb.size() == 1 && va.size() > 1) { Vec t(va.size()); t = vb[0]; vb = t; }
            if (va.size() != vb.size())
                err(f, ln, "pow: vector size mismatch (" + std::to_string(va.size()) +
                           " vs " + std::to_string(vb.size()) + ")");
            Vec r(va.size());
            for (size_t i = 0; i < va.size(); ++i) r[i] = std::pow(va[i], vb[i]);
            return Value(r);
        });
        reg("sort", [](auto& a, int ln, auto& f) -> Value {
            ck("sort", a, 1, ln, f); nv("sort", a[0], ln, f);
            auto v = a[0].as_vec();
            std::sort(std::begin(v), std::end(v));
            return Value(v);
        });

        // ── vec: constructors ─────────────────────────────────────────
        reg("range", [](auto& a, int ln, auto& f) -> Value {
            if (a.size() < 1 || a.size() > 3) err(f, ln, "range expects 1-3 args");
            nv("range", a[0], ln, f);
            double start = 0, stop, step = 1;
            if (a.size() == 1) { stop = a[0].scalar(); }
            else { start = a[0].scalar(); stop = a[1].scalar();
                   if (a.size() == 3) { nv("range", a[2], ln, f); step = a[2].scalar(); } }
            int n = std::max(0, (int)std::ceil((stop - start) / step));
            Vec r(n);
            for (int i = 0; i < n; ++i) r[i] = start + i * step;
            return Value(r);
        });
        reg("zeros", [](auto& a, int ln, auto& f) -> Value {
            ck("zeros", a, 1, ln, f); nv("zeros", a[0], ln, f);
            return Value(Vec(0.0, (size_t)a[0].scalar()));
        });
        reg("ones", [](auto& a, int ln, auto& f) -> Value {
            ck("ones", a, 1, ln, f); nv("ones", a[0], ln, f);
            return Value(Vec(1.0, (size_t)a[0].scalar()));
        });
        reg("rand", [this](auto& a, int ln, auto& f) -> Value {
            int n = 1;
            if (!a.empty()) { nv("rand", a[0], ln, f); n = (int)a[0].scalar(); }
            std::uniform_real_distribution<double> dist(0.0, 1.0);
            Vec r(n);
            for (int i = 0; i < n; ++i) r[i] = dist(rng);
            return Value(r);
        });
        reg("seed", [this](auto& a, int ln, auto& f) -> Value {
            ck("seed", a, 1, ln, f); nv("seed", a[0], ln, f);
            rng.seed((uint64_t)a[0].scalar());
            return Value(nullptr);
        });

        // ── list operations ───────────────────────────────────────────
        reg("list", [](auto& a, int, auto&) -> Value {
            return Value(List(a.begin(), a.end()));
        });
        // push: mutates the list in place and returns it (reference semantics).
        reg("push", [](auto& a, int ln, auto& f) -> Value {
            ck("push", a, 2, ln, f);
            if (!a[0].is_list()) err(f, ln, "push expects list");
            a[0].as_list_mut().push_back(a[1]);
            return a[0];
        });
        reg("pop", [](auto& a, int ln, auto& f) -> Value {
            ck("pop", a, 1, ln, f);
            if (!a[0].is_list()) err(f, ln, "pop expects list");
            auto& l = a[0].as_list_mut();
            if (l.empty()) err(f, ln, "pop: empty list");
            Value v = std::move(l.back());
            l.pop_back();
            return v;
        });
        reg("insert", [](auto& a, int ln, auto& f) -> Value {
            ck("insert", a, 3, ln, f);
            if (!a[0].is_list()) err(f, ln, "insert expects list");
            nv("insert", a[1], ln, f);
            auto& l = a[0].as_list_mut();
            int i = (int)a[1].scalar();
            if (i < 0) i += (int)l.size();
            if (i < 0 || i > (int)l.size()) err(f, ln, "insert: index out of range");
            l.insert(l.begin() + i, a[2]);
            return a[0];
        });
        // remove: polymorphic — list by index, dict by key.
        reg("remove", [](auto& a, int ln, auto& f) -> Value {
            ck("remove", a, 2, ln, f);
            if (a[0].is_list()) {
                nv("remove", a[1], ln, f);
                auto& l = a[0].as_list_mut();
                int i = (int)a[1].scalar();
                if (i < 0) i += (int)l.size();
                if (i < 0 || i >= (int)l.size()) err(f, ln, "remove: index out of range");
                Value v = std::move(l[i]);
                l.erase(l.begin() + i);
                return v;
            }
            if (a[0].is_dict()) {
                ns("remove", a[1], ln, f);
                auto& d = a[0].as_dict_mut();
                auto it = d.find(a[1].as_str());
                if (it == d.end()) return Value(nullptr);
                Value v = std::move(it->second);
                d.erase(it);
                return v;
            }
            err(f, ln, "remove expects list or dict");
        });
        reg("copy", [](auto& a, int ln, auto& f) -> Value {
            ck("copy", a, 1, ln, f);
            if (a[0].is_list())   return Value(List(a[0].as_list()));
            if (a[0].is_vec())    return Value(Vec(a[0].as_vec()));
            if (a[0].is_str())    return Value(Str(a[0].as_str()));
            if (a[0].is_dict())   return Value(std::make_shared<DictMap>(a[0].as_dict()));
            if (a[0].is_buffer()) return Value(std::make_shared<Buffer>(a[0].as_buffer()));
            return a[0];
        });

        // ── dict operations ───────────────────────────────────────────
        // dict()                — empty dict
        // dict(list-of-pairs)   — build from list of [key, value] 2-element lists
        reg("dict", [](auto& a, int ln, auto& f) -> Value {
            auto d = std::make_shared<DictMap>();
            if (a.empty()) return Value(d);
            ck("dict", a, 1, ln, f);
            if (!a[0].is_list()) err(f, ln, "dict expects a list of [key,value] pairs");
            for (auto& el : a[0].as_list()) {
                if (!el.is_list() || el.as_list().size() != 2)
                    err(f, ln, "dict: each element must be [key,value]");
                auto& pair = el.as_list();
                if (!pair[0].is_str()) err(f, ln, "dict: keys must be strings");
                (*d)[pair[0].as_str()] = pair[1];
            }
            return Value(d);
        });
        reg("keys", [](auto& a, int ln, auto& f) -> Value {
            ck("keys", a, 1, ln, f); nd("keys", a[0], ln, f);
            std::vector<std::string> ks;
            for (auto& kv : a[0].as_dict()) ks.push_back(kv.first);
            std::sort(ks.begin(), ks.end());
            List out;
            out.reserve(ks.size());
            for (auto& k : ks) out.push_back(Value(Str(k)));
            return Value(std::move(out));
        });
        reg("values", [](auto& a, int ln, auto& f) -> Value {
            ck("values", a, 1, ln, f); nd("values", a[0], ln, f);
            auto& d = a[0].as_dict();
            std::vector<std::string> ks;
            for (auto& kv : d) ks.push_back(kv.first);
            std::sort(ks.begin(), ks.end());
            List out;
            out.reserve(ks.size());
            for (auto& k : ks) out.push_back(d.at(k));
            return Value(std::move(out));
        });
        reg("has", [](auto& a, int ln, auto& f) -> Value {
            ck("has", a, 2, ln, f);
            if (!a[0].is_dict()) err(f, ln, "has expects dict");
            ns("has", a[1], ln, f);
            return Value(a[0].as_dict().count(a[1].as_str()) ? 1.0 : 0.0);
        });
        reg("get", [](auto& a, int ln, auto& f) -> Value {
            if (a.size() != 2 && a.size() != 3) err(f, ln, "get expects 2 or 3 args");
            if (!a[0].is_dict()) err(f, ln, "get expects dict");
            ns("get", a[1], ln, f);
            auto& d = a[0].as_dict();
            auto it = d.find(a[1].as_str());
            if (it != d.end()) return it->second;
            return a.size() == 3 ? a[2] : Value(nullptr);
        });

        // ── higher-order: map, filter, reduce, each, apply ────────────
        reg("map", [this](auto& a, int ln, auto& f) -> Value {
            ck("map", a, 2, ln, f);
            if (!a[0].is_list()) err(f, ln, "map expects list");
            nf("map", a[1], ln, f);
            List out;
            for (auto& x : a[0].as_list()) out.push_back(call_value(a[1], {x}, ln, f));
            return Value(std::move(out));
        });
        reg("filter", [this](auto& a, int ln, auto& f) -> Value {
            ck("filter", a, 2, ln, f);
            if (!a[0].is_list()) err(f, ln, "filter expects list");
            nf("filter", a[1], ln, f);
            List out;
            for (auto& x : a[0].as_list())
                if (call_value(a[1], {x}, ln, f).truthy()) out.push_back(x);
            return Value(std::move(out));
        });
        reg("reduce", [this](auto& a, int ln, auto& f) -> Value {
            ck("reduce", a, 3, ln, f);
            if (!a[0].is_list()) err(f, ln, "reduce expects list");
            nf("reduce", a[1], ln, f);
            Value acc = a[2];
            for (auto& x : a[0].as_list()) acc = call_value(a[1], {acc, x}, ln, f);
            return acc;
        });
        reg("each", [this](auto& a, int ln, auto& f) -> Value {
            ck("each", a, 2, ln, f);
            if (!a[0].is_list()) err(f, ln, "each expects list");
            nf("each", a[1], ln, f);
            for (auto& x : a[0].as_list()) call_value(a[1], {x}, ln, f);
            return Value(nullptr);
        });
        reg("apply", [this](auto& a, int ln, auto& f) -> Value {
            ck("apply", a, 2, ln, f);
            nf("apply", a[0], ln, f);
            if (!a[1].is_list()) err(f, ln, "apply expects list of arguments");
            std::vector<Value> args(a[1].as_list().begin(), a[1].as_list().end());
            return call_value(a[0], args, ln, f);
        });

        // ── string operations ─────────────────────────────────────────
        reg("upper", [](auto& a, int ln, auto& f) -> Value {
            ck("upper", a, 1, ln, f); ns("upper", a[0], ln, f);
            auto s = a[0].as_str();
            for (auto& c : s) c = std::toupper((unsigned char)c);
            return Value(Str(s));
        });
        reg("lower", [](auto& a, int ln, auto& f) -> Value {
            ck("lower", a, 1, ln, f); ns("lower", a[0], ln, f);
            auto s = a[0].as_str();
            for (auto& c : s) c = std::tolower((unsigned char)c);
            return Value(Str(s));
        });
        reg("trim", [](auto& a, int ln, auto& f) -> Value {
            ck("trim", a, 1, ln, f); ns("trim", a[0], ln, f);
            auto s = a[0].as_str();
            auto l = s.find_first_not_of(" \t\n\r");
            if (l == std::string::npos) return Value(Str(""));
            return Value(Str(s.substr(l, s.find_last_not_of(" \t\n\r") - l + 1)));
        });
        reg("split", [](auto& a, int ln, auto& f) -> Value {
            ck("split", a, 2, ln, f); ns("split", a[0], ln, f); ns("split", a[1], ln, f);
            List parts;
            auto& s = a[0].as_str();
            auto& d = a[1].as_str();
            size_t st = 0, p;
            while ((p = s.find(d, st)) != std::string::npos) {
                parts.push_back(Value(Str(s.substr(st, p - st))));
                st = p + d.size();
            }
            parts.push_back(Value(Str(s.substr(st))));
            return Value(std::move(parts));
        });
        reg("join", [](auto& a, int ln, auto& f) -> Value {
            ck("join", a, 2, ln, f);
            if (!a[0].is_list()) err(f, ln, "join expects list");
            ns("join", a[1], ln, f);
            std::string r;
            auto& l = a[0].as_list();
            auto& sep = a[1].as_str();
            for (size_t i = 0; i < l.size(); ++i) {
                if (i) r += sep;
                r += l[i].repr();
            }
            return Value(Str(r));
        });
        reg("substr", [](auto& a, int ln, auto& f) -> Value {
            ck("substr", a, 3, ln, f); ns("substr", a[0], ln, f);
            nv("substr", a[1], ln, f); nv("substr", a[2], ln, f);
            return Value(Str(a[0].as_str().substr(
                (size_t)a[1].scalar(), (size_t)a[2].scalar())));
        });
        reg("find", [](auto& a, int ln, auto& f) -> Value {
            ck("find", a, 2, ln, f); ns("find", a[0], ln, f); ns("find", a[1], ln, f);
            auto p = a[0].as_str().find(a[1].as_str());
            return Value(p == std::string::npos ? -1.0 : (double)p);
        });
        reg("replace", [](auto& a, int ln, auto& f) -> Value {
            ck("replace", a, 3, ln, f);
            ns("replace", a[0], ln, f); ns("replace", a[1], ln, f); ns("replace", a[2], ln, f);
            auto s = a[0].as_str();
            auto& from = a[1].as_str();
            auto& to = a[2].as_str();
            if (from.empty()) return Value(Str(s));
            size_t p = 0;
            while ((p = s.find(from, p)) != std::string::npos) {
                s.replace(p, from.size(), to);
                p += to.size();
            }
            return Value(Str(s));
        });

        // format("hello {}, you have {} new", name, count)
        // {{ and }} are escaped braces.
        reg("format", [](auto& a, int ln, auto& f) -> Value {
            if (a.empty()) err(f, ln, "format expects at least 1 arg");
            ns("format", a[0], ln, f);
            auto& fmt = a[0].as_str();
            std::string out;
            size_t arg_idx = 1;
            for (size_t i = 0; i < fmt.size(); ++i) {
                if (i + 1 < fmt.size() && fmt[i] == '{' && fmt[i+1] == '{') {
                    out += '{'; ++i; continue;
                }
                if (i + 1 < fmt.size() && fmt[i] == '}' && fmt[i+1] == '}') {
                    out += '}'; ++i; continue;
                }
                if (i + 1 < fmt.size() && fmt[i] == '{' && fmt[i+1] == '}') {
                    if (arg_idx < a.size()) out += a[arg_idx++].repr();
                    else out += "{}";
                    ++i; continue;
                }
                out += fmt[i];
            }
            return Value(Str(out));
        });

        // out(args...) — write reprs to stdout with no separator and no newline.
        reg("out", [](auto& a, int, auto&) -> Value {
            for (auto& v : a) std::cout << v.repr();
            std::cout.flush();
            return Value(nullptr);
        });

        // ── regex ─────────────────────────────────────────────────────
        reg("match", [](auto& a, int ln, auto& f) -> Value {
            ck("match", a, 2, ln, f); ns("match", a[0], ln, f); ns("match", a[1], ln, f);
            try {
                std::regex re(a[1].as_str());
                std::smatch m;
                if (std::regex_search(a[0].as_str(), m, re)) {
                    List l;
                    for (auto& g : m) l.push_back(Value(Str(g.str())));
                    return Value(std::move(l));
                }
                return Value(nullptr);
            } catch (std::regex_error&) {
                err(f, ln, "invalid regex: " + a[1].as_str());
            }
        });

        // ── type / conversion ─────────────────────────────────────────
        reg("type", [](auto& a, int ln, auto& f) -> Value {
            ck("type", a, 1, ln, f);
            if (a[0].is_nil()) return Value(Str("nil"));
            if (a[0].is_vec() && a[0].as_vec().size() == 1) return Value(Str("scalar"));
            if (a[0].is_vec())    return Value(Str("vec"));
            if (a[0].is_str())    return Value(Str("string"));
            if (a[0].is_list())   return Value(Str("list"));
            if (a[0].is_dict())   return Value(Str("dict"));
            if (a[0].is_buffer()) return Value(Str("buffer"));
            if (a[0].is_opaque()) return Value(Str("opaque"));
            return Value(Str("func"));
        });
        reg("str", [](auto& a, int ln, auto& f) -> Value {
            ck("str", a, 1, ln, f);
            return Value(Str(a[0].repr()));
        });
        reg("num", [](auto& a, int ln, auto& f) -> Value {
            ck("num", a, 1, ln, f); ns("num", a[0], ln, f);
            try { return Value(std::stod(a[0].as_str())); }
            catch (...) { err(f, ln, "num: cannot parse '" + a[0].as_str() + "'"); }
        });
        reg("vec", [](auto& a, int ln, auto& f) -> Value {
            ck("vec", a, 1, ln, f);
            if (a[0].is_list()) {
                auto& l = a[0].as_list();
                Vec v(l.size());
                for (size_t i = 0; i < l.size(); ++i) {
                    if (!l[i].is_vec()) err(f, ln, "vec: element not numeric");
                    v[i] = l[i].scalar();
                }
                return Value(v);
            }
            err(f, ln, "vec expects list");
        });

        // ── I/O (relative paths resolved from cwd, not from source file) ─
        reg("read", [](auto& a, int ln, auto& f) -> Value {
            ck("read", a, 1, ln, f); ns("read", a[0], ln, f);
            auto p = normalize_path(a[0].as_str(), "");
            std::ifstream fs(p);
            if (!fs) err(f, ln, "cannot open " + p.string());
            std::ostringstream ss; ss << fs.rdbuf();
            return Value(Str(ss.str()));
        });
        reg("write", [](auto& a, int ln, auto& f) -> Value {
            ck("write", a, 2, ln, f); ns("write", a[0], ln, f);
            auto p = normalize_path(a[0].as_str(), "");
            std::ofstream fs(p);
            if (!fs) err(f, ln, "cannot open " + p.string());
            fs << a[1].repr();
            return Value(nullptr);
        });
        reg("append", [](auto& a, int ln, auto& f) -> Value {
            ck("append", a, 2, ln, f); ns("append", a[0], ln, f);
            auto p = normalize_path(a[0].as_str(), "");
            std::ofstream fs(p, std::ios::app);
            if (!fs) err(f, ln, "cannot open " + p.string());
            fs << a[1].repr();
            return Value(nullptr);
        });

        // ── system ────────────────────────────────────────────────────
        reg("exec", [](auto& a, int ln, auto& f) -> Value {
            ck("exec", a, 1, ln, f); ns("exec", a[0], ln, f);
            std::array<char, 256> buf;
            std::string out;
            FILE* p = popen(a[0].as_str().c_str(), "r");
            if (!p) err(f, ln, "exec failed");
            while (fgets(buf.data(), buf.size(), p)) out += buf.data();
            pclose(p);
            return Value(Str(out));
        });
        reg("exit", [](auto& a, int ln, auto& f) -> Value {
            int code = 0;
            if (!a.empty()) { nv("exit", a[0], ln, f); code = (int)a[0].scalar(); }
            std::exit(code);
            return Value(nullptr);
        });
        reg("env", [](auto& a, int ln, auto& f) -> Value {
            ck("env", a, 1, ln, f); ns("env", a[0], ln, f);
            auto* v = std::getenv(a[0].as_str().c_str());
            return v ? Value(Str(v)) : Value(nullptr);
        });
        reg("clock", [](auto& a, int, auto&) -> Value {
            (void)a;
            auto t = std::chrono::high_resolution_clock::now();
            return Value((double)std::chrono::duration_cast<std::chrono::microseconds>(
                t.time_since_epoch()).count() / 1e6);
        });
        reg("sleep", [](auto& a, int ln, auto& f) -> Value {
            ck("sleep", a, 1, ln, f); nv("sleep", a[0], ln, f);
            double s = a[0].scalar();
            if (s < 0) s = 0;
            std::this_thread::sleep_for(std::chrono::duration<double>(s));
            return Value(nullptr);
        });
        reg("error", [](auto& a, int ln, auto& f) -> Value {
            ck("error", a, 1, ln, f);
            err(f, ln, a[0].repr());
        });

        // ── character codes ───────────────────────────────────────────
        reg("char", [](auto& a, int ln, auto& f) -> Value {
            ck("char", a, 1, ln, f); nv("char", a[0], ln, f);
            int code = (int)a[0].scalar();
            if (code < 0 || code > 255) err(f, ln, "char: code must be 0-255");
            return Value(Str(1, (char)code));
        });
        reg("asc", [](auto& a, int ln, auto& f) -> Value {
            ck("asc", a, 1, ln, f); ns("asc", a[0], ln, f);
            if (a[0].as_str().empty()) err(f, ln, "asc: empty string");
            return Value((double)(unsigned char)a[0].as_str()[0]);
        });

        // ── shuffle: returns a shuffled copy ──────────────────────────
        reg("shuffle", [this](auto& a, int ln, auto& f) -> Value {
            ck("shuffle", a, 1, ln, f);
            if (a[0].is_list()) {
                List l = a[0].as_list();
                for (size_t i = l.size(); i > 1; --i) {
                    std::uniform_int_distribution<size_t> dist(0, i - 1);
                    std::swap(l[i - 1], l[dist(rng)]);
                }
                return Value(std::move(l));
            }
            if (a[0].is_vec()) {
                Vec v = a[0].as_vec();
                for (size_t i = v.size(); i > 1; --i) {
                    std::uniform_int_distribution<size_t> dist(0, i - 1);
                    std::swap(v[i - 1], v[dist(rng)]);
                }
                return Value(v);
            }
            err(f, ln, "shuffle expects list or vec");
        });

        // ── stdin ─────────────────────────────────────────────────────
        reg("input", [](auto& a, int ln, auto& f) -> Value {
            if (a.size() > 1) err(f, ln, "input expects 0 or 1 args");
            if (!a.empty()) {
                ns("input", a[0], ln, f);
                std::cout << a[0].as_str() << std::flush;
            }
            std::string line;
            if (!std::getline(std::cin, line)) return Value(nullptr);
            return Value(Str(line));
        });

        // ── buffer (audio) ────────────────────────────────────────────
        // buffer(frames)              — mono, default sample rate
        // buffer(frames, channels)    — multi-channel, default sample rate
        // buffer(frames, channels, sr)
        reg_doc("buffer",
                "buffer(frames [, channels [, sample_rate]]) — allocate an audio buffer (interleaved, doubles).",
                [](auto& a, int ln, auto& f) -> Value {
            if (a.size() < 1 || a.size() > 3)
                err(f, ln, "buffer expects 1-3 args (frames [, channels [, sample_rate]])");
            for (auto& x : a) nv("buffer", x, ln, f);
            auto b = std::make_shared<Buffer>();
            b->n_frames    = (size_t)std::max(0.0, a[0].scalar());
            b->n_channels  = a.size() >= 2 ? (size_t)std::max(1.0, a[1].scalar()) : 1;
            b->sample_rate = a.size() >= 3 ? a[2].scalar() : 44100.0;
            b->data.assign(b->n_frames * b->n_channels, 0.0);
            return Value(b);
        });
        reg_doc("frames", "frames(buffer) — number of frames.",
                [](auto& a, int ln, auto& f) -> Value {
            ck("frames", a, 1, ln, f);
            if (!a[0].is_buffer()) err(f, ln, "frames expects buffer");
            return Value((double)a[0].as_buffer().n_frames);
        });
        reg_doc("channels", "channels(buffer) — number of channels.",
                [](auto& a, int ln, auto& f) -> Value {
            ck("channels", a, 1, ln, f);
            if (!a[0].is_buffer()) err(f, ln, "channels expects buffer");
            return Value((double)a[0].as_buffer().n_channels);
        });
        reg_doc("sample_rate", "sample_rate(buffer) — sample rate in Hz.",
                [](auto& a, int ln, auto& f) -> Value {
            ck("sample_rate", a, 1, ln, f);
            if (!a[0].is_buffer()) err(f, ln, "sample_rate expects buffer");
            return Value(a[0].as_buffer().sample_rate);
        });
        // Convert a mono buffer to a Vec (for arithmetic / DSP in flux).
        reg_doc("buffer_to_vec",
                "buffer_to_vec(buffer) — return Vec of all samples (interleaved if multi-channel).",
                [](auto& a, int ln, auto& f) -> Value {
            ck("buffer_to_vec", a, 1, ln, f);
            if (!a[0].is_buffer()) err(f, ln, "buffer_to_vec expects buffer");
            auto& b = a[0].as_buffer();
            Vec v(b.data.size());
            for (size_t i = 0; i < b.data.size(); ++i) v[i] = b.data[i];
            return Value(v);
        });
        // Build a mono buffer from a Vec.
        reg_doc("vec_to_buffer",
                "vec_to_buffer(vec [, sample_rate]) — build a mono buffer from a Vec.",
                [](auto& a, int ln, auto& f) -> Value {
            if (a.size() < 1 || a.size() > 2) err(f, ln, "vec_to_buffer expects 1 or 2 args");
            if (!a[0].is_vec()) err(f, ln, "vec_to_buffer expects vec");
            auto b = std::make_shared<Buffer>();
            auto& v = a[0].as_vec();
            b->n_frames = v.size();
            b->n_channels = 1;
            b->sample_rate = a.size() == 2 ? (nv("vec_to_buffer", a[1], ln, f), a[1].scalar())
                                           : 44100.0;
            b->data.assign(v.size(), 0.0);
            for (size_t i = 0; i < v.size(); ++i) b->data[i] = v[i];
            return Value(b);
        });

        // ── opaque ────────────────────────────────────────────────────
        // No constructor from Flux: opaques originate in C++ host code.
        // Flux can only inspect the type tag and pass them around.
        reg_doc("opaque_type",
                "opaque_type(opaque) — return the type tag string of an opaque value.",
                [](auto& a, int ln, auto& f) -> Value {
            ck("opaque_type", a, 1, ln, f);
            if (!a[0].is_opaque()) err(f, ln, "opaque_type expects opaque");
            return Value(Str(a[0].as_opaque().type_tag));
        });

        // ── help & introspection ──────────────────────────────────────
        reg_doc("help",
                "help(fn|name) — return docstring for a closure or named native.",
                [this](auto& a, int ln, auto& f) -> Value {
            ck("help", a, 1, ln, f);
            if (a[0].is_closure()) {
                auto& c = a[0].as_closure();
                if (c.doc.empty() && !c.name.empty()) {
                    // Try the native_docs registry by closure name as a fallback.
                    auto it = native_docs.find(c.name);
                    if (it != native_docs.end()) return Value(Str(it->second));
                }
                return Value(Str(c.doc.empty() ? "<no documentation>" : c.doc));
            }
            if (a[0].is_native()) {
                return Value(Str("<native function — call help with its name as a string>"));
            }
            if (a[0].is_str()) {
                auto& name = a[0].as_str();
                auto it = native_docs.find(name);
                if (it != native_docs.end()) return Value(Str(it->second));
                // Look the name up in the global env in case it's a closure.
                auto* v = global->find(name);
                if (v && v->is_closure()) {
                    auto& c = v->as_closure();
                    return Value(Str(c.doc.empty() ? "<no documentation>" : c.doc));
                }
                return Value(Str("<no documentation for: " + name + ">"));
            }
            err(f, ln, "help expects a function or its name as a string");
        });

        // bench(thunk) — call thunk() and return the wall-clock seconds it took.
        reg_doc("bench",
                "bench(thunk) — call thunk() and return elapsed wall-clock seconds.",
                [this](auto& a, int ln, auto& f) -> Value {
            ck("bench", a, 1, ln, f); nf("bench", a[0], ln, f);
            auto t0 = std::chrono::high_resolution_clock::now();
            call_value(a[0], {}, ln, f);
            auto t1 = std::chrono::high_resolution_clock::now();
            return Value(std::chrono::duration<double>(t1 - t0).count());
        });

        // ── constants ─────────────────────────────────────────────────
        global->def("pi",    Value(3.14159265358979323846));
        global->def("e",     Value(2.71828182845904523536));
        global->def("inf",   Value(std::numeric_limits<double>::infinity()));
        global->def("nil",   Value(nullptr));
        global->def("true",  Value(1.0));
        global->def("false", Value(0.0));
        global->def("flux_version", Value(Str(FLUX_VERSION)));
    }

    // ── broadcast binary op ───────────────────────────────────────────
    Value vec_binop(const Value& a, const Value& b, const std::string& op,
                    int ln, const std::string& f) {
        // Equality / inequality: scalar 0/1 via deep_eq for any non-(vec,vec)
        // pair (mixed types, lists, dicts, strings, nil). Vec-vs-vec keeps
        // element-wise broadcasting.
        if (op == "==" || op == "!=") {
            if (!a.is_vec() || !b.is_vec()) {
                bool eq = Value::deep_eq(a, b);
                return Value(op == "==" ? (eq ? 1.0 : 0.0) : (eq ? 0.0 : 1.0));
            }
        }
        if (!a.is_vec() || !b.is_vec()) err(f, ln, "invalid operands for '" + op + "'");
        Vec va = a.as_vec(), vb = b.as_vec();
        if (va.size() == 1 && vb.size() > 1) { Vec t(vb.size()); t = va[0]; va = t; }
        if (vb.size() == 1 && va.size() > 1) { Vec t(va.size()); t = vb[0]; vb = t; }
        if (va.size() != vb.size())
            err(f, ln, "vector size mismatch (" + std::to_string(va.size()) + " vs " + std::to_string(vb.size()) + ")");
        Vec r(va.size());
        if      (op == "+") r = va + vb;
        else if (op == "-") r = va - vb;
        else if (op == "*") r = va * vb;
        else if (op == "/") r = va / vb;
        else if (op == "%") { for (size_t i = 0; i < va.size(); ++i) r[i] = std::fmod(va[i], vb[i]); }
        else if (op == "==") { for (size_t i = 0; i < va.size(); ++i) r[i] = va[i] == vb[i]; }
        else if (op == "!=") { for (size_t i = 0; i < va.size(); ++i) r[i] = va[i] != vb[i]; }
        else if (op == "<")  { for (size_t i = 0; i < va.size(); ++i) r[i] = va[i] <  vb[i]; }
        else if (op == ">")  { for (size_t i = 0; i < va.size(); ++i) r[i] = va[i] >  vb[i]; }
        else if (op == "<=") { for (size_t i = 0; i < va.size(); ++i) r[i] = va[i] <= vb[i]; }
        else if (op == ">=") { for (size_t i = 0; i < va.size(); ++i) r[i] = va[i] >= vb[i]; }
        else err(f, ln, "unknown op '" + op + "'");
        return Value(r);
    }

    // ── lvalue resolution for indexed/member assignment ───────────────
    // Returns a pointer to the Value at the leftmost target location.
    // Handles arbitrarily nested Index / Member chains (a[i].b[j].c).
    // Note: dict reads here DO NOT auto-create missing keys — that would
    // leave dangling nil entries when a chain like a.b.c = v fails because
    // a.b is missing. The terminal write through apply_index_assign is what
    // creates new keys in dicts.
    Value* eval_lvalue(ExprPtr e, EnvPtr env) {
        if (e->type == NodeT::Id) {
            Value* p = env->find(e->str_val);
            if (!p) err(e->src_file(), e->line, "undefined: " + e->str_val);
            return p;
        }
        if (e->type == NodeT::Index) {
            if (!e->args.empty())
                err(e->src_file(), e->line,
                    "multi-index can only appear in the terminal position of an assignment");
            Value* outer = eval_lvalue(e->left, env);
            Value idx_v = eval(e->right, env);
            if (outer->is_list()) {
                if (!idx_v.is_vec()) err(e->src_file(), e->line, "list index must be numeric");
                int i = (int)idx_v.scalar();
                auto& l = outer->as_list_mut();
                if (i < 0) i += (int)l.size();
                if (i < 0 || i >= (int)l.size())
                    err(e->src_file(), e->line, "index out of range");
                return &l[i];
            }
            if (outer->is_dict()) {
                if (!idx_v.is_str()) err(e->src_file(), e->line, "dict key must be string");
                auto& d = outer->as_dict_mut();
                auto it = d.find(idx_v.as_str());
                if (it == d.end())
                    err(e->src_file(), e->line, "no such key: " + idx_v.as_str());
                return &it->second;
            }
            err(e->src_file(), e->line, "cannot index this type for assignment");
        }
        if (e->type == NodeT::Member) {
            Value* outer = eval_lvalue(e->left, env);
            if (!outer->is_dict())
                err(e->src_file(), e->line, "member access requires dict");
            auto& d = outer->as_dict_mut();
            auto it = d.find(e->str_val);
            if (it == d.end())
                err(e->src_file(), e->line, "no such member: " + e->str_val);
            return &it->second;
        }
        err(e->src_file(), e->line, "invalid assignment target");
    }

    void apply_buffer_assign(Value& container, const std::vector<Value>& indices,
                             Value val, int ln, const std::string& f) {
        auto& b = container.as_buffer_mut();
        if (indices.size() == 1) {
            if (!indices[0].is_vec()) err(f, ln, "buffer index must be numeric");
            int i = (int)indices[0].scalar();
            if (i < 0) i += (int)b.n_frames;
            if (i < 0 || i >= (int)b.n_frames) err(f, ln, "buffer frame index out of range");
            if (b.n_channels == 1) {
                if (!val.is_vec() || val.as_vec().size() != 1)
                    err(f, ln, "mono buffer assignment requires a scalar");
                b.data[i] = val.scalar();
            } else {
                if (!val.is_vec() || val.as_vec().size() != b.n_channels)
                    err(f, ln, "multichannel buffer frame assignment requires vec of size n_channels");
                auto& v = val.as_vec();
                for (size_t c = 0; c < b.n_channels; ++c)
                    b.data[i * b.n_channels + c] = v[c];
            }
            return;
        }
        if (indices.size() == 2) {
            if (!indices[0].is_vec() || !indices[1].is_vec())
                err(f, ln, "buffer indices must be numeric");
            int i = (int)indices[0].scalar();
            int c = (int)indices[1].scalar();
            if (i < 0) i += (int)b.n_frames;
            if (c < 0) c += (int)b.n_channels;
            if (i < 0 || i >= (int)b.n_frames) err(f, ln, "buffer frame index out of range");
            if (c < 0 || c >= (int)b.n_channels) err(f, ln, "buffer channel index out of range");
            if (!val.is_vec() || val.as_vec().size() != 1)
                err(f, ln, "buffer element assignment requires a scalar");
            b.data[i * b.n_channels + c] = val.scalar();
            return;
        }
        err(f, ln, "buffer assignment expects 1 or 2 indices");
    }

    void apply_index_assign(Value& container, const Value& idx, Value val,
                            int ln, const std::string& f) {
        if (container.is_list()) {
            if (!idx.is_vec()) err(f, ln, "list index must be numeric");
            int i = (int)idx.scalar();
            auto& l = container.as_list_mut();
            if (i < 0) i += (int)l.size();
            if (i < 0 || i >= (int)l.size()) err(f, ln, "index out of range");
            l[i] = std::move(val);
            return;
        }
        if (container.is_vec()) {
            if (!idx.is_vec()) err(f, ln, "vec index must be numeric");
            int i = (int)idx.scalar();
            if (!val.is_vec() || val.as_vec().size() != 1)
                err(f, ln, "vec element assignment requires a scalar");
            auto& v = std::get<Vec>(container.data);
            if (i < 0) i += (int)v.size();
            if (i < 0 || i >= (int)v.size()) err(f, ln, "index out of range");
            v[i] = val.scalar();
            return;
        }
        if (container.is_dict()) {
            if (!idx.is_str()) err(f, ln, "dict key must be string");
            container.as_dict_mut()[idx.as_str()] = std::move(val);
            return;
        }
        err(f, ln, "cannot assign to indexed value of this type");
    }

    // ── eval ──────────────────────────────────────────────────────────
    Value eval(ExprPtr e, EnvPtr env) {
        StackGuard sg(stack_depth);
        if (stack_depth > max_stack)
            err(e->src_file(), e->line, "stack overflow (depth " + std::to_string(max_stack) + ")");
        auto& f = e->src_file();
        int ln = e->line;
        switch (e->type) {
        case NodeT::Num: return Value(e->num_val);
        case NodeT::Str: return Value(e->str_val);
        case NodeT::Id: {
            auto* v = env->find(e->str_val);
            if (!v) err(f, ln, "undefined: " + e->str_val);
            return *v;
        }
        case NodeT::VecLit: {
            Vec v(e->args.size());
            for (size_t i = 0; i < e->args.size(); ++i) {
                auto a = eval(e->args[i], env);
                if (!a.is_vec()) err(f, e->args[i]->line, "vector literal: expected numeric");
                v[i] = a.scalar();
            }
            return Value(v);
        }
        case NodeT::DictLit: {
            auto d = std::make_shared<DictMap>();
            for (size_t i = 0; i < e->args.size(); ++i)
                (*d)[e->params[i]] = eval(e->args[i], env);
            return Value(d);
        }
        case NodeT::BinOp:
            if (e->op == "and") return Value(eval(e->left, env).truthy() && eval(e->right, env).truthy() ? 1.0 : 0.0);
            if (e->op == "or")  return Value(eval(e->left, env).truthy() || eval(e->right, env).truthy() ? 1.0 : 0.0);
            {
                // Force left-to-right evaluation. C++ does not guarantee
                // argument evaluation order, so a naked
                //   vec_binop(eval(left), eval(right), ...)
                // can call right before left — visible whenever operand
                // evaluation has side effects (e.g. `clock() <= clock()`).
                Value l = eval(e->left, env);
                Value r = eval(e->right, env);
                return vec_binop(l, r, e->op, ln, f);
            }
        case NodeT::UnaryOp:
            if (e->op == "-") {
                auto v = eval(e->left, env);
                if (!v.is_vec()) err(f, ln, "cannot negate non-numeric");
                return Value(-v.as_vec());
            }
            if (e->op == "not") return Value(eval(e->left, env).truthy() ? 0.0 : 1.0);
            err(f, ln, "unknown unary");
        case NodeT::Call: {
            // vars() — sorted list of names visible from the current scope.
            if (e->left->type == NodeT::Id && e->left->str_val == "vars" && e->args.empty()) {
                List names;
                std::unordered_set<std::string> seen;
                EnvPtr cur = env;
                while (cur) {
                    for (auto& kv : cur->vars)
                        if (seen.insert(kv.first).second)
                            names.push_back(Value(Str(kv.first)));
                    cur = cur->parent;
                }
                std::sort(names.begin(), names.end(), [](const Value& a, const Value& b) {
                    return a.as_str() < b.as_str();
                });
                return Value(std::move(names));
            }
            // bindings() — visible name → value mapping as a dict.
            if (e->left->type == NodeT::Id && e->left->str_val == "bindings" && e->args.empty()) {
                auto d = std::make_shared<DictMap>();
                EnvPtr cur = env;
                while (cur) {
                    for (auto& kv : cur->vars)
                        if (d->find(kv.first) == d->end()) (*d)[kv.first] = kv.second;
                    cur = cur->parent;
                }
                return Value(d);
            }
            // eval(string) — parse and execute in the current scope.
            if (e->left->type == NodeT::Id && e->left->str_val == "eval" && e->args.size() == 1) {
                auto code = eval(e->args[0], env);
                if (!code.is_str()) err(f, ln, "eval expects string");
                Lexer lex(code.as_str(), f);
                auto toks = lex.tokenize();
                Parser parser(std::move(toks), f);
                auto stmts = parser.parse_program();
                Value last(nullptr);
                for (auto& s : stmts) last = eval(s.expr, env);
                return last;
            }
            auto fn = eval(e->left, env);
            std::vector<Value> args;
            for (auto& a : e->args) args.push_back(eval(a, env));
            return call_value(fn, args, ln, f);
        }
        case NodeT::Index: {
            auto obj = eval(e->left, env);
            // Collect indices: e->right is first, e->args carry any extras
            // produced by `x[i, j, ...]` syntax.
            std::vector<Value> indices;
            indices.reserve(1 + e->args.size());
            indices.push_back(eval(e->right, env));
            for (auto& a : e->args) indices.push_back(eval(a, env));

            // Buffer: 1 or 2 indices.
            if (obj.is_buffer()) {
                auto& b = obj.as_buffer();
                if (indices.size() == 1) {
                    if (!indices[0].is_vec()) err(f, ln, "buffer index must be numeric");
                    int i = (int)indices[0].scalar();
                    if (i < 0) i += (int)b.n_frames;
                    if (i < 0 || i >= (int)b.n_frames) err(f, ln, "buffer frame index out of range");
                    if (b.n_channels == 1) return Value(b.data[i]);
                    Vec v(b.n_channels);
                    for (size_t c = 0; c < b.n_channels; ++c)
                        v[c] = b.data[i * b.n_channels + c];
                    return Value(v);
                }
                if (indices.size() == 2) {
                    if (!indices[0].is_vec() || !indices[1].is_vec())
                        err(f, ln, "buffer indices must be numeric");
                    int i = (int)indices[0].scalar();
                    int c = (int)indices[1].scalar();
                    if (i < 0) i += (int)b.n_frames;
                    if (c < 0) c += (int)b.n_channels;
                    if (i < 0 || i >= (int)b.n_frames) err(f, ln, "buffer frame index out of range");
                    if (c < 0 || c >= (int)b.n_channels) err(f, ln, "buffer channel index out of range");
                    return Value(b.data[i * b.n_channels + c]);
                }
                err(f, ln, "buffer expects 1 or 2 indices");
            }

            // Single-index path for vec, list, string, dict.
            if (indices.size() != 1)
                err(f, ln, "multi-index access only supported on buffer");
            auto& idx = indices[0];
            if (obj.is_vec()) {
                if (!idx.is_vec()) err(f, ln, "vec index must be numeric");
                int i = (int)idx.scalar();
                auto& v = obj.as_vec();
                if (i < 0) i += (int)v.size();
                if (i < 0 || i >= (int)v.size()) err(f, ln, "index out of range");
                return Value(v[i]);
            }
            if (obj.is_list()) {
                if (!idx.is_vec()) err(f, ln, "list index must be numeric");
                int i = (int)idx.scalar();
                auto& l = obj.as_list();
                if (i < 0) i += (int)l.size();
                if (i < 0 || i >= (int)l.size()) err(f, ln, "index out of range");
                return l[i];
            }
            if (obj.is_str()) {
                if (!idx.is_vec()) err(f, ln, "string index must be numeric");
                int i = (int)idx.scalar();
                auto& s = obj.as_str();
                if (i < 0) i += (int)s.size();
                if (i < 0 || i >= (int)s.size()) err(f, ln, "index out of range");
                return Value(Str(1, s[i]));
            }
            if (obj.is_dict()) {
                if (!idx.is_str()) err(f, ln, "dict key must be string");
                auto& d = obj.as_dict();
                auto it = d.find(idx.as_str());
                return it != d.end() ? it->second : Value(nullptr);
            }
            err(f, ln, "cannot index this type");
        }
        case NodeT::Member: {
            auto obj = eval(e->left, env);
            if (!obj.is_dict()) err(f, ln, "member access requires dict");
            auto& d = obj.as_dict();
            auto it = d.find(e->str_val);
            return it != d.end() ? it->second : Value(nullptr);
        }
        case NodeT::VarDecl: {
            auto v = eval(e->left, env);
            env->def(e->str_val, v);
            return v;
        }
        case NodeT::Assign: {
            auto v = eval(e->left, env);
            env->set(e->str_val, v);
            return v;
        }
        case NodeT::IndexAssign: {
            // Layout: e->right is the first index, e->args is
            // [extra_indices..., value]. Single-dim case has args = [value].
            // Evaluate value first (matches typical right-to-left store
            // ordering and avoids mutating through a stale lvalue if the
            // value-eval errs).
            Value val = eval(e->args.back(), env);
            std::vector<Value> indices;
            indices.reserve(e->args.size());           // first + extras
            indices.push_back(eval(e->right, env));
            for (size_t i = 0; i + 1 < e->args.size(); ++i)
                indices.push_back(eval(e->args[i], env));

            // Buffer assignment is multi-dim aware. For other types the
            // intermediate steps in eval_lvalue already restrict to single
            // index, so we only need a multi-dim path for buffer here.
            if (e->left->type == NodeT::Id || e->left->type == NodeT::Member ||
                e->left->type == NodeT::Index) {
                Value* target = eval_lvalue(e->left, env);
                if (target->is_buffer()) {
                    apply_buffer_assign(*target, indices, std::move(val), ln, f);
                    return *target;
                }
                if (indices.size() != 1)
                    err(f, ln, "multi-index assignment only supported on buffer");
                apply_index_assign(*target, indices[0], std::move(val), ln, f);
                return *target;
            }
            err(f, ln, "invalid assignment target");
        }
        case NodeT::FuncDecl: {
            Closure cl;
            cl.name = e->str_val;
            cl.params = e->params;
            cl.body = e->body;
            cl.env = env;
            cl.origin = e.get();
            // Docstring: a string literal as the first statement of the body
            // becomes the closure's documentation. The string still evaluates
            // at runtime as a no-op statement — keeping it in the body keeps
            // line numbers stable and the AST rendering faithful.
            if (!cl.body.empty() && cl.body[0].expr &&
                cl.body[0].expr->type == NodeT::Str) {
                cl.doc = cl.body[0].expr->str_val;
            }
            // Statement-form named functions get bound into env. The parser
            // ensures expression-form functions can never have names.
            if (!e->str_val.empty()) env->def(e->str_val, Value(cl));
            return Value(cl);
        }
        case NodeT::IfStmt:
            if (eval(e->left, env).truthy()) exec_block(e->body, make_env(env));
            else if (!e->args.empty()) eval(e->args[0], env);
            else if (e->right) exec_block(e->right->body, make_env(env));
            return Value(nullptr);
        case NodeT::WhileStmt: {
            ++loop_depth;
            try {
                while (true) {
                    yield();
                    if (!eval(e->left, env).truthy()) break;
                    try { exec_block(e->body, make_env(env)); }
                    catch (BreakSignal&) { break; }
                    catch (ContinueSignal&) { continue; }
                }
            } catch (...) { --loop_depth; throw; }
            --loop_depth;
            return Value(nullptr);
        }
        case NodeT::ForStmt: {
            auto scope = make_env(env);
            eval(e->args[0], scope);
            ++loop_depth;
            try {
                while (true) {
                    yield();
                    if (!eval(e->args[1], scope).truthy()) break;
                    try { exec_block(e->body, make_env(scope)); }
                    catch (BreakSignal&) { break; }
                    catch (ContinueSignal&) {}
                    eval(e->args[2], scope);
                }
            } catch (...) { --loop_depth; throw; }
            --loop_depth;
            return Value(nullptr);
        }
        case NodeT::ForIn: {
            auto coll = eval(e->left, env);
            auto scope = make_env(env);
            ++loop_depth;
            auto run_iter = [&](Value v) {
                scope->vars[e->str_val] = std::move(v);
                try { exec_block(e->body, make_env(scope)); }
                catch (ContinueSignal&) {}
            };
            try {
                if (coll.is_list()) {
                    // Snapshot the size so mutating the list mid-iteration doesn't
                    // produce surprising aliasing effects within this loop.
                    auto& l = coll.as_list();
                    size_t n = l.size();
                    for (size_t i = 0; i < n; ++i) {
                        yield();
                        try { run_iter(l[i]); } catch (BreakSignal&) { break; }
                    }
                } else if (coll.is_vec()) {
                    auto& v = coll.as_vec();
                    for (size_t i = 0; i < v.size(); ++i) {
                        yield();
                        try { run_iter(Value(v[i])); } catch (BreakSignal&) { break; }
                    }
                } else if (coll.is_str()) {
                    auto& s = coll.as_str();
                    for (size_t i = 0; i < s.size(); ++i) {
                        yield();
                        try { run_iter(Value(Str(1, s[i]))); } catch (BreakSignal&) { break; }
                    }
                } else if (coll.is_dict()) {
                    // Iterate keys in sorted order for stable, repeatable runs.
                    std::vector<std::string> ks;
                    for (auto& kv : coll.as_dict()) ks.push_back(kv.first);
                    std::sort(ks.begin(), ks.end());
                    for (auto& k : ks) {
                        yield();
                        try { run_iter(Value(Str(k))); } catch (BreakSignal&) { break; }
                    }
                } else if (coll.is_buffer()) {
                    // Yield per-frame values: scalar for mono, vec for multichannel.
                    auto& b = coll.as_buffer();
                    for (size_t i = 0; i < b.n_frames; ++i) {
                        yield();
                        Value frame_val;
                        if (b.n_channels == 1) {
                            frame_val = Value(b.data[i]);
                        } else {
                            Vec v(b.n_channels);
                            for (size_t c = 0; c < b.n_channels; ++c)
                                v[c] = b.data[i * b.n_channels + c];
                            frame_val = Value(v);
                        }
                        try { run_iter(std::move(frame_val)); } catch (BreakSignal&) { break; }
                    }
                } else {
                    err(f, ln, "for-in requires list, vec, string, dict, or buffer");
                }
            } catch (...) { --loop_depth; throw; }
            --loop_depth;
            return Value(nullptr);
        }
        case NodeT::ReturnStmt: {
            if (function_depth == 0) err(f, ln, "return outside function");
            // Tail-call: `return f(args...)` where the callee evaluates to a
            // closure or native is converted to a TailCall signal so the
            // enclosing call_value frame can be reused. Skip the special
            // intrinsics handled inline above (vars, bindings, eval).
            if (e->left && e->left->type == NodeT::Call) {
                auto& c = *e->left;
                bool is_intrinsic = c.left->type == NodeT::Id &&
                    (c.left->str_val == "vars" ||
                     c.left->str_val == "bindings" ||
                     c.left->str_val == "eval");
                if (!is_intrinsic) {
                    auto fnv = eval(c.left, env);
                    if (fnv.is_closure() || fnv.is_native()) {
                        std::vector<Value> aargs;
                        aargs.reserve(c.args.size());
                        for (auto& a : c.args) aargs.push_back(eval(a, env));
                        throw TailCall{std::move(fnv), std::move(aargs)};
                    }
                }
            }
            Value v = e->left ? eval(e->left, env) : Value(nullptr);
            throw ReturnSignal{v};
        }
        case NodeT::BreakStmt:
            if (loop_depth == 0) err(f, ln, "break outside loop");
            throw BreakSignal{};
        case NodeT::ContStmt:
            if (loop_depth == 0) err(f, ln, "continue outside loop");
            throw ContinueSignal{};
        case NodeT::PrintStmt: {
            for (size_t i = 0; i < e->args.size(); ++i) {
                if (i) std::cout << ' ';
                std::cout << eval(e->args[i], env).repr();
            }
            std::cout << '\n';
            return Value(nullptr);
        }
        case NodeT::AssertStmt: {
            Value v = eval(e->left, env);
            if (!v.truthy()) {
                std::string msg = "assertion failed: " + e->str_val;
                if (!e->args.empty()) {
                    Value extra = eval(e->args[0], env);
                    msg += " — " + extra.repr();
                }
                err(f, ln, msg);
            }
            return Value(nullptr);
        }
        case NodeT::TryCatch: {
            // Layout:
            //   e->body    — try block
            //   e->right   — catch block (Block expr) or null
            //   e->str_val — catch variable name (if catch present)
            //   e->args[0] — finally block (Block expr) or absent
            std::exception_ptr saved;
            try {
                exec_block(e->body, make_env(env));
            } catch (Error& err_obj) {
                if (e->right) {
                    auto catch_scope = make_env(env);
                    auto d = std::make_shared<DictMap>();
                    (*d)["message"] = Value(Str(err_obj.msg));
                    (*d)["file"]    = Value(Str(err_obj.file));
                    (*d)["line"]    = Value((double)err_obj.line);
                    List trace_list;
                    for (auto& t : err_obj.trace) trace_list.push_back(Value(Str(t)));
                    (*d)["trace"]   = Value(std::move(trace_list));
                    catch_scope->def(e->str_val, Value(d));
                    try {
                        exec_block(e->right->body, catch_scope);
                    } catch (...) {
                        saved = std::current_exception();
                    }
                } else {
                    // No catch — save the error to rethrow after finally.
                    saved = std::current_exception();
                }
            } catch (...) {
                // Return / Break / Continue / TailCall: control flow, not
                // errors. Save and rethrow after running finally so cleanup
                // still happens before the function returns or the loop breaks.
                saved = std::current_exception();
            }

            if (!e->args.empty()) {
                // Finally always runs. If finally itself throws, that
                // exception replaces any saved one (matches Java/Python).
                try {
                    exec_block(e->args[0]->body, make_env(env));
                } catch (...) {
                    saved = std::current_exception();
                }
            }

            if (saved) std::rethrow_exception(saved);
            return Value(nullptr);
        }
        case NodeT::LoadStmt: {
            auto fn = eval(e->left, env);
            if (!fn.is_str()) err(f, ln, "load expects string");
            auto resolved = resolve_load(fn.as_str(), f);
            if (!fs::exists(resolved)) err(f, ln, "cannot find '" + fn.as_str() + "'");
            std::string canon = fs::weakly_canonical(resolved).string();
            // Memoize: each canonical path is loaded at most once. This both
            // breaks cycles (recursive loads see the path already-loaded) and
            // avoids re-running modules already in scope.
            if (loaded_files.insert(canon).second) {
                run_file(canon, env);
            }
            return Value(nullptr);
        }
        case NodeT::Block:
            exec_block(e->body, make_env(env));
            return Value(nullptr);
        default:
            err(f, ln, "unhandled node");
        }
    }

    void exec_block(const std::vector<Stmt>& stmts, EnvPtr env) {
        for (auto& s : stmts) {
            yield();
            eval(s.expr, env);
        }
    }
    void run_source(const std::string& src, const std::string& fname, EnvPtr env) {
        Lexer lex(src, fname);
        auto toks = lex.tokenize();
        Parser parser(std::move(toks), fname);
        auto stmts = parser.parse_program();
        exec_block(stmts, env);
    }
    // REPL-flavored runner: returns the final value and a flag indicating
    // whether the last statement was an "expression" worth echoing.
    struct LastResult { Value value; bool printable = false; };
    LastResult run_source_repl(const std::string& src, const std::string& fname, EnvPtr env) {
        Lexer lex(src, fname);
        auto toks = lex.tokenize();
        Parser parser(std::move(toks), fname);
        auto stmts = parser.parse_program();
        LastResult lr;
        for (size_t i = 0; i < stmts.size(); ++i) {
            yield();
            Value v = eval(stmts[i].expr, env);
            if (i + 1 == stmts.size()) {
                NodeT t = stmts[i].expr->type;
                bool is_expr =
                    t == NodeT::Num || t == NodeT::Str || t == NodeT::Id ||
                    t == NodeT::BinOp || t == NodeT::UnaryOp ||
                    t == NodeT::Call || t == NodeT::Index || t == NodeT::Member ||
                    t == NodeT::VecLit || t == NodeT::DictLit;
                lr.value = v;
                lr.printable = is_expr && !v.is_nil();
            }
        }
        return lr;
    }
    void run_file(const std::string& fname, EnvPtr env) {
        std::ifstream f(fname);
        if (!f) throw std::runtime_error("cannot open " + fname);
        std::ostringstream ss; ss << f.rdbuf();
        run_source(ss.str(), fs::weakly_canonical(fs::path(fname)).string(), env);
    }
    void run_file(const std::string& fname) {
        // Top-level entry: register this path so any later `load` of the
        // same file becomes a no-op (matches the load() memoization rule).
        std::string canon = fs::weakly_canonical(fs::path(fname)).string();
        loaded_files.insert(canon);
        run_file(canon, global);
    }

    void repl();
};

// ── REPL helpers ──────────────────────────────────────────────────────
// Heuristic: scan the accumulated input for unbalanced delimiters, an
// unclosed string, or an unclosed block comment. While any of those is
// still pending we should keep prompting for more lines. Once the scan
// shows everything balanced we hand the buffer to the parser, which will
// raise an ordinary error if the input is merely *wrong* rather than
// incomplete (e.g. `try { } else { }`).
inline bool is_input_complete(const std::string& s) {
    int parens = 0, brackets = 0, braces = 0;
    bool in_str = false;
    bool in_block = false;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (in_block) {
            if (c == '*' && i + 1 < s.size() && s[i + 1] == '/') {
                in_block = false; ++i;
            }
            continue;
        }
        if (in_str) {
            if (c == '\\' && i + 1 < s.size()) { ++i; continue; }
            if (c == '"') in_str = false;
            continue;
        }
        if (c == '#') {
            while (i < s.size() && s[i] != '\n') ++i;
            continue;
        }
        if (c == '/' && i + 1 < s.size() && s[i + 1] == '*') {
            in_block = true; ++i; continue;
        }
        if (c == '"') { in_str = true; continue; }
        if (c == '(') ++parens;
        else if (c == ')') --parens;
        else if (c == '[') ++brackets;
        else if (c == ']') --brackets;
        else if (c == '{') ++braces;
        else if (c == '}') --braces;
    }
    // Complete iff: no in-progress string/comment AND no unclosed opens.
    // Negative imbalance (extra closers) is a syntax error — let the
    // parser produce the diagnostic.
    return !in_str && !in_block
        && parens <= 0 && brackets <= 0 && braces <= 0;
}

#ifdef FLUX_USE_READLINE
extern "C" {
#include <readline/readline.h>
#include <readline/history.h>
}
#endif

// Lines that are only whitespace shouldn't end up in the history file.
inline bool is_blank(const std::string& s) {
    for (char c : s) if (c != ' ' && c != '\t' && c != '\n') return false;
    return true;
}

inline void Interpreter::repl() {
#ifdef FLUX_USE_READLINE
    using_history();
    stifle_history(1000);                            // cap history length
    std::string history_path;
    if (const char* home = std::getenv("HOME")) {
        history_path = std::string(home) + "/.flux_history";
        read_history(history_path.c_str());          // ignore failure
    }
#endif

    std::string accum;
    while (true) {
        const char* prompt = accum.empty() ? ">> " : ".. ";

#ifdef FLUX_USE_READLINE
        char* raw = readline(prompt);
        if (!raw) {                                   // EOF / Ctrl-D on empty line
            std::cout << '\n';
            break;
        }
        std::string line(raw);
        free(raw);
#else
        std::cout << prompt << std::flush;
        std::string line;
        if (!std::getline(std::cin, line)) { std::cout << '\n'; break; }
#endif

        if (accum.empty() && (line == "quit" || line == "exit")) break;
        if (accum.empty() && is_blank(line)) continue;

        accum += line;
        accum += '\n';

        if (!is_input_complete(accum)) continue;

        // Trim trailing whitespace before adding to history.
        std::string for_history = accum;
        while (!for_history.empty() &&
               (for_history.back() == '\n' || for_history.back() == ' ' ||
                for_history.back() == '\t'))
            for_history.pop_back();

#ifdef FLUX_USE_READLINE
        if (!for_history.empty()) {
            // Convert embedded newlines so the whole multi-line entry shows
            // up as a single history item that we can recall and re-edit.
            std::string h;
            h.reserve(for_history.size());
            for (char c : for_history) h += (c == '\n') ? ' ' : c;
            // Skip exact-duplicate consecutive entries.
            HIST_ENTRY* last = history_get(history_length);
            if (!last || h != last->line) add_history(h.c_str());
            if (!history_path.empty())
                write_history(history_path.c_str());
        }
#endif

        try {
            auto lr = run_source_repl(accum, "<repl>", global);
            if (lr.printable) std::cout << lr.value.repr() << '\n';
        } catch (std::exception& e) {
            std::cerr << "error: " << e.what() << '\n';
            call_stack.clear();
            function_depth = 0;
            loop_depth = 0;
        }
        accum.clear();
    }
}

} // namespace flux

#endif // FLUX_H
