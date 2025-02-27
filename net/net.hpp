#pragma once

#include <mx/mx.hpp>
#include <async/async.hpp>

struct sockaddr_in;

#if defined(WIN32)
using ssize_t = ion::i64;
#endif

namespace ion {

enums(protocol, undefined,
     undefined, http, https, wss, ssh);

/// web is more descriptive than method and it doesnt conflict with invocation
/// it could be called http too, but that almost implies its not secure, these web things happen over both
enums(web, undefined, 
    undefined, Response, Get, Post, Put, Delete);

struct uri:mx {
    struct components {
        web     mtype; /// good to know where the uri comes from, in what context 
        protocol proto; /// (what method, this is part of uri because method is 
        str     host;  /// Something that makes it uniquely bindable; losing that
        int     port;  /// makes you store it somewhere else and who knows where that might be)
        str     query;
        str     resource;
        map args;
        str     version;
    };
    /// singular container type used for 'enumeration' and containment of value: method
    /// enum = method::mtype
    /// default = undefined, then the arg list
    ///
    web    & mtype() { return data->mtype;    }
    str &     host() { return data->host;     }
    str       addr();
    protocol&proto() { return data->proto;    }
    int       port();
    str &   string() { return data->query;    }
    str & resource() { return data->resource; }
    map & args() { return data->args;     }
    str &  version() { return data->version;  }

    ///
    static str encode_fields(const map &fields) {
        web m;
        str post(size_t(1024));
        if (fields) {
            for (auto& [val, key] : fields.fields()) {
                str s_key(key);
                str s_val(val);
                if (post)
                    post += "&";
                post += str::format("{0}={1}", { uri::encode(s_key), uri::encode(s_val) });
            }
        }
        return post;
    }

    mx_object(uri, mx, components);

    static memory *convert(memory *mem) { return parse(str(mem)); }

    uri(null_t             n) : uri() { }
    uri(str                s) : uri(parse(s))   { }
    uri(symbol           sym) : uri(parse(sym)) { } 
    uri(uri &r, web mtype) : uri(r.copy()) { data->mtype = mtype; }

    ///
    operator str () {
        struct def {
            protocol proto;
            int      port;
        };
        
        Array<def>   defs = {{ protocol::https, 443 }};
        bool       is_def = defs.array::select_first<bool, def>([&](def& d) -> bool {
            return data->port == d.port && data->proto == d.proto;
        });

        str      s_port   = !is_def ? str::format(":{0}", { data->port })                : "";
        str      s_fields =  data->args ? str::format("?{0}", { encode_fields(data->args) }) : "";
        
        /// return string uri
        return str::format("{0}://{1}{2}{3}{4}", { data->proto, data->host, s_port, data->query, s_fields });
    }

    uri methodize(web mtype) const {
        data->mtype = mtype;
        return *this;
    }

    bool operator==(web::etype m) { return data->mtype.value == m; }
    bool operator!=(web::etype m) { return data->mtype.value != m; }
    operator           bool() { return data->mtype.value != web::undefined; }

    /// can contain: GET/PUT/POST/DELETE uri [PROTO] (defaults to GET)
    static memory *parse(str raw, uri* ctx = null) {
        uri         result;
        Array<str>  sp  = raw.split(" ");
        components& ra  = *result.data;
        bool   has_method = sp.len() > 1;
        str lcase = sp.len() > 0 ? sp[0].lcase() : str();
        web m { str(has_method ? lcase.cs() : cstr("get")) };
        str    u =  sp[has_method ? 1 : 0];
        ra.mtype = m;

        /// find a distraught face
        num iproto      = u.index_of("://");
        if (iproto >= 0) {
            str       p = u.mid(0, iproto);
            u           = u.mid(iproto + 3);
            num ihost   = u.index_of("/");
            ra.proto    = p;
            ra.query    = ihost >= 0 ? u.mid(ihost) : str("/");
            str       h = ihost >= 0 ? u.mid(0, ihost) : u;
            num      ih = h.index_of(":");
            u           = ra.query;
            if (ih > 0) {
                ra.host = h.mid(0, ih);
                ra.port = int(h.mid(ih + 1).integer_value());
            } else {
                ra.host = h;
                ra.port = 0; /// looked up by method
            }
        } else {
            /// return default
            ra.proto    = ctx ? ctx->data->proto : protocol(protocol::undefined);
            ra.host     = ctx ? ctx->data->host  : "";
            ra.port     = ctx ? ctx->data->port  : 0;
            ra.query    = u;
        }
        /// parse resource and query
        num iq = u.index_of("?");
        if (iq > 0) {
            ra.resource  = decode(u.mid(0, iq));
            str        q = decode(u.mid(iq + 1));
            Array<str> a = q.split("&");
            ra.args      = map();
            for (str &kv: a.elements<str>()) {
                Array<str> sp = kv.split("=");
                mx    &k = sp[0];
                mx    &v = sp.len() > 1 ? sp[1] : k;
                ra.args[k] = v;
            }
        } else
            ra.resource = decode(u);
        ///
        if (sp.len() >= 3)
            ra.version  = sp[2];
        ///
        return hold(result);
    }

