#include <core/core.hpp>
#include <async/async.hpp>
#include <net/net.hpp>
#include <math/math.hpp>
#include <image/image.hpp>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <iostream>
#include <cstring>

#ifdef _WIN32
#   include <Winsock2.h>
#else
#   include <arpa/inet.h>
#endif

///
/*
#include <wolfssl/options.h>
#include <wolfssl/ssl.h>
#include <wolfssl/test.h>
#include <wolfssl/wolfio.h>
///
#ifdef _WIN32
#   include <Winsock2.h>
#else
#   include <arpa/inet.h>
#endif
///
#include <errno.h>
*/


#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/error.h>
#include <mbedtls/debug.h>
#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

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
        return;
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


void test_mbed2() {
    int                         ret, len;
    unsigned char               buf[1024];
    mbedtls_net_context         ctx;
    mbedtls_entropy_context     entropy;
    mbedtls_ctr_drbg_context    ctr_drbg;
    mbedtls_ssl_context         ssl;
    mbedtls_ssl_config          conf;
    mbedtls_x509_crt            cacert;

    const char *pers = "ion::net";

    mbedtls_net_init( &ctx );
    mbedtls_ssl_init( &ssl );
    mbedtls_ssl_config_init( &conf );
    mbedtls_x509_crt_init( &cacert );
    mbedtls_ctr_drbg_init( &ctr_drbg );
    mbedtls_entropy_init( &entropy );

    ret = mbedtls_ctr_drbg_seed( &ctr_drbg, mbedtls_entropy_func, &entropy,
        (const unsigned char *) pers, strlen( pers ) );

    str    host     = "www.googleapis.com";
    int    port     = 443;
    str    s_port   = str::from_integer(port);
    symbol sym_host = symbol(host.cs());
    symbol sym_port = symbol(s_port.cs());
    
    for (size_t ii = 0; ii < 16; ii++) {
        path p = fmt { "trust/{0}.{1}.pem", { host, int(ii) }};
        if (!p.exists()) {
            assert(ii > 0);
            break;
        }
        symbol ps = symbol(p.cs());
        ret       = mbedtls_x509_crt_parse_file(&cacert, ps);
        if (ret  != 0) {
            std::cerr << "mbedtls_x509_crt_parse_file failed for ca_cert.pem: " << ret << std::endl;
            return false;
        }
    }
    
    ret = mbedtls_ssl_config_defaults( &conf,
        MBEDTLS_SSL_IS_CLIENT,
        MBEDTLS_SSL_TRANSPORT_STREAM,
        MBEDTLS_SSL_PRESET_DEFAULT );

    mbedtls_ssl_conf_authmode( &conf, MBEDTLS_SSL_VERIFY_REQUIRED );
    mbedtls_ssl_conf_ca_chain( &conf, &cacert, NULL );
    mbedtls_ssl_conf_rng( &conf, mbedtls_ctr_drbg_random, &ctr_drbg );
    mbedtls_ssl_conf_min_version(&conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);  // TLS 1.2

    ret = mbedtls_ssl_setup( &ssl, &conf );

    ret = mbedtls_ssl_set_hostname( &ssl, sym_host );

    ret = mbedtls_net_connect( &ctx, sym_host, sym_port, MBEDTLS_NET_PROTO_TCP );

    mbedtls_ssl_set_bio( &ssl, &ctx, mbedtls_net_send, mbedtls_net_recv, NULL );

    while( ( ret = mbedtls_ssl_handshake( &ssl ) ) != 0 ) {}

    /// https://www.googleapis.com/youtube/v3/search?part=snippet&channelId=UCpVm7bg6pXKo1Pr6k5kxG9A&maxResults=1&key=AIzaSyAg4nh93xKESkGZvv7Ocv2PBBFAM1jyDSs
    /// https://gist.githubusercontent.com/prabansal/115387/raw/0e5911c791c03f2ffb9708d98cac70dd2c1bf0ba/HelloWorld.txt
    ///
    const char *GET_REQUEST = "GET /youtube/v3/search?part=snippet&channelId=UCpVm7bg6pXKo1Pr6k5kxG9A&maxResults=1&key=AIzaSyAg4nh93xKESkGZvv7Ocv2PBBFAM1jyDSs HTTP/1.1\r\nHost: www.googleapis.com\r\nUser-Agent: ion::net\r\n\r\n";

    ret = mbedtls_ssl_write( &ssl, (const unsigned char *) GET_REQUEST, strlen( GET_REQUEST ) );

    do
    {
        len = sizeof( buf ) - 1;
        memset( buf, 0, sizeof( buf ) );
        ret = mbedtls_ssl_read( &ssl, buf, len );

        if( ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE )
            continue;

        if( ret <= 0 )
            break;

        printf( " %d bytes read\n\n%s", len, (char *) buf );
    }
    while( 1 );

    mbedtls_ssl_close_notify( &ssl );
    mbedtls_net_free( &ctx );
    mbedtls_x509_crt_free( &cacert );
    mbedtls_ssl_free( &ssl );
}

