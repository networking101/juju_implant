#ifndef _UTILITY_H
#define _UTILITY_H

// ANSI colors for console
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

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

#define print_out(fmt, ...) \
	do { fprintf(stdout, "\n" fmt "\n>", __VA_ARGS__); fflush(stdout); } while (0)

#endif /* _UTILITY_H */