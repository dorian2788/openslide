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

#ifndef OPENSLIDE_DEEPZOOM_PRIVATE_H_
#define OPENSLIDE_DEEPZOOM_PRIVATE_H_

#include "openslide.h"
#include <glib.h>


/* a simple pair structure */
struct _dimensions {
  double x;
  double y;
};


/* the main structure */
struct _deepzoom {
  // openslide object
  openslide_t *osr;

  // tile size
  int32_t z_t_downsample;
  // number of extra pixels to add to each interior edge of a tile
  int32_t z_overlap;
  // True to render only the non-empty slide region
  bool limit_bounds;

  // dimensions of active areas
  struct _dimensions *l_dimensions;
  // level 0 coordinate offset
  struct _dimensions l0_offset;

  // number of levels in osr
  int32_t dz_levels;

  // level tiles
  struct _dimensions *t_dimensions;
  // level _dimensions
  struct _dimensions *z_dimensions;

  // Preferred slide levels for each Deep Zoom level
  int32_t *slide_from_dz_level;

  // Piecewise downsamples level 0
  double *l0_l_downsamples;
  // Piecewise downsamples
  double *l_z_downsamples;

  // Slide background color
  char *bg_color;

  // error handling, NULL if no error
  gpointer error; // must use g_atomic_pointer!
};



#endif // OPENSLIDE_DEEPZOOM_PRIVATE_H_
