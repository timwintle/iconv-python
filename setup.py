# -*- coding: utf-8 -*-
from distutils.core import setup, Extension

import sys
include_dirs = []
libraries = []
lib_dirs = []
if sys.platform.startswith('freebsd'):
    include_dirs.append('/usr/local/include')
    libraries.append('iconv')
    lib_dirs.append('/usr/local/lib')

setup (name = "iconv",
       version = "1.1",
       description = "iconv-based Unicode converter",
       author = "Martin v. Löwis",
       author_email = "martin@v.loewis.de",
       url = "http://sourceforge.net/projects/python-codecs/",
       long_description =
"""The iconv module exposes the operating system's iconv character
conversion routine to Python. This package provides an iconv wrapper
as well as a Python codec to convert between Unicode objects and
all iconv-provided encodings.
""",

       py_modules = ['iconvcodec'],
       ext_modules = [Extension("iconv",sources=["iconvmodule.c"],
            include_dirs=include_dirs,libraries=libraries,
            library_dirs=lib_dirs,
            runtime_library_dirs=lib_dirs),]
       )