struct Socket {
private:
    bool                      connected;
    mbedtls_net_context       ctx;
    mbedtls_entropy_context   entropy;
    mbedtls_ctr_drbg_context  ctr_drbg;
    mbedtls_ssl_context       ssl;
    mbedtls_ssl_config        conf;
    mbedtls_x509_crt          cert_chain;
    
public:
    void ssl_init() {
        mbedtls_net_init(&ctx);
        mbedtls_ssl_init(&ssl);
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&ctr_drbg);
        mbedtls_ssl_config_init(&conf);
        mbedtls_x509_crt_init(&cert_chain);
    }
    
    void ssl_cleanup() {
        mbedtls_net_free(&ctx);
        mbedtls_ssl_free(&ssl);
        mbedtls_ssl_config_free(&conf);
        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_entropy_free(&entropy);
        mbedtls_x509_crt_free(&cert_chain);
    }
    
    Socket() : connected(false) {
        ssl_init();
    }

    ~Socket() {
        close();
        ssl_cleanup();
    }
    
    void close() {
        if (connected) {
            mbedtls_ssl_close_notify(&ssl);
            connected = false;
        }
    }
    
    bool read_sz(u8 *v, size_t sz) {
        int st = 0;
        ///
        for (int len = int(sz); len > 0;) {
            int rcv = mbedtls_ssl_read(&ssl, &v[st], len);
            if (rcv <= 0)
                return false;
            len -= rcv;
            st  += rcv;
        }
        return true;
    }

    array<char> read_until(str s, int max_len) {
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
    
    operator bool() {
        return connected;
    }

    bool connect(str host, int port) {
        str ip_addr = dns(host);
        
        mbedtls_ssl_conf_dbg(&conf, mbedtls_debug, stdout);
        mbedtls_debug_set_threshold(4);
        
        int ret = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
        if (ret != 0) {
            std::cerr << "mbedtls_ssl_config_defaults failed: " << ret << std::endl;
            return false;
        }

        for (size_t ii = 0; ii < 16; ii++) {
            path p = fmt { "trust/{0}.{1}.pem", { host, int(ii) }};
            if (!p.exists()) {
                assert(ii > 0);
                break;
            }
            symbol ps = symbol(p.cs());
            ret       = mbedtls_x509_crt_parse_file(&cert_chain, ps);
            if (ret  != 0) {
                std::cerr << "mbedtls_x509_crt_parse_file failed for ca_cert.pem: " << ret << std::endl;
                return false;
            }
        }
    
        const char *pers = "ion::net";
        ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
            (const unsigned char *)pers, strlen(pers));
        
        mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
        mbedtls_ssl_conf_ca_chain(&conf, &cert_chain, NULL);
        mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
        mbedtls_ssl_conf_min_version(&conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);  // TLS 1.2
        
        ret = mbedtls_ssl_setup(&ssl, &conf);
        if (ret != 0) {
            std::cerr << "mbedtls_ssl_setup failed: " << ret << std::endl;
            return false;
        }
        
        str  str_port = str::from_integer(port);
        symbol s_host = symbol(host.cs());
        symbol s_port = symbol(str_port.cs());
        ret = mbedtls_ssl_set_hostname(&ssl, s_host);
        if (ret != 0) {
            std::cerr << "mbedtls_ssl_set_hostname failed: " << ret << std::endl;
            return false;
        }
        
        ret = mbedtls_net_connect(&ctx, s_host, s_port, MBEDTLS_NET_PROTO_TCP);
        if (ret != 0) {
            std::cerr << "mbedtls_net_connect failed: " << ret << std::endl;
            return false;
        }
        
        mbedtls_ssl_set_bio(&ssl, &ctx, mbedtls_net_send, mbedtls_net_recv, NULL);
        
        while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
            if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                std::cerr << "mbedtls_ssl_handshake failed: " << ret << std::endl;
                mbedtls_net_free(&ctx); /// seems the only way to drop a connection.  it gets freed again so initialize it to default
                mbedtls_net_init(&ctx);
                return false;
            }
        }
        connected = true;
        return true;
    }

    ssize_t recv(unsigned char* buf, size_t len) {
        return mbedtls_ssl_read(&ssl, buf, len);
    }
    
    ssize_t send(const unsigned char* buf, size_t len) {
        return mbedtls_ssl_write(&ssl, buf, len);
    }
    
    ssize_t send(str templ, array<mx> args = { }) {
        str val = str::format(templ.cs(), args);
        return mbedtls_ssl_write(&ssl, (const unsigned char*)val.cs(), val.len());
    }
    
    /// for already string-like memory; this could do something with the type on mx
    ssize_t send(mx &v) {
        return send((u8*)v.mem->origin, v.count() * v.type_size());
    }
};



