#!/bin/bash

for N in {1..100}
do
    python3 tests/test.py &
done
wait
