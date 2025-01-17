import os
import sys


inpath=sys.argv[1]
infile=sys.argv[2]
outfull="/scratch/ianli/coreGR/"+infile+".gr"

infp=open(inpath+infile, "r")
outfp=open(outfull, "w")

vertices=0
edges=0
outstring=""

while True:
    l = infp.readline().strip()
    if not l:
        break
    line = map(int, l.split(' '))
    learntID=line[0]
    if learntID > vertices:
        vertices = learntID
    for antecedentID in reversed(line[:-1]):
        if antecedentID == 0:
            break
        outstring+=str(antecedentID)+" "+str(learntID)+"\n"
        edges+=1

header="p tw "+str(vertices)+" "+str(edges)+"\n"
outfp.write(header)
outfp.write(outstring)

infp.close()
outfp.close()
