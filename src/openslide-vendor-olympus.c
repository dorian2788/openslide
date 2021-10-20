/*
 *  OpenSlide, a library for reading whole slide image files
 *
 *  Copyright (c) 2021 Nico Curti
 *  All rights reserved.
 *
 *  OpenSlide is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation, version 2.1.
 *
 *  OpenSlide is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with OpenSlide. If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

/*
 * Aperio (svs, tif) support
 *
 * quickhash comes from _openslide_tifflike_init_properties_and_hash
 *
 */


#ifndef CMAKE_BUILD
  #include <config.h>
#endif

#include "openslide-private.h"
#include "openslide-decode-jpeg.h"
#include "openslide-decode-jp2k.h"
#include "openslide-decode-png.h"
#include "openslide-decode-tiff.h"
#include "openslide-decode-tifflike.h"
#include "openslide-decode-gdkpixbuf.h"

#include <png.h>
#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <tiffio.h>
#include <setjmp.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

static const char ETS_EXT[] = ".ets";
static const char TIF_EXT[] = ".tif";
static const char VSI_EXT[] = ".vsi";
static const char SLIDEDATA_DIRNAME[] = "_%s_";
static const char ETSDATA_FILENAME[] = "frame_t.ets";
static const char TIFDATA_FILENAME[] = "frame_t.tif";
static const char SIS_MAGIC[4] = "SIS";
static const char ETS_MAGIC[4] = "ETS";

enum image_format {
  FORMAT_RAW,           // 0
  FORMAT_UNKNOWN,       // 1
  FORMAT_JPEG,          // 2
  FORMAT_JP2,           // 3
  FORMAT_UNKNOWN2,      // 4
  FORMAT_JPEG_LOSSLESS, // 5
  FORMAT_UNKNOWN3,      // 6
  FORMAT_UNKNOWN4,      // 7
  FORMAT_PNG,           // 8
  FORMAT_BMP,           // 9
};

struct sis_header {
  char magic[4]; // SIS0
  uint32_t headerSize;
  uint32_t version;
  uint32_t Ndim;
  uint64_t etsoffset;
  uint32_t etsnbytes;
  uint32_t dummy0; // reserved
  uint64_t offsettiles;
  uint32_t ntiles;
  uint32_t dummy1; // reserved
  uint32_t dummy2; // reserved
  uint32_t dummy3; // reserved
  uint32_t dummy4; // reserved
  uint32_t dummy5; // reserved
};

struct ets_header {
  char magic[4]; // ETS0
  uint32_t version;
  uint32_t pixelType;
  uint32_t sizeC;
  uint32_t colorspace;
  uint32_t compression;
  uint32_t quality;
  uint32_t dimx;
  uint32_t dimy;
  uint32_t dimz;
};

struct tile {
  uint32_t dummy1;
  uint32_t coord[3];
  uint32_t level;
  uint64_t offset;
  uint32_t numbytes;
  uint32_t dummy2;
};

struct load_state {
  int32_t w;
  int32_t h;
  GdkPixbuf *pixbuf;  // NULL until validated, then a borrowed ref
  GError *err;
};

struct level {
  struct _openslide_level base;
  struct _openslide_tiff_level tiffl;
  struct _openslide_grid *grid;

  enum image_format image_format;
  int32_t image_width;
  int32_t image_height;

  double tile_w;
  double tile_h;

  uint32_t current_lvl;
};

struct generic_tiff_ops_data {
  struct _openslide_tiffcache *tc;
};

struct png_error_ctx {
  jmp_buf env;
  GError *err;
};

enum slide_format {
  SLIDE_FMT_UNKNOWN,
  SLIDE_FMT_ETS,
  SLIDE_FMT_TIF
};

static enum slide_format _get_related_image_file(const char *filename, char **image_filename,
                                    GError **err) {
  // verify slidedat ETS or TIFF exists

  char *basename = g_path_get_basename(filename);
  basename = g_strndup(basename, strlen(basename) - strlen(VSI_EXT));

  char *dirname = g_strndup(filename, strlen(filename) - strlen(VSI_EXT) - strlen(basename));
  char *slidedat_dir = g_strdup_printf(SLIDEDATA_DIRNAME, basename);
  char *slidedat_path = g_build_filename(dirname, slidedat_dir, NULL);

  g_free(basename);
  g_free(dirname);
  g_free(slidedat_dir);

  // WORK IN PROGRESS

  // list all files in directory
  GDir *dir;
  const gchar *slide_dir;

  dir = g_dir_open(slidedat_path, 0, err);
  while ((slide_dir = g_dir_read_name(dir))) {

    // the stack1 directory stores always putative label images
    if (strncmp(slide_dir, "stack1", 6))
      continue;

    //char *etsdat_file = g_strdup_printf(ETSDATA_FILENAME, 0);
    //char *tifdat_file = g_strdup_printf(TIFDATA_FILENAME, 0);

    char *data_dir = g_build_filename(slidedat_path, slide_dir, NULL);

    char *etsdat_path = g_build_filename(data_dir, ETSDATA_FILENAME, NULL);
    char *tifdat_path = g_build_filename(data_dir, TIFDATA_FILENAME, NULL);

    bool ok_ets = g_file_test(etsdat_path, G_FILE_TEST_EXISTS);
    bool ok_tif = g_file_test(tifdat_path, G_FILE_TEST_EXISTS);

    g_free(data_dir);

    if (ok_ets) {
      *image_filename = etsdat_path;
      etsdat_path = NULL;
      g_free(slidedat_path);
      return SLIDE_FMT_ETS;
    } else if (ok_tif) {
      *image_filename = tifdat_path;
      tifdat_path = NULL;
      g_free(slidedat_path);
      return SLIDE_FMT_TIF;
    } else {
      g_free(slidedat_path);
      g_set_error(err, OPENSLIDE_ERROR, OPENSLIDE_ERROR_FAILED,
                  "Impossible to find related image file");
      return SLIDE_FMT_UNKNOWN;
    }
  }

  return SLIDE_FMT_UNKNOWN;
}