struct isock {
    sock::role               role;
    uri                      query;
    i64                      timeout_ms;
    bool                     connected;

    int                      listen_fd;
    int                      sock_fd;
    sockaddr_in*             bound;
    
    mbedtls_entropy_context  entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context      ssl;
    mbedtls_ssl_config       conf;
    mbedtls_x509_crt         cert_chain;

    mbedtls_pk_context       pkey;
    
    isock() {
        const char *pers = "ion::net";
        mbedtls_ssl_init(&ssl);
        mbedtls_ssl_config_init(&conf);
        mbedtls_x509_crt_init(&cert_chain);
        mbedtls_ctr_drbg_init(&ctr_drbg);
        mbedtls_entropy_init(&entropy);
    }
    
    ~isock() {
        mbedtls_x509_crt_free(&cert_chain); /// it is now freed in the isock
        mbedtls_ssl_free(&ssl);
        free(bound);
    }
};

ptr_impl(sock, mx, isock, i);

// wrapper around BSD send() for mbed TLS
int sock::send_wrapper(void *ctx, const u8 *buf, size_t len) {
    int fd = *(int*)ctx;
    printf("sending %d bytes\n", int(len));
    return int(send(fd, buf, len, 0));
}

// wrapper around BSD recv() for mbed TLS
int sock::recv_wrapper(void *ctx, u8 *buf, size_t len) {
    int fd = *(int*)ctx;
    printf("recving %d bytes\n", int(len));
    return int(recv(fd, buf, len, 0));
}

