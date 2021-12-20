filenames: str = "abcdefghijklmnopqrstuvwxyz"
sequence: str = "     "
repeat: int=10

fwrite = open(f'{input()}.txt', 'w')
limit = filenames[:]

for i in range(1,repeat+1):
    for item in sequence:
        for filename in limit:
            fwrite.write(f"write ./outputs/{filename}.txt {item}{filename}<{i}>\n")



for filename in limit:
    fwrite.write(f"read ./outputs/{filename}.txt\n")
    fwrite.write(f"empty ./outputs/{filename}.txt\n")
fwrite.close()


