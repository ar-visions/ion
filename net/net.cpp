
#include <mx/mx.hpp>

#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <winsock2.h>
#include <windows.h>
#endif

#include <mbedtls/build_info.h>
#include <mbedtls/platform.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/x509.h>
#include <mbedtls/ssl.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/error.h>
#include <mbedtls/debug.h>
//#include "test/certs.h"

#if defined(MBEDTLS_SSL_CACHE_C)
#include <mbedtls/ssl_cache.h>
#endif

#include <async/async.hpp>
#include <net/net.hpp>
#include <media/media.hpp>

#ifndef WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <mbedtls/mbedtls_config.h> /// always must include first!

#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/error.h>
#include <mbedtls/debug.h>

#include <cstring>

namespace ion {

#define mbed_guard(x)\
    if (int ret = (x); ret != 0) {\
        console.fault("returned {0}", {ret});\
    }\

#define HTTP_RESPONSE \
    "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n" \
    "<h2>mbed TLS Test Server</h2>\r\n" \
    "<p>Successful connection using: %s</p>\r\n"

#define DEBUG_LEVEL 0

void mbedtls_debug(void *ctx, int level, const char *file, int line, const char *str) {
    ((void) level);

    fprintf((FILE *) ctx, "mbedtls: %s:%04d: %s", file, line, str);
    fflush((FILE *) ctx);
}

static const char *pers = "ion:net";

struct iTLS {
    mbedtls_net_context fd;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    //mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt srvcert;
    mbedtls_pk_context pkey;
#if defined(MBEDTLS_SSL_CACHE_C)
    mbedtls_ssl_cache_context cache;
    uri url;
#endif

    iTLS() { }

    void init(uri url) {
        this->url = url;
        static bool init;
        if (!init) {
    #ifdef _WIN32
            static WSADATA wsa_data;
            static int wsa = WSAStartup(MAKEWORD(2,2), &wsa_data);
            if (wsa != 0) {
                printf("(sock) WSAStartup failed: %d\n", wsa);
                return;
            }
    #endif
            init = true;
        }
        int ret;
        mbedtls_net_init(&fd);

        //mbedtls_ssl_init(&ssl);
        mbedtls_ssl_config_init(&conf);
    #if defined(MBEDTLS_SSL_CACHE_C)
        mbedtls_ssl_cache_init(&cache);
    #endif
        mbedtls_x509_crt_init(&srvcert);
        mbedtls_pk_init(&pkey);
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&ctr_drbg);

    #if defined(MBEDTLS_USE_PSA_CRYPTO)
        psa_status_t status = psa_crypto_init();
        if (status != PSA_SUCCESS) {
            mbedtls_fprintf(stderr, "Failed to initialize PSA Crypto implementation: %d\n",
                            (int) status);
            ret = MBEDTLS_ERR_SSL_HW_ACCEL_FAILED;
            goto exit;
        }
    #endif /* MBEDTLS_USE_PSA_CRYPTO */

    //#if defined(MBEDTLS_DEBUG_C)
        //mbedtls_debug_set_threshold(4);
    //#endif

        mbedtls_printf("  . Seeding the random number generator...");
        fflush(stdout);

        if ((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                        (const unsigned char *) pers,
                                        strlen(pers))) != 0) {
            mbedtls_printf(" failed\n  ! mbedtls_ctr_drbg_seed returned %d\n", ret);
            return;
        }

        mbedtls_printf(" ok\n");
        mbedtls_printf("\n  . Loading the server cert. and key...");
        fflush(stdout);

        /// load public and private keys
        str host = url.host();
        str pub  = fmt { "ssl/{0}.crt", { host } };
        str prv  = fmt { "ssl/{0}.key", { host } };
        ret = mbedtls_x509_crt_parse_file(&srvcert, pub.cs());
        if (ret != 0) {
            mbedtls_printf("mbedtls_x509_crt_parse returned %d\n\n", ret);
            return;
        }
        ret = mbedtls_pk_parse_keyfile(&pkey, prv.cs(), 0, mbedtls_ctr_drbg_random, &ctr_drbg);
        if (ret != 0) {
            mbedtls_printf("mbedtls_pk_parse_key returned %d\n\n", ret);
            return;
        }

