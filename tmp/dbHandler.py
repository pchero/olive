# -*- coding: utf-8 -*-
"""
Created on Tue Jul  8 11:26:25 2014

@author: pchero
"""


''' Make db connector using twisted flatform... '''
import MySQLdb as mdb
import sys

try:
  con = mdb.connect('localhost', 'root', 'ekdtlsdmf4fkdgo', 'olive')
  
  with con:
    cur = con.cursor()
    cur.execute("DROP TABLE IF EXISTS Writers")
    cur.execute("CREATE TABLE Writers(Id INT PRIMARY KEY AUTO_INCREMENT, Name VARCHAR(23))")
    cur.execute("INSERT INTO Writers(Name) VALUES('Jack London')")
    cur.execute("INSERT INTO Writers(Name) VALUES('Honore de Balzac')")
    cur.execute("INSERT INTO Writers(Name) VALUES('Lion Feuchtwanger')")
    cur.execute("INSERT INTO Writers(Name) VALUES('Emile Zola')")
    cur.execute("INSERT INTO Writers(Name) VALUES('Truman capote')")
    
  
  
  cur = con.cursor()
  cur.execute("SELECT VERSION()")
  
  var = cur.fetchone()
  print "Database version: %s " % var
  
  cur.execute("SELECT * from sip")
  var = cur.fetchone()
  print "Database version: %s " % var
  
  
except mdb.Error, e:
  print "Error %d: %s" %(e.argv[0], e.args[1])
  sys.exit(1)

finally:
  if con:
    con.close()
  

'''
import _mysql
import sys

try:
  con = _mysql.connect('localhost', 'root', 'ekdtlsdmf4fkdgo', 'olive')
  
  con.query("SELECT VERSION()")
  result = con.use_result()
  
  print "MySQL version: %s" % result.fetch_row()
  
  con.query("SELECT * FROM sip")
  result = con.use_result()
  
  print "MySQL version: %s" % result.fetch_row()


except _mysql.Error, e:
  print "Error %d: %s" % (e.args[0], e.args[1])
  sys.exit(1)

finally:
  if con:
    con.close()
    
'''