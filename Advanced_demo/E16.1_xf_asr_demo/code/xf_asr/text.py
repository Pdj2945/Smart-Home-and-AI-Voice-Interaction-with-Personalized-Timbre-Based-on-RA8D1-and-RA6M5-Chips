import socket

# 修改为你要测试的服务器IP和端口
SERVER_IP = "ws-api.xfyun.cn"  # 例如讯飞云
SERVER_PORT = 80               # 例如80或8344等

def test_tcp_connect(ip, port, timeout=5):
    try:
        print(f"正在测试 {ip}:{port} 的TCP连通性...")
        sock = socket.create_connection((ip, port), timeout=timeout)
        print(f"连接成功！{ip}:{port}")
        sock.close()
    except Exception as e:
        print(f"连接失败：{e}")

if __name__ == "__main__":
    test_tcp_connect(SERVER_IP, SERVER_PORT)