#pragma once
#include <net/net.hpp>
#include <rtc/rtc.hpp>

using namespace std::chrono_literals;
using std::shared_ptr;
using std::weak_ptr;
template <class T> weak_ptr<T> make_weak_ptr(shared_ptr<T> ptr) { return ptr; }

/// webrtc -> service
ion::async service(ion::uri uri, ion::lambda<ion::message(ion::message)> fn_process);

/// pre-compiled headers are good place to put in shimmable utility declaration and implementation
#ifdef _WIN32
#include <windows.h>
#include <windows.h>
#include <winsock2.h>

void usleep(__int64 usec);

struct timezone {
	int tz_minuteswest;
	int tz_dsttime;
};

int gettimeofday(struct timeval *tv, struct timezone *tz);

#else
#include <unistd.h>
#include <sys/time.h>
#include <arpa/inet.h>
#endif

