__author__ = 'egarrido'
import csv
import matplotlib.pyplot as plt
dfShoe = []
dfSolar = []
dfShoe2 = []
dfSolar2 = []

shoeFilePath = "/home/jester/thesisTUDelft/statistics/"
shoeFileName = "shoeTrace2.csv"
shoeFile = shoeFilePath + shoeFileName
f_shoe = open(shoeFile, 'rt')
try:
   reader = csv.reader(f_shoe)
   for row in reader:
       # print row
       # st =
       # if row[0].find('e') == -1:
       #     dfShoe += 0
       # else:
        dfShoe +=row
        # dfShoe += float(row)
finally:
    f_shoe.close()

for i in range (0,len(dfShoe)):
    if dfShoe[i].find('e') != -1:
        dfShoe[i] = 0
    else:
        dfShoe[i] = float(dfShoe[i])*1000.0
        dfShoe[i] = int(round(dfShoe[i]))


csvName = "energyShoeTrace.txt"
csvFile = shoeFilePath + csvName
with open(csvFile, "w") as output:
    writer = csv.writer(output,lineterminator='\n')
    for val in dfShoe:
        writer.writerow([val])
    # writer.close()

solarFilePath = "/home/jester/thesisTUDelft/statistics/"
solarFileName = "solarTrace1.csv"
solarFile = solarFilePath + solarFileName

f_solar = open(solarFile, 'rt')
try:
   reader = csv.reader(f_solar)
   for row in reader:
        dfSolar += row

finally:
    f_solar.close()

    for i in range(0, len(dfSolar)):
        if dfSolar[i].find('e') != -1:
            dfSolar[i] = 0
        else:
            dfSolar[i] = float(dfSolar[i]) * 100.0
            dfSolar[i] = int(round(dfSolar[i]))



csvName2 = "energySolarTrace.txt"
csvFile2 = shoeFilePath + csvName2
with open(csvFile2, "w") as output:
    writer2 = csv.writer(output,lineterminator='\n')
    for val in dfSolar:
        writer2.writerow([val])
#
plt.figure()
plt.plot(dfSolar)
plt.draw()

plt.figure()
plt.plot(dfShoe)
plt.draw()
plt.show()