static bool olympus_ets_detect(const char *filename,
                               struct _openslide_tifflike *tl, GError **err) {
  // reject TIFFs
  if (tl) {
    g_set_error(err, OPENSLIDE_ERROR, OPENSLIDE_ERROR_FAILED,
                "Is a TIFF file");
    return false;
  }

  // verify filename
  if (!g_str_has_suffix(filename, ETS_EXT)) {
    g_set_error(err, OPENSLIDE_ERROR, OPENSLIDE_ERROR_FAILED,
                "File does not have %s extension", ETS_EXT);
    return false;
  }

  // verify existence
  if (!g_file_test(filename, G_FILE_TEST_EXISTS)) {
    g_set_error(err, OPENSLIDE_ERROR, OPENSLIDE_ERROR_FAILED,
                "File does not exist");
    return false;
  }

  return true;
}

static bool olympus_tif_detect(const char *filename,
                               struct _openslide_tifflike *tl, GError **err) {
  // reject TIFFs
  if (!tl) {
    g_set_error(err, OPENSLIDE_ERROR, OPENSLIDE_ERROR_FAILED,
                "Not a TIFF file");
    return false;
  }

  // ensure TIFF is tiled
  if (!_openslide_tifflike_is_tiled(tl, 0)) {
    g_set_error(err, OPENSLIDE_ERROR, OPENSLIDE_ERROR_FAILED,
                "TIFF is not tiled");
    return false;
  }

  // verify filename
  if (!g_str_has_suffix(filename, TIF_EXT)) {
    g_set_error(err, OPENSLIDE_ERROR, OPENSLIDE_ERROR_FAILED,
                "File does not have %s extension", TIF_EXT);
    return false;
  }

  // verify existence
  if (!g_file_test(filename, G_FILE_TEST_EXISTS)) {
    g_set_error(err, OPENSLIDE_ERROR, OPENSLIDE_ERROR_FAILED,
                "File does not exist");
    return false;
  }

  return true;
}

static bool olympus_vsi_detect(const char *filename G_GNUC_UNUSED,
                               struct _openslide_tifflike *tl, GError **err) {
#ifndef DEBUG
  // disable warnings
  TIFFSetWarningHandler(NULL);
#endif

  // reject TIFFs
  if (!tl) {
    g_set_error(err, OPENSLIDE_ERROR, OPENSLIDE_ERROR_FAILED,
                "Not a TIFF file");
    return false;
  }

  // ensure TIFF is not tiled
  if (_openslide_tifflike_is_tiled(tl, 0)) {
    g_set_error(err, OPENSLIDE_ERROR, OPENSLIDE_ERROR_FAILED,
                "TIFF is tiled");
    return false;
  }

  // verify filename
  if (!g_str_has_suffix(filename, VSI_EXT)) {
    g_set_error(err, OPENSLIDE_ERROR, OPENSLIDE_ERROR_FAILED,
                "File does not have %s extension", VSI_EXT);
    return false;
  }

  // verify existence
  if (!g_file_test(filename, G_FILE_TEST_EXISTS)) {
    g_set_error(err, OPENSLIDE_ERROR, OPENSLIDE_ERROR_FAILED,
                "File does not exist");
    return false;
  }

  // verify slidedat ETS or TIFF exists
  char *slidedat_file = NULL;
  enum slide_format fmt = _get_related_image_file(filename, &slidedat_file, err);

  switch (fmt) {
    case SLIDE_FMT_ETS: {
      GError *tmp_err = NULL;
      struct _openslide_tifflike *tl_tif = _openslide_tifflike_create(slidedat_file, &tmp_err);
      bool ok_ets = olympus_ets_detect(slidedat_file, tl_tif, err);
      _openslide_tifflike_destroy(tl_tif);
      //g_free(slidedat_file);
      return ok_ets;
    } break;
    case SLIDE_FMT_TIF: {
      GError *tmp_err = NULL;
      struct _openslide_tifflike *tl_tif = _openslide_tifflike_create(slidedat_file, &tmp_err);
      bool ok_tif = olympus_tif_detect(slidedat_file, tl_tif, err);
      _openslide_tifflike_destroy(tl_tif);
      //g_free(slidedat_file);
      return ok_tif;
    } break;
    default: {
      g_set_error(err, OPENSLIDE_ERROR, OPENSLIDE_ERROR_FAILED,
                 "Corresponding slidedat file does not exist");
      return false;
    } break;
  }

  return true;
}

