/******************************************************************************
 * @file            parted.h
 *****************************************************************************/
#ifndef     _PARTED_H
#define     _PARTED_H

struct part_info {

    int status, type;
    unsigned int start, end;

};

#include    <stddef.h>

struct parted_state {

    const char *boot, *outfile;
    
    struct part_info **parts;
    size_t nb_parts;
    
    size_t image_size;

};

extern struct parted_state *state;
extern const char *program_name;

#endif      /* _PARTED_H */
