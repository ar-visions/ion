//#define PACKAGE "libssh"
//#define VERSION "0.10.90"
//#define SYSCONFDIR "etc"
//#define BINARYDIR "C:/src/prefix/extern/libssh/ion-build"
//#define SOURCEDIR "C:/src/prefix/extern/libssh"

// remove this from the lib by handling cases where it isnt defined, thats all.
#define GLOBAL_BIND_CONFIG              "/etc/ssh/libssh_server_config"
#define GLOBAL_CLIENT_CONFIG            "/etc/ssh/ssh_config"

#define HAVE_SYS_UTIME_H                1
#define HAVE_IO_H                       1
#define HAVE_STDINT_H                   1
#define HAVE_ECC                        1

#define HAVE_SNPRINTF                   1
#define HAVE__SNPRINTF                  1
#define HAVE__SNPRINTF_S                1
#define HAVE_VSNPRINTF                  1
#define HAVE__VSNPRINTF                 1
#define HAVE__VSNPRINTF_S               1
#define HAVE_ISBLANK                    1
#define HAVE_STRNCPY                    1

#define HAVE_GETADDRINFO                1
#define HAVE_SELECT                     1
#define HAVE_NTOHLL                     1
#define HAVE_HTONLL                     1
#define HAVE_STRTOULL                   1
#define HAVE__STRTOUI64                 1
#define HAVE_SECURE_ZERO_MEMORY         1
#define HAVE_LIBMBEDCRYPTO              1

#ifdef _WIN32
#define HAVE_MSC_THREAD_LOCAL_STORAGE   1
#define HAVE_WSPIAPI_H                  1
#else
#define HAVE_GCC_THREAD_LOCAL_STORAGE   1
#define HAVE_PTHREAD                    1
#endif

#define HAVE_COMPILER__FUNC__           1
#define HAVE_COMPILER__FUNCTION__       1

#define WITH_ZLIB                       1
#define WITH_SERVER                     1

