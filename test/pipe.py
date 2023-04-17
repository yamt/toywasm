#! /usr/bin/env python3

# keep stdout open until the peer closes it

import sys
import select

p = select.poll()
p.register(sys.stdout, select.POLLHUP)
p.poll(-1)
