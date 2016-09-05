/*
        OpenLase - a realtime laser graphics toolkit

Copyright (C) 2009-2011 Hector Martin "marcan" <hector@marcansoft.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 2.1 or version 3.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ALIGN_H
#define ALIGN_H

#include "config.h"

#include <assert.h>
#include <stdlib.h>

#ifdef HAVE_MALLOC_H
# include <malloc.h>
#endif


#define CHECK_ALIGNMENT \
	assert(alignment != 0 && (alignment&(alignment-1)) == 0); \
	if (alignment < sizeof(void*)) alignment = sizeof(void*)

#if defined(HAVE_POSIX_MEMALIGN)
static inline void *malloc_align(size_t size, size_t alignment)
{
	void *p;
	CHECK_ALIGNMENT;
	if (posix_memalign(&p, alignment, size) == 0)
		return p;
	else
		return NULL;
}
# define free_align free

#elif defined(HAVE_MEMALIGN)
static inline void *malloc_align(size_t size, size_t alignment)
{
	CHECK_ALIGNMENT;
	return memalign(alignment, size);
}
# define free_align free

#elif defined(HAVE_ALIGNED_MALLOC)
static inline void *malloc_align(size_t size, size_t alignment)
{
	CHECK_ALIGNMENT;
	return _aligned_malloc(size, alignment);
}
# define free_align _aligned_free
#else
static inline void *malloc_align(size_t size, size_t alignment)
{
	CHECK_ALIGNMENT;
	void *p = malloc(size + alignment + sizeof(void*) - 1);
	uintptr_t ip = sizeof(void*) + (uintptr_t)p;
	if (ip & (alignment-1))
		ip = (ip & ~(alignment-1)) + alignment;
	((void**)ip)[-1] = p;
	return (void*)ip;
}
static inline void free_align(void *p)
{
	free(((void **)p)[-1]);
}
#endif

#undef CHECK_ALIGNMENT

#endif
