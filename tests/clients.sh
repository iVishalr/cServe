#!/bin/bash

for N in {1..300}
do
    python3 tests/test.py &
done
wait
