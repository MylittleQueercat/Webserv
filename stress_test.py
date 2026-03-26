import socket
import threading

results = {"ok": 0, "fail": 0, "error": 0}
lock = threading.Lock()

def send_request():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(('localhost', 8080))
        s.send(b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n")
        response = s.recv(4096)
        with lock:
            if b"200 OK" in response:
                results["ok"] += 1
            else:
                results["fail"] += 1
        s.close()
    except Exception as e:
        with lock:
            results["error"] += 1

threads = []
for i in range(1000):  # 1000 个并发
    t = threading.Thread(target=send_request)
    threads.append(t)

for t in threads:
    t.start()

for t in threads:
    t.join()

print("OK:   ", results["ok"])
print("FAIL: ", results["fail"])
print("ERROR:", results["error"])