    ///
    static str encode(str s) {
        static str chars = " -._~:/?#[]@!$&'()*+;%=";

        /// A-Z, a-z, 0-9, -, ., _, ~, :, /, ?, #, [, ], @, !, $, &, ', (, ), *, +, ,, ;, %, and =
        size_t  len = s.len();
        Array<char> v = Array<char>(len * 4 + 1); 
        for (size_t i = 0; i < len; i++) {
            cchar_t c = s[i];
            bool    a = ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'));
            if (!a)
                 a    = chars.index_of(c) != 0;
            if (!a) {
                v    += '%';
                std::stringstream st;
                if (c >= 16)
                    st << std::hex << int(c);
                else
                    st << "0" << std::hex << int(c);
                str n(st.str());
                v += n[0];
                v += n[1];
            } else
                v += c;
            if (c == '%')
                v += '%';
        }
        return str(v.data<char>(), v.len());
    }

    static str decode(str e) {
        size_t     sz = e.len();
        Array<char> v = Array<char>(size_t(sz * 4 + 1));
        size_t      i = 0;

        while (i < sz) {
            cchar_t c0 = e[i];
            if (c0 == '%') {
                if (i >= sz + 1)
                    break;
                cchar_t c1 = e[i + 1];
                if (c1 == '%')
                    v += '%';
                else {
                    if (i >= sz + 2)
                        break;
                    cchar_t c2 = e[i + 2];
                    v += str::char_from_nibs(c1, c2);
                }
            } else
                v += (c0 == '+') ? ' ' : c0;
            i++;
        }
        return str(v.data<char>(), v.len());
    }
};

str dns(str hostname);

struct isock;

struct Session;
struct iTLS;

struct TLS:mx {
    mx_declare(TLS, mx, iTLS);
    TLS(uri);
};

struct sock:mx {
    mx_declare(sock, mx, Session); /// default allocation needs to have nothing allocated (for these decls)

    ///
    sock(TLS tls);
    sock(uri addr);

    bool        bind(uri addr);
    bool        connect();
    bool        close();
    void        set_timeout(i64 t);
    bool        read_sz(char *v, size_t sz);
    ssize_t     recv(char* buf, size_t len);
    ssize_t     send(const char* buf, size_t len);
    ssize_t     send(str templ, Array<mx> args);
    ssize_t     send(mx &v);
    Array<char> read_until(str s, int max_len);
    
    static async listen(uri url, lambda<bool(sock&)> fn);
    static sock  accept(TLS tls);

    operator bool();
};

struct message:mx {
    struct M {
        uri     query;
        mx      code = int(0);
        map headers;
        mx      content; /// best to store as mx, so we can convert directly in lambda arg, functionally its nice to have delim access in var.
    };

    web method_type() {
        return data->query.mtype();
    }

    mx_object(message, mx, M);

    message(int server_code);
    message(symbol text);
    message(str text);
    message(path p, mx modified_since = {});
    message(mx content, map headers = {}, uri query = {});
    message(uri url, map headers = {});
    message(sock &sc);

    uri query();

    bool read_headers(sock &sc);
    bool read_content(sock &sc);

    /// query/request construction
    static message query(uri server, map headers, mx content);

    /// response construction, uri is not needed
    static message response(uri query, mx code, mx content, map headers = {});

    explicit operator bool();
    bool operator!();
    
    bool printable_value(const mx &v);
    static symbol code_symbol(int code);

    ///  code is in headers.
    bool write_status(sock &sc);
    bool write_headers(sock &sc);
    bool write(sock &sc);
    str  text();

    /// structure cookies into usable format
    map cookies();
    mx &operator[](symbol key);
    mx &header(mx key);
};

//void test_wolf();
bool test_mbed();

/// useful utility function here for general web requests; driven by the future
future request(uri url, map args);

future json(uri addr, map args, map headers);

}


