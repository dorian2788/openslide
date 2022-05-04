# distutils: language = c++
# cython: language_level=2

from cython.operator cimport dereference as deref
cimport numpy as np

from openslide cimport openslide_detect_vendor
from openslide cimport openslide_read_region
from openslide cimport openslide_get_level_count
from openslide cimport argb2rgba

import os
import numpy as np

from libcpp.string cimport string
from libc.stdlib cimport malloc

from PIL import Image
from io import BytesIO
from xml.etree.ElementTree import Element
from xml.etree.ElementTree import ElementTree
from xml.etree.ElementTree import SubElement

__author__  = ['Nico Curti']
__email__ = ['nico.curti2@unibo.it']

__all__ = ['DeepZoom', 'DeepZoomError']

BRIGHTFIELD = 0
FLUORESCENCE = 1


class DeepZoomError (Exception):
  '''
  An error produced by the DeepZoom library.

  This exception is raised if something goes wrong in the
  Openslide functions wrapped by Cython.
  The error raised is set to 1.
  '''

  def __init__ (self, message : str, errors : int=1):

    super(DeepZoomError, self).__init__(message)

    self.errors = errors


cdef class DeepZoom:
  '''
  Generates Deep Zoom tiles and metadata, creating a
  DeepZoom generator wrapping an OpenSlide object.

  Parameters
  ----------
  filename : str
    The filename to open. On Windows, this must be in UTF-8.

  dtype : int (default := BRIGHTFIELD)
    Type of WSI. Possible values are BRIGHTFIELD, DAPI, etc.

  tile_size : int (defaul := 254)
    The width and height of a single tile.  For best viewer
    performance, tile_size + 2 * overlap should be a power
    of two.

  overlap : int (default := 1)
    The number of extra pixels to add to each interior edge
    of a tile.

  limit_bounds : bool (default := False)
    True to render only the non-empty slide region.

  '''

  def __init__ (self, str filename=None, int dtype=BRIGHTFIELD, int tile_size=254, int overlap=1, bool limit_bounds=0):

    self._plane = dtype

    if filename is not None:

      self.open(filename, tile_size, overlap, limit_bounds)

  def open (self, str filename, int tile_size, int overlap, bool limit_bounds):
    '''
    Open a whole slide image.

    This function can be expensive; avoid calling it unnecessarily.  For
    example, a tile server should not call openslide_open() on every tile
    request.  Instead, it should maintain a cache of OpenSlide objects and
    reuse them when possible.

    Parameters
    ----------
      filename: str
        The filename to open.  On Windows, this must be in UTF-8.

    Returns
    -------
      self

    Notes
    -----
    .. note::
      On success, a new OpenSlide object.
      If the file is not recognized by OpenSlide, a ValueError exception is raised.
      If the file is recognized but an error occurred, an OpenSlide
      object in error state.
    '''

    if not isinstance(filename, str) and not isinstance(filename, bytes):
      raise TypeError('{} must be in string format'.format(filename))

    if not os.path.isfile(filename):
      raise FileNotFoundError('Could not open or find the data file. Given: {}'.format(filename))

    filename_bytes = filename.encode('utf-8') if isinstance(filename, str) else filename

    if self._openslide_can_open(filename_bytes):

      self.thisptr = deepzoom_open(filename_bytes, tile_size, overlap, limit_bounds)

    else:

      raise ValueError('The current file cannot be opened with Openslide library')

    return self

  def _openslide_can_open (self, bytes filename_bytes) -> bool:
    '''
    Quickly determine whether a whole slide image is recognized.

    If OpenSlide recognizes the file referenced by filename, return a
    string identifying the slide format vendor.  This is equivalent to the
    value of the #OPENSLIDE_PROPERTY_NAME_VENDOR property.  Calling
    openslide_open() on this file will return a valid OpenSlide object or
    an OpenSlide object in error state.

    Otherwise, return NULL.  Calling openslide_open() on this file will also
    return NULL.

    Parameters
    ----------
      filename_bytes : bytes
        The filename to check.  On Windows, this must be in UTF-8.

    Returns
    -------
      check : bool
        If openslide_open() will succeed.

    Notes
    -----
    .. warning::
      This function uses the openslide_detect_vendor() to efficiently check whether
      a slide file is recognized by OpenSlide, since the original openslide_can_open
      is deprecated.
    '''
    cdef const char * vendor = openslide_detect_vendor(filename_bytes)

    if vendor is NULL:
      raise DeepZoomError('Openslide does not support the given image format. '
                          'Given: {}'.format(filename_bytes.decode('utf-8')))

    return True

  def __enter__ (self):
    '''
    Context manager for file reading
    '''

    return self

  def __exit__ (self, type, value, traceback):
    '''
    Context manager (exit) for file reading

    Parameters
    ----------
      type :
      value :
      traceback :
    '''
    deepzoom_close(self.thisptr)

  @property
  def get_level_count (self) -> int:
    '''
    Get the number of levels in the deepzoom image.

    Returns
    -------
      levels : int
        The number of levels, or -1 if an error occurred.
    '''
    return deepzoom_get_level_count(self.thisptr)

  @property
  def get_plane_count (self) -> int:
    '''
    Get the number of planes in the whole slide image.

    Returns
    -------
      planes : int
        The number of planes, or -1 if an error occurred.
    '''
    return deepzoom_get_plane_count(self.thisptr)

  @property
  def get_tile_count (self) -> int:
    '''
    Get the number of tiles in the deepzoom image.

    Returns
    -------
      tiles : int
        The number of tiles.
    '''
    return deepzoom_get_tile_count(self.thisptr)

  def get_level_tiles (self) -> tuple:
    '''
    A list of (tiles_x, tiles_y) tuples for each Deep Zoom level.
    '''
    lvl = self.get_level_count
    cdef const dimensions_t *d = deepzoom_get_level_tiles(self.thisptr)
    return tuple((d[i].x, d[i].y) for i in range(lvl))

  def get_level_dimensions (self) -> tuple:
    '''
    A list of (pixels_x, pixels_y) tuples for each Deep Zoom level.
    '''
    lvl = deref(self.thisptr).dz_levels
    cdef const dimensions_t *d = deepzoom_get_level_dimensions(self.thisptr)
    return tuple((d[i].x, d[i].y) for i in range(lvl))

  def get_l0_dimensions (self) -> tuple:
    '''
    '''
    cdef long int [1] w
    cdef long int [1] h

    deepzoom_get_l0_dimensions(self.thisptr, w, h)

    return (w[0], h[0])

  def get_micron_per_pixel (self) -> tuple:
    '''
    '''
    cdef double [1] mppx
    cdef double [1] mppy

    deepzoom_get_micron_per_pixel(self.thisptr, mppx, mppy)

    return (mppx[0], mppy[0])

  def _read_brightfield_region(self, int level, long int x, long int y, long int w, long int h, long int sw, long int sh):
    '''
    '''

    cdef unsigned int * tile = <unsigned int *> malloc(w * h * sizeof(unsigned int))
    openslide_read_region(deref(self.thisptr).osr, tile, x, y, 0, level, w, h)

    cdef unsigned char * u8 = <unsigned char*>tile;
    argb2rgba(u8, w * h * 4)
    wsi = np.asarray(<np.uint32_t[: h * w]> tile)

    wsi = wsi.view(dtype=np.uint8).reshape(h, w, 4)

    wsi = Image.fromarray(wsi)
    ### TODO: add real bg color
    bg = Image.new('RGB', wsi.size, '#ffffff')
    wsi = Image.composite(wsi, bg, wsi)

    # scale to the correct size
    if wsi.size != (sw, sh):
      wsi.thumbnail((sw, sh), Image.ANTIALIAS)

    return wsi

  def _read_fluorescence_region(self, int level, long int x, long int y, long int plane, long int w, long int h, long int sw, long int sh):
    '''
    '''

    cdef unsigned int * tile = <unsigned int *> malloc(w * h * sizeof(unsigned int))
    openslide_read_region(deref(self.thisptr).osr, tile, x, y, plane, level, w, h)

    wsi = np.asarray(<np.uint32_t[: h * w]> tile)
    wsi = wsi.reshape(h, w)

    wsi = Image.fromarray(wsi)

    # scale to the correct size
    if wsi.size != (sw, sh):
      wsi.thumbnail((sw, sh), Image.ANTIALIAS)

    return wsi

  def get_tile (self, int level, int w, int h):
    '''
    '''
    cdef long int [1] x
    cdef long int [1] y
    cdef int [1] lvl
    cdef long int [1] outw
    cdef long int [1] outh
    cdef long int [1] sw
    cdef long int [1] sh

    deepzoom_get_tile_info(self.thisptr, level, w, h, x, y, lvl, outw, outh, sw, sh)

    if (outw[0], outh[0]) == (-1, -1) or (sw[0], sh[0]) == (-1, -1):
      raise ValueError('Incorrect region read.')

    if self._plane == BRIGHTFIELD:
      return self._read_brightfield_region(lvl[0], x[0], y[0], outw[0], outh[0], sw[0], sh[0])

    elif self._plane == FLUORESCENCE:
      planes = self.get_plane_count
      images = []
      for i in range(planes):
        img = self._read_fluorescence_region(lvl[0], x[0], y[0], i, outw[0], outh[0], sw[0], sh[0])
        images.append(img)
      return images

  def get_dzi (self, str fmt) -> str:
    '''
    '''
    tile_size = deref(self.thisptr).z_t_downsample
    overlap = deref(self.thisptr).z_overlap

    image = Element('Image',
                    TileSize=str(tile_size),
                    Overlap=str(overlap),
                    Format=fmt,
                    xmlns='http://schemas.microsoft.com/deepzoom/2008',
                    )
    w, h = self.get_l0_dimensions()

    SubElement(image, 'Size', Width=str(w), Height=str(h))

    tree = ElementTree(element=image)

    buffer = BytesIO()
    tree.write(buffer, encoding='utf-8')

    return buffer.getvalue().decode('utf-8')

  def _get_property (self, string name) -> str:
    '''
    Wrap for the properties getter
    '''

    cdef const char * value = deepzoom_get_property_value(self.thisptr, name.c_str())
    return value.decode('utf-8') if value is not NULL else 'Not Available'

  @property
  def properties (self) -> dict:
    '''
    Get the properties of the WSI image as dict.

    Certain vendor-specific metadata properties may exist
    within a whole slide image. They are encoded as key-value
    pairs.

    Returns
    -------
      properties : dict
        Dictionary of properties found in the WSI image

    Notes
    -----
    .. warning::
      If no properties are found the default value is an empty string.
    '''

    names = {'COMMENT'          : b'openslide.comment',
             'VENDOR'           : b'openslide.vendor',
             'LEVEL_COUNT'      : b'openslide.level-count',
             'QUICKHASH1'       : b'openslide.quickhash-1',
             'BACKGROUND_COLOR' : b'openslide.background-color',
             'OBJECTIVE_POWER'  : b'openslide.objective-power',
             'MPP_X'            : b'openslide.mpp-x',
             'MPP_Y'            : b'openslide.mpp-y',
             'BOUNDS_X'         : b'openslide.bounds-x',
             'BOUNDS_Y'         : b'openslide.bounds-y',
             'BOUNDS_WIDTH'     : b'openslide.bounds-width',
             'BOUNDS_HEIGHT'    : b'openslide.bounds-height',
    }

    cdef int lvl = openslide_get_level_count(deref(self.thisptr).osr)

    for i in range(lvl):
      names['level {:d} - width'.format(i)] = 'openslide.level[{:d}].width'.format(i).encode('utf-8')
      names['level {:d} - height'.format(i)] = 'openslide.level[{:d}].height'.format(i).encode('utf-8')
      names['level {:d} - downsample'.format(i)] = 'openslide.level[{:d}].downsample'.format(i).encode('utf-8')
      names['level {:d} - tile-width'.format(i)] = 'openslide.level[{:d}].tile-width'.format(i).encode('utf-8')
      names['level {:d} - tile-height'.format(i)] = 'openslide.level[{:d}].tile-height'.format(i).encode('utf-8')
      names['region {:d} - x'.format(i)] = 'openslide.region[{:d}].x'.format(i).encode('utf-8')
      names['region {:d} - y'.format(i)] = 'openslide.region[{:d}].y'.format(i).encode('utf-8')
      names['region {:d} - width'.format(i)] = 'openslide.region[{:d}].width'.format(i).encode('utf-8')
      names['region {:d} - height'.format(i)] = 'openslide.region[{:d}].height'.format(i).encode('utf-8')

    properties = {name : self._get_property(code) for name, code in names.items()}

    return properties


  def __repr__(self):

    tile_size = deref(self.thisptr).z_t_downsample
    overlap = deref(self.thisptr).z_overlap
    limit_bounds = deref(self.thisptr).limit_bounds

    return '{}(tile_size={:d}, overlap={:d}, limit_bounds={:d})'.format(
        self.__class__.__name__,
        tile_size,
        overlap,
        limit_bounds,
    )
