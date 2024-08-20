#ifndef _UTILITY_H
#define _UTILITY_H

#ifdef UNIT_TESTING
#define STATIC
#else
#define STATIC static
#endif /* UNIT_TESTING */

#ifdef DEBUG
#define debug_print(fmt, ...) \
	do { if (DEBUG) fprintf(stderr, "--%s:%d:%s(): " fmt, __FILE__, \
		 __LINE__, __func__, __VA_ARGS__); } while (0)
#else
#define debug_print(fmt, ...) do {} while (0)
#endif /* DEBUG */

#endif /* _UTILITY_H */