#!/usr/bin/env python3
import os
import datetime

print("Content-Type: text/html; charset=utf-8\r")
print("\r")
print("<html>")
print("<head>")
print("  <meta http-equiv='refresh' content='1'>")
print("</head>")
print("<body>")
print("<h1>实时时间</h1>")
print("<p>现在是：" + str(datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")) + "</p>")
print("<p>Method: " + os.environ.get("REQUEST_METHOD", "unknown") + "</p>")
print("</body>")
print("</html>")
