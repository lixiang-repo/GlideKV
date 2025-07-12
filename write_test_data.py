import aerospike
import random
import sys

# 连接 Aerospike - 添加更多配置选项
config = {
    'hosts': [('127.0.0.1', 3000)],
    'policies': {
        'timeout': 1000,  # 1秒超时
        'max_retries': 3
    }
}

try:
    print("正在连接 Aerospike 服务器...")
    client = aerospike.client(config).connect()
    print("连接成功！")
    
    # 插入 10 条数据（vector 为 List 类型，元素为 float）
    for pk in range(10):
        # 生成 8 维随机 float32 向量
        vector = [round(random.uniform(0, 1), 4) for _ in range(8)]
        # 插入数据（vector 字段为 List 类型）
        client.put(('test', 'vectors', pk), {'vector': vector})
        print(f"插入数据 {pk}: {vector}")
    
    print("所有数据插入完成！")
    client.close()
    
except Exception as e:
    print(f"连接失败: {e}")
    print(f"错误类型: {type(e).__name__}")
    sys.exit(1)