        str port = str::from_integer(url.port());
        if ((ret = mbedtls_net_bind(&fd, host.cs(), port.cs(), MBEDTLS_NET_PROTO_TCP)) != 0) {
            mbedtls_printf("mbedtls_net_bind returned %d\n\n", ret);
            return;
        }

        if ((ret = mbedtls_ssl_config_defaults(&conf,
                                            MBEDTLS_SSL_IS_SERVER,
                                            MBEDTLS_SSL_TRANSPORT_STREAM,
                                            MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
            mbedtls_printf("mbedtls_ssl_config_defaults returned %d\n\n", ret);
            return;
        }

        mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
        mbedtls_ssl_conf_dbg(&conf, mbedtls_debug, stdout);

    #if defined(MBEDTLS_SSL_CACHE_C)
        mbedtls_ssl_conf_session_cache(&conf, &cache,
                                    mbedtls_ssl_cache_get,
                                    mbedtls_ssl_cache_set);
    #endif

        mbedtls_ssl_conf_ca_chain(&conf, srvcert.next, NULL);
        if ((ret = mbedtls_ssl_conf_own_cert(&conf, &srvcert, &pkey)) != 0) {
            mbedtls_printf("mbedtls_ssl_conf_own_cert returned %d\n\n", ret);
            return;
        }
    }

    ~iTLS() {
        mbedtls_net_free(&fd);
        mbedtls_x509_crt_free(&srvcert);
        mbedtls_pk_free(&pkey);
        //mbedtls_ssl_free(&ssl);
        mbedtls_ssl_config_free(&conf);
    #if defined(MBEDTLS_SSL_CACHE_C)
        mbedtls_ssl_cache_free(&cache);
    #endif
        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_entropy_free(&entropy);
    #if defined(MBEDTLS_USE_PSA_CRYPTO)
        mbedtls_psa_crypto_free();
    #endif /* MBEDTLS_USE_PSA_CRYPTO */
    }
};

mx_implement(TLS, mx, iTLS);

void tls_init(tls t, uri url) {
    t->init(url);
}

int uri::port() {
    if (data->port) return data->port;
    switch (data->proto.value) {
        case protocol::undefined: break;
        case protocol::http:  return 80;
        case protocol::https: return 443;
        case protocol::ssh:   return 22;
        case protocol::wss:   return 443;
    }
    assert(false);
    return 0;
}

struct Session {
    TLS                 tls;
    mbedtls_net_context fd = {};
    mbedtls_ssl_context ssl = {};
    bool                connected = false;
    num                 timeout_ms = 0;

    Session() {
        mbedtls_ssl_init(&ssl);
        mbedtls_net_init(&fd);
    }

    ///
    void init_accept(TLS tls) {
        mbedtls_ssl_init(&ssl);
        mbedtls_net_init(&fd);
        mbedtls_ssl_setup(&ssl, &tls->conf);
        // this worked sometimes, but now it doesnt
        //mbedtls_ssl_session_reset(&ssl);
    }

    /// was 
    void init_connect(uri addr) {
        tls = TLS(addr);
    }

    ~Session() {
        mbedtls_ssl_free(&ssl);
        mbedtls_net_free(&fd);
    }

    bool bind(uri addr) {
        str s_port = str::from_integer(addr.port());
        int res = mbedtls_net_bind(&fd, addr.host().cs(), (symbol)s_port.cs(), MBEDTLS_NET_PROTO_TCP);
        if (res != 0) {
            printf("mbedtls_net_bind: fails with %d\n", res);
            return false;
        }
        return true;
    }

