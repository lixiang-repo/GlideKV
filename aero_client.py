#!/usr/bin/env python3
"""
验证 Aerospike 中的数据结构和内容
"""

import aerospike
import sys
import numpy as np
import random

namespace = "test"
set_name = "vectors"
field_name = "vector"

def connect_to_aerospike():
    """连接到 Aerospike"""
    try:
        config = {
            'hosts': [('localhost', 3000)]
        }
        client = aerospike.client(config)
        client.connect()
        print("✅ 成功连接到 Aerospike")
        return client
    except Exception as e:
        print(f"❌ 连接失败: {e}")
        return None

def get_vector(keys, client, namespace="test", set_name="vectors"):
    """检查指定的 keys 是否存在"""
    try:
        vectors = []
        for key_val in keys:
            key = (namespace, set_name, key_val)
            (key, meta, bins) = client.get(key)
            # print(f"✅ Key {key_val}: 存在")
            # print(f"   Meta: {meta}")
            # print(f"   Bins: {bins}")
            
            # 检查是否有 vector bin
            if 'vector' in bins:
                vector_data = bins['vector']
                vectors.append(vector_data)
        return np.array(vectors)
    except aerospike.exception.RecordNotFound:
        print(f"❌ Key {key_val}: 不存在")
    except Exception as e:
        print(f"❌ Key {key_val}: 检查失败 - {e}")
        

def random_vector():
    length = random.randint(1, 10)
    vector = np.random.randint(1, 10, length)
    return np.unique(vector)

def test():
    client = connect_to_aerospike()
    ids = random_vector();
    for _ in range(1000):
        e = table[ids] - get_vector(ids.tolist(), client)
        print(tf.reduce_sum(tf.math.abs(e)).numpy())

def main():
    """主函数"""
    print("=== Aerospike 数据验证工具 ===")
    client = connect_to_aerospike()
    ids = random_vector();
    for i in range(1, 10):
        e = get_vector([i], client)
        print(e)

    client.close()
if __name__ == "__main__":
    main() 