static bool sis_header_read(struct sis_header * self, FILE * stream, GError **err) {
  int __attribute__((unused)) check = 0;
  check = fread(self->magic, 1, sizeof(self->magic), stream);
  g_assert((strncmp( self->magic, SIS_MAGIC, 4 ) == 0));
  check = fread((char*)&self->headerSize, 1, sizeof(self->headerSize), stream);
  g_assert((self->headerSize == 64)); // size of struct
  check = fread((char*)&self->version, 1, sizeof(self->version), stream);
  //g_assert((self->version == 2)); // version ??
  check = fread((char*)&self->Ndim, 1, sizeof(self->Ndim), stream);
  g_assert(((self->Ndim == 4) || (self->Ndim == 6))); // dim ?
  check = fread((char*)&self->etsoffset, 1, sizeof(self->etsoffset), stream);
  g_assert((self->etsoffset == 64)); // offset of ETS struct
  check = fread((char*)&self->etsnbytes, 1, sizeof(self->etsnbytes), stream);
  g_assert((self->etsnbytes == 228)); // size of ETS struct
  check = fread((char*)&self->dummy0, 1, sizeof(self->dummy0), stream);
  g_assert((self->dummy0 == 0)); // ??
  check = fread((char*)&self->offsettiles, 1, sizeof(self->offsettiles), stream); // offset to tiles
  check = fread((char*)&self->ntiles, 1, sizeof(self->ntiles), stream); // number of tiles
  check = fread((char*)&self->dummy1, 1, sizeof(self->dummy1), stream); // ??
  g_assert((self->dummy1 == 0)); // always zero ?
  check = fread((char*)&self->dummy2, 1, sizeof(self->dummy2), stream); // some kind of offset ?
  //g_assert((dummy2 == 0)); // not always
  check = fread((char*)&self->dummy3, 1, sizeof(self->dummy3), stream);
  g_assert((self->dummy3 == 0)); // always zero ?
  check = fread((char*)&self->dummy4, 1, sizeof(self->dummy4), stream);
  //g_assert((dummy4 == 0)); // not always
  check = fread((char*)&self->dummy5, 1, sizeof(self->dummy5), stream);
  g_assert((self->dummy5 == 0)); // always zero ?

  return true;
}

static bool ets_header_read(struct ets_header * self, FILE * stream, GError **err) {
  int __attribute__((unused)) check = 0;
  check = fread(self->magic, 1, sizeof(self->magic), stream);
  g_assert((strncmp( self->magic, ETS_MAGIC, 4 ) == 0));
  check = fread((char*)&self->version, 1, sizeof(self->version), stream);
  //g_assert((self->version == 0x30001 || self->version == 0x30003)); // some kind of version ?
  check = fread((char*)&self->pixelType, 1, sizeof(self->pixelType), stream);
  g_assert(((self->pixelType == 2) || (self->pixelType == 4) /* when sis_header->dim == 4 */));
  check = fread((char*)&self->sizeC, 1, sizeof(self->sizeC), stream);
  g_assert(((self->sizeC == 3) || (self->sizeC == 1)));
  check = fread((char*)&self->colorspace, 1, sizeof(self->colorspace), stream);
  g_assert(((self->colorspace == 4) || (self->colorspace == 1)));
  check = fread((char*)&self->compression, 1, sizeof(self->compression), stream); // codec
  g_assert(((self->compression == FORMAT_JPEG) || (self->compression == FORMAT_JP2)));
  check = fread((char*)&self->quality, 1, sizeof(self->quality), stream );
  //g_assert( self->quality == 90 || self->quality == 100 ); // some kind of JPEG quality ?
  check = fread((char*)&self->dimx, 1, sizeof(self->dimx), stream );
  //g_assert((self->dimx == 512)); // always tile of 512x512 ?
  check = fread((char*)&self->dimy, 1, sizeof(self->dimy), stream );
  //g_assert((self->dimy == 512)); //
  check = fread((char*)&self->dimz, 1, sizeof(self->dimz), stream );
  g_assert((self->dimz == 1) ); // dimz ?

  return true;
}

static void tile_read(struct tile * self, FILE * stream) {
  int __attribute__((unused)) check = 0;

  check = fread((char*)&self->dummy1, 1, sizeof(self->dummy1), stream);
  check = fread((char*)self->coord, 1, sizeof(self->coord), stream);
  check = fread((char*)&self->level, 1, sizeof(self->level), stream);
  check = fread((char*)&self->offset, 1, sizeof(self->offset), stream);
  check = fread((char*)&self->numbytes, 1, sizeof(self->numbytes), stream);
  check = fread((char*)&self->dummy2, 1, sizeof(self->dummy2), stream);
}