bool test_mbed() {

    #define SERVER_PORT  "443"
    #define SERVER_NAME  "www.googleapis.com"
    
    str  api_key     = "AIzaSyAg4nh93xKESkGZvv7Ocv2PBBFAM1jyDSs";
    str  channel_id  = "UCpVm7bg6pXKo1Pr6k5kxG9A";
    uri  request_url = fmt { "https://www.googleapis.com/youtube/v3/search?part=snippet&channelId={0}&maxResults=1&key={1}", { channel_id, api_key }};

    #define GET_REQUEST_ "GET /youtube/v3/search?part=snippet&channelId=UCpVm7bg6pXKo1Pr6k5kxG9A&maxResults=1&key=AIzaSyAg4nh93xKESkGZvv7Ocv2PBBFAM1jyDSs HTTP/1.0\r\n\r\n"


    int ret, len;
    unsigned char buf[1024];
    mbedtls_net_context server_fd;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt cacert;

    const char *pers = "ssl_client1";

    mbedtls_net_init( &server_fd );
    mbedtls_ssl_init( &ssl );
    mbedtls_ssl_config_init( &conf );
    mbedtls_x509_crt_init( &cacert );
    mbedtls_ctr_drbg_init( &ctr_drbg );
    mbedtls_entropy_init( &entropy );

    ret = mbedtls_ctr_drbg_seed( &ctr_drbg, mbedtls_entropy_func, &entropy,
        (const unsigned char *) pers, strlen( pers ) );

    mbedtls_x509_crt_init( &cacert );

    // Replace "root_cert.pem" and "intermediate_cert.pem" with your actual file paths
    if (mbedtls_x509_crt_parse_file( &cacert, "trust/gtsr1.pem") != 0) {
        printf( "Failed to parse root certificate\n" );
        return false;
    }

    if (mbedtls_x509_crt_parse_file( &cacert, "trust/gts1c3.pem") != 0) {
        printf( "Failed to parse intermediate certificate\n" );
        return false;
    }
    
    //ret = mbedtls_x509_crt_parse( &cacert, (const unsigned char *) ca_cert, strlen(ca_cert) + 1 );

    ret = mbedtls_ssl_config_defaults( &conf,
        MBEDTLS_SSL_IS_CLIENT,
        MBEDTLS_SSL_TRANSPORT_STREAM,
        MBEDTLS_SSL_PRESET_DEFAULT );

    mbedtls_ssl_conf_authmode( &conf, MBEDTLS_SSL_VERIFY_REQUIRED );
    mbedtls_ssl_conf_ca_chain( &conf, &cacert, NULL );
    mbedtls_ssl_conf_rng( &conf, mbedtls_ctr_drbg_random, &ctr_drbg );
    mbedtls_ssl_conf_min_version(&conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);  // TLS 1.2

    ret = mbedtls_ssl_setup( &ssl, &conf );

    ret = mbedtls_ssl_set_hostname( &ssl, SERVER_NAME );

    ret = mbedtls_net_connect( &server_fd, SERVER_NAME, SERVER_PORT, MBEDTLS_NET_PROTO_TCP );

    //mbedtls_ssl_set_bio(&ssl, &server_fd,
    //    (mbedtls_ssl_send_t*)sock::send_wrapper,
    //    (mbedtls_ssl_recv_t*)sock::recv_wrapper, NULL);

    while( ( ret = mbedtls_ssl_handshake( &ssl ) ) != 0 ) {}

    const char *GET_REQUEST = GET_REQUEST_;

    ret = mbedtls_ssl_write( &ssl, (const unsigned char *) GET_REQUEST, strlen( GET_REQUEST ) );

    do
    {
        len = sizeof( buf ) - 1;
        memset( buf, 0, sizeof( buf ) );
        ret = mbedtls_ssl_read( &ssl, buf, len );

        if( ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE )
            continue;

        if( ret <= 0 )
            break;

        printf( " %d bytes read\n\n%s", len, (char *) buf );
    }
    while( 1 );

    mbedtls_ssl_close_notify( &ssl );
    mbedtls_net_free( &server_fd );
    mbedtls_x509_crt_free( &cacert );
    mbedtls_ssl_free( &ssl );
   
    return false;
}

str address_string(const sockaddr* sa) {
    static char ip[INET6_ADDRSTRLEN];
    ip[0] = 0;
    
    if (sa->sa_family == AF_INET) {
        const sockaddr_in* sa_in = reinterpret_cast<const sockaddr_in*>(sa);
        inet_ntop(AF_INET, &(sa_in->sin_addr), ip, INET_ADDRSTRLEN);
    } else if (sa->sa_family == AF_INET6) {
        const sockaddr_in6* sa_in6 = reinterpret_cast<const sockaddr_in6*>(sa);
        inet_ntop(AF_INET6, &(sa_in6->sin6_addr), ip, INET6_ADDRSTRLEN);
    }
    return str(ip);
}

