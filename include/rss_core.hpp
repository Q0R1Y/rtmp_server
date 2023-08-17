#ifndef RSS_CORE_HPP
#define RSS_CORE_HPP


/**
* the core provides the common defined macros, utilities.
*/

// for int64_t print using PRId64 format.
#ifndef __STDC_FORMAT_MACROS
    #define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#include <assert.h>
#define rss_assert(expression) assert(expression)

#include <stddef.h>
#include <sys/types.h>

// free the p and set to NULL.
// p must be a T*.
#define rss_freep(p) \
	if (p) { \
		delete p; \
		p = NULL; \
	} \
	(void)0
// free the p which represents a array
#define rss_freepa(p) \
	if (p) { \
		delete[] p; \
		p = NULL; \
	} \
	(void)0

// server info.
#define RTMP_SIG_RSS_KEY "rss"
#define RTMP_SIG_RSS_NAME RTMP_SIG_RSS_KEY"(simple rtmp server)"
#define RTMP_SIG_RSS_URL "https://github.com/Q0R1Y/rtmp_server"
#define RTMP_SIG_RSS_EMAIL "q0r1y@outlook.com"
#define RTMP_SIG_RSS_VERSION "0.1"

// compare
#define rss_min(a, b) ((a < b)? a : b)
#define rss_max(a, b) ((a > b)? a : b)

#endif