import os
import sys
import json
from typing import Literal
import platform

TypeChipVariant = str
TypeChipId = Literal[
    "esp32",
    "esp32c3",
    "esp32c5",
    "esp32c6",
    "esp32h2",
    "esp32p4",
    "esp32s2",
    "esp32s3"
]

xtensa_chips = {
    'esp32',
    'esp32s2',
    'esp32s3'
}

default_arduino15_path = \
    os.path.join(os.environ['LOCALAPPDATA'], 'Arduino15') if sys.platform == 'win32' else \
    os.path.join(os.environ['HOME'], '.arduino15')

default_user_lib_path = \
    os.path.join(os.environ['USERPROFILE'], 'Documents', 'Arduino', 'libraries') if sys.platform == 'win32' else \
    os.path.join(os.environ['HOME'], 'Arduino', 'libraries')

def find_include_dirs(
                      chip_variant:TypeChipVariant,
                      chip_id:TypeChipId,
                      idf_version:str,
                      use_cpp=False,
                      user_lib_path:str=default_user_lib_path,
                      arduino15_dir:str=default_arduino15_path,
                      ):
    inc_dirs = []
    defs = []
    # idf
    idflibs = ""
    packages_dir = os.path.join(arduino15_dir,'packages')
    if os.path.isdir(os.path.join(packages_dir,'esp32','tools','esp32-arduino-libs')) and os.listdir(os.path.join(packages_dir,'esp32','tools','esp32-arduino-libs')):
        for d in os.listdir(os.path.join(packages_dir,'esp32','tools','esp32-arduino-libs')):
            if d.startswith('idf-') and os.path.isdir(os.path.join(packages_dir,'esp32','tools','esp32-arduino-libs',d)):
                idflibs = os.path.join(packages_dir,'esp32','tools','esp32-arduino-libs',d,chip_id)
                print("idflibs: type 1 (esp32-arduino-libs/%s/%s)"%(d,chip_id))
                break
        else:
            raise FileNotFoundError('idf')
    else:
        idflibs = os.path.join(packages_dir,'esp32','tools',f'{chip_id}-libs',idf_version)
        print(f'idflibs: type 2 ({chip_id}-libs/{idf_version})')
    if not os.path.isdir(idflibs):raise FileNotFoundError('idflibs')
    flags_includes_file = os.path.join(idflibs,'flags','includes')
    flags_defines_file = os.path.join(idflibs,'flags','defines')
    idflib_include_dir = os.path.join(idflibs,'include')
    qio_inc = os.path.join(idflibs,'qio_qspi','include')
    print('flags_includes_file:',flags_includes_file)
    print('flags_defines_file:',flags_defines_file)
    print("qio_inc:",qio_inc)
    if not os.path.isdir(qio_inc):
        raise FileNotFoundError('qio_qspi/include')
    inc_dirs.append(qio_inc)
    if not os.path.isfile(flags_includes_file):raise FileNotFoundError('flags/includes')
    if not os.path.isfile(flags_defines_file):raise FileNotFoundError('flags/defines')
    if not os.path.isdir(idflib_include_dir):raise FileNotFoundError('include')
    # parse flags
    with open(flags_includes_file,'r',encoding='utf-8') as f:
        for i in f.read().strip().split('-iwithprefixbefore '):
            i = i.strip()
            if i:
                p = os.path.join(idflib_include_dir,i)
                print('flags include:',p)
                inc_dirs.append(p)
    # parse defines
    with open(flags_defines_file,'r',encoding='utf-8') as f:
        for i in f.read().strip().split('-D'):
            i = i.strip().replace('\\"','"')
            if i:
                print("flags define:",i)
                defs.append(i)
    # cores
    hwdir = os.path.join(packages_dir,'esp32','hardware','esp32',idf_version)
    cores_inc = os.path.join(hwdir,'cores','esp32')
    print('cores_inc:',cores_inc)
    if not os.path.isdir(cores_inc):
        raise FileNotFoundError('cores')
    inc_dirs.append(cores_inc)
    # libs
    for d in os.listdir(os.path.join(hwdir,'libraries')):
        if os.path.isdir(os.path.join(hwdir,'libraries',d,'include')):
            inc_dirs.append(os.path.join(hwdir,'libraries',d,'include'))
        elif os.path.isdir(os.path.join(hwdir,'libraries',d,'src')):
            inc_dirs.append(os.path.join(hwdir,'libraries',d,'src'))
        elif os.path.isdir(os.path.join(hwdir,'libraries',d)):
            inc_dirs.append(os.path.join(hwdir,'libraries',d))
        else:
            continue
        print("libs:",inc_dirs[-1])
    # variant
    variant_inc = os.path.join(hwdir,'variants',chip_variant)
    print('variant_inc:',variant_inc)
    if not os.path.isdir(variant_inc):
        raise FileNotFoundError('variants/'+chip_variant)
    inc_dirs.append(variant_inc)
    # default libraries
    for d in os.listdir(os.path.join(arduino15_dir, 'libraries')):
        if os.path.isdir(os.path.join(arduino15_dir,'libraries',d,'include')):
            inc_dirs.append(os.path.join(arduino15_dir,'libraries',d,'include'))
        elif os.path.isdir(os.path.join(arduino15_dir,'libraries',d,'src')):
            inc_dirs.append(os.path.join(arduino15_dir,'libraries',d,'src'))
        elif os.path.isdir(os.path.join(arduino15_dir,'libraries',d)):
            inc_dirs.append(os.path.join(arduino15_dir,'libraries',d))
        else:
            continue
        print('default libs:',inc_dirs[-1])
    # user libraries
    for d in os.listdir(user_lib_path):
        if os.path.isdir(os.path.join(user_lib_path,d,'include')):
            inc_dirs.append(os.path.join(user_lib_path,d,'include'))
        elif os.path.isdir(os.path.join(user_lib_path,d,'src')):
            inc_dirs.append(os.path.join(user_lib_path,d,'src'))
        elif os.path.isdir(os.path.join(user_lib_path,d)):
            inc_dirs.append(os.path.join(user_lib_path,d))
        else:
            continue
        print('user libs:',inc_dirs[-1])
    # stdlib
    is_xtensa = chip_id in xtensa_chips
    cc_dir = os.path.join(packages_dir,'esp32','tools','esp-x32' if is_xtensa else 'esp-rv32')
    for d in os.listdir(cc_dir):
        compiler = os.path.join(cc_dir,d,'bin',
            ('%s.exe' if sys.platform == 'win32' else '%s')%
            ('xtensa-esp-elf-g++' if is_xtensa else 'riscv32-esp-elf-g++') if use_cpp else 
            ('xtensa-esp-elf-gcc' if is_xtensa else 'riscv32-esp-elf-gcc'))
        elfpath = os.path.join(cc_dir,d,'xtensa-esp-elf' if is_xtensa else 'riscv32-esp-elf')
        if os.path.isdir(elfpath) and os.path.isfile(compiler):
            print('compiler:',compiler)
            print('elfpath:',elfpath)
            cc_dir = elfpath
            break
    else:
        raise FileNotFoundError('cc')
    # cpp
    inc_dirs.append(os.path.join(cc_dir,'include'))
    print('c_stdlib:',inc_dirs[-1])
    for d in os.listdir(os.path.join(cc_dir,'include','c++')):
        path = os.path.join(os.path.join(cc_dir,'include','c++',d))
        if os.path.isdir(path):
            print('cpplib:',path)
            inc_dirs.append(path)
            break
    else:
        raise FileNotFoundError('cpp')
    return inc_dirs,defs,compiler