sock::sock(role r, uri bind) : sock() {
    /// if its release it must be secure
    symbol pers = "ion:net";
    static bool wolf;
    if (!wolf) {
#ifdef _WIN32
        static WSADATA wsa_data;
        static int wsa = WSAStartup(MAKEWORD(2,2), &wsa_data);
        if (wsa != 0) {
            printf("(sock) WSAStartup failed: %d\n", wsa);
            return;
        }
#endif
        wolf = true; /// wolves should always be true
        //wolfSSL_Init();
    }
    
    /// get port, and set dns lookup
    size_t   addr_sz = sizeof(sockaddr_in);
    uint16_t port    = (uint16_t)bind.port();
    str      host    = bind.host();
    str      ip_addr = dns(host);
    str      query   = bind;
    ///
    i->query              = bind;
    i->sock_fd            = socket(AF_INET, SOCK_STREAM, 0);
    i->bound              = (sockaddr_in*)calloc(1, sizeof(sockaddr_in));
    i->bound->sin_family  = AF_INET;
    i->bound->sin_port    = htons(port);
    
    /// set method based on role
    const bool is_server = (r == role::server);
    
    /// load cert and key based on role
    str rtype = is_server ? "server" : "client";
    
    i->role = r;
    if (is_server) {
        /// developer must provide public/private (entire dir .gitignore'd)
        str cert  = str::format("certs/{0}-cert.pem", { rtype });
        str key   = str::format("certs/{0}-key.pem",  { rtype });
        
        // replace "root_cert.pem" and "intermediate_cert.pem" with your actual file paths
        if (mbedtls_x509_crt_parse_file(&i->cert_chain, cert.cs()) != 0) {
            console.fault("failed to parse public cert: {0}", { cert });
            return false;
        }
        
        // private key
        if (mbedtls_pk_parse_keyfile(&i->pkey, symbol(key.cs()), null, null, null) != 0) {
            console.fault("failed to parse private key: {0}", { key });
            return false;
        }
    } else {
        /// host just has a sequence starting with root, intermediate, etc, so 0 = CA Root, 1 = Signed from CA Root, 2 = Signed by 1
        for (size_t ii = 0; ii < 16; ii++) {
            path p = fmt { "trust/{0}.{1}.pem", { bind.host(), int(ii) }};
            bool exists = p.exists();
            if (!exists) {
                if (i == 0) console.fault("no trust for this host..."); /// this is a fault. all hosts should have something
                break;
            }
            symbol file = symbol(p.cs());
            if (mbedtls_x509_crt_parse_file(&i->cert_chain, file) != 0) {
                console.fault("failed to parse trust: {0}", { file });
                return false;
            }
        }
    }
    
    /// cstring to ipv4 in network order
    inet_pton(AF_INET, ip_addr.cs(), &i->bound->sin_addr);
    
    mbedtls_ssl_set_bio(&i->ssl, &i->sock_fd, send_wrapper, recv_wrapper, NULL);
    
    /// if this is a server, block until a new connection arrives
    if (is_server) {
        printf("listening on %d\n", int(port));
        ::bind   (i->sock_fd, (sockaddr *)&i->bound, sizeof(sockaddr_in));
        ::listen (i->sock_fd, 0);
    } else {
        console.log("connect to {0} ({1}) -> {2}", { host, ip_addr, i->query->query });
        sockaddr_in *s_addr_in = (sockaddr_in *)i->bound;
        int conn = ::connect(i->sock_fd, (sockaddr *)i->bound, sizeof(sockaddr_in));
        if (conn == 0) {
            console.log("connection established");
            establish();
        } else {
            console.log("could not connect: ", { address_string((const sockaddr *)i->bound) });
        }
    }
}

uri   sock::query()      const { return  i->query;      }
bool  sock::timeout_ms() const { return  i->timeout_ms; }
bool  sock::connected()  const { return  i->connected;  }
bool  sock::operator!()  const { return !i->connected;  }
sock::operator   bool()  const { return  i->connected;  }

///
void sock::set_timeout(i64 t) {
    i->timeout_ms = t;
}

