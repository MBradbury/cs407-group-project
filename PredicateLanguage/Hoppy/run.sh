#!/bin/bash
make; echo -e "[01,00] using Neighbours(2) as twohopn in @(x : twohopn ~ fn(x) == abs(6 - 9 * 7) + len(twohopn) + mean(twohopn, temperature))" | java -cp . Hoppy

