filenames: str = "abcdefghijklmnopqrstuvwxyz"
sequence: str = "     "
repeat: int=25

fwrite = open(f'{input()}.txt', 'w')
limit = filenames[:]


def printdiamond(h: int, filename: str):
    for i in range(h):
        fwrite.write(f'write ./outputs/{filename}.txt {" "*(h-i)+filename*(i*2+1)}\n')
    for i in range(h-2, -1, -1):
        fwrite.write(f'write ./outputs/{filename}.txt {" "*(h-i)+ filename*(i*2+1)}\n')


for filename in filenames:
    printdiamond(15, filename)

for i in range(1,repeat):
    fwrite.write(f'read ./outputs/{i}.txt\n')
    fwrite.write(f'empty ./outputs/{i}.txt\n')


fwrite.close()


