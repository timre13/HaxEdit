#!/usr/bin/env python3

ICONS_SIZE = 32

import os
import sys
import threading as th
import time

#if len(sys.argv) != 3:
#    print("Error: Usage: python "+sys.argv[0]+" <SOURCE_DIR> <DEST_DIR>")
#    sys.exit(1)
#_, SOURCE_DIR, DEST_DIR = sys.argv

SCRIPT_PATH = os.path.dirname(__file__)
SOURCE_DIR = os.path.join(SCRIPT_PATH, "../external/icons/icons")
DEST_DIR = os.path.join(SCRIPT_PATH, "../icons")
print("Source directory: "+SOURCE_DIR)
print("Destination directory: "+DEST_DIR)



IS_INKSCAPE_AVAILABLE = os.system("inkscape -V") == 0
if not IS_INKSCAPE_AVAILABLE:
    print("Error: please install Inkscape")
    sys.exit(1)

MAX_NUM_OF_THREADS = int(12*1.5)
threads = []

FILE_LIST = sorted(os.listdir(SOURCE_DIR))

def render(from_: str, to: str, size: int):
    if os.system("inkscape -o {} -w {} -h {} {} 2>/dev/null".format(to, size, size, from_)):
        print("\nError: conversion of \"{}\" failed".format(from_))
        sys.exit(1)

numOfFilesDone = 0

def removeFinishedThreads():
    global numOfFilesDone
    for j, thread in enumerate(threads):
        if not thread.is_alive():
            print(("Done: {}/{} - {}"+" "*40)
                    .format(
                        numOfFilesDone+1,
                        len(FILE_LIST),
                        thread.getName().split(":")[1]),
                    end="\r", flush=True)
            numOfFilesDone += 1
            del threads[j]

for file in FILE_LIST:
    if os.path.splitext(file)[1] != ".svg":
        continue

    while len(threads) >= MAX_NUM_OF_THREADS:
        removeFinishedThreads()
        time.sleep(0.1)

    thread = th.Thread(target=render,
                       name="thread:"+file,
                       args=(
                           os.path.join(SOURCE_DIR, file),
                           os.path.join(DEST_DIR, file.replace(".svg", ".png")),
                           ICONS_SIZE
                       ))
    threads.append(thread)
    thread.start()

# Wait for the threads to finish
while len(threads) > 0:
    removeFinishedThreads()
    time.sleep(0.1)
