#ifdef HAVE_CONFIG_H
  #include "../config.h"
#endif
#include "common/darktable.h"
#include "common/image.h"
#include "common/image_cache.h"
#include "library/library.h"
#include "develop/develop.h"
#include "control/control.h"
#include "gui/gtk.h"
#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_MAGICK
  #include <magick/MagickCore.h>
#endif
#include <string.h>
#ifdef _OPENMP
#  include <omp.h>
#endif

darktable_t darktable;

int dt_init(int argc, char *argv[])
{
#ifdef _OPENMP
  omp_set_num_threads(omp_get_num_procs());
#endif
  darktable.unmuted = 0;
  for(int k=0;k<argc;k++)
  {
    if(argv[k][0] == '-' && argv[k][1] == 'd' && argc > k+1)
    {
      if(!strcmp(argv[k+1], "cache"))   darktable.unmuted |= DT_DEBUG_CACHE;   // enable debugging for lib/film/cache module
      if(!strcmp(argv[k+1], "control")) darktable.unmuted |= DT_DEBUG_CONTROL; // enable debugging for scheduler module
      if(!strcmp(argv[k+1], "dev"))     darktable.unmuted |= DT_DEBUG_DEV; // develop module
    }
  }

#ifdef HAVE_MAGICK
  MagickCoreGenesis(*argv, MagickTrue);
#endif
  char *homedir = getenv("HOME");
  char filename[512], *c = NULL;
  snprintf(filename, 512, "%s/.darktablerc", homedir);
  FILE *f = fopen(filename, "rb");
  if(f)
  {
    c = fgets(filename, 512, f);
    if(c) for(;c<filename+MIN(512,strlen(filename));c++) if(*c == '\n') *c = '\0';
    fclose(f);
  }
  if(!c) snprintf(filename, 512, "%s/.darktabledb", homedir);
  if(sqlite3_open(filename, &(darktable.db)))
  {
    fprintf(stderr, "[init] could not open database %s!\n", filename);
    sqlite3_close(darktable.db);
    exit(1);
  }
  pthread_mutex_init(&(darktable.db_insert), NULL);

  // has to go first for settings needed by all the others.
  darktable.control = (dt_control_t *)malloc(sizeof(dt_control_t));
  dt_control_init(darktable.control);

  darktable.mipmap_cache = (dt_mipmap_cache_t *)malloc(sizeof(dt_mipmap_cache_t));
  dt_mipmap_cache_init(darktable.mipmap_cache, 500);

  darktable.image_cache = (dt_image_cache_t *)malloc(sizeof(dt_image_cache_t));
  dt_image_cache_init(darktable.image_cache, 500);

  darktable.library = (dt_library_t *)malloc(sizeof(dt_library_t));
  dt_library_init(darktable.library);

  darktable.develop = (dt_develop_t *)malloc(sizeof(dt_develop_t));
  dt_dev_init(darktable.develop, 1);

  darktable.gui = (dt_gui_gtk_t *)malloc(sizeof(dt_gui_gtk_t));
  dt_gui_gtk_init(darktable.gui, argc, argv);

  dt_control_load_config(darktable.control);
  strncpy(darktable.control->global_settings.dbname, filename, 512); // overwrite if relocated.

  return 0;
}

void dt_cleanup()
{
  dt_ctl_gui_mode_t gui;
  DT_CTL_GET_GLOBAL(gui, gui);
  if(gui == DT_DEVELOP) dt_dev_leave();
  char *homedir = getenv("HOME");
  char filename[512];
  snprintf(filename, 512, "%s/.darktablerc", homedir);
  FILE *f = fopen(filename, "wb");
  if(f)
  {
    if(fputs(darktable.control->global_settings.dbname, f) == EOF) fprintf(stderr, "[cleanup] could not write to %s!\n", filename);
    fclose(f);
  }
  else fprintf(stderr, "[cleanup] could not write to %s!\n", filename);
  dt_control_write_config(darktable.control);

  dt_control_cleanup(darktable.control);
  free(darktable.control);
  dt_dev_cleanup(darktable.develop);
  free(darktable.develop);
  dt_library_cleanup(darktable.library);
  free(darktable.library);
  dt_gui_gtk_cleanup(darktable.gui);
  free(darktable.gui);
  dt_image_cache_cleanup(darktable.image_cache);
  free(darktable.image_cache);
  dt_mipmap_cache_cleanup(darktable.mipmap_cache);
  free(darktable.mipmap_cache);

  sqlite3_close(darktable.db);
  pthread_mutex_destroy(&(darktable.db_insert));

#ifdef HAVE_MAGICK
  MagickCoreTerminus();
#endif
}

void dt_print(dt_debug_thread_t thread, const char *msg, ...)
{
  if(darktable.unmuted & thread)
  {
    va_list ap;
    va_start(ap, msg);
    vprintf(msg, ap);
    va_end(ap);
  }
}
