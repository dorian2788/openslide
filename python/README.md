# OpenSlide

| **Windows CI** | **MacOS CI** | **Linux CI** | **Python CI** |
|:--------------:|:------------:|:------------:|:-------------:|
| [![Windows CI](https://github.com/Nico-Curti/openslide/actions/workflows/windows.yml/badge.svg)](https://github.com/Nico-Curti/openslide/actions/workflows/windows.yml) | [![MacOS CI](https://github.com/Nico-Curti/openslide/actions/workflows/macos.yml/badge.svg)](https://github.com/Nico-Curti/openslide/actions/workflows/macos.yml) | [![Linux CI](https://github.com/Nico-Curti/openslide/actions/workflows/linux.yml/badge.svg)](https://github.com/Nico-Curti/openslide/actions/workflows/linux.yml) | [![Python CI](https://github.com/Nico-Curti/openslide/actions/workflows/python.yml/badge.svg)](https://github.com/Nico-Curti/openslide/actions/workflows/python.yml) |

**C supported compilers:**

![gcc version](https://img.shields.io/badge/gcc-4.9.*|5.*|6.*|7.*|8.*|9.*|10.*-yellow.svg)

![clang version](https://img.shields.io/badge/clang-3.*|4.*|5.*|6.*|7.*|8.*|9.*|10.*-red.svg)

![msvc version](https://img.shields.io/badge/msvc-vs2017%20x86%20|%20vs2017%20x64|%20vs2019%20x86%20|%20vs2019%20x64-blue.svg)

**Python version supported:**

![Python version](https://img.shields.io/badge/python-2.7|3.3|3.4|3.5|3.6|3.7|3.8|3.9-blue.svg)

-----------------------------------

## Python version (forked version)

In the current forked version we propose an "alternative" python wrap of the library.
The installation of the Python version could be done in parallel with the C library using Cython.

## Supported format

This forked version aims to extend the portability of Openslide library to multiple OS and multiple WSI formats.
The complete list of supported formats is


|  **Scanner**                                                  |     **Format**             |    **Brightfield**          |    **Fluorescence**   |
|:-------------------------------------------------------------:|:--------------------------:|:---------------------------:|:---------------------:|
| [Aperio](https://openslide.org/formats/aperio/)               |     (.svs, .tif)           |         :+1:                |                       |
| :star: [Tiff](https://openslide.org/formats/generic-tiff/)    |     (.tif)                 |         :+1:                |         :+1:          |
| [Hamamatsu](https://openslide.org/formats/hamamatsu/)         |     (.vms, .vmu, .ndpi)    |         :+1:                |                       |
| [Leica](https://openslide.org/formats/leica/)                 |     (.scn)                 |         :+1:                |                       |
| [Mirax](https://openslide.org/formats/mirax/)                 |     (.mrxs)                |         :+1:                |                       |
| :star: Olympus                                                |     (.vsi, .ets)           |         :+1:                |         :+1:          |
| [Philips](https://openslide.org/formats/philips/)             |     (.tiff)                |         :+1:                |                       |
| [Sakura](https://openslide.org/formats/sakura/)               |     (.svslide)             |         :+1:                |                       |
| [Trestle](https://openslide.org/formats/trestle/)             |     (.tif)                 |         :+1:                |                       |
| [Ventana](https://openslide.org/formats/ventana/)             |     (.bif, .tif)           |         :+1:                |                       |

| :triangular_flag_on_post: Note |
|:-------------------------------|
| In the Olympus format we extend also the Tiff support to the OME-Tiff format :muscle: |

| :triangular_flag_on_post: Note |
|:-------------------------------|
| In the Tiff format we extend also the Tiff support to the JP2K compression :muscle: |

### Usage example

First of all you need to import the Openslide library.

There are only two objects available in the package (beyond the version string)

```python
from openslide import Openslide
from openslide import OpenslideError
from openslide import BRIGHTFIELD, FLUORESCENCE
```

| :triangular_flag_on_post: Note |
|:-------------------------------|
| In this new version of the library we added the `BRIGHTFIELD` and `FLUORESCENCE` as readable variables to switch from brightfield (already supported) and fluorescence! |

The first one is the "real" openslide object, while the second is just an utility class for the exception management.

The openslide interface is quite different from the classical one, since we tried to wrap as much as possible some functionalities in a Pythonic-way.

Lets start with the reading of a file: there are two ways in which you can open a file.

1. Simple constructor

```python
try:
    osr = Openslide(filename='file.vsi', dtype=BRIGHTFIELD)
except OpenslideError as e:
    raise ValueError('An OpenslideError is raised when something '
          'goes wrong with the wrapped functions')
```

2. With the (more Pythonic) context manager

```python
with Openslide(filename, dtype=BRIGHTFIELD) as osr:
    pass
```

Now you can access some WSI informations as

```python
from pprint import pprint

with Openslide(filename, dtype=BRIGHTFIELD) as osr:
    # print some information
    pprint('Number of levels: {:d}'.format(osr.get_level_count))
    pprint('Number of planes: {:d}'.format(osr.get_plane_count))
    pprint('Dimensions of the current level: {}'.format(osr.shape))
    pprint('Other information: ')
    pprint(osr.header)
```

| :triangular_flag_on_post: Note |
|:-------------------------------|
| In this new version of the library we added an extra member to the `Openslide` class called `plane_count` as index of "planes" (aka fluorescence channels). In brightfield images all the RGB channels are stored into a single "plane", so the `plane_count` will be always 1. |

The most important thing to take care managing WSI data is the pyramid of resolution levels.
To handle this feature we built a naive utility function to set the resolution level on which work.

```python
# The HR level is very big so we can move
# along the resolution levels using the
# 'set_level' function

with Openslide(filename, dtype=BRIGHTFIELD) as osr:
    pprint('Current resolution level: {:d}'.format(osr.level))
    pprint('Dimensions of the current level: {}'.format(osr.shape))

    osr = osr.set_level(4)

    pprint('Current resolution level: {:d}'.format(osr.level))
    pprint('Dimensions of the current level: {}'.format(osr.shape))
```

Now you can just plot the desired image as any other numpy array

```python
import pylab as plt

# The Openslide object could be seen just a
# numpy object so we can directly pass it
# to a plot function for the rendering

fig, ax = plt.subplots(nrows=1, ncols=1, figsize=(10, 10))

with Openslide(filename, dtype=BRIGHTFIELD) as osr:
    osr = osr.set_level(4)
    ax.imshow(osr)
```

#### Fluorescence Images

The fluorescence supports is guaranteed for an arbitrary number of channels, but each channel must be read as independent image (aka numpy array).
We override the standard `openslide_read_region` function to support this feature.
Also in this case the result of the call is a classical `numpy` array with `int32_t` values.

| :triangular_flag_on_post: Note |
|:-------------------------------|
| For brightfield images the RGB channels are stored as `uint8_t` as any other classical RGB image! Fluorescence images support a wider range of gray-levels and therefore the data are stored with higher precision integers! |

```python
filename = 'fluorescence.vsi'

with Openslide(filename, dtype=FLUORESCENCE) as osr:
    osr = osr.set_level(3)
    w, h = osr.shape

    dapi  = osr.read_region(x=0, y=0, plane=0, w=w, h=h)
    fitch = osr.read_region(x=0, y=0, plane=1, w=w, h=h)
    cy3   = osr.read_region(x=0, y=0, plane=2, w=w, h=h)
    cy5   = osr.read_region(x=0, y=0, plane=3, w=w, h=h)
```

The buffers read from the image store the raw data acquired by the microscope.
For a better visualization (eg Olyvia one) we need to standardize the images clip the values into a desired range.

```python
import pylab as plt
import numpy as np

filename = 'fluorescence.vsi'

fig, ax = plt.subplots(nrows=1, ncols=1, figsize=(15, 15))

true_min = lambda x : np.nanmin(np.where(x > x.min(), x, np.nan))
norm = lambda x, a, b : np.clip((x - true_min(x)), a, b) / (a + b)

with Openslide(filename, dtype=FLUORESCENCE) as osr:
    osr = osr.set_level(3)
    w, h = osr.shape

    dapi  = osr.read_region(x=0, y=0, plane=0, w=w, h=h)
    fitch = osr.read_region(x=0, y=0, plane=1, w=w, h=h)
    cy3   = osr.read_region(x=0, y=0, plane=2, w=w, h=h)
    cy5   = osr.read_region(x=0, y=0, plane=3, w=w, h=h)

    dapi  = norm(dapi.astype(np.float32),  a=95,  b=2858)
    fitch = norm(fitch.astype(np.float32), a=101, b=1968)
    cy3   = norm(cy3.astype(np.float32),   a=78,  b=2626)
    cy5   = norm(cy5.astype(np.float32),   a=93,  b=690)

    img = np.dstack((cy3, fitch, dapi))
    ax.imshow(img)
```

![fluorescence](https://drive.google.com/uc?export=view&id=1Yrt95E6rPn6-tUuYduSAWkVP3FYgybf8)

| :triangular_flag_on_post: Note |
|:-------------------------------|
| The resolution level size could not be an exact multiple of the tile size, so when we read the image via `openslide` it tries to pad it in the best way. This step produces a darker border around the image which should be excluded when we consider overall statistics of the image (eg. the global minimum). To avoid this issue, it is good practice to use the 2nd minimum of the image, obtained by the `true_min` lambda function declared above. |

## Acknowledgments

Thanks goes to all contributors of this project.
In particular we thank [malaterre](https://github.com/malaterre) who provided a very useful starting point for the management of the ETS format.

### Citation

If you have found this version of `openslide` library helpful in your research, please consider citing this project

```BibTeX
@misc{openslide_extended,
  author = {Nico Curti},
  title = {openslide - extended version},
  year = {2022},
  publisher = {GitHub},
  howpublished = {\url{https://github.com/Nico-Curti/openslide}},
}
```
