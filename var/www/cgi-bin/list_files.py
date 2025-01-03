#!/usr/bin/env python3
import os
import json

UPLOAD_DIR = "var/www/upload"

def list_files():
    try:
        files = os.listdir(UPLOAD_DIR)
        files = [f for f in files if os.path.isfile(os.path.join(UPLOAD_DIR, f))]
        print(json.dumps({"files": files}))
    except Exception as e:
        print(json.dumps({"error": str(e)}))

if __name__ == "__main__":
    list_files()
