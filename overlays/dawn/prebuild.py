import subprocess
print('running dawn dependency fetch')
subprocess.run(['python3', 'tools/fetch_dawn_dependencies.py', '--use-test-deps'])