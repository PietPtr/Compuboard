import subprocess
import os, sys

shell = subprocess.Popen(['sh'], stdout=subprocess.PIPE, shell=True)
line = ''

while True:
    print("> ", end='')
    sys.stdout.flush()

    line = shell.stdout.readline().decode('utf-8')
    print("< " + line, end='')
    sys.stdout.flush()
