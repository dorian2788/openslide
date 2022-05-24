#ifdef __EMSCRIPTEN__

#include <iostream>
#include <emscripten/bind.h>

#include "openslide-private.h"
#include "deepzoom-private.h"


class Openslide {

private:

  openslide_t * osr;

public:

  Openslide () {
  }

  void open (const std :: string & filename) {
    this->osr = openslide_open(filename.c_str());
  }

  void close () {
    if (this->osr)
      openslide_close (this->osr);
  }

  int get_level_width (int32_t level)  {
    if (this->osr) {
      int64_t w, h;
      openslide_get_level_dimensions(this->osr, level, &w, &h);
      return w;
    } else {
      return -1;
    }
  }

  int get_level_height (int32_t level) {
    if (this->osr) {
      int64_t w, h;
      openslide_get_level_dimensions(this->osr, level, &w, &h);
      return h;
    } else {
      return -1;
    }
  }

  int get_level_count () {
    if (this->osr) {
      return openslide_get_level_count(this->osr);
    } else {
      return -1;
    }
  }

  int get_plane_count () {
    if (this->osr) {
      return openslide_get_plane_count(this->osr);
    } else {
      return -1;
    }
  }

  double get_best_level_for_downsample (int32_t level) {
    if (this->osr) {
      return openslide_get_best_level_for_downsample(this->osr, level);
    } else {
      return -1.;
    }
  }

  std :: string get_error () {
    return openslide_get_error(this->osr);
  }

  std :: string get_property_value (const std :: string & name) {
    if (this->osr) {
      return openslide_get_property_value(this->osr, name.c_str());
    } else {
      return "";
    }
  }

  std :: string detect_vendor (const std :: string & filename) {
    return openslide_detect_vendor(filename.c_str());
  }

};


/**** Openslide emscripten :: functions ****/

EMSCRIPTEN_BINDINGS(openslide) {

  emscripten :: class_ < Openslide >("Openslide")
    .constructor <> ()
    .function("open", &Openslide::open)
    .function("close", &Openslide::close)
    .function("get_level_width", &Openslide::get_level_width)
    .function("get_level_height", &Openslide::get_level_height)
    .function("get_level_count", &Openslide::get_level_count)
    .function("get_plane_count", &Openslide::get_plane_count)
    .function("get_best_level_for_downsample", &Openslide::get_best_level_for_downsample)
    .function("get_error", &Openslide::get_error)
    .function("get_property_value", &Openslide::get_property_value)
    .function("detect_vendor", &Openslide::detect_vendor)
    ;
}


#endif // __EMSCRIPTEN__
