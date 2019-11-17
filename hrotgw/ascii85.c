// Convert between binary and ASCII-85 representation.
//
// Allow binary data to be conveyed as only printable ASCII characters.
// 4 binary bytes are converted to a base 85 number with 5 characters in the range 
// of '!' representing 0 thru 'u' representing 84.  
// A single 'z' is a special case for all 4 bytes being zero.
// This is hard coded for our specific purpose of 16 bytes in and usually 20 (+ nul) out.

#include <stdlib.h>
#include <assert.h>

void ascii85_encode (const char *in, char *out)
{
	assert (sizeof(unsigned int) == 4);

	for (int i = 0; i < 4; i++) {		// 4 groups in
	  unsigned int n = 0;
	  for (int j = 0; j < 4; j++) {		// 4 bytes per group
	    n = (n << 8) | (*in++ & 0xff);	
	  }
	  if (n == 0) {
	    *out++ = 'z';			// special case, 1 out of 4 billion chance
	  }					// when dealing with random values.
	  else {
	    unsigned int d;
	    unsigned int v = 85 * 85 * 85 * 85;
	    for (int k = 0; k < 5; k++) {	// 5 digits out per group.
	      d = n / v;
	      n -= d * v;
	      *out++ = '!' + d;
	    }
	  }
	}
	*out = '\0';
}


