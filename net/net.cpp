#include <mx/mx.hpp>
#include <async/async.hpp>
#include <net/net.hpp>
#include <image/image.hpp>
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

void mbedtls_debug(void *ctx, int level, const char *file, int line, const char *str) {
    ((void) level);

    fprintf((FILE *) ctx, "mbedtls: %s:%04d: %s", file, line, str);
    fflush((FILE *) ctx);
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
        std::cerr << "DNS lookup failed: " << gai_strerror(status) << std::endl;
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

/// isolated isock containing state info for socket
struct isock {
    sock::role                role;
    uri                       query;
    i64                       timeout_ms;
    bool                      connected;

    mbedtls_net_context       ctx;
    mbedtls_entropy_context   entropy;
    mbedtls_ctr_drbg_context  ctr_drbg;
    mbedtls_ssl_context       ssl;
    mbedtls_x509_crt          cert_chain;
    mbedtls_pk_context        pkey;
    mbedtls_ssl_config        conf;
        
    void load_keypair(path pub, path priv, str pass = null) {
        assert(!pass);
        /// replace "root_cert.pem" and "intermediate_cert.pem" with your actual file paths
        if (mbedtls_x509_crt_parse_file(&cert_chain, pub) != 0)
            console.fault("failed to parse public cert");
        /// private key
        if (mbedtls_pk_parse_keyfile(&pkey, priv, null, null, null) != 0)
            console.fault("failed to parse private key");
    }

    type_register(isock);

    /// methods
    void ssl_reinit() {

        mbedtls_net_init(&ctx);
        mbedtls_ssl_init(&ssl);
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&ctr_drbg);
        mbedtls_x509_crt_init(&cert_chain);
        mbedtls_ssl_config_init(&conf);
    }
    
    void ssl_cleanup() {
        mbedtls_net_free(&ctx);
        mbedtls_ssl_free(&ssl);
        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_entropy_free(&entropy);
        mbedtls_x509_crt_free(&cert_chain);
        mbedtls_ssl_config_free(&conf);
    }
    
    isock() {
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
        ssl_reinit();
    }
    
    ~isock() {
        ssl_cleanup();
    }
};

mx_implement(sock, mx);

sock::~sock() {
    close();
    data->ssl_cleanup();
}

///
void sock::set_timeout(i64 t) {
    data->timeout_ms = t;
}

sock &sock::establish() {
    return *this;
}

sock sock::accept() {
    sock sc;
    ///
    /// establish could be rewritten with connect
    return sc.establish();
}



/// listen on https using mbedtls
/// ---------------------------------
async sock::listen(uri url, lambda<bool(sock&)> fn) {
    return async(1, [&, url, fn](runtime *rt, int i) -> mx {
        uri bind = url.methodize(method::get);
        
        /// proceed if https; that is our protocol and we know nothing else
        protocol &pr = bind.proto();
        assert(pr == "https");
        
        int      port = bind.port();
        str host_name = bind.host();
        console.log("(net) listen on: {0}:{1}", { host_name, port });
        sock server { role::server, bind };
        
        /// loop around grabbing anyone who comes waltzing
        while (1) {
            sock client = server.accept();
            if (!client)
                break;
            
            /// spawn thread for the given callback -- this lets us accept again right away, on this thread
            async {1, [client=client, fn=fn](runtime *rt, int i) -> mx {
                sock& cl = (sock&)client;
                if (fn(cl))
                    cl.close();
                return true;
            }};
        }
        return true;
    });
}

void sock::close() {
    if (data->connected) {
        mbedtls_ssl_close_notify(&data->ssl);
        data->connected = false;
    }
}

bool sock::read_sz(u8 *v, size_t sz) {
    int st = 0;
    ///
    for (int len = int(sz); len > 0;) {
        int rcv = mbedtls_ssl_read(&data->ssl, &v[st], len);
        if (rcv <= 0)
            return false;
        len -= rcv;
        st  += rcv;
    }
    return true;
}

array<char> sock::read_until(str s, int max_len) {
    auto rbytes = array<char>(size_t(max_len));
    size_t slen = s.len();
    ///
    for (;;) {
        rbytes   += '\0';
        size_t sz = rbytes.len();
        if (!recv((unsigned char*)&rbytes[sz - 1], size_t(1)))
            return array<char> { };
        if (sz >= slen && memcmp(&rbytes[sz - slen], s.cs(), slen) == 0)
            break;
        if (sz == max_len)
            return array<char> { };
    }
    return rbytes;
}

sock::operator bool() {
    return data->connected;
}

bool sock::bind(str adapter, int port) {
    str    str_port = str::from_integer(port);
    symbol sym_port = symbol(str_port);
    int res = mbedtls_net_bind(&data->ctx, adapter ? adapter.cs() : "0.0.0.0",
                     sym_port, MBEDTLS_NET_PROTO_TCP);
    return res != 0;
}