    bool connect() {
        str    host = tls->url.host();
        int    port = tls->url.port();

        /// this operation runs first and its cache is used by mbed
        int ret = mbedtls_ssl_setup(&ssl, &tls->conf);
        if (ret != 0) {
            std::cerr << "mbedtls_ssl_setup failed: " << ret << std::endl;
            return {};
        }
        
        str  str_port = str::from_integer(port);
        ion::symbol s_hostname = ion::symbol(host.cs());
        ion::symbol s_port = ion::symbol(str_port.cs());
        ret = mbedtls_ssl_set_hostname(&ssl, s_hostname);

        if (ret != 0) {
            std::cerr << "mbedtls_ssl_set_hostname failed: " << ret << std::endl;
            return {};
        }
        
        ret = mbedtls_net_connect(&fd, s_hostname, s_port, MBEDTLS_NET_PROTO_TCP);
        if (ret != 0) {
            std::cerr << "mbedtls_net_connect failed: " << ret << std::endl;
            return {};
        }
        
        mbedtls_ssl_set_bio(&ssl, &fd, mbedtls_net_send, mbedtls_net_recv, NULL);
        
        while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
            if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                std::cerr << "mbedtls_ssl_handshake failed: " << ret << std::endl;
                return false;
            }
        }
        connected = true;
        return true;
    }


    bool close() {
        int ret;
        while ((ret = mbedtls_ssl_close_notify(&ssl)) < 0) {
            if (ret != MBEDTLS_ERR_SSL_WANT_READ &&
                ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                mbedtls_printf(" failed\n  ! mbedtls_ssl_close_notify returned %d\n\n", ret);
                return false;
            }
        }
        return true;
    }

    void set_timeout(i64 t) {
        timeout_ms = t;
    }

    bool read_sz(char *v, size_t sz) {
        int st = 0;
        ///
        for (int len = int(sz); len > 0;) {
            int rcv = mbedtls_ssl_read(&ssl, (u8*)&v[st], len);
            if (rcv <= 0)
                return false;
            len -= rcv;
            st  += rcv;
        }
        return true;
    }

    ssize_t recv(char* buf, size_t len) {
        ssize_t sz;
        do {
            sz = mbedtls_ssl_read(&ssl, (u8*)buf, len);
            if (sz == MBEDTLS_ERR_SSL_WANT_READ || sz == MBEDTLS_ERR_SSL_WANT_WRITE)
                continue;
            break;
        } while(1);
        return sz;
    }

    ssize_t send(const char* buf, size_t len) {
        ssize_t ret;
        while ((ret = mbedtls_ssl_write(&ssl, (const u8*)buf, len)) <= 0) {
            if (ret == MBEDTLS_ERR_NET_CONN_RESET)
                return 0;
            if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
                break;
        }
        return ret;
    }

    ssize_t send(str templ, Array<mx> args) {
        str val = str::format(templ.cs(), args);
        return send(val.cs(), val.len());
    }

    /// for already string-like memory; this could do something with the type on mx
    ssize_t send(mx &v) {
        return send((const char*)v.mem->origin, v.count() * v.mem->type->data_size()); /// total_size if it has a schema, base_sz otherwise
    }

    Array<char> read_until(str s, int max_len) {
        auto rbytes = Array<char>(size_t(max_len));
        size_t slen = s.len();
        ///
        for (;;) {
            rbytes   += '\0';
            size_t sz = rbytes.len();
            if (!recv(&rbytes[sz - 1], size_t(1)))
                return Array<char> { };
            if (sz >= slen && memcmp(&rbytes[sz - slen], s.cs(), slen) == 0)
                break;
            if (sz == max_len)
                return Array<char> { };
        }
        return rbytes;
    }

    static sock accept(TLS &tls) {
        sock client;
        client->init_accept(tls);
        
        /// only accept client that passes a handshake
        for (;;) {
            mbedtls_net_init(&client->fd);
            //mbedtls_ssl_session_reset(&client->ssl);
            mbedtls_ssl_setup(&client->ssl, &client->tls->conf);

            int ret;
            /// accept into client_fd from tls server
            if ((ret = mbedtls_net_accept(&tls->fd, &client->fd, null, 0, null)) != 0) {
                return {};
            }
            mbedtls_ssl_session_reset(&client->ssl);
            
            bool retry = false;
            mbedtls_ssl_set_bio(&client->ssl, &client->fd, mbedtls_net_send, mbedtls_net_recv, NULL);
            while ((ret = mbedtls_ssl_handshake(&client->ssl)) != 0) {
                if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                    console.log("mbedtls_ssl_handshake: {0}", {ret});
                    retry = true;
                    break;
                }
            }
            if (!retry)
                break;
        }
        client->connected = true;
        
        return client;
    }
};