static struct tile *findtile(struct tile *tiles, uint32_t ntiles, uint32_t coord[3], uint32_t lvl) {
  //const struct tile *ret = NULL;
  for (uint32_t n = 0; n < ntiles; ++n) {
    struct tile * t = tiles + n;
    if( (t->level == lvl) && (t->coord[0] == coord[0] && t->coord[1] == coord[1]) ) {
      return t;
    }
  }
  return NULL;
}

struct olympus_ops_data {
  struct tile *tiles;
  const gchar *datafile_path;
  int32_t num_tiles;
};

//bool _openslide_png_decode_buffer(const void *buf, uint32_t len,
//                                  uint32_t *dest,
//                                  int32_t w, int32_t h,
//                                  GError **err) {
//}


//static void area_prepared(GdkPixbufLoader *loader, void *data) {
//  struct load_state *state = data;
//
//  if (state->err) {
//    return;
//  }
//
//  GdkPixbuf *pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
//
//  // validate image parameters
//  // when adding RGBA support, note that gdk-pixbuf does not
//  // premultiply alpha
//  if (gdk_pixbuf_get_colorspace(pixbuf) != GDK_COLORSPACE_RGB ||
//      gdk_pixbuf_get_bits_per_sample(pixbuf) != 8 ||
//      gdk_pixbuf_get_has_alpha(pixbuf) ||
//      gdk_pixbuf_get_n_channels(pixbuf) != 3) {
//    g_set_error(&state->err, OPENSLIDE_ERROR, OPENSLIDE_ERROR_FAILED,
//                "Unsupported pixbuf parameters");
//    return;
//  }
//  int w = gdk_pixbuf_get_width(pixbuf);
//  int h = gdk_pixbuf_get_height(pixbuf);
//  if (w != state->w || h != state->h) {
//    g_set_error(&state->err, OPENSLIDE_ERROR, OPENSLIDE_ERROR_FAILED,
//                "Dimensional mismatch reading pixbuf: "
//                "expected %dx%d, found %dx%d", state->w, state->h, w, h);
//    return;
//  }
//
//  // commit
//  state->pixbuf = pixbuf;
//}
//
//bool _openslide_gdkpixbuf_decode_buffer(const void *buf, uint32_t len,
//                                        uint32_t *dest,
//                                        int32_t w, int32_t h,
//                                        GError **err) {
//  GdkPixbufLoader *loader = NULL;
//  bool success = false;
//  struct load_state state = {
//    .w = w,
//    .h = h,
//  };
//  // create loader
//  loader = gdk_pixbuf_loader_new_with_type("bmp", err);
//  g_signal_connect(loader, "area-prepared", G_CALLBACK(area_prepared), &state);
//
//  if (!gdk_pixbuf_loader_write(loader, buf, len, err)) {
//    g_prefix_error(err, "gdk-pixbuf error: ");
//    goto DONE;
//  }
//  if (state.err) {
//    goto DONE;
//  }
//
//  // finish load
//  if (!gdk_pixbuf_loader_close(loader, err)) {
//    g_prefix_error(err, "gdk-pixbuf error: ");
//    goto DONE;
//  }
//  if (state.err) {
//    goto DONE;
//  }
//  g_assert(state.pixbuf);
//
//  uint8_t *pixels = gdk_pixbuf_get_pixels(state.pixbuf);
//  int rowstride = gdk_pixbuf_get_rowstride(state.pixbuf);
//  // decode bits
//  for (int32_t y = 0; y < h; y++) {
//    for (int32_t x = 0; x < w; x++) {
//      dest[y * w + x] = 0xFF000000 |                              // A
//                        pixels[y * rowstride + x * 3 + 0] << 16 | // R
//                        pixels[y * rowstride + x * 3 + 1] << 8 |  // G
//                        pixels[y * rowstride + x * 3 + 2];        // B
//    }
//  }
//
//  success = true;
//
//DONE:
//  // clean up
//  if (loader) {
//    gdk_pixbuf_loader_close(loader, NULL);
//    g_object_unref(loader);
//  }
//
//  // now that the loader is closed, we know state.err won't be set
//  // behind our back
//  if (state.err) {
//    // signal handler validation errors override GdkPixbuf errors
//    g_clear_error(err);
//    g_propagate_error(err, state.err);
//    // signal handler errors should have been noticed before falling through
//    g_assert(!success);
//  }
//  return success;
//}

