"""glidekv with TensorFlow custom operations."""

__version__ = "0.1.0"
__author__ = "lixiang"
__email__ = "lixiang.qa@qq.com"

# Import main modules
from . import glidekv

# Import specific functions/classes that should be available at package level
from .glidekv import LookupTable, LookupInterface

# Create alias for easier access
lookupTable = LookupTable

# Create package alias for import glidekv as gkv
import sys
sys.modules['glidekv'] = sys.modules[__name__]

__all__ = [
    'glidekv',
    'LookupTable',
    'LookupInterface', 
    'lookupTable',
]