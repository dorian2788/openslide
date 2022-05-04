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

/**
 * @file deepzoom.h
 * The API for the OpenSlide DeepZoom library.
 *
 * All functions except openslide_close() are thread-safe.
 * See the openslide_close() documentation for its restrictions.
 */

#ifndef OPENSLIDE_DEEPZOOM_H_
#define OPENSLIDE_DEEPZOOM_H_


#include "deepzoom-private.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * The main DeepZoom type.
 */
typedef struct _deepzoom deepzoom_t;

/**
 * An utility type.
 */
typedef struct _dimensions dimensions_t;


/**
 * @name Basic Usage
 * Opening, reading, and closing.
 */
//@{


/**
 * Open a whole slide image via DeepZoom approach.
 *
 * This function can be expensive; avoid calling it unnecessarily.
 *
 * @param filename The filename to open.  On Windows, this must be in UTF-8.
 * @param tile_size The width and height of a single tile.  For best viewer
                    performance, tile_size + 2 * overlap should be a power
                    of two.
 * @param overlap The number of extra pixels to add to each interior edge
                  of a tile.
 * @param limit_bounds True to render only the non-empty slide region.
 * @return
 *         On success, a new DeepZoom object.
 *         If the file is not recognized by OpenSlide, NULL.
 *         If the file is recognized but an error occurred, a DeepZoom
 *         object in error state.
 */
OPENSLIDE_PUBLIC()
deepzoom_t *deepzoom_open(const char *filename,
                          int32_t tile_size, int32_t overlap,
                          bool limit_bounds);


/**
 * Close a DeepZoom object.
 * No other threads may be using the object.
 * After this call returns, the object cannot be used anymore.
 *
 * @param dpz The DeepZoom object.
 */
OPENSLIDE_PUBLIC()
void deepzoom_close(deepzoom_t *dpz);


/**
 * Get DeepZoom level count.
 * DeepZoom levels should be different from Openslide levels.
 * They are evaluated as a progressive 2x division from the level 0.
 *
 * @param dpz The DeepZoom object.
 * @return
 *        Count of DeepZoom level evaluated.
 *
 */
OPENSLIDE_PUBLIC()
const int32_t deepzoom_get_level_count(deepzoom_t *dpz);


/**
 * Get DeepZoom plane count.
 * DeepZoom planes are the same of Openslide planes.
 *
 * @param dpz The DeepZoom object.
 * @return
 *        Count of DeepZoom planes evaluated.
 *
 */
OPENSLIDE_PUBLIC()
const int32_t deepzoom_get_plane_count(deepzoom_t *dpz);


/**
 * A list of (tiles_x, tiles_y) tuples for each Deep Zoom level.
 *
 * @param dpz The DeepZoom object.
 * @return
 *         Pointer to pair objects {.x, .y}
 *         NULL if something goes wrong
 */
OPENSLIDE_PUBLIC()
const dimensions_t* deepzoom_get_level_tiles(deepzoom_t *dpz);


/**
 * A list of (pixels_x, pixels_y) tuples for each Deep Zoom level.
 *
 * @param dpz The DeepZoom object.
 * @return
 *         Pointer to pair objects {.x, .y}
 *         NULL if something goes wrong
 */
OPENSLIDE_PUBLIC()
const dimensions_t* deepzoom_get_level_dimensions(deepzoom_t *dpz);


/**
 * The total number of Deep Zoom tiles in the image.
 *
 * @param dpz The DeepZoom object.
 * @return
 *         The sum of w x h of each dimension.
 */
OPENSLIDE_PUBLIC()
const int64_t deepzoom_get_tile_count(deepzoom_t *dpz);


/**
 * Get micron per pixel conversion
 *
 * @param dpz The DeepZoom object.
 * @param[out] mppx Micron per pixel scale along width.
 * @param[out] mppy Micron per pixel scale along height.
 */
OPENSLIDE_PUBLIC()
void deepzoom_get_micron_per_pixel(deepzoom_t *dpz, double *mppx, double *mppy);

/**
 * Get the dimension of level 0.
 *
 * @param dpz The DeepZoom object.
 * @param[out] w The width of the deepzoom, or -1 if an error occurred.
 * @param[out] h The height of the deepzoom, or -1 if an error occurred.
 */
OPENSLIDE_PUBLIC()
void deepzoom_get_l0_dimensions(deepzoom_t *dpz, int64_t *w, int64_t *h);

/**
 * Get the NULL-terminated array of property names.
 *
 * Certain vendor-specific metadata properties may exist
 * within a whole slide image. They are encoded as key-value
 * pairs. This call provides a list of names as strings
 * that can be used to read properties with openslide_get_property_value().
 *
 * @param dpz The DeepZoom object.
 * @return A NULL-terminated string array of property names, or
 *         an empty array if an error occurred.
 */
OPENSLIDE_PUBLIC()
const char * const *deepzoom_get_property_names(deepzoom_t *dpz);


/**
 * Get the value of a single property.
 *
 * Certain vendor-specific metadata properties may exist
 * within a whole slide image. They are encoded as key-value
 * pairs. This call provides the value of the property given
 * by @p name.
 *
 * @param dpz The DeepZoom object.
 * @param name The name of the desired property. Must be
               a valid name as given by openslide_get_property_names().
 * @return The value of the named property, or NULL if the property
 *         doesn't exist or an error occurred.
 */
OPENSLIDE_PUBLIC()
const char *deepzoom_get_property_value(deepzoom_t *dpz, const char *name);


OPENSLIDE_PUBLIC()
void deepzoom_get_tile_info(deepzoom_t *dpz, int32_t level, int64_t w, int64_t h,
  int64_t *initx, int64_t *inity, int32_t *lvl,
  int64_t *outw, int64_t *outh, int64_t *scalew, int64_t *scaleh);

/**
 * Copy pre-multiplied ARGB data from a whole slide image.
 *
 * @param dpz The DeepZoom object.
 * @param[out] dest The destination buffer for the ARGB data.
 * @param plane Image plane to read (0 for brightfield; >= 0 for fluorescence).
 * @param level The desired level.
 * @param w The width of the region. Must be non-negative.
 * @param h The height of the region. Must be non-negative.
 * @param[out] outw The width of the read region.
 * @param[out] outh The height of the read region.
 * @param[out] scalew Tile width size for final scaling.
 * @param[out] scaleh Tile height size for final scaling.
 */
OPENSLIDE_PUBLIC()
void deepzoom_get_tile(deepzoom_t *dpz,
                       uint32_t *dest,
                       int64_t plane,
                       int32_t level,
                       int64_t w, int64_t h);


#ifdef __cplusplus
}
#endif

#endif // OPENSLIDE_DEEPZOOM_H_
