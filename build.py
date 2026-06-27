#!/usr/bin/python3
# -*- coding: utf-8 -*-

import sys
import subprocess
import threading
import os
import shutil

BOARD = "esp32:esp32:esp32c3"

def receive_stream(stream,lock=None,fo=None,pr=False):
    for line in iter(stream.readline, ''):
        line = line.rstrip()
        if lock:
            with lock:
                if fo:
                    fo.write(line+'\n')
                if pr:
                    print(line)
        else:
            if fo:
                fo.write(line+'\n')
            if pr:
                print(line)
    stream.close()    

def get_arduino_raw_flags(name:str):
    proc = subprocess.Popen([
        'arduino-cli.exe' if sys.platform=='win32' else 'arduino-cli',
        'compile',
        '--fqbn',
        BOARD,
        '--show-properties',
        '.'
    ],text=True,shell=False,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    data = ''
    for line in iter(proc.stdout.readline, ''): # type: ignore
        line = line.rstrip()
        if not data and line.startswith(name):
            data = line[len(name):]
    if not data:
        raise KeyError("property not found")
    if not proc.wait():
        return data
    

def compile_sketch(cfg:list[str],logfile:str,build_path:str,preprocessor_flags:str):
    cpprawflags  = get_arduino_raw_flags('compiler.cpp.flags=')
    preprocflags = get_arduino_raw_flags('compiler.cpreprocessor.flags=')
    args = [
        'arduino-cli.exe' if sys.platform=='win32' else 'arduino-cli',
        'compile',
        '--fqbn',
        BOARD,
        '-v',
        '--build-property',
        f'compiler.cpp.flags={cpprawflags} -std=gnu++23',
        '--build-property',
        f'compiler.cpreprocessor.flags={preprocflags} {preprocessor_flags}',
        '--build-path',
        build_path
    ]
    os.makedirs(build_path,exist_ok=True)
    for c in cfg:
        c = c.strip()
        if c:
            args.append('--build-property')
            args.append(c.strip())
    args.append('.')
    lock = threading.Lock()
    with open(logfile,'w',encoding='utf-8') as f:
        proc = subprocess.Popen(args,text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        threading.Thread(target=receive_stream,args=(proc.stdout,lock,f,True),daemon=True).start()
        threading.Thread(target=receive_stream,args=(proc.stderr,lock,f,True),daemon=True).start()
        if proc.wait():
            raise Exception(proc.poll())

if __name__=='__main__':
    if len(sys.argv) == 2:
        mode = sys.argv[1]
        if mode == 'client':
            with open('./build_config/preproc_client.cfg','r',encoding='utf-8') as f:
                preproc_flags = f.read().strip()
            with open('./build_config/build_client.cfg','r',encoding='utf-8') as f:
                cfg = f.readlines()
            compile_sketch(cfg,'./compile_client.log','./build/client',preproc_flags)
            sys.exit(0)
        elif mode == 'server':
            with open('./build_config/preproc_server.cfg','r',encoding='utf-8') as f:
                preproc_flags = f.read().strip()
            with open('./build_config/build_server.cfg','r',encoding='utf-8') as f:
                cfg = f.readlines()
            compile_sketch(cfg,'./compile_server.log','./build/server',preproc_flags)
            sys.exit(0)
        elif mode == 'clean-all':
            if os.path.isdir("./build"):
                print("Removing")
                shutil.rmtree('./build')
            else:
                print("Not found")
            sys.exit(0)
        elif mode == 'clean-client':
            if os.path.isdir('./build/client'):
                print("Removing")
                shutil.rmtree("./build/client")
            else:
                print("Not found")
            sys.exit(0)
        elif mode == 'clean-server':
            if os.path.isdir('./build/server'):
                print("Removing")
                shutil.rmtree("./build/server")
            else:
                print("Not found")
            sys.exit(0)
        else:
            print(f"ERROR: unknown mode: {sys.argv[1]}")
    print(f"Usage: \"{sys.argv[0]}\" <mode>")
    print("  mode:")
    print("    'client': build client firmware")
    print("    'server': build server firmware")
    print("    'clean-all': remove all build result")
    print("    'clean-client': remove client build result")
    print("    'clean-server': remove server build result")
    sys.exit(1)