static uint32_t *read_ets_image(openslide_t *osr,
                                struct tile * t,
                                enum image_format format,
                                int w, int h,
                                GError **err
                                ) {
  struct olympus_ops_data *data = osr->data;
  const gchar *filename = data->datafile_path;

  //int32_t num_tiles = data->num_tiles;

  uint32_t *dest = g_slice_alloc(w * h * 4);

  FILE *f = _openslide_fopen(filename, "rb", err);

  bool result = false;

  // read buffer of data
  fseeko(f, t->offset, SEEK_SET);
  int32_t buflen = t->numbytes / sizeof(uint8_t);

  uint8_t * buffer = g_slice_alloc(buflen);
  int32_t check = fread(buffer, 1, t->numbytes, f);
  if (!check)
    goto FAIL;

  switch (format) {
  case FORMAT_JPEG:
    result = _openslide_jpeg_decode_buffer(buffer, buflen,
                                           dest,
                                           w, h,
                                           err);
    break;
  case FORMAT_JP2:
    result = _openslide_jp2k_decode_buffer(dest,
                                           w, h,
                                           buffer, buflen,
                                           OPENSLIDE_JP2K_RGB,
                                           err);
    break;
//  case FORMAT_PNG:
//    result = _openslide_png_decode_buffer(buffer, buflen,
//                                          dest,
//                                          w, h,
//                                          err);
//    break;
//  case FORMAT_BMP:
//    result = _openslide_gdkpixbuf_decode_buffer(buffer, buflen,
//                                                dest,
//                                                w, h,
//                                                err);
//    break;
  default:
    g_assert_not_reached();
  }

  fclose(f);

  if (!result)
    goto FAIL;

  return dest;

FAIL:

  g_slice_free1(w * h * 4, dest);
  return NULL;
}


static bool read_ets_tile(openslide_t *osr,
                          cairo_t *cr,
                          struct _openslide_level *level,
                          int64_t tile_col, int64_t tile_row, // i.e TileIdx_x, TileIdx_y
                          void *arg G_GNUC_UNUSED,
                          GError **err) {
  struct level *l = (struct level *) level;
  struct olympus_ops_data *data = osr->data;
  struct tile *tiles = data->tiles;

  uint32_t ref[] = {tile_col, tile_row, 0};
  struct tile *t = findtile(tiles, data->num_tiles, ref, l->current_lvl);
  bool success = true;

  int iw = l->image_width; // Tilew
  int ih = l->image_height; // Tileh

  // get the image data, possibly from cache
  struct _openslide_cache_entry *cache_entry;
  uint32_t *tiledata = _openslide_cache_get(osr->cache,
                                            level, tile_col, tile_row,
                                            &cache_entry);

  if (!tiledata) {
    tiledata = read_ets_image(osr, t, l->image_format, iw, ih, err);

    if (tiledata == NULL) {
      return false;
    }

    _openslide_cache_put(osr->cache,
                         level, tile_col, tile_row,
                         tiledata,
                         iw * ih * 4,
                         &cache_entry);
  }

  // draw it
  cairo_surface_t *surface = cairo_image_surface_create_for_data((unsigned char *) tiledata,
                                                                 CAIRO_FORMAT_RGB24,
                                                                 iw, ih,
                                                                 iw * 4);

  // if we are drawing a subregion of the tile, we must do an additional copy,
  // because cairo lacks source clipping
  if ((l->image_width > l->tile_w) ||
      (l->image_height > l->tile_h)) {
    cairo_surface_t *surface2 = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                                           ceil(l->tile_w),
                                                           ceil(l->tile_h));
    cairo_t *cr2 = cairo_create(surface2);
    cairo_set_source_surface(cr2, surface, t->coord[0], t->coord[1]);

    // replace original image surface
    cairo_surface_destroy(surface);
    surface = surface2;

    cairo_rectangle(cr2, 0, 0,
                    ceil(l->tile_w),
                    ceil(l->tile_h));
    cairo_fill(cr2);
    success = _openslide_check_cairo_status(cr2, err);
    cairo_destroy(cr2);
  }

  cairo_set_source_surface(cr, surface, 0, 0);
  cairo_surface_destroy(surface);
  cairo_paint(cr);

  // done with the cache entry, release it
  _openslide_cache_entry_unref(cache_entry);

  return success;
}

static bool paint_ets_region(openslide_t *osr G_GNUC_UNUSED, cairo_t *cr,
                             int64_t x, int64_t y,
                             struct _openslide_level *level,
                             int32_t w, int32_t h,
                             GError **err) {
  struct level *l = (struct level *) level;

  return _openslide_grid_paint_region(l->grid, cr, NULL,
                                      x / level->downsample,
                                      y / level->downsample,
                                      level, w, h,
                                      err);
}

static void destroy_ets(openslide_t *osr) {
  // each level in turn
  for (int32_t i = 0; i < osr->level_count; i++) {
    struct level *l = (struct level *) osr->levels[i];
    _openslide_grid_destroy(l->grid);
    g_slice_free(struct level, l);
  }

  // the level array
  g_free(osr->levels);
}

static const struct _openslide_ops olympus_ets_ops = {
  .paint_region = paint_ets_region,
  .destroy = destroy_ets,
};