mx_implement(sock, mx, Session);

/// how do avoid a default allocation?
/// what argument/type performs an allocation? construct with type_t
sock::sock(TLS tls)  : sock() { data->tls = tls; }
sock::sock(uri addr) : sock(TLS(addr)) { }

bool        sock::bind(uri addr)                    { return data->bind(addr); }
bool        sock::connect()                         { return data->connect(); }
bool        sock::close()                           { return data->close(); }
void        sock::set_timeout(i64 t)                { data->set_timeout(t); }
bool        sock::read_sz(char *v, size_t sz)       { return data->read_sz(v, sz); }
ssize_t     sock::recv(char* buf, size_t len)       { return data->recv(buf, len); }
ssize_t     sock::send(const char* buf, size_t len) { return data->send(buf, len); }
ssize_t     sock::send(str templ, Array<mx> args)   { return data->send(templ, args); }
ssize_t     sock::send(mx &v)                       { return data->send(v); }
Array<char> sock::read_until(str s, int max_len)    { return data->read_until(s, max_len); }
sock        sock::accept(TLS tls)                   {
    return Session::accept(tls);
}
            sock::operator bool()                   { return data->connected; }

async sock::listen(uri url, lambda<bool(sock&)> fn) {
    TLS tls = TLS(url);

    return async(1, [tls, url, fn](runtime *rt, int i) -> mx {
        for (;;) {
            sock client { sock::accept(tls) };
            if (!client)
                break;
            /// run fn async from next accept
            async(1, [fn, client](runtime *rt0, int i0) -> mx {
                fn((sock&)client);
                return true;
            });
        }
        return true;
    });
}


str dns(str hostname) {
    struct addrinfo hints, *res, *p;
    int    status;
    char   ip[INET6_ADDRSTRLEN];
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;   // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // Specify socket type

    /// perform DNS lookup
    status = getaddrinfo(symbol(hostname.cs()), nullptr, &hints, &res);
    if (status != 0) {
        console.log("DNS lookup failed: {0}", { status });
        return null;
    }
    
    str result;

    /// iterate through the address list and print the IP addresses
    for (p = res; p != nullptr; p = p->ai_next) {
        void* addr;
        if (p->ai_family == AF_INET) {
            struct sockaddr_in* ipv4 = reinterpret_cast<struct sockaddr_in*>(p->ai_addr);
            addr = &(ipv4->sin_addr);
        } else {
            struct sockaddr_in6* ipv6 = reinterpret_cast<struct sockaddr_in6*>(p->ai_addr);
            addr = &(ipv6->sin6_addr);
        }

        /// convert IP address to string
        inet_ntop(p->ai_family, addr, ip, sizeof(ip));
        result = ip;
        break;
    }

    /// free memory
    freeaddrinfo(res);
    return result;
}

str uri::addr() {
    return dns(data->host);
}

message::message(int server_code) : message() {
    data->code = mx(server_code);
}

message::message(symbol text) : message() {
    data->content = text;
    data->code    = 200;
}

message::message(str text) : message() {
    data->content = text;
    data->code    = 200;
}

message::message(path p, mx modified_since) : message() {
    assert(p.exists());
    str s_content = p.read<str>();
    data->content = s_content;
    str s_mime = p.mime_type();
    data->headers["Content-Type"]   = s_mime;
    //data->headers["Content-Length"] = int(s_content.len());
    data->code                      = 200;
}

message::message(mx content, map headers, uri query) : message() {
    data->query   = query;
    data->headers = headers;
    data->content = content; /// assuming the content isnt some error thing
    data->code    = 200;
}

message::message(uri url, map headers) : message() {
    data->query   = url;
    data->headers = headers; // ion::pair<ion::mx, ion::mx>    ion::pair<ion::mx, ion::mx>
}

/// important to note we do not keep holding onto this socket
message::message(sock &sc) : message() {
    if (read_headers(sc)) {
        //console.log("received headers:");
        //data->headers.print();
        read_content(sc);
        str status = hold(data->headers["Status"]);
        data->code = int(status.integer_value());
    }
}

