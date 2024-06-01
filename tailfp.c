#include <stdio.h>
#include <glob.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#define BUFSIZE 131072
#define MAXOPENFILES 32
#define MAXFILEAGE 600

char *lastFile(char *filePattern) {
  glob_t globList;
  char *ret = NULL;
  int gres = glob(filePattern, GLOB_PERIOD, NULL, &globList);
  switch(gres) {
  case GLOB_NOSPACE:
    // fprintf(stderr, "Ran out of space.\n");
    return NULL;
    break;
  case GLOB_NOMATCH:
    // fprintf(stderr, "No match.\n");
    return NULL;
    break;
  case GLOB_ABORTED:
    // fprintf(stderr, "Aborted.\n");
    return NULL;
    break;
  }
  for (int i=0; globList.gl_pathv[i]; i++) {
    ret = globList.gl_pathv[i];
  }
  ret = strdup(ret);
  globfree(&globList);
  return ret;
}

struct finfo_s {
  char *name;
  time_t lastmod;
};

struct openfile_s {
  char *filename;
  FILE *file;
  time_t lastchange;
};

int tcompare(const void *av, const void *bv) {
  const struct finfo_s *a = av;
  const struct finfo_s *b = bv;
  return (b->lastmod > a->lastmod)?1:((b->lastmod < a->lastmod)?-1:0);
}

char *newestFile(char *filePattern) {
  glob_t globList;
  struct stat fstats;
  int gres = glob(filePattern, GLOB_PERIOD, NULL, &globList);
  char *ret = NULL;
  switch(gres) {
  case GLOB_NOSPACE:
    // fprintf(stderr, "Ran out of space.\n");
    return NULL;
    break;
  case GLOB_NOMATCH:
    // fprintf(stderr, "No match.\n");
    return NULL;
    break;
  case GLOB_ABORTED:
    // fprintf(stderr, "Aborted.\n");
    return NULL;
    break;
  }
  int nFiles = 0;
  while (globList.gl_pathv[nFiles]) {
    nFiles++;
  }
  if (nFiles > 0) {
    struct finfo_s *files = (struct finfo_s *)malloc(nFiles * sizeof(struct finfo_s));
    for (int i=0; i<nFiles; i++) {
      files[i].name = globList.gl_pathv[i];
      files[i].lastmod = 0;
      if (stat(files[i].name, &fstats) == -1) {
	perror(globList.gl_pathv[i]);
      } else {
	files[i].lastmod = fstats.st_mtim.tv_sec;
      }
    }
    qsort(files, nFiles, sizeof(struct finfo_s), tcompare);
    ret = strdup(files[0].name);
    free(files);
    globfree(&globList);
  }
  return ret;
}

// TODO: Add an array of structs with FILE pointers and last change date, so that we can keep
// tailing any open files for a while in case new stuff comes in on them. Timeout and close them
// after a set time.
int main(int argc, char **argv)
{
  int debug = 1;
  char *filePattern = argv[1];
  char *filename = NULL;
  static struct openfile_s infiles[MAXOPENFILES];
  static char inbuf[BUFSIZE];
  if (argc == 1) {
    return (-1);
  }
  memset(infiles, 0, sizeof(infiles));

  while (1) {
    for (int i=0; i<MAXOPENFILES; i++) {
      if (infiles[i].file) {
	size_t nRead = 0;
	int didread = 0;
	do {
	  nRead = fread(inbuf, 1, sizeof(inbuf), infiles[i].file);
	  if (nRead > 0) {
	    didread = 1;
	    fwrite(inbuf, 1, nRead, stdout);
	    infiles[i].lastchange = time(NULL);
	  }
	} while (nRead > 0);
	if (!didread) {
	  if (time(NULL) - infiles[i].lastchange > MAXFILEAGE) {
	    if (debug) {
	      fprintf(stderr, "Closing inactive file %s\n", infiles[i].filename);
	    }
	    fclose(infiles[i].file);
	    free(infiles[i].filename);
	    infiles[i].file = NULL;
	    infiles[i].filename = NULL;
	    infiles[i].lastchange = 0;
	  }
	}
      }
    }
    filename = newestFile(filePattern);
    int isOpen = 0;
    if (filename != NULL) {
      for (int i=0; i<MAXOPENFILES; i++) {
	if (infiles[i].filename && !strcmp(filename, infiles[i].filename)) {
	  isOpen = 1;
	}
      }
      if (!isOpen) {
	if (debug) {
	  fprintf(stderr, "New file %s\n", filename);
	}
	int newfileno = 0;
	time_t oldesttime = time(NULL);
	int oldestfileno = 0;
	// Find the first NULL file, and at the same time find the oldest one
	while (newfileno < MAXOPENFILES && infiles[newfileno].file != NULL) {
	  if (infiles[newfileno].lastchange < oldesttime) {
	    oldestfileno = newfileno;
	    oldesttime = infiles[newfileno].lastchange;
	  }
	  newfileno++;
	}
	if (newfileno == MAXOPENFILES) {
	  if (debug) {
	    fprintf(stderr, "Closing oldest file %d\n", oldestfileno);
	  }
	  fclose(infiles[oldestfileno].file);
	  infiles[oldestfileno].file = NULL;
	  free(infiles[oldestfileno].filename);
	  infiles[oldestfileno].filename = NULL;
	  infiles[oldestfileno].lastchange = 0;
	  newfileno = oldestfileno;
	}
	if (debug) {
	  fprintf(stderr, "Opening file as #%d\n", newfileno);
	}
	if (!(infiles[newfileno].file = fopen(filename, "r"))) {
	  perror(filename);
	} else {
	  infiles[newfileno].filename = filename;
	  infiles[newfileno].lastchange = time(NULL);
	}
      } else {
	free(filename);
      }
    }
    usleep(50000);
  }
  return (0);
}
