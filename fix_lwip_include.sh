#!/bin/bash
set -e

python3 -c "
with open('kernel/net/socket.c', 'r') as f: content = f.read()

search = '#include <lwip/sockets.h>'
replace = '#include \"lwip/sockets.h\"'
content = content.replace(search, replace)

with open('kernel/net/socket.c', 'w') as f: f.write(content)
"