sock sock::connect(uri url) {
    str    host = url.host();
    int    port = url.port();
    sock   res { sock::role::client, url };
    isock *isc  = res.data;
    /// this operation runs first and its cache is used by mbed
    int ret = mbedtls_ssl_setup(&isc->ssl, &isc->conf);
    if (ret != 0) {
        std::cerr << "mbedtls_ssl_setup failed: " << ret << std::endl;
        return {};
    }
    
    str  str_port = str::from_integer(port);


    ion::symbol s_hostname = ion::symbol(host.cs());
    ion::symbol s_port = ion::symbol(str_port.cs());


    ret = mbedtls_ssl_set_hostname(&isc->ssl, s_hostname);
    if (ret != 0) {
        std::cerr << "mbedtls_ssl_set_hostname failed: " << ret << std::endl;
        return {};
    }
    
    ret = mbedtls_net_connect(&isc->ctx, s_hostname, s_port, MBEDTLS_NET_PROTO_TCP);
    if (ret != 0) {
        std::cerr << "mbedtls_net_connect failed: " << ret << std::endl;
        return {};
    }
    
    mbedtls_ssl_set_bio(&isc->ssl, &isc->ctx, mbedtls_net_send, mbedtls_net_recv, NULL);
    
    while ((ret = mbedtls_ssl_handshake(&isc->ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            std::cerr << "mbedtls_ssl_handshake failed: " << ret << std::endl;
            mbedtls_net_free(&isc->ctx);
            return {};
        }
    }
    isc->connected = true;
    return res;
}


sock::sock(role r, uri addr) : sock() {
    data->role  = r;
    data->query = addr;
    ///
    assert(addr.proto() == "https");
    ///
    str s_address = addr.host();
    int i_port = addr.port();
    load_certs(s_address);
    
    if (data->role == role::server)
        bind(s_address, i_port);
}

ssize_t sock::recv(unsigned char* buf, size_t len) {
    return mbedtls_ssl_read(&data->ssl, buf, len);
}

ssize_t sock::send(const unsigned char* buf, size_t len) {
    return mbedtls_ssl_write(&data->ssl, buf, len);
}

ssize_t sock::send(str templ, array<mx> args) {
    str val = str::format(templ.cs(), args);
    return mbedtls_ssl_write(&data->ssl, (const unsigned char*)val.cs(), val.len());
}

/// for already string-like memory; this could do something with the type on mx
ssize_t sock::send(mx &v) {
    return send((u8*)v.mem->origin, v.count() * v.mem->type->size()); /// total_size if it has a schema, base_sz otherwise
}

void sock::load_certs(str host) {
    mbedtls_ssl_config *conf = &data->conf;
    mbedtls_ssl_conf_dbg(conf, mbedtls_debug, stdout);
    mbedtls_debug_set_threshold(4);
    
    int ret = mbedtls_ssl_config_defaults(
        conf,
        MBEDTLS_SSL_IS_CLIENT,
        MBEDTLS_SSL_TRANSPORT_STREAM,
        MBEDTLS_SSL_PRESET_DEFAULT);
    
    if (ret != 0) console.fault("mbedtls_ssl_config_defaults failed: {0}", { ret });
    
    ret = mbedtls_ctr_drbg_seed(&data->ctr_drbg, mbedtls_entropy_func, &data->entropy,
        (const unsigned char *)pers, strlen(pers));
    
    
    mbedtls_ssl_conf_authmode   (conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain   (conf, &data->cert_chain, NULL);
    mbedtls_ssl_conf_rng        (conf, mbedtls_ctr_drbg_random, &data->ctr_drbg);
    mbedtls_ssl_conf_min_version(conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);  // TLS 1.2
    
    /// load cert and key based on role
    if (data->role == role::server) {
        data->load_keypair("ssl/public.crt", "ssl/priv.key");
    } else {
        /// host just has a sequence starting with root, intermediate, etc, so 0 = CA Root, 1 = Signed from CA Root, 2 = Signed by 1
        for (size_t ii = 0; ii < 16; ii++) {
            path p = fmt { "trust/{0}.{1}.pem", { host, int(ii) }};
            bool exists = p.exists();
            if (!exists) {
                if (ii == 0) console.fault("no trust for this host..."); /// all hosts should have something
                break;
            }
            symbol file = symbol(p.cs());
            if (mbedtls_x509_crt_parse_file(&data->cert_chain, file) != 0)
                console.fault("failed to parse trust: {0}", { file });
        }
    }
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
    data->content = p.read<str>();
    data->headers["Content-Type"] = p.mime_type();
    data->code    = 200;
}

message::message(mx content, map<mx> headers, uri query) : message() {
    data->query   = query;
    data->headers = headers;
    data->content = content; /// assuming the content isnt some error thing
    data->code    = 200;
}

message::message(uri url, map<mx> headers) : message() {
    data->query   = url;
    data->headers = headers; // ion::pair<ion::mx, ion::mx>    ion::pair<ion::mx, ion::mx>
}

/// important to note we do not keep holding onto this socket
message::message(sock &sc) : message() {
    read_headers(sc);
    //console.log("received headers:");
    //data->headers.print();
    read_content(sc);
    data->code = data->headers["Status"];
}

uri message::query() { return data->query; }

bool message::read_headers(sock &sc) {
    int line = 0;
    ///
    for (;;) {
        array<char> rbytes = sc.read_until({ "\r\n" }, 8192);
        size_t sz = rbytes.len();
        if (sz == 0)
            return false;
        
        if (sz == 2)
            break;
        ///
        if (line++ == 0) {
            str hello = str(rbytes.data, int(sz - 2));
            i32  code = 0;
            auto   sp = hello.split(" ");
            if (hello.len() >= 12) {
                if (sp.len() >= 2)
                    code = i32(sp[1].integer_value());
            }
            data->headers["Status"] = str::from_integer(code);
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
    str        encoding =     data->headers->count(te) ? str(data->headers[ce].grab()) : str();
    int            clen = int(data->headers->count(cl) ? str(data->headers[cl].grab()).integer_value() : -1);
    bool        chunked = encoding && data->headers[te] == "chunked";
    num     content_len = clen;
    num            rlen = 0;
    const num     r_max = 1024;
    bool          error = false;
    int            iter = 0;
    array<uint8_t> v_data;
    ///
    assert(!(clen >= 0 && chunked));
    ///
    if (!(!chunked && clen == 0)) {
        do {
            if (chunked) {
                if (iter++ > 0) {
                    u8 crlf[2];
                    if (!sc.read_sz(crlf, 2) || memcmp(crlf, "\r\n", 2) != 0) {
                        error = true;
                        break;
                    }
                }
                array<char> rbytes = sc.read_until({ "\r\n" }, 64);
                if (!rbytes) {
                    error = true;
                    break;
                }
                std::stringstream ss;
                ss << std::hex << rbytes.data;
                ss >> content_len;
                if (content_len == 0) /// this will effectively drop out of the while loop
                    break;
            }
            num   rx = math::min(1, 1);
            bool sff = content_len == -1;
            for (num rcv = 0; sff || rcv < content_len; rcv += rlen) {
                num   rx = math::min(1, 1);
                uint8_t  buf[r_max];
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
    str ctype = data->headers->count("Content-Type") ? str(data->headers["Content-Type"].grab()) : str("");

    ///
    if (ctype.split(";").count("application/json")) {
        if (encoding) {
            if (encoding == "gzip") {
                rcv = ion::inflate(rcv);
            }
        }
        /// read content
        size_t sz = rcv.byte_len();
        array<char> js { rcv };
        var   obj = var::json(js);
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
message message::query(uri server, map<mx> headers, mx content) {
    /// default state
    message m;
    
    /// acquire members, and set
    message::members& q = m;
    q.query   = { server, method::get };
    q.headers = headers;
    q.content = content;
    
    /// return state
    return m;
}

/// response construction, uri is not needed
message message::response(uri query, mx code, mx content, map<mx> headers) {
    message rs;
    message::members& r = rs;
    r.query    = { query, method::response };
    r.code     = code;
    r.headers  = headers;
    r.content  = content;
    return rs;
}

message::operator bool() {
    type_t ct = data->code.type();
    assert(ct == typeof(i32) || ct == typeof(str));
    return (data->query.mtype() != method::undefined) &&
    ((ct == typeof(i32) && int(data->code) >= 200 && int(data->code) < 300) ||
     (ct == typeof(str) && data->code.byte_len()));
}

bool message::operator!() { return !(operator bool()); }

bool message::printable_value(mx &v) {
    return v.type() != typeof(mx) || v.count() > 0;
}

symbol message::code_symbol(int code) {
    static map<symbol> symbols = {
        {200, "OK"}, {201, "Created"}, {202, "Accepted"}, {203, "Non-Authoritative Information"},
        {204, "No Content"}, {205, "Reset Content"}, {206, "Partial Content"},
        {300, "Multiple Choices"}, {301, "Moved Permanently"}, {302, "Found"},
        {303, "See Other"}, {304, "Not Modified"}, {307, "Temporary Redirect"},
        {308, "Permanent Redirect"}, {400, "Bad Request"}, {402, "Payment Required"},
        {403, "Forbidden"}, {404, "Not Found"}, {500, "Internal Server Error"},
        {0,   "Unknown"}
    };
    size_t count  = symbols->count(code);
    symbol result = count ? symbols[code] : symbols[0]; /// map doesnt work.
    return result;
}

bool message::write_status(sock &sc) {
    mx status = "Status";
    int  code = data->headers->count(status) ? int(data->headers[status]) : (data->code ? int(data->code) : int(200));
    return sc.send("HTTP/1.1 {0} {1}\r\n", { code, code_symbol(code) });
}

///  code is in headers.
bool message::write_headers(sock &sc) {
    for (auto &[v, k]: data->headers) { /// mx key, mx value
        /// check if header is valid data; holding ref to mx
        /// requires one to defer valid population of a
        /// resulting header injected by query
        if (k == "Status" || !printable_value(v))
            continue;
        
        console.log("{0}: {1}", { k, v });
        
        ///
        if (!sc.send("{0}: {1}", { k, v }))
            return false;
        ///
        if (!sc.send("\r\n"))
            return false;
    }
    if (!sc.send("\r\n"))
        return false;
    ///
    return true;
}

bool message::write(sock &sc) {
    if (data->content) {
        type_t ct = data->content.type();
        /// map of mx must be json compatible, or be structured in that process
        if (!data->headers->count("Content-Type") && (ct == typeof(map<mx>) || ct == typeof(mx)))
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
        } else if (ct == typeof(map<mx>)) {
            /// post fields transfer
            str post = uri::encode_fields(map<mx>(data->content));
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
map<str> message::cookies() {
    str  dec = uri::decode(data->headers["Set-Cookie"].grab());
    str  all = dec.split(",")[0];
    auto sep = all.split(";");
    auto res = map<str>();
    ///
    for (str &s: sep) {
        auto pair = s.split("="); assert(pair.len() >= 2);
        str   key = pair[0];
        if (!key)
            break;
        str   val = pair[1];
        res[key]  = val;
    }
    ///
    return res;
}

mx &message::operator[](symbol key) {
    return data->headers[key];
}

mx &message::header(mx key) { return data->headers[key]; }

async service(uri bind, lambda<message(message)> fn_process) {
    return sock::listen(bind, [fn_process](sock &sc) -> bool {
        bool close = false;
        for (close = false; !close;) {
            close  = true;
            array<char> msg = sc.read_until("\r\n", 4092); // this read 0 bytes, and issue ensued
            str param;
            ///
            if (!msg) break;
            
            /// its 2 chars off because read_until stops at the phrase
            uri req_uri = uri::parse(str(msg.data, int(msg.len() - 2)));
            ///
            if (req_uri) {
                console.log("request: {0}", { req_uri.resource() });
                message  req { req_uri };
                message  res = fn_process(req);
                close        = req.header("Connection") == "close";
                res.write(sc);
            }
        }
        return close;
    });
}

/// utility function for web requests
/// uri is copied as a methodized uri based on the type given
future request(uri url, map<mx> args) {
    return async(1, [url, args](auto p, int i) -> mx {
        map<mx> st_headers;
        mx    null_content; /// consider null static on var, assertions on its write by sequence on debug
        map<mx>         &a = *(map<mx> *)&args; /// so lame.  stop pretending that this cant change C++
        map<mx>    headers = a->count("headers") ? map<mx>(a["headers"]) : st_headers;
        mx         content = a->count("content") ?         a["content"]  : null_content;
        method        type = a->count("method")  ?  method(a["method"])  : method(method::get);
        uri          query = url.methodize(type);
        
        ///
        assert(query != method::undefined);
        console.log("(net) request: {0}", { url });
        sock client = sock::connect(query);
        if (!client) return {};
        
        /// start request (this time get the casing right)
        client.send("{0} {1} HTTP/1.1\r\n", { query.mtype().name().ucase(), query.string() });
        console.log("{0} {1} HTTP/1.1\r\n", { query.mtype().name().ucase(), query.string() });
        
        /// default headers
        array<field<mx>> defs = {
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
        
        uri::components *comp = query.get<uri::components>(0);
        
        message request { content, headers, query }; /// best to handle post creation within message
        request.write(client);
        
        /// read response, this is a message which constructs with a sock; does not store
        message response { client };
        
        /// close socket
        client.close();
        return response;
    });
}

future json(uri addr, map<mx> args, map<mx> headers) {
    lambda<void(mx)> success, failure;
    completer c  = { success, failure };
    ///
    request(addr, headers).then([ success, failure ](mx d) {
        (d.type() == typeof(map<mx>)) ?
        success(d) : failure(d);
        ///
    }).except([ failure ](mx d) {
        failure(d);
    });
    ///
    return future(c);
}

}
