# GlideKV with TensorFlow Custom Operations

A high-performance GlideKV library featuring custom TensorFlow operations, written in C++ and Python, and seamlessly integrated with Bazel for reproducible builds.

## Features
- 🚀 **Custom TensorFlow Ops**: High-performance C++ ops for advanced GlideKV tasks
- 🛠️ **Bazel Build**: Reproducible, robust, and scalable build system
- 🐍 **Python API**: Easy-to-use Python interface for integration with ML workflows
- 🔄 **Automatic Environment Detection**: Automatically finds and links your TensorFlow installation
- 📦 **Wheel Packaging**: Easily build and distribute as a Python wheel
- 🔧 **TensorFlow Serving**: Production-ready model serving capabilities

## Quick Start

### Option 1: Direct Installation (Recommended)
```bash

# Build wheel package
bazel build //GlideKV:_lookup_ops.so

```

## 开发与构建说明
- **Bazel 构建**：所有 C++/TF 自定义算子通过 Bazel 构建，确保与 Python 环境 TensorFlow 版本一致。
- **环境变量**：需设置 `TF_SO_PATH` 指向当前 Python 环境下的 TensorFlow 路径（`python configure.py` 可自动完成）。

## 贡献指南
欢迎提交 Issue 和 PR！请确保：
- 遵循 [PEP8](https://www.python.org/dev/peps/pep-0008/) 代码风格
- 新增功能需附带测试
- 详细描述你的更改

## License

[MIT](LICENSE)
