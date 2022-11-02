/******************************************************************************
 * @file            parted.h
 *****************************************************************************/
#ifndef     _PARTED_H
#define     _PARTED_H

#include    <stddef.h>

struct part_info {

    int status, type;
    size_t start, end;

};

struct parted_state {

    const char *boot, *outfile;
    
    struct part_info **parts;
    size_t nb_parts;
    
    size_t image_size;

};

extern struct parted_state *state;
extern const char *program_name;

#endif      /* _PARTED_H */
