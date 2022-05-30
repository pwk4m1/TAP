/*
 * This file provides some common helpers for intercepting traffic.
 *
 */

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#define SHIFT_LEFT  	1
#define SHIFT_RIGHT 	0

#define PAD_HERE 	1
#define PAD_END 	0

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
findseq(void *data, void *what, size_t dlen, size_t wlen)
{
	size_t off;
	size_t maxlen;
	int stat;
	void *current;

	maxlen = (dlen - wlen);
	current = data;
	for (off = 0; off <= maxlen; off++) {
		stat = memcmp(current, what, wlen);
		if (!stat) {
			goto found;
		}
		current = (void *)((size_t)current + 1);
	}
	current = 0;
found:
	return current;
}

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
shift_bytes(unsigned char *data, size_t cnt, size_t off, int dir)
{
	size_t i;
	size_t j;

	if (dir == SHIFT_LEFT) {
		goto shift_left;
	}

	/* shift right here */
	for (i = off, j = i-1; i > (off - cnt); i--, j--) {
		data[i] = data[j];
	}
shift_left:
	for (i = off, j = (i + 1); i < (off + cnt); i++, j++) {
		data[i] = data[j];
	}
	return;
}

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
		unsigned char *what, unsigned char *with)
{
 	unsigned char *where;
	size_t off;

	off = 0;
	do {
		where = findseq(data, what, dlen, count);
		if (!where) {
			break;
		}
		memcpy(where, with, count);
	       	off += (size_t)(where - data);
	} while (off < dlen);
}

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
		unsigned char pad, int padloc)
{
	/* Variables with s_ prefix are used for shift_bytes() */
	size_t off;
	size_t s_cnt;
	size_t s_off;
	size_t pad_size;
	unsigned char *where;
	unsigned char *padptr;

	do {
		where = (unsigned char *)findseq(data, what, dlen, rlen);
		if (!where) {
			break;
		}
		if (padloc == PAD_HERE) {
			/* 
			 * If we pad data to equal lenght, just perform
			 * replacing stuff immediately, no biggies. 
			 * First add padding, then replace
			 */
			memset(where, pad, wlen);
			memcpy(where, with, rlen);
		} else {
			/*
			 * If padding is to be added at the end of the
			 * packet, then we first replace what/with, then
			 * shift left, and finally pad at the end
			 */
			memcpy(where, with, rlen);
			s_cnt = (rlen - wlen);
			s_off = (size_t)((size_t)where - (size_t)data);
			shift_bytes(data, s_cnt, s_off, SHIFT_LEFT);
			
			/* calculate where to pad + pad size */
			pad_size = (rlen - wlen);
			padptr = (unsigned char *)((size_t)data + \
					(dlen - pad_size));
			memset(padptr, pad, pad_size);
		}
	       	off += (size_t)(where - data);
	} while (off < dlen);
}