def get_hostname():
    try:
        if sys.platform == 'win32':
            return os.environ['COMPUTERNAME']
        else:
            import socket
            return socket.gethostname()
    except Exception as e:
        print("WARNING: cannot get hostname:",e)
        return 'localhost'

def generate_c_cpp_properties_json(
                         data_ref:dict,
                         chip_variant:TypeChipVariant,
                         chip_id:TypeChipId,
                         idf_version:str,
                         use_cpp=False,
                         user_lib_path=default_user_lib_path,
                         arduino15_dir=default_arduino15_path,
                         c_std='c23',
                         cpp_std='c++23',
                         custom_includes:list[str]=[],
                         custom_defines:list[str]=[]
                         ):
    incs,defs,cc = find_include_dirs(chip_variant,chip_id,idf_version,use_cpp,user_lib_path,arduino15_dir)
    if type(data_ref['configurations']) != list:
        raise TypeError
    username = os.environ['USERNAME'if sys.platform=='win32'else'USER']
    cur_name = f"{chip_variant}-{username}@{get_hostname()}({platform.system()})"
    ref = {}
    for ci in custom_includes:
        print('custom_include:',ci)
        incs.append(ci)
    print('cur_name:',cur_name)
    for cd in custom_defines:
        defs.append(cd)
    ref = {
                "name": cur_name,
                "defines": defs,
                "compilerPath": cc,
                "cStandard": c_std,
                "cppStandard": cpp_std,
                "intelliSenseMode": "${default}",
                "includePath": incs
            }
    index = 0
    for i,conf in enumerate(data_ref['configurations']):
        if type(conf)==dict and 'name' in conf and conf['name'] == cur_name:
            print('WriteMode: Modify')
            index = i
            break
    else:
        print("WriteMode: Append")
        data_ref['configurations'].append(ref)
        return
    data_ref['configurations'][index] = ref

if __name__ == "__main__":
    cwd = os.path.abspath(os.path.dirname(sys.argv[0]))
    print('current_work_dir:',cwd)
    with open('./cpptoolsjson_idfversion.cfg','r',encoding='utf-8') as f:
        idfversion = f.read().strip()
    print('idfversion:',idfversion)
    vscode_dir = os.path.join(cwd,'.vscode')
    json_path = os.path.join(vscode_dir,'c_cpp_properties.json')
    os.makedirs(vscode_dir,exist_ok=True)
    data = {
        'configurations':[],
        'version':4
    }
    if os.path.isfile(json_path):
        try:
            with open(json_path,'r',encoding='utf-8') as f:
                rdata = json.load(f)
            if type(rdata)==dict and 'configurations' in rdata:
                data = rdata
        except json.decoder.JSONDecodeError:
            print('load old json failed')
            pass
    custom_includes = []
    custom_defines = []
    filename_custom_inc = "cpptoolsjson_includes.cfg"
    filename_custom_def = "cpptoolsjson_defines.cfg"
    if os.path.isfile(os.path.join(cwd,filename_custom_inc)):
        with open(os.path.join(cwd,filename_custom_inc)) as f:
            for line in f.readlines():
                line = line.strip()
                if line:
                    custom_includes.append(line)
    else:
        print('custom includes file not found')
    if os.path.isfile(os.path.join(cwd,filename_custom_def)):
        with open(os.path.join(cwd,filename_custom_def),'r',encoding='utf-8') as f:
            for line in f.readlines():
                line = line.strip()
                if line:
                    custom_defines.append(line)
    else:
        print('custom defines file not found')
    generate_c_cpp_properties_json(data,'esp32c3','esp32c3',idfversion,True,custom_includes=custom_includes,custom_defines=custom_defines)
    with open(json_path,'w',encoding='utf-8') as f:
        json.dump(
            data,
            f,
            indent=4
        )


        