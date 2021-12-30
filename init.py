#!/usr/bin/python3

import os

assert(os.system("scripts/get_submodules.sh") == 0)
assert(os.system("scripts/gen_icon_index.py") == 0)
assert(os.system("scripts/render_icons.py")   == 0)
assert(os.system("scripts/dl_themes.py")   == 0)