uri message::query() { return data->query; }

bool message::read_headers(sock &sc) {
    int line = 0;
    ///
    for (;;) {
        Array<char> rbytes = sc.read_until({ "\r\n" }, 8192);
        size_t sz = rbytes.len();
        if (sz == 0)
            return false;
        
        if (sz == 2)
            break;
        ///
        if (line++ == 0) {
            str hello = str(rbytes.data<char>(), int(sz - 2));
            i32  code = 0;
            Array<str> sp = hello.split(" ");
            /// this needs to adjust for from-server or from-client (HTTP/1.1 200 OK) (GET uri type)
            if (hello.len() >= 12) { // HTTP/1.1 200 OK is the smallest, 12 chars
                if (sp.len() >= 3) { /// METHOD URI HTTP_VER <- minimum
                    data->query = hello;
                    code = i32(sp[1].integer_value());
                    data->headers["Status"] = str::from_integer(code);
                }
            }
            ///
        } else {
            for (size_t i = 0; i < sz; i++) {
                if (rbytes[i] == ':') {
                    str k = str(&rbytes[0], int(i));
                    str v = str(&rbytes[i + 2], int(sz) - int(k.len()) - 2 - 2);
                    data->headers[k] = v;
                    break;
                }
            }
        }
    }
    return true;
}

///
bool message::read_content(sock &sc) {
    str              te = "Transfer-Encoding";
    str              cl = "Content-Length";
    str              ce = "Content-Encoding";
    str        encoding =     data->headers->count(te) ? str(hold(data->headers[ce])) : str();
    int            clen = int(data->headers->count(cl) ? str(hold(data->headers[cl])).integer_value() : -1);
    bool        chunked = encoding && data->headers[te] == "chunked";
    num     content_len = clen;
    num            rlen = 0;
    const num     r_max = 1024;
    bool          error = false;
    int            iter = 0;
    Array<char> v_data;
    ///
    assert(!(clen >= 0 && chunked));

    if (!chunked && clen <= 0) {
        data->content = {};
        return true;
    }
    ///
    if (!(!chunked && clen == 0)) {
        do {
            if (chunked) {
                if (iter++ > 0) {
                    char crlf[2];
                    if (!sc.read_sz(crlf, 2) || memcmp(crlf, "\r\n", 2) != 0) {
                        error = true;
                        break;
                    }
                }
                Array<char> rbytes = sc.read_until({ "\r\n" }, 64);
                if (!rbytes) {
                    error = true;
                    break;
                }
                std::stringstream ss;
                ss << std::hex << rbytes.data<char>();
                ss >> content_len;
                if (content_len == 0) /// this will effectively drop out of the while loop
                    break;
            }
            bool sff = content_len == -1;
            for (num rcv = 0; sff || rcv < content_len; rcv += rlen) {
                num   rx = math::min(1, 1);
                char  buf[r_max];
                rlen = sc.recv(buf, rx);
                if (rlen > 0)
                    v_data.push(buf, rlen);
                else if (rlen < 0) {
                    error = !sff;
                    /// it is an error if we expected a certain length
                    /// if the connection is closed during a read then
                    /// it just means the peer ended the transfer (hopefully at the end)
                    break;
                } else if (rlen == 0) {
                    error = true;
                    break;
                }
            }
        } while (!error && chunked && content_len != 0);
    }
    ///
    mx    rcv = error ? mx() : mx(v_data);
    str ctype = data->headers->count("Content-Type") ? str(hold(data->headers["Content-Type"])) : str("");

    ///
    if (ctype.split(";").count_of("application/json")) {
        if (encoding) {
            if (encoding == "gzip") {
                rcv = ion::inflate(rcv);
            }
        }
        /// read content
        Array<char> js { rcv };
        var   obj = var::json(js, null);
        data->content = obj;
    }
    else if (ctype.starts_with("text/")) {
        data->content = str(&rcv.ref<char>(), int(rcv.byte_len()));
    } else {
        /// other non-text types must be supported, not going to just leave them as array for now
        assert(v_data.len() == 0);
        data->content = var(null);
    }
    return !error;
}

