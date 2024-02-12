# bitfield logic test script

stri = [0x0,0x40,0x0,-127,-127,-127,-127]


idx = 3

for x in range(2,len(stri)):
    if stri[x] < 0:
        stri[1] |= 1<<(8-idx)
        idx+=1

print(bin(stri[1]),(1<<(8-idx)))