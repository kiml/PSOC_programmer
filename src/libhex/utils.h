#ifndef _UTILS_H
#define _UTILS_H

// FIXME: Don't actually have dest types so to_Uxx is academic
#define B4LE_to_U32(c)  (((c)[3]<<24) | ((c)[2] << 16) | ((c)[1] << 8) | (c)[0] )
#define B4BE_to_U32(c)  (((c)[0]<<24) | ((c)[1] << 16) | ((c)[2] << 8) | (c)[3] )
#define B2LE_to_U32(c)  (((c)[1] << 8) | (c)[0] )
#define B2BE_to_U16(c)  (((c)[0] << 8) | (c)[1] )

#ifndef MIN
#define MIN(a,b)        ((a) <= (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b)        ((a) >= (b) ? (a) : (b))
#endif

#endif
