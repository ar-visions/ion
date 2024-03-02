import sys
import os
import platform
import shutil

os_name = platform.system()
apple = os_name == 'Darwin'
linux = os_name == 'Linux'
pre = 'lib' if apple or linux else ''
lib_ext = '.a' if apple or linux else '.lib'
sdk = os.environ.get('BUILD_SDK') if os.environ.get('BUILD_SDK') else 'native'
cfg = os.environ.get('BUILD_CONFIG') if os.environ.get('BUILD_CONFIG') else 'debug'

# Get the absolute path of the current script
script_path = os.path.abspath(__file__)

# Get the directory of the current script
script_dir = os.path.dirname(script_path)

build_root = f'{script_dir}/ion-{cfg}'
prefix_install = f'{script_dir}/../../install/{sdk}-{cfg}'

assert(os.path.exists(build_root))
assert(os.path.exists(prefix_install))

def cp_includes(src, dst):
    assert os.path.isdir(src)
    assert os.path.isdir(dst)
    shutil.copytree(src, dst, dirs_exist_ok=True)

def cp_lib_files(p, dest):
    for root, dirs, files in os.walk(p):
        for f in files:
            _, ext = os.path.splitext(f)
            if ext == lib_ext:
                lib_file = os.path.join(root, f)
                assert os.path.exists(lib_file)
                print(f'copying library file {lib_file}')
                shutil.copyfile(lib_file, f'{dest}/{f}') 
                
def cp_lib_folders(path, dest):
    for root, dirs, files in os.walk(path):
        for subfolder in dirs:
            p = os.path.join(root, subfolder)
            cp_lib_files(p, dest)

cp_lib_files(f'{build_root}/src/dawn', f'{prefix_install}/lib')
cp_lib_files(f'{build_root}/src/tint', f'{prefix_install}/lib')


cp_lib_folders(f'{build_root}/third_party/abseil/absl', f'{prefix_install}/lib')
cp_lib_folders(f'{build_root}/third_party/glfw',        f'{prefix_install}/lib')
cp_lib_folders(f'{build_root}/third_party/glslang',     f'{prefix_install}/lib')
cp_lib_folders(f'{build_root}/third_party/spirv-tools', f'{prefix_install}/lib')

cp_includes(f'{build_root}/../include',  f'{prefix_install}/include')
cp_includes(f'{build_root}/gen/include', f'{prefix_install}/include')