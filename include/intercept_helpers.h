/*
 * BSD 3-Clause License
 * 
 * Copyright (c) 2022, k4m1
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, 
 *    this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIA LDAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVE RCAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Helpers for intercepting stuff
 */

#ifndef __INTERCEPT_HELPERS_H__
#define __INTERCEPT_HELPERS_H__

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#define SHIFT_LEFT 0
#define SHIFT_RIGHT 1
#define PAD_HERE 0

/* Helpers for modding data that's passed through */

/*
 * Find 1st occurance of byte sequence in data being processed.
 * Works with 0-bytes within the data unlike strstr.
 *
 * Requires:
 * 	void 	*data, 	data being processed
 * 	void 	*what, 	what to find look for
 * 	size_t 	dlen, 	size of data
 * 	size_t 	wlen, 	size of what
 * Returns:
 * 	0 if what isn't found from data or
 * 	pointer to what if it was found.
 */
void *
findseq(void *data, void *what, size_t dlen, size_t wlen);

/*
 * Shift bytes of data to left or right.
 *
 * Requires:
 * 	unsigned char  *data, 	pointer to data to operate with
 * 	size_t 		cnt,   	how many bytes to shift
 * 	size_t 		off, 	offset where from to shift
 * 	int    		dir, 	shift left or right
 */
void
shift_bytes(unsigned char *data, size_t cnt, size_t off, int dir);

/*
 * Replace a string with equal-lenght string from data
 * that is being passed through
 *
 * Requires:
 * 	unsigned char 	*data, 	pointer to data to operate with
 * 	size_t 		dlen,   size of data
 * 	size_t 		count, 	size of string to replace
 *	unsigned char 	*what, 	what to replace from *data
 * 	unsigned char 	*with, 	string to replace data with
 */
void
replace_str_of_equal_size(unsigned char *data, size_t dlen, size_t count, 
		unsigned char *what, unsigned char *with);

/*
 * Replace string with shorter string from data that is being
 * passed through, pad the remaining data right after the replaced string
 * or at the end of data.
 *
 * Requires:
 * 	unsigned char 	*data, 	pointer to data to operate with
 * 	size_t 		dlen, 	size of data
 * 	size_t 		rlen, 	size of string to replace
 * 	size_t 		wlen, 	size of string with to replace
 * 	unsigned char 	*what, 	what to replace from *data
 * 	unsigned char 	*with, 	string to replace data with
 * 	unsigned char 	pad, 	unsigned character to pad with with.
 * 	int 		padloc,	where to add pad, at the data or at the end
 */
void
replace_str_with_pad(unsigned char *data, size_t dlen, size_t rlen,  
		size_t wlen, unsigned char *what, unsigned char *with, 
		unsigned char pad, int padloc);

#endif /* __INTERCEPT_HELPERS_H__ */