static bool olympus_open_ets(openslide_t *osr, const char *filename,
                             struct _openslide_tifflike *tl,
                             struct _openslide_hash *quickhash1, GError **err) {

  struct tile *tiles = NULL;
  struct level **levels = NULL;
  uint32_t *tilexmax = NULL;
  uint32_t *tileymax = NULL;

  // open file
  FILE *f = _openslide_fopen(filename, "rb", err);
  if (!f) {
    goto FAIL;
  }

  // SIS:
  struct sis_header *sh = g_slice_new0(struct sis_header);
  if (!sis_header_read( sh, f, err )) {
    g_set_error(err, OPENSLIDE_ERROR, OPENSLIDE_ERROR_FAILED,
                "Errors in SIS header");
    goto FAIL;
  }

  // ETS:
  struct ets_header *eh = g_slice_new0(struct ets_header);
  if (!ets_header_read( eh, f, err )) {
    g_set_error(err, OPENSLIDE_ERROR, OPENSLIDE_ERROR_FAILED,
                "Errors in ETS header");
    goto FAIL;
  }

  // individual tiles
  if (fseeko(f, sh->offsettiles, SEEK_SET)) {
    _openslide_io_error(err, "Couldn't seek to JPEG start");
    goto FAIL;
  }
  int32_t level_count = 1;

  // computes tiles dims
  tiles = g_new0(struct tile, sh->ntiles);
  for (int i = 0; i < sh->ntiles; ++i) {
    struct tile t;
    // read tile images
    tile_read(&t, f);
    tiles[i] = t;

    level_count = t.level > level_count ? t.level : level_count;
  }

  // close the input file
  fclose(f);

  tilexmax = g_new0(uint32_t, level_count);
  tileymax = g_new0(uint32_t, level_count);

  for (int i = 0; i < sh->ntiles; ++i) {
    struct tile t = tiles[i];
    uint32_t lvl = t.level;

    tilexmax[lvl] = t.coord[0] > tilexmax[lvl] ? t.coord[0] : tilexmax[lvl];
    tileymax[lvl] = t.coord[1] > tileymax[lvl] ? t.coord[1] : tileymax[lvl];
  }

  levels = g_new0(struct level *, level_count);

  for (int i = 0; i < level_count; ++i) {
    struct level *l = g_slice_new0(struct level);

    // compute image info:
    uint32_t image_width = eh->dimx*(tilexmax[i] + 1);
    uint32_t image_height = eh->dimy*(tileymax[i] + 1);

    // TODO: Up to now it works ONLY for image without z-stack!
    g_assert( eh->dimz == 1 );

    uint32_t tile_w = eh->dimx;
    uint32_t tile_h = eh->dimy;

    uint32_t tile_across = (image_width + tile_w - 1) / tile_w;
    uint32_t tile_down = (image_height + tile_h - 1) / tile_h;
    //uint32_t tiles_per_image = tile_across * tile_down;

    levels[i] = l;

    l->tile_w = (double)tile_w;
    l->tile_h = (double)tile_h;
    l->base.w = (double)image_width;
    l->base.h = (double)image_height;
    l->image_format = eh->compression;
    l->image_width = eh->dimx;
    l->image_height = eh->dimy;
    l->current_lvl = i;

    //l->base.downsample = pow(2., i);

    l->grid = _openslide_grid_create_simple(osr,
                                            tile_across, tile_down,
                                            tile_w, tile_h,
                                            read_ets_tile);
  }

  g_free(tilexmax);
  g_free(tileymax);

  _openslide_set_bounds_props_from_grid(osr, levels[0]->grid);

  osr->level_count = level_count;
  g_assert(osr->levels == NULL);
  osr->levels = (struct _openslide_level **) levels;
  levels = NULL;

  // set private data
  g_assert(osr->data == NULL);
  struct olympus_ops_data *data = g_slice_new0(struct olympus_ops_data);
  data->tiles = tiles;
  data->num_tiles = sh->ntiles;
  data->datafile_path = filename;

  tiles = NULL;
  osr->data = data;

  osr->ops = &olympus_ets_ops;

  return true;

FAIL:
  // free
  if (f) {
    fclose(f);
  }

  if (levels != NULL) {
    for (int i = 0; i < level_count; ++i) {
      struct level *l = levels[i];
      if (l) {
        _openslide_grid_destroy(l->grid);
        g_slice_free(struct level, l);
      }
    }
    g_free(levels);
  }

  g_free(tiles);
  g_free(tilexmax);
  g_free(tileymax);

  return false;
}

static int width_compare(gconstpointer a, gconstpointer b) {
  const struct level *la = *(const struct level **) a;
  const struct level *lb = *(const struct level **) b;

  if (la->tiffl.image_w > lb->tiffl.image_w) {
    return -1;
  } else if (la->tiffl.image_w == lb->tiffl.image_w) {
    return 0;
  } else {
    return 1;
  }
}

static void destroy_tif(openslide_t *osr) {
  struct generic_tiff_ops_data *data = osr->data;
  _openslide_tiffcache_destroy(data->tc);
  g_slice_free(struct generic_tiff_ops_data, data);

  for (int32_t i = 0; i < osr->level_count; i++) {
    struct level *l = (struct level *) osr->levels[i];
    _openslide_grid_destroy(l->grid);
    g_slice_free(struct level, l);
  }
  g_free(osr->levels);
}

