/*
 * This file only contains functions handling user input & output.
 */

#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdarg.h>

#ifdef DEBUG
	#define LOG_msg(where, ...) \
		do { \
			fprintf(where, "%s: %d: ", __FILE__, __LINE__); \
			fprintf(where, __VA_ARGS__); \
		} while (0);

	#define LOG(...) LOG_msg(stdout, __VA_ARGS__);
	#define ERR(...) fprintf(stderr, "Error: "); \
		LOG_msg(stderr, __VA_ARGS__);
#else
	#define LOG_msg(where, ...) \
		do { \
			fprintf(where, __VA_ARGS__); \
		} while (0);

	#define LOG(...) LOG_msg(stdout,  __VA_ARGS__);
	#define ERR(...) LOG_msg(stderr,  __VA_ARGS__);
#endif /* DEBUG */

#endif /* LOG_H */
