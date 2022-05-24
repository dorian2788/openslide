# distutils: language = c++
# cython: language_level=2

import sys
from numpy cimport int64_t

cdef extern from "openslide.h":

  cppclass openslide_t


  openslide_t * openslide_open(const char * filename);
  int openslide_get_level_count(openslide_t * osr);
  int openslide_get_plane_count(openslide_t * osr);
  void openslide_get_level_dimensions(openslide_t * osr, int level, int64_t * w, int64_t * h);
  double openslide_get_level_downsample(openslide_t * osr, int level);
  int openslide_get_best_level_for_downsample(openslide_t * osr, double downsample);
  void openslide_read_region(openslide_t * osr, unsigned int * dest, int64_t x, int64_t y, int64_t plane, int level, int64_t w, int64_t h);
  void openslide_close(openslide_t * osr);
  const char * openslide_get_error(openslide_t * osr);

  const char * openslide_get_property_value(openslide_t * osr, const char * name);

  const char * openslide_detect_vendor(const char * filename);

  void openslide_get_associated_image_dimensions(openslide_t * osr, const char * name, int64_t * w, int64_t * h);
  void openslide_read_associated_image(openslide_t * osr, const char * name, unsigned int * dest);


cdef extern from "argb2rgba.h":

  void argb2rgba (unsigned char * buf, unsigned int len);

cdef class Openslide:

  cdef openslide_t * thisptr
  cdef int _level
  cdef int _plane
