import sys
import os
import shutil

sdk = os.environ['BUILD_SDK']
cfg = os.environ["BUILD_CONFIG"]

# Get the absolute path of the current script
script_path = os.path.abspath(__file__)

# Get the directory of the current script
script_dir = os.path.dirname(script_path)

build_root = f'{script_dir}/ion-{cfg}'
prefix_install = f'{script_dir}/../../install/{sdk}-{cfg}'


assert(os.path.exists(build_root))
assert(os.path.exists(prefix_install))

def cp_lib(path, name, dest):
    file = f'lib{name}.a'
    shutil.copyfile(f'{path}/{file}', f'{prefix_install}/lib/{file}')

cp_lib(f'{build_root}/src/dawn/common',  'dawn_common', f'{prefix_install}/lib')
cp_lib(f'{build_root}/src/dawn/glfw',    'dawn_glfw', f'{prefix_install}/lib')
cp_lib(f'{build_root}/src/dawn/native',  'dawn_native', f'{prefix_install}/lib')
cp_lib(f'{build_root}/src/dawn/native',  'webgpu_dawn', f'{prefix_install}/lib')
cp_lib(f'{build_root}/src/dawn/platform', 'dawn_platform', f'{prefix_install}/lib')
cp_lib(f'{build_root}/src/dawn/samples', 'dawn_sample_utils', f'{prefix_install}/lib')
cp_lib(f'{build_root}/src/dawn/utils',   'dawn_utils', f'{prefix_install}/lib')
cp_lib(f'{build_root}/src/dawn/wire',    'dawn_wire',  f'{prefix_install}/lib')
cp_lib(f'{build_root}/src/dawn',         'dawn_proc',  f'{prefix_install}/lib')
cp_lib(f'{build_root}/src/dawn',         'dawncpp',    f'{prefix_install}/lib')

shutil.copytree(f'{build_root}/../include', f'{prefix_install}/include', dirs_exist_ok=True)

shutil.copytree(f'{build_root}/gen/include', f'{prefix_install}/include', dirs_exist_ok=True)
