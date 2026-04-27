#!/usr/bin/env python3
# tests/cgi-bin/timeout_test.py

import time
import sys

time.sleep(15)  # sleep longer than your 60s timeout

print("Content-Type: text/html\r")
print("\r")
print("<h1>You should never see this</h1>")

sys.stdout.flush()
sys.exit(0)