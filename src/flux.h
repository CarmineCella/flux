//
// flux.h — Flux interpreter, single-header.
//

#ifndef FLUX_H
#define FLUX_H

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

struct Stmt;
struct Expr;
struct Closure {
    std::string name;
    std::vector<std::string> params;
    std::vector<Stmt> body;
    EnvPtr env;
    const Expr* origin = nullptr;   // shared pointer to the FuncDecl AST node
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
    std::variant<Vec, Str, ListPtr, DictPtr, Closure, NativeFn, std::nullptr_t> data;
    Value() : data(nullptr) {}
    Value(double d) : data(Vec{d}) {}
    Value(Vec v)    : data(std::move(v)) {}
    Value(Str s)    : data(std::move(s)) {}
    Value(List l)   : data(std::make_shared<List>(std::move(l))) {}
    Value(ListPtr p): data(std::move(p)) {}
    Value(DictPtr p): data(std::move(p)) {}
    Value(Closure c): data(std::move(c)) {}
    Value(NativeFn f): data(std::move(f)) {}
    Value(std::nullptr_t) : data(nullptr) {}

    bool is_vec()     const { return std::holds_alternative<Vec>(data); }
    bool is_str()     const { return std::holds_alternative<Str>(data); }
    bool is_list()    const { return std::holds_alternative<ListPtr>(data); }
    bool is_dict()    const { return std::holds_alternative<DictPtr>(data); }
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
        return true;
    }
    static std::string fmt(double d) {
        auto s = std::to_string(d);
        s.erase(s.find_last_not_of('0') + 1);
        if (s.back() == '.') s.pop_back();
        return s;
    }
    std::string repr() const {
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
            std::string r = "(";
            auto& l = as_list();
            for (size_t i = 0; i < l.size(); ++i) { if (i) r += ", "; r += l[i].repr(); }
            return r + ")";
        }
        if (is_dict()) {
            // Sort keys for deterministic, readable output.
            auto& d = as_dict();
            std::vector<std::string> keys;
            keys.reserve(d.size());
            for (auto& kv : d) keys.push_back(kv.first);
            std::sort(keys.begin(), keys.end());
            std::string r = "{";
            for (size_t i = 0; i < keys.size(); ++i) {
                if (i) r += ", ";
                r += keys[i] + ": " + d.at(keys[i]).repr();
            }
            return r + "}";
        }
        if (is_closure()) return "<func>";
        if (is_native())  return "<native>";
        return "?";
    }

    // Structural equality — recurses into lists and dicts. Closures and
    // natives are never considered equal (their identity isn't observable
    // here, since copies of a Value duplicate the underlying function object).
    static bool deep_eq(const Value& a, const Value& b) {
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
            auto& la = a.as_list(); auto& lb = b.as_list();
            if (la.size() != lb.size()) return false;
            for (size_t i = 0; i < la.size(); ++i)
                if (!deep_eq(la[i], lb[i])) return false;
            return true;
        }
        if (b.is_list()) return false;
        if (a.is_dict()) {
            if (!b.is_dict()) return false;
            auto& da = a.as_dict(); auto& db = b.as_dict();
            if (da.size() != db.size()) return false;
            for (auto& kv : da) {
                auto it = db.find(kv.first);
                if (it == db.end()) return false;
                if (!deep_eq(kv.second, it->second)) return false;
            }
            return true;
        }
        if (b.is_dict()) return false;
        return false;
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
    Print, Load, Try, Catch, Assert
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
                {"try", Tk::Try}, {"catch", Tk::Catch}, {"assert", Tk::Assert}
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
        eat(Tk::Catch);
        eat(Tk::LPar);
        e->str_val = eat(Tk::Id).text;     // catch variable name
        eat(Tk::RPar);
        auto bl = make(NodeT::Block, cur().line);
        bl->body = parse_block();
        e->right = bl;                     // catch block
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
                a->right = e->right;                // index
                a->args.push_back(parse_expr());    // value
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
                idx->right = parse_expr();
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
    // Public hook for hosts to register additional builtins after construction.
    void register_builtin(const std::string& name, NativeFn fn) {
        global->def(name, Value(std::move(fn)));
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
            if (a[0].is_vec())  return Value((double)a[0].as_vec().size());
            if (a[0].is_str())  return Value((double)a[0].as_str().size());
            if (a[0].is_list()) return Value((double)a[0].as_list().size());
            if (a[0].is_dict()) return Value((double)a[0].as_dict().size());
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
            if (a[0].is_list()) return Value(List(a[0].as_list()));
            if (a[0].is_vec())  return Value(Vec(a[0].as_vec()));
            if (a[0].is_str())  return Value(Str(a[0].as_str()));
            if (a[0].is_dict()) return Value(std::make_shared<DictMap>(a[0].as_dict()));
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
            if (a[0].is_vec())  return Value(Str("vec"));
            if (a[0].is_str())  return Value(Str("string"));
            if (a[0].is_list()) return Value(Str("list"));
            if (a[0].is_dict()) return Value(Str("dict"));
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

        // ── constants ─────────────────────────────────────────────────
        global->def("pi",    Value(3.14159265358979323846));
        global->def("e",     Value(2.71828182845904523536));
        global->def("inf",   Value(std::numeric_limits<double>::infinity()));
        global->def("nil",   Value(nullptr));
        global->def("true",  Value(1.0));
        global->def("false", Value(0.0));
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
            auto idx = eval(e->right, env);
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
            // Evaluate value first (matches typical right-to-left store
            // ordering and avoids mutating through a stale lvalue if the
            // value-eval errs).
            Value val = eval(e->args[0], env);
            Value idx = eval(e->right, env);
            Value* target = eval_lvalue(e->left, env);
            apply_index_assign(*target, idx, std::move(val), ln, f);
            return *target;
        }
        case NodeT::FuncDecl: {
            Closure cl;
            cl.name = e->str_val;
            cl.params = e->params;
            cl.body = e->body;
            cl.env = env;
            cl.origin = e.get();
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
                } else {
                    err(f, ln, "for-in requires list, vec, string, or dict");
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
            try {
                exec_block(e->body, make_env(env));
            } catch (Error& err_obj) {
                auto catch_scope = make_env(env);
                auto d = std::make_shared<DictMap>();
                (*d)["message"] = Value(Str(err_obj.msg));
                (*d)["file"]    = Value(Str(err_obj.file));
                (*d)["line"]    = Value((double)err_obj.line);
                List trace_list;
                for (auto& t : err_obj.trace) trace_list.push_back(Value(Str(t)));
                (*d)["trace"]   = Value(std::move(trace_list));
                catch_scope->def(e->str_val, Value(d));
                exec_block(e->right->body, catch_scope);
            }
            // ReturnSignal, BreakSignal, ContinueSignal, TailCall are NOT
            // caught here — they are control-flow primitives, not errors.
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

    void repl() {
        std::string input;
        while (true) {
            std::cout << (input.empty() ? ">> " : ".. ") << std::flush;
            std::string line;
            if (!std::getline(std::cin, line)) break;
            if (input.empty() && (line == "quit" || line == "exit")) break;
            if (input.empty() && line.empty()) continue;
            input += line + "\n";
            int depth = 0;
            bool in_str = false;
            for (size_t i = 0; i < input.size(); ++i) {
                char c = input[i];
                if (c == '"' && (i == 0 || input[i-1] != '\\')) { in_str = !in_str; continue; }
                if (in_str) continue;
                if (c == '#') { while (i < input.size() && input[i] != '\n') ++i; continue; }
                if (c == '{' || c == '(' || c == '[') ++depth;
                if (c == '}' || c == ')' || c == ']') --depth;
            }
            if (depth > 0 || in_str) continue;
            try {
                auto lr = run_source_repl(input, "<repl>", global);
                if (lr.printable) std::cout << lr.value.repr() << '\n';
            } catch (std::exception& e) {
                std::cerr << "error: " << e.what() << '\n';
                call_stack.clear();
                function_depth = 0;
                loop_depth = 0;
            }
            input.clear();
        }
    }
};

} // namespace flux

#endif // FLUX_H
