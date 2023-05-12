"""Holds all relevant information for packaging and publishing to PyPI."""
from setuptools import Extension, setup
from setuptools.command.build_ext import build_ext

import os
from pathlib import Path
import platform
import re
import subprocess
import sys
from distutils.version import LooseVersion

VERSION = "3.3.1"


with open("INFO.md") as readme_file:
    readme = readme_file.read()

# List any runtime requirements here
requirements = ["numpy", "cmake", "xmltodict"]


class CMakeExtension(Extension):
    def __init__(self, name, sourcedir=""):
        Extension.__init__(self, name, sources=[])
        self.sourcedir = os.path.abspath(sourcedir)


class CMakeBuild(build_ext):
    def run(self):
        try:
            out = subprocess.check_output(["cmake", "--version"])
        except OSError:
            raise RuntimeError(
                "CMake must be installed and available at PATH ({0}) to build the following extensions: {1}".format(
                    os.environ.get("PATH"), ", ".join(e.name for e in self.extensions)
                )
            )
        cmake_version = LooseVersion(re.search(r"version\s*([\d.]+)", out.decode()).group(1))
        if platform.system() == "Windows":
            cmake_version = LooseVersion(re.search(r"version\s*([\d.]+)", out.decode()).group(1))
            if cmake_version < "3.1.0":
                raise RuntimeError("CMake >= 3.1.0 is required on Windows")

        for ext in self.extensions:
            self.build_extension(ext)

    def build_extension(self, ext):
        path_var = os.environ.get("PATH")
        path_var = str(Path(sys.executable).parent) + ":" + path_var
        env = dict(os.environ.copy(), PATH=path_var)
        extdir = os.path.abspath(os.path.dirname(self.get_ext_fullpath(ext.name)))
        cmake_args = ["-DCMAKE_LIBRARY_OUTPUT_DIRECTORY=" + extdir,
                      "-DPYTHON_EXECUTABLE=" + sys.executable,  # used for pybind11
                      "-DLIBCZI_BUILD_UNITTESTS=" + "OFF", # used for libczi
                      "-DLIBCZI_BUILD_CZICMD=" + "OFF", # used for libczi
                      "-DLIBCZI_BUILD_CZICHECK=" + "OFF"]  # used for libczi

        cfg = "Debug" if self.debug else "Release"
        build_args = ["--config", cfg]

        if platform.system() == "Windows":
            cmake_args += ["-DCMAKE_LIBRARY_OUTPUT_DIRECTORY_{}={}".format(cfg.upper(), extdir)]
            cmake_args += ["-DCMAKE_GENERATOR_PLATFORM=x64"]
            build_args += ["--", "/m"]
        else:
            cmake_args += ["-DCMAKE_BUILD_TYPE=" + cfg]
            build_args += ["--", "-j2"]

        env["CXXFLAGS"] = '{} -DVERSION_INFO=\\"{}\\"'.format(env.get("CXXFLAGS", ""), self.distribution.get_version())
        if not os.path.exists(self.build_temp):
            os.makedirs(self.build_temp)
        subprocess.check_call(["cmake", ext.sourcedir] + cmake_args, cwd=self.build_temp, env=env)
        subprocess.check_call(
            ["cmake", "--build", ".", "--target", "_pylibCZIrw"] + build_args, cwd=self.build_temp, env=env
        )

setup(
    name="pylibCZIrw",
    version=VERSION,
    author="Felix Scheffler",
    author_email="felix.scheffler@zeiss.com",
    description="A python wrapper around the libCZI C++ library with reading and writing functionality.",
    long_description=readme,
    long_description_content_type="text/markdown",
    # See https://setuptools.pypa.io/en/latest/userguide/datafiles.html
    include_package_data=True,
    keywords="czi, imaging",
    ext_modules=[CMakeExtension("_pylibCZIrw")],
    packages=["pylibCZIrw"],
    cmdclass=dict(build_ext=CMakeBuild),
    install_requires=requirements,
    # we require at least python version 3.7
    python_requires=">=3.7,<4",
    license_files=['COPYING.txt', 'COPYING.LESSER.txt', 'NOTICE.txt'],
    # Classifiers help users find your project by categorizing it.
    # For a list of valid classifiers, see https://pypi.org/classifiers/
    classifiers=[
        "Development Status :: 5 - Production/Stable",
        "Topic :: Scientific/Engineering :: Image Processing",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "License :: OSI Approved :: GNU Lesser General Public License v3 (LGPLv3)",
        "Operating System :: Microsoft :: Windows",
        "Operating System :: Unix"
    ],
    zip_safe=False,
)
