/******************************************************************************
 * @file            parted.c
 *****************************************************************************/
#include    <limits.h>
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>
#include    <time.h>

#include    "lib.h"
#include    "parted.h"
#include    "report.h"
#include    "write7x.h"

#ifndef     __PDOS__
# if     defined (__GNUC__)
#  include  <sys/time.h>
#  include  <unistd.h>
# else
#  include  <io.h>
# endif
#endif

static int cylinders = 0;
static int heads_per_cylinder = 0;
static int sectors_per_track = 0;

struct parted_state *state = 0;
const char *program_name = 0;

struct part {

    unsigned char status;
    unsigned char start_chs[3];
    unsigned char type;
    unsigned char end_chs[3];
    unsigned char start_lba[4];
    unsigned char end_lba[4];

};

struct mbr {

    unsigned char boot_code[440];
    unsigned char signature[4];
    unsigned char copy_protected[2];
    
    struct part parts[4];
    unsigned char boot_sig[2];

};

struct footer {

    unsigned char cookie[8];
    unsigned char features[4];
    
    struct {
    
        unsigned char major[2];
        unsigned char minor[2];
    
    } version;
    
    unsigned char next_offset[8];
    unsigned char modified_time[4];
    unsigned char creator_name[4];
    
    struct {
    
        unsigned char major[2];
        unsigned char minor[2];
    
    } creator_version;
    
    unsigned char creator_host[4];
    unsigned char disk_size[8];
    unsigned char data_size[8];
    
    struct {
    
        unsigned char cylinders[2];
        unsigned char heads_per_cyl;
        unsigned char secs_per_track;
    
    } disk_geom;
    
    unsigned char disk_type[4];
    unsigned char checksum[4];
    unsigned char identifier[16];
    unsigned char saved_state;
    unsigned char reserved[427];

};

static void calculate_geometry (void) {

    int cylinder_times_heads;
    long sectors = state->image_size / 512;
    
    if (sectors > (long) 65535 * 16 * 255) {
        sectors = (long) 65535 * 16 * 255;
    }
    
    if (sectors > (long) 65535 * 16 * 63) {
    
        heads_per_cylinder = 16;
        sectors_per_track = 63;
        
        cylinder_times_heads = sectors / sectors_per_track;
    
    } else {
    
        sectors_per_track = 17;
        cylinder_times_heads = sectors / sectors_per_track;
        
        heads_per_cylinder = (cylinder_times_heads + 1023) >> 10;
        
        if (heads_per_cylinder < 4) {
            heads_per_cylinder = 4;
        }
        
        if (cylinder_times_heads >= heads_per_cylinder << 10 || heads_per_cylinder > 16) {
        
            sectors_per_track = 31;
            heads_per_cylinder = 16;
            
            cylinder_times_heads = sectors / sectors_per_track;
        
        }
        
        if (cylinder_times_heads >= heads_per_cylinder << 10) {
        
            sectors_per_track = 63;
            heads_per_cylinder = 16;
            
            cylinder_times_heads = sectors / sectors_per_track;
        
        }
    
    }
    
    cylinders = cylinder_times_heads / heads_per_cylinder;

}

/** Generate a unique volume id. */
static unsigned int generate_signature (void) {

#if     defined (__PDOS__)

    srand (time (NULL));
    
    /* rand() returns int from [0,RAND_MAX], therefor only 31-bits. */
    return (((unsigned int) (rand () & 0xFFFF)) << 16) | ((unsigned int) (rand() & 0xFFFF));

#elif   defined (__GNUC__)

    struct timeval now;
    
    if (gettimeofday (&now, 0) != 0 || now.tv_sec == (time_t) -1 || now.tv_sec < 0) {
    
        srand (getpid ());
        
        /*- rand() returns int from [0,RAND_MAX], therefor only 31-bits. */
        return (((unsigned int) (rand () & 0xFFFF)) << 16) | ((unsigned int) (rand() & 0xFFFF));
    
    }
    
    /* volume id = current time, fudged for more uniqueness. */
    return ((unsigned int) now.tv_sec << 20) | (unsigned int) now.tv_usec;

#elif   defined (__WATCOMC__)

    srand (getpid ());
    
    /* rand() returns int from [0,RAND_MAX], therefor only 31-bits. */
    return (((unsigned int) (rand () & 0xFFFF)) << 16) | ((unsigned int) (rand() & 0xFFFF));

#endif

}

