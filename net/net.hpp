#pragma once

#include <mx/mx.hpp>
#include <async/async.hpp>

struct WOLFSSL_CTX;
struct WOLFSSL;
struct WOLFSSL_METHOD;
struct sockaddr_in;

#if defined(WIN32)
using ssize_t = ion::i64;
#endif

namespace ion {

enums(method, undefined, 
   "undefined, response, get, post, put, delete",
    undefined, response, get, post, put, del);

struct uri:mx {
    struct components {
        method  mtype; /// good to know where the uri comes from, in what context 
        str     proto; /// (what method, this is part of uri because method is 
        str     host;  /// Something that makes it uniquely bindable; losing that
        int     port;  /// makes you store it somewhere else and who knows where that might be)
        str     query;
        str     resource;
        map<mx> args;
        str     version;
        type_register(components);
    };
    /// singular container type used for 'enumeration' and containment of value: method
    /// enum = method::mtype
    /// default = undefined, then the arg list
    ///
    method & mtype() { return data->mtype;    }
    str &     host() { return data->host;     }
    str &    proto() { return data->proto;    }
    int &     port() { return data->port;     }
    str &   string() { return data->query;    }
    str & resource() { return data->resource; }
    map<mx> & args() { return data->args;     }
    str &  version() { return data->version;  }

    ///
    static str encode_fields(map<mx> fields) {
        method m;
        str post(size_t(1024));
        if (fields) {
            for (auto& [val, key] : fields) {
                str s_key = key.grab();
                str s_val = val.grab();
                if (post)
                    post += "&";
                post += str::format("{0}={1}", {uri::encode(s_key), uri::encode(s_val)});
            }
        }
        return post;
    }

    mx_object(uri, mx, components);

    static memory *convert(memory *mem) { return parse(str(mem)); }

    uri(str                s) : uri(parse(s))   { }
    uri(symbol           sym) : uri(parse(sym)) { } 
    uri(uri &r, method mtype) : uri(r.copy()) { data->mtype = mtype; }

    ///
    operator str () {
        struct def {
            symbol proto;
            int        port;
        };
        
        array<def>   defs = {{ "https", 443 }, { "http", 80 }};
        bool       is_def = defs.select_first<bool>([&](def& d) -> bool {
            return data->port == d.port && data->proto == d.proto;
        });

        str      s_port   = !is_def ? str::format(":{0}", { data->port })                : "";
        str      s_fields =  data->args ? str::format("?{0}", { encode_fields(data->args) }) : "";
        
        /// return string uri
        return str::format("{0}://{1}{2}{3}{4}", { data->proto, data->host, s_port, data->query, s_fields });
    }

    uri methodize(method mtype) const {
        data->mtype = mtype;
        return *this;
    }

    bool operator==(method::etype m) { return data->mtype.value == m; }
    bool operator!=(method::etype m) { return data->mtype.value != m; }
    operator           bool() { return data->mtype.value != method::undefined; }

    /// can contain: GET/PUT/POST/DELETE uri [PROTO] (defaults to GET)
    static memory *parse(str raw, uri* ctx = null) {
        uri         result;
        array<str>  sp  = raw.split(" ");
        components& ra  = *result.data;
        bool   has_meth = sp.len() > 1;
        str lc = sp.len() > 0 ? sp[0].lcase() : str();
        method m { str(has_meth ? lc.cs() : cstr("get")) };
        str    u =  sp[has_meth ? 1 : 0];
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
                ra.port = p == "https" ? 443 : 80;
            }
        } else {
            /// return default
            ra.proto    = ctx ? ctx->data->proto : "";
            ra.host     = ctx ? ctx->data->host  : "";
            ra.port     = ctx ? ctx->data->port  : 0;
            ra.query    = u;
        }
        /// parse resource and query
        num iq = u.index_of("?");
        if (iq > 0) {
            ra.resource  = decode(u.mid(0, iq));
            str        q = decode(u.mid(iq + 1));
            array<str> a = q.split("&");
            ra.args      = map<mx>();
            for (str &kv:  a) {
                array<str> sp = kv.split("=");
                mx    &k = sp[0];
                mx    &v = sp.len() > 1 ? sp[1] : k;
                char *sk = k.data<char>();
                char *sv = v.data<char>();
                ra.args[k] = v;
            }
        } else
            ra.resource = decode(u);
        ///
        if (sp.len() >= 3)
            ra.version  = sp[2];
        ///
        return result.grab();
    }

    ///
    static str encode(str s) {
        static const char *s_chars = " -._~:/?#[]@!$&'()*+;%=";
        static array<char>   chars { (char*)s_chars, strlen(s_chars) };

        /// A-Z, a-z, 0-9, -, ., _, ~, :, /, ?, #, [, ], @, !, $, &, ', (, ), *, +, ,, ;, %, and =
        size_t  len = s.len();
        array<char> v = array<char>(len * 4 + 1); 
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
        return str(v.data, int(v.len()));
    }

    static str decode(str e) {
        size_t     sz = e.len();
        array<char> v = array<char>(size_t(sz * 4 + 1));
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
        return str(v.data, int(v.len()));
    }
};

struct isock;

struct sock:mx {
private:
    inline static symbol pers = "ion:net";
    //bool connect(str host, int port);
    bool bind(str adapter, int port);
    sock &establish();
    sock accept();
    void load_certs(str host);
    
public:
    mx_declare(sock, mx, isock);
    
    enums(role, none,
          "none, client, server",
           none, client, server);

    static async listen(uri url, lambda<bool(sock&)> fn);
    static sock connect(uri url);

     sock(role r, uri bind);
    ~sock();
    
    operator bool();
    
    void set_timeout(i64 t);
    
    void close();
    bool read_sz(u8 *v, size_t sz);
    array<char> read_until(str s, int max_len);
    
    ssize_t recv(unsigned char* buf, size_t len);
    ssize_t send(const unsigned char* buf, size_t len);
    ssize_t send(str templ, array<mx> args = { });
    ssize_t send(mx &v);
};

struct message:mx {
    struct members {
        uri     query;
        mx      code = int(0);
        map<mx> headers;
        mx      content; /// best to store as mx, so we can convert directly in lambda arg, functionally its nice to have delim access in var.
    };

    method method_type() {
        return data->query.mtype();
    }

    mx_object(message, mx, members);

    message(int server_code);
    message(symbol text);
    message(str text);
    message(path p, mx modified_since = {});
    message(mx content, map<mx> headers = {}, uri query = {});
    message(uri url, map<mx> headers = {});
    message(sock &sc);

    uri query();

    bool read_headers(sock &sc);
    bool read_content(sock &sc);

    /// query/request construction
    static message query(uri server, map<mx> headers, mx content);

    /// response construction, uri is not needed
    static message response(uri query, mx code, mx content, map<mx> headers = {});

    explicit operator bool();
    bool operator!();
    
    bool printable_value(mx &v);
    static symbol code_symbol(int code);

    ///  code is in headers.
    bool write_status(sock &sc);
    bool write_headers(sock &sc);
    bool write(sock &sc);
    str  text();

    /// structure cookies into usable format
    map<str> cookies();
    mx &operator[](symbol key);
    mx &header(mx key);
};

//void test_wolf();
bool test_mbed();
///
/// high level server method (on top of listen)
/// you receive messages from clients through lambda; supports https
/// web sockets should support the same interface with some additions
async service(uri bind, lambda<message(message)> fn_process);

/// useful utility function here for general web requests; driven by the future
future request(uri url, map<mx> args);

future json(uri addr, map<mx> args, map<mx> headers);
}
