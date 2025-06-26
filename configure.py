#!/usr/bin/env python3
"""
TensorFlow Environment Configuration Script
Automatically configures Bazel build environment with TensorFlow installation path.
"""

import os
import sys

def find_tensorflow_path():
    """Find TensorFlow installation path in current Python environment"""
    try:
        import tensorflow as tf
        tf_path = os.path.dirname(tf.__file__)
        print(f"✓ Found TensorFlow at: {tf_path}")
        return tf_path
    except ImportError:
        print("✗ Error: TensorFlow is not installed in this environment.")
        sys.exit(1)

def configure_bazelrc():
    """Configure .bazelrc with TensorFlow environment path"""
    tf_path = find_tensorflow_path()
    bazelrc_path = ".bazelrc"
    
    # Read existing .bazelrc
    lines = []
    if os.path.exists(bazelrc_path):
        with open(bazelrc_path, 'r', encoding='utf-8') as f:
            lines = f.readlines()
    
    # Check if TF_SO_PATH already exists
    tf_so_path_exists = any('TF_SO_PATH' in line for line in lines)
    
    if not tf_so_path_exists:
        # Add the TF_SO_PATH configuration
        lines.append(f'\n\nbuild --action_env=TF_SO_PATH={tf_path}\n')
        
        # Write back to .bazelrc
        with open(bazelrc_path, 'w', encoding='utf-8') as f:
            f.writelines(lines)
        
        print(f"✓ Added to .bazelrc: build --action_env=TF_SO_PATH={tf_path}")
    else:
        print("ℹ TF_SO_PATH already exists in .bazelrc")
    
    # Also export to current shell environment
    print(f"\n📋 To use in current shell, run:")
    print(f"   export TF_SO_PATH={tf_path}")
    print(f"\n🚀 Ready to build with: bazel build //GlideKV:_lookup_ops.so")

if __name__ == "__main__":
    print("🔧 Configuring TensorFlow environment for Bazel build...")
    configure_bazelrc() 