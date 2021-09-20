import subprocess as sp
import threading as th
from string import Template
import json

SERVER_CMD = "clangd --background-index"
#SERVER_CMD = "gopls"


def sendMsg(msgs):
    proc = sp.Popen(SERVER_CMD.split(), stdin=sp.PIPE, stdout=sp.PIPE, stderr=sp.PIPE)
    for msg in msgs:
        print("Sending: \n"+msg)
        proc.stdin.write(msg.encode("utf-8")) # type: ignore
        proc.stdin.flush() # type: ignore
        print("-"*20+"\nReceived (stdout):\n")
        print(proc.stdout.readline().decode("utf-8")) # type: ignore
        print(proc.stdout.readline().decode("utf-8")) # type: ignore
        print("-"*20+"\nReceived (stderr):\n")
        print(proc.stderr.readline().decode("utf-8")) # type: ignore
        print(proc.stderr.readline().decode("utf-8")) # type: ignore

def genMsg(method: str, params: dict={}):
    body = Template("""\
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "$METHOD",
    "params": $PARAMS
}\
""").substitute({"METHOD": method, "PARAMS": json.dumps(params)})
    return Template("""\
Content-Length: $CONTENT_LEN

$BODY
""").substitute({"CONTENT_LEN": len(body), "BODY": body})

thread = th.Thread(target=sendMsg, args=(
    (
        genMsg(method="initialize", params={}),
        genMsg(method="textDocument/didOpen", params={"DocumentUri": "file:///home/mike/programming/cpp/opengl_text_editor/src/main.cpp"}),
        genMsg(method="shutdown", params={}),
        genMsg(method="exit", params={}),
    ),
))
thread.daemon = True
thread.start()
thread.join()
