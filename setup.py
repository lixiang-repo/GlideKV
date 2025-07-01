#!/usr/bin/env python3
"""Setup script for GlideKV package."""

import os
import glob
from setuptools import setup, find_packages

# Get the directory containing this setup.py
here = os.path.abspath(os.path.dirname(__file__))

# Read the README file
with open(os.path.join(here, 'README.md'), encoding='utf-8') as f:
    long_description = f.read()

# Read version from __init__.py or create a default
def get_version():
    """Get version from __init__.py or return default."""
    version_file = os.path.join(here, 'GlideKV', '__init__.py')
    if os.path.exists(version_file):
        with open(version_file, 'r') as f:
            for line in f:
                if line.startswith('__version__'):
                    return line.split('=')[1].strip().strip('"\'')
    return '0.1.0'

# Automatically include all .so files in GlideKV/
so_files = [os.path.basename(f) for f in glob.glob(os.path.join(here, 'GlideKV', '*.so'))]

# Get all .py files in GlideKV/
py_files = [os.path.basename(f) for f in glob.glob(os.path.join(here, 'GlideKV', '*.py'))]

setup(
    name='GlideKV',
    version=get_version(),
    description='GlideKV with TensorFlow custom operations',
    long_description=long_description,
    long_description_content_type='text/markdown',
    author='lixiang',
    author_email='183570397@qq.com',
    url='git@e.coding.net:g-pkgy9519/lixiang/GlideKV.git',
    packages=find_packages(),
    package_data={
        'GlideKV': so_files + py_files,
    },
    include_package_data=True,
    install_requires=[
        'tensorflow==2.15.1',
        'numpy>=1.19.0',
        'grpcio>=1.50.0',
        'protobuf>=3.20.0',
    ],
    extras_require={
        'dev': [
            'pytest>=6.0.0',
            'pytest-cov>=2.0.0',
        ],
    },
    python_requires='>=3.8',
    classifiers=[
        'Development Status :: 3 - Alpha',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: MIT License',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.8',
        'Programming Language :: Python :: 3.9',
        'Programming Language :: Python :: 3.10',
        'Programming Language :: Python :: 3.11',
        'Topic :: Scientific/Engineering :: Artificial Intelligence',
    ],
    keywords='tensorflow, GlideKV, custom operations, machine learning',
    zip_safe=False,
) 