static void get_random_bytes (void *buf, size_t nbytes) {

    unsigned char *cp = (unsigned char *) buf;
    unsigned int seed;
    
    size_t i;
    
#if     defined (__PDOS__)
    seed = time (NULL);
#elif   defined (__GNUC__)

    struct timeval now;
    seed = (getpid () << 16);
    
    if (gettimeofday (&now, 0) != 0 || now.tv_sec == (time_t) -1 || now.tv_sec < 0) {
        seed = seed ^ now.tv_sec ^ now.tv_usec;
    } else {
        seed ^= time (NULL);
    }

#else

    seed = (getpid () << 16);
    seed ^= time (NULL);

#endif
    
    srand (seed);
    
    for (i = 0; i < nbytes; i++) {
        *cp++ ^= (rand () >> 7) % (CHAR_MAX + 1);
    }

}

static unsigned int generate_checksum (unsigned char *data, size_t len) {

    unsigned int sum = 0;
    int i = 0;
    
    size_t j;
    
    for (j = 0; j < len; j++) {
        sum += data[i++];
    }
    
    return sum ^ 0xffffffff;

}

int main (int argc, char **argv) {

    size_t flen;
    FILE *ofp;
    
    struct footer footer;
    struct mbr mbr;
    
    size_t p, sector = 1;
    
    if (argc && *argv) {
    
        char *p;
        program_name = *argv;
        
        if ((p = strrchr (program_name, '/'))) {
            program_name = (p + 1);
        }
    
    }
    
    state = xmalloc (sizeof (*state));
    parse_args (&argc, &argv, 1);
    
    if (!state->outfile) {
    
        report_at (program_name, 0, REPORT_ERROR, "no outfile file provided");
        return EXIT_FAILURE;
    
    }
    
    if (!state->outfile || state->nb_parts == 0) {
        print_help (EXIT_FAILURE);
    }
    
    memset (&mbr, 0, sizeof (mbr));
    
    if (state->boot) {
    
        FILE *ifp;
        
        if ((ifp = fopen (state->boot, "rb")) == NULL) {
        
            report_at (program_name, 0, REPORT_ERROR, "failed to open '%s' for reading", state->boot);
            return EXIT_FAILURE;
        
        }
        
        fseek (ifp, 0, SEEK_END);
        
        if (ftell (ifp) != 440) {
        
            report_at (program_name, 0, REPORT_ERROR, "size of %s is not equal to 440 bytes", state->boot);
            fclose (ifp);
            
            return EXIT_FAILURE;
        
        }
        
        fseek (ifp, 0, SEEK_SET);
        
        if (fread (&mbr, 440, 1, ifp) != 1) {
        
            report_at (program_name, 0, REPORT_ERROR, "failed whilst reading '%s'", state->boot);
            fclose (ifp);
            
            return EXIT_FAILURE;
        
        }
        
        fclose (ifp);
    
    }
    
    write721_to_byte_array (mbr.boot_sig, 0xAA55, 1);
    write741_to_byte_array (mbr.signature, generate_signature (), 1);
    
    calculate_geometry ();
    
    for (p = 0; p < state->nb_parts; ++p) {
    
        struct part_info *info = state->parts[p];
        unsigned cyl, head, sect, temp;
        
        if (p >= 4) {
        
            report_at (program_name, 0, REPORT_ERROR, "too many partitions provided");
            return EXIT_FAILURE;
        
        }
        
        mbr.parts[p].status = info->status;
        mbr.parts[p].type = info->type;
        
        if (info->start < sector) {
        
            report_at (program_name, 0, REPORT_ERROR, "overlapping partitions");
            return EXIT_FAILURE;
        
        }
        
        sector = info->start;
        
        if (sector * 512 > state->image_size) {
        
            report_at (program_name, 0, REPORT_ERROR, "partition exceeds image size");
            return EXIT_FAILURE;
        
        }
        
        cyl = sector / (heads_per_cylinder * sectors_per_track);
        temp = sector % (heads_per_cylinder * sectors_per_track);
        
        head = temp / sectors_per_track;
        sect = temp % sectors_per_track + 1;
        
        mbr.parts[p].start_chs[0] = head;
        mbr.parts[p].start_chs[1] = sect;
        mbr.parts[p].start_chs[2] = cyl;
        
        write741_to_byte_array (mbr.parts[p].start_lba, info->start, 1);
        
        if (info->end < sector) {
        
            report_at (program_name, 0, REPORT_ERROR, "overlapping partitions");
            return EXIT_FAILURE;
        
        }
        
        sector = info->end;
        
        if (sector * 512 > state->image_size) {
        
            report_at (program_name, 0, REPORT_ERROR, "partition exceeds image size");
            return EXIT_FAILURE;
        
        }
        
        cyl = sector / (heads_per_cylinder * sectors_per_track);
        temp = sector % (heads_per_cylinder * sectors_per_track);
        
        head = temp / sectors_per_track;
        sect = temp % sectors_per_track + 1;
        
        mbr.parts[p].end_chs[0] = head;
        mbr.parts[p].end_chs[1] = sect;
        mbr.parts[p].end_chs[2] = cyl;
        
        write741_to_byte_array (mbr.parts[p].end_lba, info->end, 1);
    
    }
    
    state->image_size += sizeof (footer);
    
    if ((ofp = fopen (state->outfile, "r+b")) == NULL) {
    
        size_t len;
        void *zero;
        
        if ((ofp = fopen (state->outfile, "wb")) == NULL) {
        
            report_at (program_name, 0, REPORT_ERROR, "failed to open '%s' for writing", state->outfile);
            return EXIT_FAILURE;
        
        }
        
        len = state->image_size;
        zero = xmalloc (512);
        
        while (len > 0) {
        
            if (fwrite (zero, 512, 1, ofp) != 1) {
            
                report_at (program_name, 0, REPORT_ERROR, "failed whilst writing '%s'", state->outfile);
                fclose (ofp);
                
                free (zero);
                remove (state->outfile);
                
                return EXIT_FAILURE;
            
            }
            
            len -= 512;
        
        }
        
        free (zero);
    
    }
    
    fseek (ofp, 0, SEEK_END);
    flen = ftell (ofp);
    
    if (flen < state->image_size) {
    
        report_at (program_name, 0, REPORT_ERROR, "file size mismatch (%lu vs %lu)", flen, state->image_size);
        fclose (ofp);
        
        remove (state->outfile);
        return EXIT_FAILURE;
    
    }
    
    fseek (ofp, 0, SEEK_SET);
    
    if (fwrite (&mbr, sizeof (mbr), 1, ofp) != 1) {
    
        report_at (program_name, 0, REPORT_ERROR, "failed whilst writing the master boot record");
        
        fclose (ofp);
        remove (state->outfile);
        
        return EXIT_FAILURE;
    
    }
    
    memset (&footer, 0, sizeof (footer));
    
    memcpy (footer.cookie, "conectix", 8);
    memcpy (footer.creator_host, "Wi2k", 4);
    memcpy (footer.creator_name, "win ", 4);
    
    write721_to_byte_array (footer.version.major, 1, 0);
    write721_to_byte_array (footer.version.minor, 0, 0);
    write721_to_byte_array (footer.creator_version.major, 6, 0);
    write721_to_byte_array (footer.creator_version.minor, 1, 0);
    
    write741_to_byte_array (footer.features, 2, 0);
    write741_to_byte_array (footer.disk_type, 2, 0);
    
    memset (footer.next_offset, 0xff, 8);
    
    write781_to_byte_array (footer.disk_size, flen - sizeof (footer), 0);
    write781_to_byte_array (footer.data_size, flen - sizeof (footer), 0);
    
    write721_to_byte_array (footer.disk_geom.cylinders, cylinders, 0);
    footer.disk_geom.heads_per_cyl = heads_per_cylinder;
    footer.disk_geom.secs_per_track = sectors_per_track;
    
    get_random_bytes (footer.identifier, sizeof (footer.identifier));
    
    write741_to_byte_array (footer.modified_time, time (NULL) + 946684800, 0);
    write741_to_byte_array (footer.checksum, generate_checksum ((unsigned char *) &footer, sizeof (footer)), 0);
    
    if (fseek (ofp, state->image_size - sizeof (footer), SEEK_SET)) {
    
        report_at (program_name, 0, REPORT_ERROR, "failed whilst seeking '%s'", state->outfile);
        
        fclose (ofp);
        remove (state->outfile);
        
        return EXIT_FAILURE;
    
    }
    
    if (fwrite (&footer, sizeof (footer), 1, ofp) != 1) {
    
        report_at (program_name, 0, REPORT_ERROR, "failed whilst writing footer");
        
        fclose (ofp);
        remove (state->outfile);
        
        return EXIT_FAILURE;
    
    }
    
    fclose (ofp);
    return EXIT_SUCCESS;

}
