/* p3d-dir - shows the directory of a disc-image file together with
 * the used disc space.  Currently there is only support for PCW
 * DDDSDT alternatives sides disc.  The calculated filesize can't see
 * the extents, meaning files >16KB will be showed with filesize 16KB.
 *
 * This is totally FreeWare, so use at your own risk.
 * but please report any bugs and mystical behaviours to
 * Thomas A. Kjaer
 * email: takjaer@daimi.aau.dk
 *
 * Written in ANSI C for GCC under Linux. 
 */

#include <stdio.h>
#include <stdlib.h>

#define DIR_BLOCK -100 /* used for find_block() */

struct p3boot_sec {
  unsigned char disktype;
  unsigned char sideness;
  unsigned char tracks_per_side;
  unsigned char sectors_per_track;
  unsigned char log_sectorsize;
  unsigned char res_tracks;
  unsigned char log_blocksize;
  unsigned char dir_blocks;
  unsigned char gap_rw;
  unsigned char gap_format;
  unsigned char reserved[5];
  unsigned char checksum;
} *boot;

struct dir_entry {
  unsigned char drive_code;
  unsigned char name[8];
  unsigned char type[3];
  unsigned char ex;
  unsigned char t1;
  unsigned char t2;
  unsigned char rc;
  unsigned char index[16];
} *disc_dir;

FILE *in;
unsigned int block_size, sector_size, dir_entries;

void open_disc_image(char *);
void exit_and_close(void);
void print_boot_sector(void);
void read_dir(void);
int print_dir_entry(struct dir_entry *);
int find_pos(unsigned char, unsigned char);

main(int argc, char *argv[]) 
{ 
  if (argc !=2) { 
    fprintf(stderr, "\nDiskImage directory\n"); 
    fprintf(stderr, "Usage: p3d-dir <image-file>\n"); 
    fprintf(stderr, "\nFreeware 1994 by Thomas A.Kjaer takjaer@daimi\n\n"); 
    exit(1); 
  };
  
  /* open disc-image, read the boot-sector and initialise some usefull vars. */
  open_disc_image(argv[1]);

  print_boot_sector();
  read_dir();
  close(in);
  
}

/* Open the disc-image given, and read the boot-sector */
void open_disc_image(char *image_name)
{

  /* initialize space for boot-sector */
  if ((boot = malloc(sizeof(struct p3boot_sec))) == NULL) {
    fprintf(stderr,"error: couldn't allocate memory for boot-sector\n");
    exit(1);
  }
  
  if ((in = fopen(image_name,"r")) == NULL) {
    fprintf(stderr, "error, couldn't open file %s\n", image_name);
    exit_and_close();
  }
  
  /* Read the boot-sector, which also contains informations about 
   * the disc-format used 
   */
  
  if (fread(boot, sizeof(struct p3boot_sec), 1, in) != 1) {
    fprintf(stderr, "error in reading boot-sector\n");
    exit_and_close();
  }
  
  /* set some usefull vars. */
  sector_size = 1 << (boot->log_sectorsize+7);
  block_size = 128 * (1 << (boot->log_blocksize));
  dir_entries = (block_size * boot->dir_blocks ) / 32;
}

/* close file in, and exit with code 1 */
void exit_and_close(void)
{
  close(in);
  exit(1);
}