/// query/request construction
message message::query(uri server, map headers, mx content) {
    message m;
    m->query   = { server, web::Get };
    m->headers = headers;
    m->content = content;
    return m;
}

/// response construction, uri is not needed
message message::response(uri query, mx code, mx content, map headers) {
    message r;
    r->query    = { query, web::Response };
    r->code     = code;
    r->headers  = headers;
    r->content  = content;
    return r;
}

message::operator bool() {
    type_t ct = data->code.type();
    assert(ct == typeof(i32) || ct == typeof(str));
    int ic = int(data->code);
    return (data->query.mtype() != web::undefined || 
        (ct == typeof(i32) && (ic == 0 && (data->content || data->headers)) || 
        (ic >= 200 && ic < 300)));
}

bool message::operator!() { return !(operator bool()); }

bool message::printable_value(const mx &v) {
    return v.type() != typeof(mx) || v.count() > 0;
}

symbol message::code_symbol(int code) {
    static map symbols = {
        field {200, "OK"},
        field {201, "Created"},
        field {202, "Accepted"},
        field {203, "Non-Authoritative Information"},
        field {204, "No Content"},
        field {205, "Reset Content"},
        field {206, "Partial Content"},
        field {300, "Multiple Choices"},
        field {301, "Moved Permanently"},
        field {302, "Found"},
        field {303, "See Other"},
        field {304, "Not Modified"},
        field {307, "Temporary Redirect"},
        field {308, "Permanent Redirect"},
        field {400, "Bad Request"},
        field {402, "Payment Required"},
        field {403, "Forbidden"},
        field {404, "Not Found"},
        field {500, "Internal Server Error"},
        field {0,   "Unknown"}
    };
    size_t count  = symbols->count(code);
    symbol result = count ? symbols.get<symbol>(code) : symbols.get<symbol>(0); /// map doesnt work.
    return result;
}

bool message::write_status(sock &sc) {
    mx status = "Status";
    int  code = data->headers->count(status) ? int(data->headers[status]) : (data->code ? int(data->code) : int(200));
    return sc.send("HTTP/1.1 {0} {1}\r\n", { code, code_symbol(code) });
}

///  code is in headers.
bool message::write_headers(sock &sc) {
    for (auto &[k,v]: data->headers.fields()) { /// mx key, mx value
        /// check if header is valid data; holding ref to mx
        /// requires one to defer valid population of a
        /// resulting header injected by query
        mx mx_k(k);
        if (mx_k == "Status" || !printable_value(v))
            continue;
        
        console.log("{0}: {1}", { k, v });
        
        ///
        if (!sc.send("{0}: {1}", { k, v }))
            return false;
        ///
        if (!sc.send("\r\n", 2))
            return false;
    }
    if (!sc.send("\r\n", 2))
        return false;
    ///
    return true;
}

bool message::write(sock &sc) {
    /// send http response code if this message has one
    int ic = int(data->code);
    if (ic > 0) {
        symbol s = code_symbol(ic);
        assert(s);
        str header = fmt { "HTTP/1.1 {0} {1}\r\n", { ic, s } };
        if (!sc.send(header))
            return false;
    }

    if (data->content) {
        type_t ct = data->content.type();
        /// map of mx must be json compatible, or be structured in that process
        if (!data->headers->count("Content-Type") && (ct == typeof(map) || ct == typeof(mx)))
            data->headers["Content-Type"] = "application/json";
        ///
        data->headers["Connection"] = "keep-alive";
        
        /// do different things based on type
        if (data->headers->count("Content-Type") && data->headers["Content-Type"] == "application/json") {
            /// json transfer; content can be anything, its just mx..
            /// so we convert to json explicitly. var is just an illusion as you can see friend.
            str json = var(data->content).stringify();
            data->headers["Content-Length"] = str(json.len());
            write_headers(sc);
            return sc.send(json);
            ///
        } else if (ct == typeof(map)) {
            /// post fields transfer
            str post = uri::encode_fields(map(data->content));
            data->headers["Content-Length"] = str::from_integer(post.len());
            write_headers(sc);
            return sc.send(post);
            ///
        } else if (ct == typeof(u8)) { /// different from char.  char is for text and u8 is for binary.
            /// binary transfer
            data->headers["Content-Length"] = str::from_integer(data->content.byte_len()); /// size() is part of mx object, should always return it size in its own unit, but a byte_size is there too
            return sc.send(data->content);
            ///
        } else if (ct == typeof(null_t)) {
            /// can be null_t
            data->headers["Content-Length"] = str("0");
            console.log("sending Content-Length of {0}", { data->headers["Content-Length"] });
            return write_headers(sc);
            ///
        } else {
            /// cstring
            assert(ct == typeof(char));
            data->headers["Content-Length"] = str::from_integer(data->content.byte_len());
            console.log("sending Content-Length of {0}", { data->headers["Content-Length"] });
            write_headers(sc);
            return sc.send(data->content);
        }
    }
    data->headers["Content-Length"] = 0;
    data->headers["Connection"]     = "keep-alive";
    return write_headers(sc);
}

