# A program to check if the compiler passed
# is a Mingw-w64 based compiler.

import os
import subprocess
import argparse
import sys


def is_mingw(CC):
    if sys.platform != 'win32':
        # Let's check only if we are in Windows
        return False

    # Check if the compiler in VS
    if os.path.basename(CC) in ['cl', 'cl.exe']:
        return False

    try:
        com = subprocess.run(
            [CC, '--version'], check=False, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
        )
    except FileNotFoundError:
        return False

    if com.returncode != 0:
        # Both gcc and clang compilers from mingw should understand that argument
        # and should return a status code of 0.
        return False

    out, err = com.stdout, com.stderr
    if 'clang' in out or 'Clang' in out:
        # Check it's not clang-cl wrapper
        if 'CL.EXE COMPATIBILITY' in out:
            return False

        return True

    if 'gcc.exe' in out:
        return True

    return True

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='Check if the compiler passed in a mingw based compiler.')
    parser.add_argument('compiler', help='path to the compiler to check.')

    args = parser.parse_args()
    CC = args.compiler
    print('true' if is_mingw(CC) else 'false')
