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


#include "argb2rgba.h"

void argb2rgba(unsigned char *buf, unsigned int len) {

  unsigned int cur;
  unsigned char a, r, g, b;

  for (cur = 0; cur < len; cur += 4) {
    a = buf[cur + CA];
    r = buf[cur + CR];
    g = buf[cur + CG];
    b = buf[cur + CB];

    if (a != 0 && a != 255) {
      r = r * 255 / a;
      g = g * 255 / a;
      b = b * 255 / a;
    }

    buf[cur + 0] = r;
    buf[cur + 1] = g;
    buf[cur + 2] = b;
    buf[cur + 3] = a;
  }
}
