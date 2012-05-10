/*
   xxHash - Fast Hash algorithm
   Copyright (C) 2012, Yann Collet.
   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

	You can contact the author at :
	- xxHash source repository : http://code.google.com/p/xxhash/
*/


//**************************************
// Includes
//**************************************
#include "xxhash.h"



//**************************************
// Compiler Options
//**************************************
#ifdef _MSC_VER              // Visual Studio
#define inline __forceinline // Visual is not C99, but supports some kind of inline
#endif

// GCC does not support _rotl outside of Windows
#if !defined(_WIN32)
#define _rotl(x,r) ((x << r) | (x >> (32 - r)))
#endif



//**************************************
// Constants
//**************************************
#define PRIME1   2654435761U
#define PRIME2   2246822519U
#define PRIME3   3266489917U
#define PRIME4    668265263U
#define PRIME5   0x165667b1



//****************************
// Private functions
//****************************

// This version is for very small inputs (< 16  bytes)
inline unsigned int XXH_small(const void* key, int len, unsigned int seed)
{
	const unsigned char* p = (unsigned char*)key;
	const unsigned char* const bEnd = p + len;
	unsigned int idx = seed + PRIME1;
	unsigned int crc = PRIME5;
	const unsigned char* const limit = bEnd - 4;

	while (p<limit)
	{
		crc += ((*(unsigned int*)p) + idx++);
		crc += _rotl(crc, 17) * PRIME4;
		crc *= PRIME1;
		p+=4;
	}

	while (p<bEnd)
	{
		crc += ((*p) + idx++);
		crc *= PRIME1;
		p++;
	}

	crc += len;

	crc ^= crc >> 15;
	crc *= PRIME2;
	crc ^= crc >> 13;
	crc *= PRIME3;
	crc ^= crc >> 16;

	return crc;
}



//******************************
// Hash functions
//******************************
unsigned int XXH_fast32(const void* input, int len, unsigned int seed)
{
	// Special case, for small inputs
	if (len < 16) return XXH_small(input, len, seed);

	{
		const unsigned char* p = (const unsigned char*)input;
		const unsigned char* const bEnd = p + len;
		unsigned int v1 = seed + PRIME1;
		unsigned int v2 = v1 * PRIME2 + len;
		unsigned int v3 = v2 * PRIME3;
		unsigned int v4 = v3 * PRIME4;	
		const unsigned char* const limit = bEnd - 16;
		unsigned int crc;

		while (p<limit)
		{
			v1 = _rotl(v1, 13) + (*(unsigned int*)p); p+=4;
			v2 = _rotl(v2, 11) + (*(unsigned int*)p); p+=4;
			v3 = _rotl(v3, 17) + (*(unsigned int*)p); p+=4;
			v4 = _rotl(v4, 19) + (*(unsigned int*)p); p+=4;
		} 

		p = bEnd - 16;
		v1 += _rotl(v1, 17); v2 += _rotl(v2, 19); v3 += _rotl(v3, 13); v4 += _rotl(v4, 11); 
		v1 *= PRIME1; v2 *= PRIME1; v3 *= PRIME1; v4 *= PRIME1; 
		v1 += *(unsigned int*)p; p+=4; v2 += *(unsigned int*)p; p+=4; v3 += *(unsigned int*)p; p+=4; v4 += *(unsigned int*)p;   // p+=4;
		v1 *= PRIME2; v2 *= PRIME2; v3 *= PRIME2; v4 *= PRIME2; 
		v1 += _rotl(v1, 11); v2 += _rotl(v2, 17); v3 += _rotl(v3, 19); v4 += _rotl(v4, 13); 
		v1 *= PRIME3; v2 *= PRIME3; v3 *= PRIME3; v4 *= PRIME3;

		crc = v1 + _rotl(v2, 3) + _rotl(v3, 6) + _rotl(v4, 9);
		crc ^= crc >> 11;
		crc += (PRIME4+len) * PRIME1;
		crc ^= crc >> 15;
		crc *= PRIME2;
		crc ^= crc >> 13;

		return crc;
	}

}



unsigned int XXH_strong32(const void* input, int len, unsigned int seed)
{
	// Special case, for small inputs
	if (len < 16) return XXH_small(input, len, seed);

	{
		const unsigned char* p = (const unsigned char*)input;
		const unsigned char* const bEnd = p + len;
		unsigned int v1 = seed + PRIME1;
		unsigned int v2 = v1 * PRIME2 + len;
		unsigned int v3 = v2 * PRIME3;
		unsigned int v4 = v3 * PRIME4;	
		const unsigned char* const limit = bEnd - 16;
		unsigned int crc;

		while (p<limit)
		{
			v1 += _rotl(v1, 13); v1 *= PRIME1; v1 += (*(unsigned int*)p); p+=4;
			v2 += _rotl(v2, 11); v2 *= PRIME1; v2 += (*(unsigned int*)p); p+=4;
			v3 += _rotl(v3, 17); v3 *= PRIME1; v3 += (*(unsigned int*)p); p+=4;
			v4 += _rotl(v4, 19); v4 *= PRIME1; v4 += (*(unsigned int*)p); p+=4;
		} 

		p = bEnd - 16;
		v1 += _rotl(v1, 17); v2 += _rotl(v2, 19); v3 += _rotl(v3, 13); v4 += _rotl(v4, 11); 
		v1 *= PRIME1; v2 *= PRIME1; v3 *= PRIME1; v4 *= PRIME1; 
		v1 += *(unsigned int*)p; p+=4; v2 += *(unsigned int*)p; p+=4; v3 += *(unsigned int*)p; p+=4; v4 += *(unsigned int*)p;   // p+=4;
		v1 *= PRIME2; v2 *= PRIME2; v3 *= PRIME2; v4 *= PRIME2; 
		v1 += _rotl(v1, 11); v2 += _rotl(v2, 17); v3 += _rotl(v3, 19); v4 += _rotl(v4, 13); 
		v1 *= PRIME3; v2 *= PRIME3; v3 *= PRIME3; v4 *= PRIME3;

		crc = v1 + _rotl(v2, 3) + _rotl(v3, 6) + _rotl(v4, 9);
		crc ^= crc >> 11;
		crc += (PRIME4+len) * PRIME1;
		crc ^= crc >> 15;
		crc *= PRIME2;
		crc ^= crc >> 13;

		return crc;
	}

}