static bool read_tif_tile(openslide_t *osr,
                          cairo_t *cr,
                          struct _openslide_level *level,
                          int64_t tile_col, int64_t tile_row,
                          void *arg,
                          GError **err) {
  struct level *l = (struct level *) level;
  struct _openslide_tiff_level *tiffl = &l->tiffl;
  TIFF *tiff = arg;

  // tile size
  int64_t tw = tiffl->tile_w;
  int64_t th = tiffl->tile_h;

  // cache
  struct _openslide_cache_entry *cache_entry;
  uint32_t *tiledata = _openslide_cache_get(osr->cache,
                                            level, tile_col, tile_row,
                                            &cache_entry);
  if (!tiledata) {
    tiledata = g_slice_alloc(tw * th * 4);
    if (!_openslide_tiff_read_tile(tiffl, tiff,
                                   tiledata, tile_col, tile_row,
                                   err)) {
      g_slice_free1(tw * th * 4, tiledata);
      return false;
    }

    // clip, if necessary
    if (!_openslide_tiff_clip_tile(tiffl, tiledata,
                                   tile_col, tile_row,
                                   err)) {
      g_slice_free1(tw * th * 4, tiledata);
      return false;
    }

    // put it in the cache
    _openslide_cache_put(osr->cache, level, tile_col, tile_row,
                         tiledata, tw * th * 4,
                         &cache_entry);
  }

  // draw it
  cairo_surface_t *surface = cairo_image_surface_create_for_data((unsigned char *) tiledata,
                                                                 CAIRO_FORMAT_ARGB32,
                                                                 tw, th,
                                                                 tw * 4);
  cairo_set_source_surface(cr, surface, 0, 0);
  cairo_surface_destroy(surface);
  cairo_paint(cr);

  // done with the cache entry, release it
  _openslide_cache_entry_unref(cache_entry);

  return true;
}

static bool paint_tif_region(openslide_t *osr, cairo_t *cr,
                             int64_t x, int64_t y,
                             struct _openslide_level *level,
                             int32_t w, int32_t h,
                             GError **err) {
  struct generic_tiff_ops_data *data = osr->data;
  struct level *l = (struct level *) level;

  TIFF *tiff = _openslide_tiffcache_get(data->tc, err);
  if (tiff == NULL) {
    return false;
  }

  bool success = _openslide_grid_paint_region(l->grid, cr, tiff,
                                              x / l->base.downsample,
                                              y / l->base.downsample,
                                              level, w, h,
                                              err);
  _openslide_tiffcache_put(data->tc, tiff);

  return success;
}

static const struct _openslide_ops generic_tiff_ops = {
  .paint_region = paint_tif_region,
  .destroy = destroy_tif,
};


static bool olympus_open_tif(openslide_t *osr, const char *filename,
                             struct _openslide_tifflike *tl,
                             struct _openslide_hash *quickhash1, GError **err) {

  GPtrArray *level_array = g_ptr_array_new();

  // open TIFF
  struct _openslide_tiffcache *tc = _openslide_tiffcache_create(filename);
  TIFF *tiff = _openslide_tiffcache_get(tc, err);
  if (!tiff) {
    goto FAIL;
  }

  // accumulate tiled levels
  do {
    // confirm that this directory is tiled
    if (!TIFFIsTiled(tiff)) {
      continue;
    }

    // confirm it is either the first image, or reduced-resolution
    if (TIFFCurrentDirectory(tiff) != 0) {
      uint32_t subfiletype;
      if (!TIFFGetField(tiff, TIFFTAG_SUBFILETYPE, &subfiletype)) {
        continue;
      }

      if (!(subfiletype & FILETYPE_REDUCEDIMAGE)) {
        continue;
      }
    }

    // verify that we can read this compression (hard fail if not)
    uint16_t compression;
    if (!TIFFGetField(tiff, TIFFTAG_COMPRESSION, &compression)) {
      g_set_error(err, OPENSLIDE_ERROR, OPENSLIDE_ERROR_FAILED,
                  "Can't read compression scheme");
      goto FAIL;
    };
    if (!TIFFIsCODECConfigured(compression)) {
      g_set_error(err, OPENSLIDE_ERROR, OPENSLIDE_ERROR_FAILED,
                  "Unsupported TIFF compression: %u", compression);
      goto FAIL;
    }

    // create level
    struct level *l = g_slice_new0(struct level);
    struct _openslide_tiff_level *tiffl = &l->tiffl;
    if (!_openslide_tiff_level_init(tiff,
                                    TIFFCurrentDirectory(tiff),
                                    (struct _openslide_level *) l,
                                    tiffl,
                                    err)) {
      g_slice_free(struct level, l);
      goto FAIL;
    }
    l->grid = _openslide_grid_create_simple(osr,
                                            tiffl->tiles_across,
                                            tiffl->tiles_down,
                                            tiffl->tile_w,
                                            tiffl->tile_h,
                                            read_tif_tile);

    // add to array
    g_ptr_array_add(level_array, l);
  } while (TIFFReadDirectory(tiff));

  // sort tiled levels
  g_ptr_array_sort(level_array, width_compare);

  // set hash and properties
  struct level *top_level = level_array->pdata[level_array->len - 1];
  if (!_openslide_tifflike_init_properties_and_hash(osr, tl, quickhash1,
                                                    top_level->tiffl.dir,
                                                    0,
                                                    err)) {
    goto FAIL;
  }

  // unwrap level array
  int32_t level_count = level_array->len;
  struct level **levels =
    (struct level **) g_ptr_array_free(level_array, false);
  level_array = NULL;

  // allocate private data
  struct generic_tiff_ops_data *data =
    g_slice_new0(struct generic_tiff_ops_data);

  // store osr data
  g_assert(osr->data == NULL);
  g_assert(osr->levels == NULL);
  osr->levels = (struct _openslide_level **) levels;
  osr->level_count = level_count;
  osr->data = data;
  osr->ops = &generic_tiff_ops;

  // put TIFF handle and store tiffcache reference
  _openslide_tiffcache_put(tc, tiff);
  data->tc = tc;

  return true;

 FAIL:
  // free the level array
  if (level_array) {
    for (uint32_t n = 0; n < level_array->len; n++) {
      struct level *l = level_array->pdata[n];
      _openslide_grid_destroy(l->grid);
      g_slice_free(struct level, l);
    }
    g_ptr_array_free(level_array, true);
  }
  // free TIFF
  _openslide_tiffcache_put(tc, tiff);
  _openslide_tiffcache_destroy(tc);
  return false;
}


