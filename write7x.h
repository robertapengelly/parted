/******************************************************************************
 * @file            write7x.h
 *****************************************************************************/
#ifndef     _WRITE7X_H
#define     _WRITE7X_H

void write721_to_byte_array (unsigned char *dest, unsigned short val, int little_endian);
void write741_to_byte_array (unsigned char *dest, unsigned int val, int little_endian);
void write781_to_byte_array (unsigned char *dest, unsigned long val, int little_endian);

#endif      /* _WRITE7X_H */
