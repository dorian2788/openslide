/*
 *  OpenSlide, a library for reading whole slide image files
 *
 *  Copyright (c) 2007-2014 Carnegie Mellon University
 *  Copyright (c) 2021      Benjamin Gilbert
 *  Copyright (c) 2022      Nico Curti
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

#include "deepzoom.h"
#include <math.h>


static deepzoom_t *create_deepzoom(void) {

  deepzoom_t *dpz = g_slice_new0(deepzoom_t);

  return dpz;
}


int32_t *deepzoom_slide_from_dz_level_eval(deepzoom_t *dpz) {
  int32_t *slide_from_dz_level = NULL;

  if (dpz->dz_levels == 0) {
    goto FAIL;
  }

  slide_from_dz_level = g_new0(int32_t, dpz->dz_levels);

  for (int32_t i = 0; i < dpz->dz_levels; ++i) {
    double downsample = pow(2., dpz->dz_levels - i - 1);
    slide_from_dz_level[i] = openslide_get_best_level_for_downsample(dpz->osr, downsample);
  }

  return slide_from_dz_level;

FAIL:
  return NULL;
}


dimensions_t *deepzoom_level_dimensions_eval(deepzoom_t *dpz) {
  dimensions_t *z_dimensions = NULL;

  if (dpz->dz_levels == 0) {
    goto FAIL;
  }

  z_dimensions = g_new0(dimensions_t, dpz->dz_levels);

  int64_t w, h;
  openslide_get_level0_dimensions(dpz->osr, &w, &h);

  for (int32_t i = dpz->dz_levels - 1; i >= 0; --i) {
    dimensions_t d = {.x = w, .y = h};
    z_dimensions[i] = d;

    w = fmax(1, ceil(w * .5));
    h = fmax(1, ceil(h * .5));
  }

  return z_dimensions;

FAIL:
  return NULL;
}


dimensions_t *deepzoom_level_tiles_eval(deepzoom_t *dpz) {
  dimensions_t *t_dimension = NULL;

  if (dpz->dz_levels == 0) {
    goto FAIL;
  }
  else if (!dpz->z_dimensions) {
    goto FAIL;
  }

  t_dimension = g_new0(dimensions_t, dpz->dz_levels);

  for (int32_t i = 0; i < dpz->dz_levels; ++i) {
    dimensions_t zd = dpz->z_dimensions[i];

    dimensions_t d = {
      .x = ceil(zd.x / dpz->z_t_downsample),
      .y = ceil(zd.y / dpz->z_t_downsample)
    };

    t_dimension[i] = d;
  }

  return t_dimension;

FAIL:
  return NULL;
}


void _deepzoom_propagate_error(deepzoom_t *dpz, GError *err) {
  g_return_if_fail(err);
  gchar *msg = g_strdup(err->message);
  if (!g_atomic_pointer_compare_and_exchange(&dpz->error, NULL, msg)) {
    // didn't replace the error, free it
    g_free(msg);
  }
  g_error_free(err);
}


deepzoom_t *deepzoom_open(const char *filename, int32_t tile_size, int32_t overlap, bool limit_bounds) {

  GError *tmp_err = NULL;

  // alloc memory
  deepzoom_t *dpz = create_deepzoom();

  dpz->osr = openslide_open(filename);

  if (openslide_get_error(dpz->osr)) {
    // failed to read slide
    _deepzoom_propagate_error(dpz, tmp_err);
    return dpz;
  }

  dpz->z_t_downsample = tile_size;
  dpz->z_overlap = overlap;
  dpz->limit_bounds = limit_bounds;

  int32_t levels = openslide_get_level_count(dpz->osr);

  dpz->l_dimensions = g_new0(dimensions_t, levels);
  dpz->l0_l_downsamples = g_new0(double, levels);

  // Precompute dimensions
  // slide level and offset
  if (limit_bounds) {

    const char *px = openslide_get_property_value(dpz->osr, OPENSLIDE_PROPERTY_NAME_BOUNDS_X);
    const char *py = openslide_get_property_value(dpz->osr, OPENSLIDE_PROPERTY_NAME_BOUNDS_Y);

    // Level 0 coordinate offset
    dpz->l0_offset.x = px ? strtod(px, NULL) : 0;
    dpz->l0_offset.y = py ? strtod(py, NULL) : 0;

    const char *pw = openslide_get_property_value(dpz->osr, OPENSLIDE_PROPERTY_NAME_BOUNDS_WIDTH);
    const char *ph = openslide_get_property_value(dpz->osr, OPENSLIDE_PROPERTY_NAME_BOUNDS_HEIGHT);

    int64_t w, h;
    openslide_get_level_dimensions(dpz->osr, 0, &w, &h);

    // Slide level dimensions scale factor in each axis
    dimensions_t s = {
      .x = pw ? strtod(pw, NULL) / w : 1,
      .y = ph ? strtod(ph, NULL) / h : 1
    };

    for (int32_t i = 0; i < levels; ++i) {

      openslide_get_level_dimensions(dpz->osr, i, &w, &h);

      dimensions_t d = {.x = ceil(w * s.x), .y = ceil(h * s.y)};

      dpz->l_dimensions[i] = d;

      dpz->l0_l_downsamples[i] = openslide_get_level_downsample(dpz->osr, i);
    }

  } else {

    int64_t w, h;

    for (int32_t i = 0; i < levels; ++i) {
      openslide_get_level_dimensions(dpz->osr, i, &w, &h);

      // Slide level dimensions scale factor in each axis
      dimensions_t d = {.x = w, .y = h};
      dpz->l_dimensions[i] = d;

      dpz->l0_l_downsamples[i] = openslide_get_level_downsample(dpz->osr, i);
    }

    dpz->l0_offset.x = 0; dpz->l0_offset.y = 0;
  }

  // Number of levels in dpz
  dpz->dz_levels = deepzoom_get_level_count(dpz);

  // DeepZoom level dimensions
  dpz->z_dimensions = deepzoom_level_dimensions_eval(dpz);
  // Tiles dimensions
  dpz->t_dimensions = deepzoom_level_tiles_eval(dpz);

  // Preferred slide levels for each Deep Zoom level
  dpz->slide_from_dz_level = deepzoom_slide_from_dz_level_eval(dpz);

  // Piecewise downsamples
  dpz->l_z_downsamples = g_new0(double, dpz->dz_levels);

  for (int32_t i = 0; i < dpz->dz_levels; ++i) {
    double l0_z_downsample = pow(2., dpz->dz_levels - i - 1);
    int32_t idx = dpz->slide_from_dz_level[i];
    double l0_l_downsample = dpz->l0_l_downsamples[idx];
    dpz->l_z_downsamples[i] = l0_z_downsample / l0_l_downsample;
  }

//  dpz->bg_color = openslide_get_property_value(dpz->osr, OPENSLIDE_PROPERTY_NAME_BACKGROUND_COLOR);
//
//  if (dpz->bg_color) {
//    dpz->bg_color = "ffffff";
//  }

  // TODO: prepend '#' at the beginning

  return dpz;
}


void deepzoom_close(deepzoom_t *dpz) {

  openslide_close(dpz->osr);

  g_free(dpz->l_dimensions);
  g_free(dpz->z_dimensions);
  g_free(dpz->slide_from_dz_level);
  g_free(dpz->l0_l_downsamples);
  g_free(dpz->l_z_downsamples);
  g_free(dpz->bg_color);

  g_free(g_atomic_pointer_get(&dpz->error));
}


const int32_t deepzoom_get_level_count(deepzoom_t *dpz) {

  int64_t w, h;
  openslide_get_level0_dimensions(dpz->osr, &w, &h);

  int32_t count = 1;

  while (w > 1 || h > 1) {
    w = fmax(1, ceil(w * .5));
    h = fmax(1, ceil(h * .5));
    ++count;
  }

  return count;
}


const int32_t deepzoom_get_plane_count(deepzoom_t *dpz) {
  return openslide_get_plane_count(dpz->osr);
}


const dimensions_t* deepzoom_get_level_tiles(deepzoom_t *dpz) {
  return dpz->t_dimensions;
}


const dimensions_t* deepzoom_get_level_dimensions(deepzoom_t *dpz) {
  return dpz->z_dimensions;
}


const int64_t deepzoom_get_tile_count(deepzoom_t *dpz) {
  int64_t s = 0;

  for (int32_t i = 0; i < dpz->dz_levels; ++i) {
    dimensions_t d = dpz->t_dimensions[i];
    s += d.x * d.y;
  }

  return s;
}


void deepzoom_get_micron_per_pixel(deepzoom_t *dpz, double *mppx, double *mppy)
{
  const char * mpp_x = openslide_get_property_value(dpz->osr, "openslide.mpp-x");
  const char * mpp_y = openslide_get_property_value(dpz->osr, "openslide.mpp-y");

  if (mpp_x && mpp_y) {
    *mppx = strtod(mpp_x, NULL);
    *mppy = strtod(mpp_y, NULL);
  } else {
    *mppx = 0;
    *mppy = 0;
  }
}


void deepzoom_get_l0_dimensions(deepzoom_t *dpz, int64_t *w, int64_t *h) {
  *w = dpz->l_dimensions[0].x;
  *h = dpz->l_dimensions[0].y;
}


const double deepzoom_l0_from_l(deepzoom_t *dpz, int32_t level, int32_t l) {
  return dpz->l0_l_downsamples[level] * l;
}


const double deepzoom_l_from_z(deepzoom_t *dpz, int32_t level, int32_t z) {
  return dpz->l_z_downsamples[level] * z;
}


const int32_t deepzoom_z_from_t(deepzoom_t *dpz, int32_t t) {
  return dpz->z_t_downsample * t;
}


void deepzoom_get_tile_info(deepzoom_t *dpz, int32_t level, int64_t w, int64_t h,
  int64_t *initx, int64_t *inity, int32_t *lvl,
  int64_t *outw, int64_t *outh, int64_t *scalew, int64_t *scaleh) {

  if (level < 0 || level >= dpz->dz_levels) {
    goto FAIL;
  }
  if (w < 0 || w >= dpz->t_dimensions[level].x) {
    goto FAIL;
  }
  if (h < 0 || h >= dpz->t_dimensions[level].y) {
    goto FAIL;
  }

  // Get preferred slide level
  int32_t slide_level = dpz->slide_from_dz_level[level];

  // Calculate top/left and bottom/right overla
  dimensions_t z_overlap_tl = {
    .x = dpz->z_overlap * w != 0,
    .y = dpz->z_overlap * h != 0
  };

  dimensions_t z_overlap_br = {
    .x = dpz->z_overlap * (w != dpz->t_dimensions[level].x - 1),
    .y = dpz->z_overlap * (h != dpz->t_dimensions[level].y - 1)
  };

  // Get final size of the tile
  dimensions_t z_size = {
    .x = fmin(dpz->z_t_downsample, dpz->z_dimensions[level].x - dpz->z_t_downsample * w) + z_overlap_tl.x + z_overlap_br.x,
    .y = fmin(dpz->z_t_downsample, dpz->z_dimensions[level].y - dpz->z_t_downsample * h) + z_overlap_tl.y + z_overlap_br.y
  };

  // Obtain the region coordinates
  dimensions_t z_location = {
    .x = deepzoom_z_from_t(dpz, w),
    .y = deepzoom_z_from_t(dpz, h),
  };

  dimensions_t l_location = {
    .x = deepzoom_l_from_z(dpz, level, z_location.x - z_overlap_tl.x),
    .y = deepzoom_l_from_z(dpz, level, z_location.y - z_overlap_tl.y),
  };

  // Round location down and size up, and add offset of active area
  dimensions_t l0_location = {
    .x = deepzoom_l0_from_l(dpz, slide_level, l_location.x) + dpz->l0_offset.x,
    .y = deepzoom_l0_from_l(dpz, slide_level, l_location.y) + dpz->l0_offset.y,
  };

  dimensions_t l_size = {
    .x = fmin(ceil(deepzoom_l_from_z(dpz, level, z_size.x)), dpz->l_dimensions[slide_level].x - ceil(l_location.x)),
    .y = fmin(ceil(deepzoom_l_from_z(dpz, level, z_size.y)), dpz->l_dimensions[slide_level].y - ceil(l_location.y))
  };

  *initx = l0_location.x;
  *inity = l0_location.y;

  *lvl = slide_level;

  *outw = l_size.x;
  *outh = l_size.y;

  *scalew = z_size.x;
  *scaleh = z_size.y;

  return;

FAIL:

  *initx = -1;
  *inity = -1;
  *lvl = -1;
  *outw = -1;
  *outh = -1;
  *scalew = -1;
  *scaleh = -1;
}


void deepzoom_get_tile(deepzoom_t *dpz, uint32_t *dest, int64_t plane, int32_t level, int64_t w, int64_t h) {

  int64_t x, y, outw, outh, sw, sh;
  int32_t lvl;

  deepzoom_get_tile_info(dpz, level, w, h, &x, &y, &lvl, &outw, &outh, &sw, &sh);

  if (outw == -1 || outh == -1) {
    goto FAIL;
  }

  dest = g_slice_alloc(outw * outh * 4);;

  // Return read_region() parameters plus tile size for final scaling
  openslide_read_region(dpz->osr, dest, x, y, plane, lvl, outw, outh);

  char *error = g_strdup(openslide_get_error(dpz->osr));
  if (error) {
    goto FAIL;
  }

  return;

FAIL:

  dest = NULL;
}

const char * const *deepzoom_get_property_names(deepzoom_t *dpz) {
  const char * const * properties = openslide_get_property_names(dpz->osr);
  return properties;
}

const char *deepzoom_get_property_value(deepzoom_t *dpz, const char *name) {

  return openslide_get_property_value(dpz->osr, name);
}
