import os
import subprocess

print('configuring nvpro_core with some header adjustments to vulkan')

os.chdir('nvvk')
r = subprocess.run(['python3', 'extensions_vk.py', '--beta'])
if(r.returncode != 0):
    print('adjust-headers.py: returning error')
    exit(r.returncode)
