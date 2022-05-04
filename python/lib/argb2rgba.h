/*
 *  OpenSlide, a library for reading whole slide image files
 *
 *  Copyright (c) 2022 Nico Curti
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

#ifndef OPENSLIDE_PYOPENSLIDE_H_
#define OPENSLIDE_PYOPENSLIDE_H_

#include "openslide-features.h"

#ifdef WORDS_BIGENDIAN
  #define CA 0
  #define CR 1
  #define CG 2
  #define CB 3
#else
  #define CB 0
  #define CG 1
  #define CR 2
  #define CA 3
#endif


#ifdef __cplusplus
extern "C" {
#endif


/**
 * Convert a buffer from ARGB to RGBA format using the correct endianess.
 *
 * @param[out] buf Buffer to process.
 * @param len Lenght of buffer.
 */
OPENSLIDE_PUBLIC()
void argb2rgba(unsigned char *buf, unsigned int len);


#ifdef __cplusplus
}
#endif

#endif // OPENSLIDE_PYOPENSLIDE_H_