str message::text() { return data->content.to_string(); }

/// structure cookies into usable format
map message::cookies() {
    str  dec = uri::decode(hold(data->headers["Set-Cookie"]));
    Array<str> sp = dec.split(",");
    str  all = sp[0];
    auto sep = all.split(";");
    auto res = map();
    ///
    for (str &s: sep.elements<str>()) {
        auto pair = s.split("="); assert(pair.len() >= 2);
        str  &key = pair.get<str>(0);
        if (!key)
            break;
        str  &val = pair.get<str>(1);
        res[key]  = val;
    }
    ///
    return res;
}

mx &message::operator[](symbol key) {
    return data->headers[key];
}

mx &message::header(mx key) {
    return data->headers[key];
}

/// utility function for web requests
/// uri is copied as a methodized uri based on the type given
future request(uri url, map args) {
    return async(1, [url, args](auto p, int i) -> mx {
        map st_headers;
        mx    null_content; /// consider null static on var, assertions on its write by sequence on debug
        map         &a = *(map *)&args; /// so lame.  stop pretending that this cant change C++
        map    headers = a->count("headers") ? map(a["headers"]) : st_headers;
        mx         content = a->count("content") ?     a["content"] : null_content;
        web           type = a->count("method")  ? web(a["method"]) : web(web::Get);
        uri          query = url.methodize(type);
        
        ///
        assert(query != web::undefined);
        sock client { query };
        console.log("(net) request: {0}", { url });
        if (!client.connect()) return {};
        
        /// start request (this time get the casing right)
        client.send("{0} {1} HTTP/1.1\r\n", { query.mtype().name().ucase(), query.string() });
        console.log("{0} {1} HTTP/1.1\r\n", { query.mtype().name().ucase(), query.string() });
        
        /// default headers
        Array<field> defs = {
            { "User-Agent",      "ion:net"              },
            { "Accept",          "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8" },
            { "Accept-Language", "en-US,en;q=0.9"       },
            { "Accept-Encoding", "gzip, deflate, br"    },
            { "Host",             query.host()          }
        };
        
        /// User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/16.3 Safari/605.1.15
        /// Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8
        /// Accept-Language: en-US,en;q=0.9
        /// Connection: keep-alive
        /// Accept-Encoding: gzip, deflate, br
        /// Host: www.googleapis.com
        
        for (auto &d: defs)
            if (!headers(d.key))
                 headers[d.key] = d.value;
        
        message request { content, headers, query }; /// best to handle post creation within message
        request.write(client);
        
        /// read response, this is a message which constructs with a sock; does not store
        message response { client };
        
        /// close socket
        client.close();
        return response;
    });
}

future json(uri addr, map args, map headers) {
    lambda<void(mx)> success, failure;
    completer c  = { success, failure };
    ///
    request(addr, headers).then([ success, failure ](mx d) {
        (d.type() == typeof(map)) ?
        success(d) : failure(d);
        ///
    }).except([ failure ](mx d) {
        failure(d);
    });
    ///
    return future(c);
}

}
