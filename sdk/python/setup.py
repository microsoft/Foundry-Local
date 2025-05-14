# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------
from pathlib import Path

from setuptools import find_packages, setup


def read(rel_path: str) -> str:
    """Read a file and return its contents.

    Args:
        rel_path (str): Relative path to the file.

    Returns:
        str: Contents of the file.
    """
    this_dir = Path(__file__).parent
    with (this_dir / rel_path).open(encoding="utf-8") as fp:
        return fp.read()


def get_version(rel_path):
    for line in read(rel_path).splitlines():
        if line.startswith("__version__"):
            delim = '"' if '"' in line else "'"
            return line.split(delim)[1]
    raise RuntimeError("Unable to find version string.")


VERSION = get_version("foundry_local/__init__.py")

requirements = read("requirements.txt").splitlines()

CLASSIFIERS = [
    "Development Status :: 3 - Alpha",
    "Intended Audience :: Developers",
    "License :: OSI Approved :: MIT License",
    "Operating System :: POSIX :: Linux",
    "Operating System :: Microsoft :: Windows",
    "Topic :: Scientific/Engineering",
    "Topic :: Scientific/Engineering :: Artificial Intelligence",
    "Topic :: Software Development",
    "Topic :: Software Development :: Libraries",
    "Topic :: Software Development :: Libraries :: Python Modules",
    "Programming Language :: Python",
    "Programming Language :: Python :: 3 :: Only",
    "Programming Language :: Python :: 3.9",
    "Programming Language :: Python :: 3.10",
    "Programming Language :: Python :: 3.11",
    "Programming Language :: Python :: 3.12",
    "Programming Language :: Python :: 3.13",
]

description = "Foundry Local Manager Python SDK: Control-plane SDK for Foundry Local."

setup(
    name="foundry-local-sdk",
    version=VERSION,
    description=description,
    long_description=description,
    author="Microsoft Corporation",
    author_email="jambaykinley@microsoft.com",
    license="MIT License",
    classifiers=CLASSIFIERS,
    packages=find_packages(include=["foundry_local"]),
    python_requires=">=3.9",
    install_requires=requirements,
    include_package_data=False,
)
