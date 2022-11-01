/******************************************************************************
 * @file            lib.c
 *****************************************************************************/
#include    <ctype.h>
#include    <errno.h>
#include    <stddef.h>
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>

#include    "lib.h"
#include    "parted.h"
#include    "report.h"

struct option {

    const char *name;
    int index, flags;

};

#define     OPTION_NO_ARG               0x0001
#define     OPTION_HAS_ARG              0x0002

enum options {

    OPTION_IGNORED = 0,
    OPTION_BOOT,
    OPTION_HELP,
    OPTION_LABEL,
    OPTION_PART

};

static struct option opts[] = {

    { "-boot",      OPTION_BOOT,        OPTION_HAS_ARG  },
    { "-help",      OPTION_HELP,        OPTION_NO_ARG   },
    { 0,            0,                  0               }

};

static struct option opts1[] = {

    { "mklabel",    OPTION_LABEL,       OPTION_HAS_ARG  },
    { "mkpart",     OPTION_PART,        OPTION_HAS_ARG  },
    { 0,            0,                  0               }

};

static int strstart (const char *val, const char **str) {

    const char *p = val;
    const char *q = *str;
    
    while (*p != '\0') {
    
        if (*p != *q) {
            return 0;
        }
        
        ++p;
        ++q;
    
    }
    
    *str = q;
    return 1;

}

static char *skip_whitespace (char *p) {

    while (*p && *p == ' ') {
        ++p;
    }
    
    return p;

}

static char *trim (char *p) {

    size_t len = strlen (p);
    size_t i;
    
    for (i = len; i > 0; --i) {
    
        if (p[i] == ' ') {
            p[i] = '\0';
        }
    
    }
    
    if (p[i] == ' ') {
        p[i] = '\0';
    }
    
    return p;

}

char *xstrdup (const char *str) {

    char *ptr = xmalloc (strlen (str) + 1);
    strcpy (ptr, str);
    
    return ptr;

}

int xstrcasecmp (const char *s1, const char *s2) {

    const unsigned char *p1;
    const unsigned char *p2;
    
    p1 = (const unsigned char *) s1;
    p2 = (const unsigned char *) s2;
    
    while (*p1 != '\0') {
    
        if (toupper (*p1) < toupper (*p2)) {
            return (-1);
        } else if (toupper (*p1) > toupper (*p2)) {
            return (1);
        }
        
        p1++;
        p2++;
    
    }
    
    if (*p2 == '\0') {
        return (0);
    } else {
        return (-1);
    }

}

void *xmalloc (size_t size) {

    void *ptr = malloc (size);
    
    if (ptr == NULL && size) {
    
        report_at (program_name, 0, REPORT_ERROR, "memory full (malloc)");
        exit (EXIT_FAILURE);
    
    }
    
    memset (ptr, 0, size);
    return ptr;

}

void *xrealloc (void *ptr, size_t size) {

    void *new_ptr = realloc (ptr, size);
    
    if (new_ptr == NULL && size) {
    
        report_at (program_name, 0, REPORT_ERROR, "memory full (realloc)");
        exit (EXIT_FAILURE);
    
    }
    
    return new_ptr;

}

void dynarray_add (void *ptab, size_t *nb_ptr, void *data) {

    int nb, nb_alloc;
    void **pp;
    
    nb = *nb_ptr;
    pp = *(void ***) ptab;
    
    if ((nb & (nb - 1)) == 0) {
    
        if (!nb) {
            nb_alloc = 1;
        } else {
            nb_alloc = nb * 2;
        }
        
        pp = xrealloc (pp, nb_alloc * sizeof (void *));
        *(void ***) ptab = pp;
    
    }
    
    pp[nb++] = data;
    *nb_ptr = nb;

}

