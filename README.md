# GlideKV with TensorFlow Custom Operations

A high-performance GlideKV library featuring custom TensorFlow operations, written in C++ and Python, and seamlessly integrated with Bazel for reproducible builds.

## Features
- 🚀 **Custom TensorFlow Ops**: High-performance C++ ops for advanced GlideKV tasks
- 🛠️ **Bazel Build**: Reproducible, robust, and scalable build system
- 🐍 **Python API**: Easy-to-use Python interface for integration with ML workflows
- 🔄 **Automatic Environment Detection**: Automatically finds and links your TensorFlow installation
- 📦 **Wheel Packaging**: Easily build and distribute as a Python wheel
- 🔧 **TensorFlow Serving**: Production-ready model serving capabilities

## TensorFlow Serving Deployment

### Docker Deployment

GlideKV supports deployment via TensorFlow Serving with Docker. The following command starts a TensorFlow Serving container with GlideKV custom operations:

```bash
docker run -itd \
  -p 8080:8080 \  # Prometheus metrics port
  -p 8500:8500 \  # gRPC API port
  -p 8501:8501 \  # REST API port
  -e GLIDEKV_METRICS_ENABLED=1 \
  -v /data:/data \
  --privileged=true \
  --network=host \
  registry.cn-beijing.aliyuncs.com/bidops/bidops:GlideKV-v1 \
  --file_system_poll_wait_seconds=10 \
  --model_base_path=/data/model/dnn_winr_v1/export_dir/dense_model/
```

### Configuration Parameters

| Parameter | Description | Default |
|-----------|-------------|---------|
| `-p 8080:8080` | Prometheus metrics port | 8080 |
| `-p 8500:8500` | gRPC API port | 8500 |
| `-p 8501:8501` | REST API port | 8501 |
| `-e GLIDEKV_METRICS_ENABLED=1` | Enable GlideKV metrics collection | 0 |
| `-v /data:/data` | Mount local data directory | - |
| `--privileged=true` | Enable privileged mode for custom ops | false |
| `--network=host` | Use host network mode | bridge |
| `--file_system_poll_wait_seconds=10` | Model polling interval | 10 |
| `--model_base_path` | Path to the TensorFlow model | - |

### Environment Variables

- `GLIDEKV_METRICS_ENABLED=1`: Enables Prometheus metrics collection for monitoring
- `AEROSPIKE_HOST`: Aerospike server host (default: localhost)
- `AEROSPIKE_PORT`: Aerospike server port (default: 3000)
- `AEROSPIKE_NAMESPACE`: Aerospike namespace (default: test)
- `AEROSPIKE_SET`: Aerospike set name (default: vectors)
- `AEROSPIKE_FIELD`: Aerospike field name (default: vector)

### Model Requirements

The TensorFlow model must include GlideKV custom operations:
- `HashTableOfTensors`: For vector lookup operations
- `LookupFind`: For key-value retrieval

### Health Check

After deployment, you can check the service status:

```bash
# Check HTTP REST API
curl http://localhost:8501/v1/models

# Check gRPC API (requires grpcurl installation)
grpcurl -plaintext localhost:8500 list

# Check Prometheus metrics
curl http://localhost:8080/metrics
```

### Install grpcurl (Optional)

If you need to test gRPC API, install grpcurl:

```bash
# Ubuntu/Debian
sudo apt-get update && sudo apt-get install -y grpcurl

# CentOS/RHEL
sudo yum install -y grpcurl

# Or download from GitHub releases
wget https://github.com/fullstorydev/grpcurl/releases/download/v1.8.7/grpcurl_1.8.7_linux_x86_64.tar.gz
tar -xzf grpcurl_1.8.7_linux_x86_64.tar.gz
sudo mv grpcurl /usr/local/bin/
rm grpcurl_1.8.7_linux_x86_64.tar.gz
```

## 开发与构建说明
- **Bazel 构建**：所有 C++/TF 自定义算子通过 Bazel 构建，确保与 Python 环境 TensorFlow 版本一致。
- **环境变量**：需设置 `TF_SO_PATH` 指向当前 Python 环境下的 TensorFlow 路径（`python configure.py` 可自动完成）。
- **.gitignore** 已优化，避免无关文件入库。

## 贡献指南
欢迎提交 Issue 和 PR！请确保：
- 遵循 [PEP8](https://www.python.org/dev/peps/pep-0008/) 代码风格
- 新增功能需附带测试
- 详细描述你的更改

## License

[MIT](LICENSE)
