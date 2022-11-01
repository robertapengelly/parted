/******************************************************************************
 * @file            write7x.c
 *****************************************************************************/
#include    <limits.h>

#include    "write7x.h"

void write721_to_byte_array (unsigned char *dest, unsigned short val, int little_endian) {

    if (little_endian) {
    
        dest[0] = (val & 0xFF);
        dest[1] = (val >> 8) & 0xFF;
    
    } else {
    
        dest[1] = (val & 0xFF);
        dest[0] = (val >> 8) & 0xFF;
    
    }

}

void write741_to_byte_array (unsigned char *dest, unsigned int val, int little_endian) {

    if (little_endian) {
    
        dest[0] = (val & 0xFF);
        dest[1] = (val >> 8) & 0xFF;
        dest[2] = (val >> 16) & 0xFF;
        dest[3] = (val >> 24) & 0xFF;
    
    } else {
    
        dest[3] = (val & 0xFF);
        dest[2] = (val >> 8) & 0xFF;
        dest[1] = (val >> 16) & 0xFF;
        dest[0] = (val >> 24) & 0xFF;
    
    }

}

void write781_to_byte_array (unsigned char *dest, unsigned long val, int little_endian) {

    if (little_endian) {
    
        dest[0] = (val & 0xFF);
        dest[1] = (val >> 8) & 0xFF;
        dest[2] = (val >> 16) & 0xFF;
        dest[3] = (val >> 24) & 0xFF;
        
#if     ULONG_MAX > 4294967295UL
        
        dest[4] = (val >> 32) & 0xFF;
        dest[5] = (val >> 40) & 0xFF;
        dest[6] = (val >> 48) & 0xFF;
        dest[7] = (val >> 56) & 0xFF;
        
#else
        
        dest[4] = 0;
        dest[5] = 0;
        dest[6] = 0;
        dest[7] = 0;
        
#endif
    
    } else {
    
        dest[7] = (val & 0xFF);
        dest[6] = (val >> 8) & 0xFF;
        dest[5] = (val >> 16) & 0xFF;
        dest[4] = (val >> 24) & 0xFF;
        
#if     ULONG_MAX > 4294967295UL
        
        dest[3] = (val >> 32) & 0xFF;
        dest[2] = (val >> 40) & 0xFF;
        dest[1] = (val >> 48) & 0xFF;
        dest[0] = (val >> 56) & 0xFF;
     
#else
        
        dest[3] = 0;
        dest[2] = 0;
        dest[1] = 0;
        dest[0] = 0;
        
#endif
    
    }

}