sock sock::accept() {
    sock      sc = i;
    ///
#ifdef _WIN32
    using socklen_t = int;
    sc.i->bound->sin_addr.S_un.S_addr = INADDR_ANY;
#else
    sc.i->bound->sin_addr.s_addr = INADDR_ANY;
#endif
    ///
    socklen_t addr_sz = sizeof(sockaddr_in);
    sc.i->sock_fd = ::accept(i->sock_fd, (sockaddr*)&sc.i->bound, &addr_sz);
    printf("accepted: %i\n", sc.i->sock_fd);
    return sc.establish();
}

sock &sock::establish() {
    /// init and mbed-TLS on this socket -- some things may not be required for client
    mbedtls_ssl_config_defaults(&i->conf,
        (i->role == role::server) ? MBEDTLS_SSL_IS_SERVER : MBEDTLS_SSL_IS_CLIENT,
        MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    ///
    mbedtls_ssl_conf_rng(&i->conf, mbedtls_ctr_drbg_random, &i->ctr_drbg);
    mbedtls_ctr_drbg_seed(&i->ctr_drbg, mbedtls_entropy_func, &i->entropy, NULL, 0); /// mbedtls_entropy_func is mbed function
    mbedtls_ssl_setup(&i->ssl, &i->conf);
    ///
    if (i->sock_fd > 0) {
        int h;
        printf("scanning\n");
        while ((h = mbedtls_ssl_handshake(&i->ssl)) != 0) { }
        i->connected = true;
        printf("connected\n");
    }
    return *this;
}

int sock::read(u8 *v, size_t sz) {
    int    len = int(sz);
    int    rcv = mbedtls_ssl_read(&i->ssl, v, len);
    ///
    if (rcv < 0) {
        char ebuf[128];
        mbedtls_strerror(rcv, ebuf, sizeof(ebuf));
        console.log("mbedtls_ssl_read: {0}\n", str(ebuf));
        return 0;
    }
    return rcv;
}

bool sock::read_sz(u8 *v, size_t sz) {
    int st = 0;
    ///
    for (int len = int(sz); len > 0;) {
        int rcv = mbedtls_ssl_read(&i->ssl, &v[st], len);
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
        if (!read((u8*)&rbytes[sz - 1], 1))
            return array<char> { };
        if (sz >= slen and memcmp(&rbytes[sz - slen], s.cs(), slen) == 0)
            break;
        if (sz == max_len)
            return array<char> { };
    }
    return rbytes;
}

bool sock::write(u8 *v, size_t sz) {
    assert(sz > 0);
    int      len = int(sz);
    int     wlen = mbedtls_ssl_write(&i->ssl, (const u8 *)v, sz);
    return (wlen == sz);
}

bool sock::write(array<char> v) {
    return write((u8*)v.data(), v.len());
}

bool sock::write(str s, array<mx> a) {
    str f = str::format(s.cs(), a);
    return write((u8*)f.cs(), f.len());
}

bool sock::write(str s) {
    return write((u8*)s.cs(), s.len());
}

/// for already string-like memory; this could do something with the type on mx
bool sock::write(mx &v) {
    return write((u8*)v.mem->origin, v.count() * v.type_size());
}

void sock::close() const {
#if _WIN32
    closesocket((SOCKET)i->sock_fd);
#else
    ::close(i->sock_fd);
#endif
    mbedtls_ssl_close_notify(&i->ssl);
    //mbedtls_x509_crt_free(&i->cert_chain); /// it is now freed in the isock
    //mbedtls_ssl_free(&->ssl);
}

/// ---------------------------------
void sock::logging(void *ctx, int level, symbol file, int line, symbol str) {
    fprintf((FILE *)ctx, "%s:%04d: %s", file, line, str);
}

/// ---------------------------------
sock sock::connect(uri u) {
    uri a = u.methodize(method::get);
    return sock(role::client, a);
}

/// listen on https using mbedtls
/// ---------------------------------
async sock::listen(uri url, lambda<bool(sock&)> fn) {
    return async(1, [&, url, fn](runtime &rt, int i) -> mx {
        uri bind = url.methodize(method::get);
        
        /// proceed if https; that is our protocol and we know nothing else
        assert(bind.proto() == "https");
        
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
            async {1, [client=client, fn=fn](runtime &rt, int i) -> mx {
                if (fn((sock&)client)) client.close();
                return true;
            }};
        }
        return true;
    });
}

