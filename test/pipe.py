#! /usr/bin/env python3

# keep stdout open until the peer closes it

import sys
import select

p = select.poll()
p.register(sys.stdout, select.POLLHUP)
# http://gnats.netbsd.org/cgi-bin/query-pr-single.pl?number=57369
while True:
    l = p.poll(1)
    if l:
        break
