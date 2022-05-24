# distutils: language = c++
# cython: language_level=2

from libcpp cimport bool
from openslide cimport openslide_t
from numpy cimport int64_t

cdef extern from "deepzoom.h":

  cppclass dimensions_t:
    double x
    double y

  cppclass deepzoom_t:
    int z_t_downsample
    int z_overlap
    int dz_levels
    bool limit_bounds
    openslide_t *osr


  deepzoom_t * deepzoom_open(const char * filename, int tile_size, int overlap, int limit_bounds);
  void deepzoom_close(deepzoom_t * dpz);
  const int deepzoom_get_level_count(deepzoom_t * dpz);
  const int deepzoom_get_plane_count(deepzoom_t * osr);
  const dimensions_t * deepzoom_get_level_tiles(deepzoom_t * dpz);
  const dimensions_t * deepzoom_get_level_dimensions(deepzoom_t * dpz);
  void deepzoom_get_l0_dimensions(deepzoom_t *dpz, int64_t *w, int64_t *h)
  void deepzoom_get_micron_per_pixel(deepzoom_t *dpz, double *mppx, double *mppy)
  const int64_t deepzoom_get_tile_count(deepzoom_t * dpz);
  void deepzoom_get_tile_info(deepzoom_t * dpz, int level, int64_t w, int64_t h, int64_t *initx, int64_t *inity, int *lvl, int64_t *outw, int64_t *outh, int64_t *sw, int64_t *sh);
  void deepzoom_get_tile(deepzoom_t * dpz, int64_t plane, int level, int64_t w, int64_t h);

  const char * const *deepzoom_get_property_names(deepzoom_t *dpz);
  const char *deepzoom_get_property_value(deepzoom_t *dpz, const char *name)

cdef class DeepZoom:

  cdef deepzoom_t * thisptr
  cdef int _plane
  