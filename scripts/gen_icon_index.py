#!/usr/bin/env python3

import os

SCRIPT_PATH = os.path.dirname(__file__)
INPUT_DIR = os.path.join(SCRIPT_PATH, "../external/icons/src/icons")
OUTPUT_DIR = os.path.join(SCRIPT_PATH, "../icons")

class FileIcon:
    def __init__(self):
        self.name = ""
        self.exts = ""
        self.fileNames = ""

class FolderIcon:
    def __init__(self):
        self.name = ""
        self.folderNames = ""

def getRawData(filename: str):
    with open(os.path.join(INPUT_DIR, filename), "r") as file:
        raw = file.readlines()
        startLine = 0
        for i, line in enumerate(raw):
            if line.strip().startswith("icons:"):
                startLine = i
                break
        raw = "".join(raw[startLine:-1]).strip()
        raw = raw[raw.find("[")+1:-raw[::-1].find("]")-1].strip()
        raw = " ".join(raw.split())
    return raw

def getValue(string: str):
    value: str = ""
    if string[0] == "'":
        for c in string[1:]:
            if c != "'":
                value += c
            else:
                break
        return value
    elif string[0] == "[":
        for c in string[1:]:
            if c != "]": # NOTE: Nested lists are not supported
                value += c
            else:
                break
        return value
    else:
        for c in string:
            if c != " ":
                value += c
            else:
                break
        if value == "true":
            return True
        elif value == "false":
            return False
        else:
            raise ValueError("Invalid token", value)

def getFileIcons():
    raw = getRawData("fileIcons.ts")
    i = 0
    icon = FileIcon()
    icons = []
    while i < len(raw):
        if raw[i:].startswith("{ name: "):
            if icon:
                icons.append(icon)
            icon = FileIcon()
            i += 8 # Skip name key
            icon.name = str(getValue(raw[i:])) # Get name
            i += len(icon.name)+1 # Skip name value
        elif raw[i:].startswith("fileExtensions: "):
            i += 16
            val = getValue(raw[i:])
            assert(val != True and val != False)
            icon.exts = eval(val)
            i += len(val)+2
        elif raw[i:].startswith("fileNames: "):
            i += 11
            val = getValue(raw[i:])
            assert(val != True and val != False)
            icon.fileNames = eval(val)
            i += len(val)+2
        i += 1
    icons.sort(key=lambda x: x.name)
    return icons

def getFolderIcons():
    raw = getRawData("folderIcons.ts")
    i = 0
    icon = FolderIcon()
    icons = []
    while i < len(raw):
        if raw[i:].startswith("{ name: "):
            if icon:
                icons.append(icon)
            icon = FileIcon()
            i += 8 # Skip name key
            icon.name = str(getValue(raw[i:])) # Get name
            i += len(icon.name)+1 # Skip name value
        elif raw[i:].startswith("folderNames: "):
            i += 13
            val = getValue(raw[i:])
            assert(val != True and val != False)
            icon.folderNames = eval(val)
            i += len(val)+2
        i += 1
    icons.sort(key=lambda x: x.name)
    return icons

# Write file icons
with open(os.path.join(OUTPUT_DIR, "file_icon_index.txt"), "w+") as outFile:
    outFile.write("name|extension1/extension2|filename1/filename2\n")
    for icon in getFileIcons()[1:]:
        outLine = ""
        outLine += icon.name
        outLine += "|"
        if type(icon.exts) == str:
            outLine += icon.exts
        else:
            outLine += "/".join(icon.exts)
        outLine += "|"
        if type(icon.fileNames) == str:
            outLine += icon.fileNames
        else:
            outLine += "/".join(icon.fileNames)
        outLine += "\n"
        outFile.write(outLine)
    print("Wrote output to "+os.path.join(OUTPUT_DIR, "file_icon_index.txt"))

# Write folder icons
with open(os.path.join(OUTPUT_DIR, "folder_icon_index.txt"), "w+") as outFile:
    outFile.write("name|foldername1/foldername2\n")
    for icon in getFolderIcons()[1:]:
        outLine = ""
        outLine += icon.name
        outLine += "|"
        if type(icon.folderNames) == str:
            outLine += icon.folderNames
        else:
            outLine += "/".join(icon.folderNames)
        outLine += "\n"
        outFile.write(outLine)
    print("Wrote output to "+os.path.join(OUTPUT_DIR, "folder_icon_index.txt"))

