#!/usr/bin/env python3

import json
import sys

import json5

with open(sys.argv[1], 'r') as input:
    j5 = json5.load(input)
    output_name = sys.argv[1].replace('.json5', '.json')
    if len(output_name) == len(sys.argv[1]):
        output_name += '.json'
    with open(output_name, 'w') as output:
        json.dump(j5, output, indent=4)