/* Print informations found in the boot-sector */
void print_boot_sector()
{
  fprintf(stderr, "\n");
  fprintf(stderr, "DISKTYPE: ");
  
  /* Print disk-type */
  switch(boot->disktype) {
  case 0: 
    fprintf(stderr, "Standard +3DOS\n");
    break;
  case 1:
    fprintf(stderr, "Standard System format\n");
    break;
  case 2:
    fprintf(stderr, "Standard Data only\n");
    break;
  case 3:
    fprintf(stderr, "PCW DD.DS.DT\n");
    break;
  };
  switch(boot->sideness && 3) {
  case 0:
    fprintf(stderr, "Single side");
    break;
  case 1:
    fprintf(stderr, "Double side, alternative sides");
    break;
  case 2:
    fprintf(stderr, "Double side, successive sides");
    break;
  }
  if ((boot->sideness && 128) == 0) 
    fprintf(stderr, "\n");
  else 
    fprintf(stderr, ", Double track\n");

  fprintf(stderr, "Number of tracks per side %d\n", boot->tracks_per_side);
  fprintf(stderr, "Number of sectors per track %d\n", boot->sectors_per_track);
  fprintf(stderr, "Number of reserved track %d\n", boot->res_tracks);
  fprintf(stderr, "Number of directory blocks %d\n", boot->dir_blocks);
  fprintf(stderr, "Gap length Read/Write %d\n", boot->gap_rw);
  fprintf(stderr, "Gap length Format %d\n", boot->gap_format);
  fprintf(stderr, "Sector Size %d bytes\n", 1 << (boot->log_sectorsize+7));
  fprintf(stderr, "Block Size %d bytes\n", 128 * (1 << boot->log_blocksize));
  fprintf(stderr, "\n");
}

int compare(unsigned char *s1, unsigned char *s2)
{
  return memcmp(s1, s2, 32);
}

/* Show the directory */
void read_dir(void)
{
  unsigned int dir_count;
  unsigned char dir_track, dir_sector;
  struct dir_entry *current_entry;

  if ((disc_dir = malloc(dir_entries*sizeof(struct dir_entry))) == NULL) {
    fprintf(stderr, "error in function read_dir, allocating for dir.\n");
    exit_and_close();
  }
  
  dir_count = dir_entries;
  fprintf(stderr, "Number of directory entries: %d\n", dir_entries);
  
  /* find start of directory block */
  dir_track = boot->res_tracks;
  dir_sector = 0;
  if (find_pos(dir_track, dir_sector) != 0) {
    fprintf(stderr, "error in finding track, sector\n");
    exit_and_close();
  }
 
  /* Read the directory block into disc_dir */
  if (fread(disc_dir, sizeof(struct dir_entry), dir_entries, in) != dir_entries) {
    fprintf(stderr, "error in reading directory.\n");
    exit_and_close();
  }
  
  /* sort dir block */
  qsort(disc_dir, dir_entries, sizeof(struct dir_entry), (int(*))&compare);
  current_entry = disc_dir;
  while (dir_count > 0) {
    /* skip deleted entries */
    if (current_entry->drive_code == 0xe5)
      ;
    else 
      print_dir_entry(current_entry);
    dir_count--;
    current_entry++;
  }
}

int print_dir_entry(struct dir_entry *entry)
{
  int readed_entries; /* how many extents did this file occupy */
  unsigned int i, filesize;

  readed_entries = 1;
  if (entry->drive_code == 0xe5) return; /* deleted entry */
  if (entry->ex == 0) { /* last entry in an extent */
    readed_entries = 0;
    for (i = 0; i < 16;) if (entry->index[i+=2] != 0) readed_entries++;
    /* print the filename */
    printf("%d: %.8s.%.3s %d KB\n", entry->drive_code, entry->name, 
	   entry->type, (readed_entries*block_size) / 1024);
  }
  return readed_entries;
}

/* Sets the file position according to track, sector 
 * returns the result from fseek (non zero on errors)
 */
int find_pos(unsigned char track, unsigned char sector)
{
  unsigned int offset;
  
  switch(boot->sideness && 3) {
  case 1:
    offset = track*(boot->sectors_per_track)*sector_size + sector_size*sector;
    break;
  }
  return fseek(in, offset, SEEK_SET);
}

/* Sets the file position according to block */
int find_block(unsigned int block)
{
  unsigned int offset;

  switch(boot->sideness && 3) {
  case 1:
    offset = 1;
  }
}
