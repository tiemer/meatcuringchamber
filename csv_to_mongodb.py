import pymongo
import csv

myclient = pymongo.MongoClient("mongodb://localhost:27017/")

mydb = myclient["test"]
mycol = mydb["log"]

with open('test.csv') as csvfile:
    readCSV = csv.reader(csvfile, delimiter=';')
    for row in readCSV:
        mydoc = {
                "datetime":row[0],
                "tempSP":row[1],
                "tempErr":row[2],
                "tempPV":row[3],
                "tempExt":row[4],
                "relhumSP":row[5],
                "relhumErr":row[6],
                "relhumPV":row[7],
                "relhumExt":row[8]
                }
        mycol.insert_one(mydoc)
