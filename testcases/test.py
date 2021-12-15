filenames: str = "abcdefghijklmnopqrstuv"
sequence: str = "abcde"
repeat: int=15

fwrite = open(f'{input()}.txt', 'w')

for i in range(1,repeat+1):
    for item in sequence:
        for filename in filenames[:3]:
            fwrite.write(f"write ./outputs/{filename}.txt {filename} <{i}>\n")



for filename in filenames[:3]:
    fwrite.write(f"read ./outputs/{filename}.txt\n")
    fwrite.write(f"empty ./outputs/{filename}.txt\n")
fwrite.close()