void parse_args (int *pargc, char ***pargv, int optind) {

    char **argv = *pargv;
    int argc = *pargc;
    
    struct option *popt;
    const char *optarg, *r;
    
    if (argc == optind) {
        print_help (EXIT_SUCCESS);
    }
    
    while (optind < argc) {
    
        r = argv[optind++];
        
        if (r[0] != '-' || r[1] == '\0') {
        
            for (popt = opts1; popt->name; ++popt) {
            
                const char *p1 = popt->name;
                
                if (xstrcasecmp (p1, r) != 0) {
                    continue;
                }
                
                if (popt->flags & OPTION_HAS_ARG) {
                
                    if (optind >= argc) {
                    
                        report_at (program_name, 0, REPORT_ERROR, "argument to '%s' is missing", r);
                        exit (EXIT_FAILURE);
                    
                    }
                    
                    optarg = argv[optind++];
                    goto have_opt;
                
                }
            
            }
            
            if (state->outfile) {
            
                report_at (program_name, 0, REPORT_ERROR, "multiple output files provided");
                exit (EXIT_FAILURE);
            
            }
            
            state->outfile = xstrdup (r);
            continue;
        
        }
        
        for (popt = opts; popt; ++popt) {
        
            const char *p1 = popt->name;
            const char *r1 = (r + 1);
            
            if (!p1) {
            
                report_at (program_name, 0, REPORT_ERROR, "invalid option -- '%s'", r);
                exit (EXIT_FAILURE);
            
            }
            
            if (!strstart (p1, &r1)) {
                continue;
            }
            
            optarg = r1;
            
            if (popt->flags & OPTION_HAS_ARG) {
            
                if (*optarg == '\0') {
                
                    if (optind >= argc) {
                    
                        report_at (program_name, 0, REPORT_ERROR, "argument to '%s' is missing", r);
                        exit (EXIT_FAILURE);
                    
                    }
                    
                    optarg = argv[optind++];
                
                }
            
            } else if (*optarg != '\0') {
                continue;
            }
            
            break;
        
        }
        
    have_opt:
        
        switch (popt->index) {
        
            case OPTION_BOOT: {
            
                state->boot = xstrdup (optarg);
                break;
            
            }
            
            case OPTION_HELP: {
            
                print_help (EXIT_SUCCESS);
                break;
            
            }
            
            case OPTION_LABEL: {
            
                /*report_at (__FILE__, __LINE__, REPORT_INTERNAL_ERROR, "label: %s", optarg);*/
                break;
            
            }
            
            case OPTION_PART: {
            
                long conversion, size;
                char *temp;
                
                struct part_info *part = xmalloc (sizeof (*part));
                
                char *p1 = skip_whitespace ((char *) optarg);
                char *p2 = p1;
                
                while (*p2 && *p2 != ',') {
                    p2++;
                }
                
                if (*p2 == ',') {
                    *p2++ = '\0';
                }
                
                p1 = trim (p1);
                
                errno = 0;
                conversion = strtol (p1, &temp, 0);
                
                if (!*p1 || isspace ((int) *p1) || *temp || errno) {
                
                    report_at (program_name, 0, REPORT_ERROR, "failed to get partition status");
                    exit (EXIT_FAILURE);
                
                }
                
                if (conversion != 0 && conversion != 128) {
                
                    report_at (program_name, 0, REPORT_ERROR, "partition status must be either 0 or 128");
                    exit (EXIT_FAILURE);
                
                }
                
                part->status = conversion;
                
                p2 = skip_whitespace (p2);
                p1 = p2;
                
                while (*p2 && *p2 != ',') {
                    p2++;
                }
                
                if (*p2 == ',') {
                    *p2++ = '\0';
                }
                
                p1 = trim (p1);
                
                errno = 0;
                conversion = strtol (p1, &temp, 0);
                
                if (!*p1 || isspace ((int) *p1) || *temp || errno) {
                
                    report_at (program_name, 0, REPORT_ERROR, "failed to get partition type");
                    exit (EXIT_FAILURE);
                
                }
                
                switch (conversion) {
                
                    case 0x01:
                    case 0x04:
                    case 0x06:
                    case 0x0B:
                    case 0x0C:
                    case 0x0E:
                    
                        break;
                    
                    default:
                    
                        report_at (program_name, 0, REPORT_ERROR, "unsupported partition type provided");
                        exit (EXIT_FAILURE);
                
                }
                
                part->type = conversion;
                
                p2 = skip_whitespace (p2);
                p1 = p2;
                
                while (*p2 && *p2 != ',') {
                    p2++;
                }
                
                if (*p2 == ',') {
                    *p2++ = '\0';
                }
                
                p1 = trim (p1);
                
                errno = 0;
                conversion = strtol (p1, &temp, 0);
                
                if (!*p1 || isspace ((int) *p1) || *temp || errno) {
                
                    report_at (program_name, 0, REPORT_ERROR, "failed to get partition start");
                    exit (EXIT_FAILURE);
                
                }
                
                if (conversion < 0) {
                
                    report_at (program_name, 0, REPORT_ERROR, "partition start must be greater than 0");
                    exit (EXIT_FAILURE);
                
                }
                
                part->start = (unsigned int) conversion;
                
                p2 = skip_whitespace (p2);
                p1 = p2;
                
                while (*p2 && *p2 != ',') {
                    p2++;
                }
                
                if (*p2 == ',') {
                    *p2++ = '\0';
                }
                
                p1 = trim (p1);
                
                errno = 0;
                conversion = strtol (p1, &temp, 0);
                
                if (!*p1 || isspace ((int) *p1) || *temp || errno) {
                
                    report_at (program_name, 0, REPORT_ERROR, "failed to get partition end");
                    exit (EXIT_FAILURE);
                
                }
                
                if (conversion < 0) {
                
                    report_at (program_name, 0, REPORT_ERROR, "partition end must be greater than 0");
                    exit (EXIT_FAILURE);
                
                }
                
                part->end = (unsigned int) conversion;
                size = (part->start + part->end) * 512;
                
                if (size < 0) {
                
                    report_at (program_name, 0, REPORT_ERROR, "partition start must be greater than partition end");
                    exit (EXIT_FAILURE);
                
                }
                
                if ((long) state->image_size < size) {
                    state->image_size = size;
                }
                
                dynarray_add (&state->parts, &state->nb_parts, part);
                break;
            
            }
            
            default: {
            
                report_at (program_name, 0, REPORT_ERROR, "unsupported option '%s'", r);
                exit (EXIT_FAILURE);
            
            }
        
        }
    
    }

}

void print_help (int exitval) {

    if (!program_name) {
        goto _exit;
    }
    
    fprintf (stderr, "Usage: %s [options] [commands] file\n\n", program_name);
    fprintf (stderr, "Options:\n\n");
    fprintf (stderr, "    --boot FILE           Write FILE to start of image\n");
    fprintf (stderr, "    --help                Print this help information\n");
    fprintf (stderr, "\n");
    fprintf (stderr, "Commands:\n\n");
    fprintf (stderr, "    mkpart status,type,start,end  Make a partition\n");
    
_exit:
    
    exit (exitval);

}