message::message(int server_code) : message() {
    m.code = mx(server_code);
}

message::message(symbol text) : message() {
    m.content = text;
    m.code    = 200;
}

message::message(str text) : message() {
    m.content = text;
    m.code    = 200;
}

message::message(path p, mx modified_since) : message() {
    m.content = p.read<str>();
    m.headers["Content-Type"] = p.mime_type();
    m.code    = 200;
}

message::message(mx content, map<mx> headers, uri query) : message() {
    m.query   = query;
    m.headers = headers;
    m.content = content; /// assuming the content isnt some error thing
    m.code    = 200;
}

message::message(uri url, map<mx> headers) : message() {
    m.query   = url;
    m.headers = headers; // ion::pair<ion::mx, ion::mx>    ion::pair<ion::mx, ion::mx>
}

/// important to note we do not keep holding onto this socket
message::message(Socket *psc) : message() {
    read_headers(psc);
    //console.log("received headers:");
    //m.headers.print();
    read_content(psc);
    m.code = m.headers["Status"];
}

uri message::query() { return m.query; }

bool message::read_headers(Socket *psc) {
    Socket &sc = *psc;
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
            str hello = str(rbytes.data(), int(sz - 2));
            i32  code = 0;
            auto   sp = hello.split(" ");
            if (hello.len() >= 12) {
                if (sp.len() >= 2)
                    code = i32(sp[1].integer_value());
            }
            m.headers["Status"] = str::from_integer(code);
            ///
        } else {
            for (size_t i = 0; i < sz; i++) {
                if (rbytes[i] == ':') {
                    str k = str(&rbytes[0], int(i));
                    str v = str(&rbytes[i + 2], int(sz) - int(k.len()) - 2 - 2);
                    m.headers[k] = v;
                    break;
                }
            }
        }
    }
    return true;
}

///
bool message::read_content(Socket *psc) {
    Socket &sc = *psc;
    str              te = "Transfer-Encoding";
    str              cl = "Content-Length";
    str              ce = "Content-Encoding";
    str        encoding = m.headers.count(te) ? str(m.headers[ce].grab()) : str();
    int            clen = int(m.headers.count(cl) ? str(m.headers[cl].grab()).integer_value() : -1);
    bool        chunked = encoding && m.headers[te] == "chunked";
    num     content_len = clen;
    num            rlen = 0;
    const num     r_max = 1024;
    bool          error = false;
    int            iter = 0;
    array<uint8_t> v_data;
    ///
    assert(!(clen >= 0 and chunked));
    ///
    if (!(!chunked and clen == 0)) {
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
                ss << std::hex << rbytes.data();
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
        } while (!error and chunked and content_len != 0);
    }
    ///
    mx    rcv = error ? mx() : mx(v_data);
    str ctype = m.headers.count("Content-Type") ? str(m.headers["Content-Type"].grab()) : str("");

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
        m.content = obj;
    }
    else if (ctype.starts_with("text/")) {
        m.content = str(&rcv.ref<char>(), int(rcv.byte_len()));
    } else {
        /// other non-text types must be supported, not going to just leave them as array for now
        assert(v_data.len() == 0);
        m.content = var(null);
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
    type_t ct = m.code.type();
    assert(ct == typeof(i32) || ct == typeof(str));
    return (m.query.mtype() != method::undefined) &&
    ((ct == typeof(i32) && int(m.code) >= 200 && int(m.code) < 300) ||
     (ct == typeof(str) && m.code.byte_len()));
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
    size_t count  = symbols.count(code);
    symbol result = count ? symbols[code] : symbols[0]; /// map doesnt work.
    return result;
}

