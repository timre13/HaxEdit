#!/usr/bin/python3

LIST_URL        = "http://www.eclipsecolorthemes.org/?list=downloads"
DL_URL_TEMPLATE = "http://www.eclipsecolorthemes.org/?view=empty&action=download&theme={}&type=epf"

print("----- Downloading color themes... -----")

from urllib.request import urlopen
from bs4 import BeautifulSoup
import os

SAVE_PATH       = os.path.realpath(os.path.join(os.path.dirname(os.path.realpath(__file__)), "../external/themes"))
if not os.path.exists(SAVE_PATH):
    os.mkdir(SAVE_PATH)
    print("Created output directory")

print("Fetching list... ", end="", flush=True)
list = [[it.find("a", href=True).text, it.find("a", href=True)["href"].split("=")[2]] for it in BeautifulSoup(urlopen(LIST_URL), features="html.parser").find("div", class_="listing").find_all("div", class_="theme")] #type:ignore
print("Found {} themes".format(len(list)))
# Add some more nice themes
list.extend([
    ["Atom One Dark",   "46248"],
    ["Darkula",         "15515"],
    ["Gruvbox Dark",    "23468"],
    ["Molokai",         "3908" ],
])

for i, it in enumerate(list):
    print("[{}/{}] Downloading: {} (id={})... ".format(str(i+1).rjust(2, ' '), len(list), it[0], it[1]), end="", flush=True)
    filename = os.path.join(SAVE_PATH, "".join([x if x.isascii() and x.isalnum() else "_" for x in it[0].lower().replace(" ", "_")])+".epf")
    if os.path.exists(filename):
        print("Already downloaded")
    else:
        content = urlopen(DL_URL_TEMPLATE.format(it[1])).read().decode("ascii")
        with open(filename, "w+") as file:
            file.write(content)
        print("Wrote {} bytes to {}".format(len(content), filename))

print("----- Done -----")