static void set_resolution_prop(openslide_t *osr, TIFF *tiff,
                                const char *property_name,
                                ttag_t tag) {
  float f;
  uint16_t unit;

  if (TIFFGetFieldDefaulted(tiff, TIFFTAG_RESOLUTIONUNIT, &unit) &&
      TIFFGetField(tiff, tag, &f)) {
    if (unit == RESUNIT_CENTIMETER) {
      g_hash_table_insert(osr->properties, g_strdup(property_name),
                          _openslide_format_double(10000.0 / f));
    }
    else if (unit == RESUNIT_INCH) {
      g_hash_table_insert(osr->properties, g_strdup(property_name),
                          _openslide_format_double(25400.0 / f)); // TODO: correct according to inches
    }
  }
}

static bool olympus_open_vsi(openslide_t *osr, const char *filename,
                         struct _openslide_tifflike *tl,
                         struct _openslide_hash *quickhash1, GError **err) {
  bool success = true;

  struct _openslide_tiffcache *tc = _openslide_tiffcache_create(filename);
  TIFF *tiff = _openslide_tiffcache_get(tc, err);
  if (!tiff) {
    goto FAIL;
  }

  uint16_t compression;
  if (!TIFFGetField(tiff, TIFFTAG_COMPRESSION, &compression)) {
    g_set_error(err, OPENSLIDE_ERROR, OPENSLIDE_ERROR_FAILED,
                "Can't read compression scheme");
    goto FAIL;
  };
  if (!TIFFIsCODECConfigured(compression)) {
    g_set_error(err, OPENSLIDE_ERROR, OPENSLIDE_ERROR_FAILED,
                "Unsupported TIFF compression: %u", compression);
    goto FAIL;
  }

  if (!_openslide_tifflike_init_properties_and_hash(osr, tl, quickhash1,
                                                    0, 0,
                                                    err)) {
    goto FAIL;
  }

  // keep the XML document out of the properties
  // (in case pyramid level 0 is also directory 0)
  g_hash_table_remove(osr->properties, OPENSLIDE_PROPERTY_NAME_COMMENT);
  g_hash_table_remove(osr->properties, "tiff.ImageDescription");

  // set MPP properties
  if (!_openslide_tiff_set_dir(tiff, 0, err)) {
    goto FAIL;
  }
  set_resolution_prop(osr, tiff, OPENSLIDE_PROPERTY_NAME_MPP_X,
                      TIFFTAG_XRESOLUTION);
  set_resolution_prop(osr, tiff, OPENSLIDE_PROPERTY_NAME_MPP_Y,
                      TIFFTAG_YRESOLUTION);

  if (!_openslide_tiff_add_associated_image(osr, "label", tc,
                                            1, err)) {
    goto FAIL;
  }

  // verify slidedat ETS or TIFF exists
  char *slidedat_file = NULL;
  enum slide_format fmt = _get_related_image_file(filename, &slidedat_file, err);

  switch (fmt) {
    case SLIDE_FMT_ETS: {
      success = olympus_open_ets(osr, slidedat_file, tl, quickhash1, err);
    } break;
    case SLIDE_FMT_TIF: {
      success = olympus_open_tif(osr, slidedat_file, tl, quickhash1, err);
    } break;
    default: {
      g_set_error(err, OPENSLIDE_ERROR, OPENSLIDE_ERROR_FAILED,
                 "Corresponding slidedat file does not exist");
      goto FAIL;
    } break;
  }

  //g_free(slidedat_file);

  _openslide_tiffcache_put(tc, tiff);

  return success;

FAIL:

  success = false;

  return success;
}

const struct _openslide_format _openslide_format_olympus = {
  .name = "olympus-vsi",
  .vendor = "olympus",
  .detect = olympus_vsi_detect,
  .open = olympus_open_vsi,
};
