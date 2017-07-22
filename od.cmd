#!/bin/bash

od --address-radix=d --format=x1 --skip-bytes=14 --width=5 $1 | more -5
