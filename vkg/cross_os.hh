#ifndef CROSS_OS_H
#define CROSS_OS_H

//cross platform os helpers
#if defined(_WIN32) || defined(_WIN64)
	//disable warning on iostream functions on windows
	#define _CRT_SECURE_NO_WARNINGS
	#include "windows.h"
	#if defined(_WIN64)
		#ifndef isnan
			#define isnan _isnanf
		#endif
	#endif
	#define vkg_inline __forceinline
	#define disable_warning (warn)
	#define reset_warning (warn)
#elif __APPLE__
	#include <math.h>
	#define vkg_inline static
	#define disable_warning (warn)
	#define reset_warning (warn)
#elif __unix__
	#include <unistd.h>
	#include <sys/types.h>
	#include <pwd.h>
	#define vkg_inline static inline __attribute((always_inline))
	#define disable_warning (warn) #pragma GCC diagnostic ignored "-W"#warn
	#define reset_warning (warn) #pragma GCC diagnostic warning "-W"#warn
	#if __linux__
		void _linux_register_error_handler ();
	#endif
#endif

const char* getUserDir ();

#endif // CROSS_OS_H
