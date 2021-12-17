filenames: str = "abcdefghijklmnopqrstuvwxyz"
sequence: str = "xyz"
repeat: int=7

fwrite = open(f'{input()}.txt', 'w')

for i in range(1,repeat+1):
    for item in sequence:
        for filename in filenames:
            fwrite.write(f"write ./outputs/{filename}.txt {filename}<{item}{i}>\n")



for filename in filenames:
    fwrite.write(f"read ./outputs/{filename}.txt\n")
    fwrite.write(f"empty ./outputs/{filename}.txt\n")
fwrite.close()