bool message::write_status(sock sc) {
    mx status = "Status";
    int  code = m.headers.count(status) ? int(m.headers[status]) : (m.code ? int(m.code) : int(200));
    return sc.write("HTTP/1.1 {0} {1}\r\n", { code, code_symbol(code) });
}

///  code is in headers.
bool message::write_headers(Socket *psc) {
    Socket &sc = *psc;
    ///
    for (auto &[v, k]: m.headers) { /// mx key, mx value
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

bool message::write(Socket *psc) {
    Socket &sc = *psc;
    if (m.content) {
        type_t ct = m.content.type();
        /// map of mx must be json compatible, or be structured in that process
        if (!m.headers.count("Content-Type") && (ct == typeof(map<mx>) || ct == typeof(mx)))
            m.headers["Content-Type"] = "application/json";
        ///
        m.headers["Connection"] = "keep-alive";
        
        /// do different things based on type
        if (m.headers.count("Content-Type") && m.headers["Content-Type"] == "application/json") {
            /// json transfer; content can be anything, its just mx..
            /// so we convert to json explicitly. var is just an illusion as you can see friend.
            str json = var(m.content).stringify();
            m.headers["Content-Length"] = str(json.len());
            write_headers(&sc);
            return sc.send(json);
            ///
        } else if (ct == typeof(map<mx>)) {
            /// post fields transfer
            str post = uri::encode_fields(map<mx>(m.content));
            m.headers["Content-Length"] = str::from_integer(post.len());
            write_headers(&sc);
            return sc.send(post);
            ///
        } else if (ct == typeof(u8)) { /// different from char.  char is for text and u8 is for binary.
            /// binary transfer
            m.headers["Content-Length"] = str::from_integer(m.content.byte_len()); /// size() is part of mx object, should always return it size in its own unit, but a byte_size is there too
            return sc.send(m.content);
            ///
        } else if (ct == typeof(null_t)) {
            /// can be null_t
            m.headers["Content-Length"] = str("0");
            console.log("sending Content-Length of {0}", { m.headers["Content-Length"] });
            return write_headers(&sc);
            ///
        } else {
            /// cstring
            assert(ct == typeof(char));
            m.headers["Content-Length"] = str::from_integer(m.content.byte_len());
            console.log("sending Content-Length of {0}", { m.headers["Content-Length"] });
            write_headers(&sc);
            return sc.send(m.content);
        }
    }
    m.headers["Content-Length"] = 0;
    m.headers["Connection"]     = "keep-alive";
    return write_headers(&sc);
}

str message::text() { return m.content.to_string(); }

/// structure cookies into usable format
map<str> message::cookies() {
    str  dec = uri::decode(m.headers["Set-Cookie"].grab());
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
    return m.headers[key];
}

mx &message::header(mx key) { return m.headers[key]; }

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
            uri req_uri = uri::parse(str(msg.data(), int(msg.len() - 2)));
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
        map<mx>    headers = a.count("headers") ? map<mx>(a["headers"]) : st_headers;
        mx         content = a.count("content") ?         a["content"]  : null_content;
        method        type = a.count("method")  ?  method(a["method"])  : method(method::get);
        uri          query = url.methodize(type);
        
        ///
        assert(query != method::undefined);
        console.log("(net) request: {0}", { url });
        
        Socket client = Socket();
        
        str host = query.host();
        int port = query.port();
        
        if (!client.connect(host, port)) return {};
        
        /// start request (this time get the casing right)
        client.send("{0} {1} HTTP/1.1\r\n", {
            query.mtype().name().ucase(), query.string() });
        
        ///
        console.log("{0} {1} HTTP/1.1\r\n", {
            query.mtype().name().ucase(), query.string()
        });
        
        /// default headers
        array<field<mx>> defs = {
            { "User-Agent",      "ion:net"              },
            { "Accept",          "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8" },
            { "Accept-Language", "en-US,en;q=0.9"       },
            { "Accept-Encoding", "gzip, deflate, br"    },
            { "Host",             host                  }
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
        request.write(&client);
        
        /// read response, this is a message which constructs with a sock; does not store
        message response { &client };
        
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
