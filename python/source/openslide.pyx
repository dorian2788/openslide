# distutils: language = c++
# cython: language_level=2

from libc.stdlib cimport malloc, free
from libcpp.string cimport string
cimport numpy as np
from numpy cimport int64_t

import os
import numpy as np

__author__  = ['Nico Curti']
__email__ = ['nico.curti2@unibo.it']

__all__ = ['Openslide', 'OpenslideError',
           'detect_vendor',
           'BRIGHTFIELD', 'FLUORESCENCE']


BRIGHTFIELD = 0
FLUORESCENCE = 1


class OpenslideError (Exception):
  '''
  An error produced by the OpenSlide library.

  This exception is raised if something goes wrong in the
  Openslide functions wrapped by Cython.
  The error raised is set to 1.
  '''

  def __init__ (self, message : str, errors : int=1):

    super(OpenslideError, self).__init__(message)

    self.errors = errors

cdef class Openslide:

  '''
  Openslide class for WSI reading.

  The original library of Openslide [1]_ is a Ansi-C library
  for reading whole slide image files.
  The current wrap has been tested using a particular branch
  of the original library [2]_ which extends the building
  using CMake support.

  Parameters
  ----------
    filename : str
      The filename to open. On Windows, this must be in UTF-8.

    dtype : int
      Type of WSI. Possible values are BRIGHTFIELD, DAPI, etc.

  Notes
  -----
  The Openslide object tries to be as much as possible compatible
  with Numpy [3]_, providing a close user interface to the Numpy one.

  References
  ----------
  .. [1] Original Openslide library. https://github.com/openslide/openslide
  .. [2] CMake compatible branch of Openslide library. https://github.com/Nico-Curti/openslide
  .. [3] Numpy library. https://github.com/numpy/numpy
  '''

  cdef unsigned int* _dest

  def __init__ (self, str filename=None, int dtype=BRIGHTFIELD):

    self._level = 0
    self._plane = dtype

    if self._plane != BRIGHTFIELD and self._plane != FLUORESCENCE:
       raise OpenslideError('Invalid Acquisition type give.')

    if filename is not None:

      self.open(filename)

  def set_level (self, int level):
    '''
    Set the level for next processing

    Parameters
    ----------
      level : int
        The desired level.

    Returns
    -------
      self

    Notes
    -----
    .. note::
      The default level is set to 0. This function
      is usefull if you want to manage the WSI like any other
      numpy array and perform array-slicing on different resolution
      levels.
    '''
    self._level = level
    return self

  @property
  def level (self):
    '''
    Get the resolution level used

    Returns
    -------
      level : int
        The level set with the set_level function (default=0)
    '''
    return self._level

  @property
  def shape (self) -> tuple:
    '''
    Get the dimensions of a level.

    Returns
    -------
      w : int
        Width of the level image

      h : int
        Height of the level image

    Notes
    -----
    .. warning::
      If some errors occurred or the level was out of range,
      the width and height are set to -1 and a OpenslideError
      is raised.
    '''
    cdef int64_t [1] w
    cdef int64_t [1] h

    openslide_get_level_dimensions(self.thisptr, self._level, w, h)

    if (w[0] < 0 or h[0] < 0):
      raise OpenslideError('Incorrect level found. '
                           'The maximum number of levels is {:d}. '
                           'Given {:d}'.format(self.get_level_count, self._level))

    return (w[0], h[0])

  def get_associated_image_dimensions (self, string name):
    '''
    Get the dimensions of the associated image.

    Returns
    -------
      w : int
        Width of the level image

      h : int
        Height of the level image

    Notes
    -----
    .. warning::
      If some errors occurred or the level was out of range,
      the width and height are set to -1 and a OpenslideError
      is raised.
    '''
    cdef int64_t [1] w
    cdef int64_t [1] h

    openslide_get_associated_image_dimensions(self.thisptr, name.c_str(), w, h)

    if (w[0] < 0 or h[0] < 0):
      raise OpenslideError('Incorrect image found. '
                           'Given {:}'.format(name.decode('utf-8')))

    return (w[0], h[0])

  def get_downsample (self) -> float:
    '''
    Get the downsampling factor of the current level.

    Returns
    -------
      downsampling : float
        The downsampling factor for the current level, or -1.0 if an error occurred
        or the level was out of range.
    '''

    downsample = openslide_get_level_downsample(self.thisptr, self._level)

    if (downsample < 0):
      raise OpenslideError('Incorrect level found. '
                           'The maximum number of levels is {:d}. '
                           'Given {:d}'.format(self.get_level_count, self._level))

    return downsample

  @property
  def get_level_count (self) -> int:
    '''
    Get the number of levels in the whole slide image.

    Returns
    -------
      levels : int
        The number of levels, or -1 if an error occurred.
    '''
    return openslide_get_level_count(self.thisptr)

  @property
  def get_plane_count (self) -> int:
    '''
    Get the number of planes in the whole slide image.

    Returns
    -------
      planes : int
        The number of planes, or -1 if an error occurred.
    '''
    return openslide_get_plane_count(self.thisptr)

  def __array__ (self) -> np.ndarray:
    '''
    Compatibility with numpy array.
    In this way np.array(Openslide_obj) is a valid 3D array and you can
    also simply call plt.imshow(Openslide_obj) without other intermediate steps.

    Returns
    -------
      arr : array-like
        The WSI as numpy array using the level set

    Notes
    -----
    .. note::
      If you want a different level you have to call the
      set_level() member function with the desired level
    '''

    w, h = self.shape
    x, y = (0, 0)
    planes = self.get_plane_count

    img = [self.read_region(x, y, i, w, h) for i in range(planes)]

    return np.dstack(img)

  def __getitem__(self, item) -> np.ndarray:
    '''
    Compatibility with numpy array.
    Make the object subscriptable in the same way of standard NumPy array.

    Parameters
    ----------
      item : list or int or iterable
        Subscribe key

    Returns
    -------
      dst : array-like
        Subscripted array

    Notes
    -----
    This magic function is particularly useful for the color conversion:
    OpenCV image are stored in BGR fmt and to move to the common RGB color
    space can be used the syntax [..., ::-1]
    '''
    return self.__array__().__getitem__(item)

  def _get_error (self):
    '''
    Get the current error string.

    For a given OpenSlide object, once this function returns a non-NULL
    value, the only useful operation on the object is to call
    openslide_close() to free its resources.

    Returns
    -------
      self

    Notes
    -----
    .. warning::
      If an error occurred a OpenslideError is raised with
      a string describing the original error that caused
      the problem.
    '''

    cdef const char * error = openslide_get_error(self.thisptr)

    if error is not NULL:
      raise OpenslideError(error.decode('utf-8'))

    return self

  def _get_property (self, string name) -> str:
    '''
    Wrap for the properties getter
    '''

    cdef const char * value = openslide_get_property_value(self.thisptr, name.c_str())
    return value.decode('utf-8') if value is not NULL else None

  @property
  def header (self) -> dict:
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
      If no properties are found the default value is None.
    '''

    names = {'COMMENT'          : b'openslide.comment',
             'VENDOR'           : b'openslide.vendor',
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

    properties = {name : self._get_property(code) for name, code in names.items()}

    return properties

  def get_best_level_for_downsample (self, float downsample) -> int:
    '''
    Get the best level to use for displaying the given downsample.

    Parameters
    ----------
      downsample : float
        The downsample factor.

    Returns
    -------
      level : int
        The level identifier, or -1 if an error occurred.
    '''

    level = openslide_get_best_level_for_downsample(self.thisptr, downsample)

    if (level < 0):
      raise OpenslideError('Incorrect downsample found. '
                           'Given {:d}'.format(level))
    return level


  def _read_brightfield_region(self, int level, long int x, long int y, long int w, long int h) -> np.ndarray:
    cdef unsigned int * dest = <unsigned int *> malloc(w * h * sizeof(unsigned int))

    openslide_read_region(self.thisptr, dest, x, y, 0, level, w, h)

    if dest is NULL:
      raise OpenslideError('Incorrect region read. '
                           'The level image has shape: ({:d}, {:d}). '
                           'The request shape is: ({:d}, {:d})'.format(*self.shape, w - x, h - y))

    cdef unsigned char * u8 = <unsigned char*>dest;
    argb2rgba(u8, w * h * 4)

    wsi = np.asarray(<np.uint32_t[: h * w]> dest)
    #free(dest)

    # convert to a readable image
    wsi = wsi.view(dtype=np.uint8).reshape(h, w, 4)
    self._dest = dest

    return wsi

  def free_pointer_pls(self):
    free(self._dest)
    return

  def _read_fluorescence_region(self, int level, long int x, long int y, long int plane, long int w, long int h) -> np.ndarray:
    cdef unsigned int * dest = <unsigned int *> malloc(w * h * sizeof(unsigned int))

    openslide_read_region(self.thisptr, dest, x, y, plane, level, w, h)

    if dest is NULL:
      raise OpenslideError('Incorrect region read. '
                           'The level image has shape: ({:d}, {:d}). '
                           'The request shape is: ({:d}, {:d})'.format(*self.shape, w - x, h - y))

    wsi = np.asarray(<np.uint32_t[: h * w]> dest)
    # free(dest)
    wsi = wsi.reshape(h, w)
    self._dest = dest

    return wsi

  def read_region (self, long int x, long int y, long int plane, long int w, long int h) -> np.ndarray:
    '''
    Copy pre-multiplied ARGB data from a whole slide image.

    This function reads and decompresses a region of a whole slide
    image into the specified memory location.

    Parameters
    ----------
      x : int
        The top left x-coordinate, in the level 0 reference frame.

      y : int
        The top left y-coordinate, in the level 0 reference frame.

      plane : int
        Image plane to read (0 for brightfield; >= 0 for fluorescence).

      w : int
        The width of the region. Must be non-negative.

      h : int
        The height of the region. Must be non-negative.

    Returns
    -------
      arr : numpy-array
        The destination buffer for the ARGB data.

    Notes
    -----
    .. note::
      The destination buffer stores the value as unsigned int32 values.

    .. warning::
      If an error occurs or has occurred an OpenslideError is raised
    '''

    if self._plane == BRIGHTFIELD:
      return self._read_brightfield_region(self._level, x, y, w, h)

    elif self._plane == FLUORESCENCE:
      return self._read_fluorescence_region(self._level, x, y, plane, w, h)

  def read_associated_image (self, str name):
    '''
    Copy pre-multiplied ARGB data from an associated image.

    This function reads and decompresses the required associated
    image.

    Parameters
    ----------
      name : str
        The name of the desired associated image.

    Returns
    -------
      arr : numpy-array
        The destination buffer for the ARGB data.

    Notes
    -----
    .. note::
      The destination buffer stores the value as unsigned int32 values.

    .. warning::
      If an error occurs or has occurred an OpenslideError is raised
    '''
    w, h = self.get_associated_image_dimensions(name.encode('utf-8'))

    cdef unsigned int * dest = <unsigned int *> malloc(w * h * sizeof(unsigned int))

    openslide_read_associated_image(self.thisptr, name.encode('utf-8'), dest)

    if dest is NULL:
      raise OpenslideError('Incorrect associated image read. '
                           'The request image is: {}'.format(name))

    cdef unsigned char * u8 = <unsigned char*>dest;
    argb2rgba(u8, w * h * 4)

    img = np.asarray(<np.uint32_t[: h * w]> dest)
    # free(dest)

    # convert to a readable image
    img = img.view(dtype=np.uint8).reshape(h, w, 4)
    self._dest = dest

    return img


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
    openslide_close(self.thisptr)

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
      raise OpenslideError('Openslide does not support the given image format. '
                           'Given: {}'.format(filename_bytes.decode('utf-8')))

    return True

  def open (self, str filename):
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

      self.thisptr = openslide_open(filename_bytes)
      self._get_error()

    else:

      raise ValueError('The current file cannot be opened with Openslide library')

    return self

  def close (self):
    '''
    Close an OpenSlide object.
    No other threads may be using the object.
    After this call returns, the object cannot be used anymore.
    '''
    openslide_close(self.thisptr)

  def __repr__ (self) -> str:
    '''
    Object representation
    '''
    class_name = self.__class__.__qualname__
    return '{0}(level={1})'.format(class_name, self.level)


def detect_vendor (str filename) -> bool:
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
    filename : str
      The filename to check. On Windows, this must be in UTF-8.

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
  if not isinstance(filename, str) and not isinstance(filename, bytes):
    raise TypeError('{} must be in string format'.format(filename))

  if not os.path.isfile(filename):
    raise FileNotFoundError('Could not open or find the data file. Given: {}'.format(filename))

  filename_bytes = filename.encode('utf-8') if isinstance(filename, str) else filename

  cdef const char * vendor = openslide_detect_vendor(filename_bytes)

  return not (vendor is NULL)
