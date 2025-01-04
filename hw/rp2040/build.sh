#!/bin/bash
docker run --rm -ti -v`pwd`/../..:/work picobuild2 bash -c "cd /work/hw/rp2040; $1"
