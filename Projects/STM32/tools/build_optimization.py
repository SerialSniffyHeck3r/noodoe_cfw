"""Shared Debug defaults, independent of the selected firmware profile.

Cube may recreate O0 defaults. Synchronization restores Os/g3 at the root,
removes only our obsolete folder policies, and retains deliberate user file
overrides. No C source, heap, assertion or linker boundary is changed.
"""
from pathlib import Path
import re
import xml.etree.ElementTree as ET

CC = 'com.st.stm32cube.ide.mcu.gnu.managedbuild.tool.c.compiler'
CPP = CC.replace('.c.compiler', '.cpp.compiler')
PROJECT = Path(__file__).resolve().parents[1]


def _root(config):
    compiler = config.find(f'folderInfo[@resourcePath=""]/toolChain/tool[@superClass="{CC}"]')
    if compiler is None:
        raise ValueError('Missing root C compiler')
    return compiler


def sync_debug(config):
    """Set default flags in a memory XML tree; Release is byte-for-byte untouched.

    Existing user file/folder settings remain explicit debugging overrides.
    They cannot enable LTO: validation fails rather than erasing user settings.
    """
    if config.get('name') != 'Debug':
        return
    prefix = config.get('id') + '.noodoe.'
    for folder in list(config.findall('folderInfo')):
        ident = folder.get('id', '')
        if ident == prefix+'lvgl' or ident.startswith(prefix+'Middlewares.'):
            config.remove(folder)
    _root(config)  # Fail explicitly if the required C root is absent.
    compilers = [t for t in config.findall('folderInfo[@resourcePath=""]/toolChain/tool')
                 if t.get('superClass') in (CC, CPP)]
    for compiler in compilers:
        for suffix, value in (('optimization.level', 'os'), ('debuglevel', 'g3')):
            cls = compiler.get('superClass')+'.option.'+suffix
            option = compiler.find(f'option[@superClass="{cls}"]')
            if option is None:
                option = ET.SubElement(compiler, 'option', id=compiler.get('id')+'.noodoe.debug.'+suffix,
                                       superClass=cls, valueType='enumerated')
            option.set('value', cls+'.value.'+value)
    # Explicitly disable LTO in compilation AND linking. A stale slim-LTO object
    # must fail the build rather than silently re-enable whole-program codegen.
    for tool in config.findall('.//tool'):
        kind = tool.get('superClass', '')
        if tool not in compilers and not kind.endswith(('tool.c.linker', 'tool.cpp.linker')):
            continue
        cls = kind+'.option.otherflags'
        option = tool.find(f'option[@superClass="{cls}"]')
        if option is None:
            option = ET.SubElement(tool, 'option', id=tool.get('id')+'.noodoe.debug.flags',
                                   superClass=cls, valueType='stringList')
        if not any(x.get('value') == '-fno-lto' for x in option):
            ET.SubElement(option, 'listOptionValue', builtIn='false', value='-fno-lto')


def validate_debug(config):
    """Reject stale defaults/LTO and preserve file-level Og/O0 escape hatches.

    This is configuration validation, not a substitute for inspecting the
    actual generated compiler commands and the linked ELF memory budget.
    """
    if config.get('name') != 'Debug':
        return
    _root(config)
    owned = config.get('id')+'.noodoe.'
    for scope in config.findall('fileInfo')+config.findall('folderInfo'):
        if scope.get('resourcePath') and scope.get('id', '').startswith(owned):
            raise ValueError('Obsolete owned Debug override: '+scope.get('resourcePath'))
    compilers = [t for t in config.findall('folderInfo[@resourcePath=""]/toolChain/tool')
                 if t.get('superClass') in (CC, CPP)]
    for compiler in compilers:
        for suffix, value in (('optimization.level', 'os'), ('debuglevel', 'g3')):
            cls = compiler.get('superClass')+'.option.'+suffix
            option = compiler.find(f'option[@superClass="{cls}"]')
            if option is None or option.get('value') != cls+'.value.'+value:
                raise ValueError('Debug root must use -Os -g3: '+suffix)
    for option in config.findall('.//option'):
        cls = option.get('superClass', '')
        if cls.endswith('.option.otherflags'):
            values = [option.get('value', '')]+[x.get('value', '') for x in option]
            for value in values:
                if re.search(r'(?:^|\s)-flto(?:=\S+)?(?:\s|$)', value):
                    raise ValueError('Debug LTO override is not allowed: '+value)
                if re.search(r'(?:^|\s)-O(?:[0-9]+|g|s|z|fast)?(?:\s|$)', value):
                    raise ValueError('Use the explicit optimization setting, not hidden Debug flags: '+value)
        if cls.endswith('.option.debuglevel') and option.get('value') != cls+'.value.g3':
            raise ValueError('Debug scope must retain -g3')
    for tool in compilers+[t for t in config.findall('.//tool')
                           if t.get('superClass', '').endswith(('tool.c.linker', 'tool.cpp.linker'))]:
        if not any(x.get('value') == '-fno-lto' for x in tool.findall('.//listOptionValue')):
            raise ValueError('Debug compiler/linker must explicitly disable LTO')


if __name__ == '__main__':
    configs = ET.parse(PROJECT/'.cproject').findall('.//configuration')
    if sum(c.get('name') == 'Debug' for c in configs) != 1:
        raise ValueError('Expected exactly one Debug configuration')
    for config in configs:
        validate_debug(config)
    print('Debug policy verified: root -Os/-g3, no LTO, no hidden optimization flags.')
