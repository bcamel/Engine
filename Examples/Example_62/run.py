#!/usr/bin/env python3

import os
import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")

