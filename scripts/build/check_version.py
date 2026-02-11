#!/usr/bin/env python3
import socket
import json

# Test the actual version running
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.settimeout(2)
sock.connect(('localhost', 3001))

init_cmd = '{"jsonrpc":"2.0","method":"initialize","id":1}\n'
sock.send(init_cmd.encode())
response = sock.recv(4096).decode()
sock.close()

data = json.loads(response)
server_info = data['result']['serverInfo']
version = server_info['version']
name = server_info['name']

print(f'Currently running: {name} v{version}')
