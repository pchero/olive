#! /usr/bin/env python


from twisted.application import service, internet
from twisted.internet import reactor, defer
from starpy import manager, fastagi
import utilapplication
import menu
import os, logging, pprint, time


from twisted.trial import unittest

log = logging.getLogger('callduration')
APPLICATION = utilapplication.UtilApplication()

def main1():
  def onConnect(ami):
    def onResult(result):
      print 'Result type:{}'.format(type(result))
      print result
      return ami.logoff()
    
    def onError(reason):
      print reason.getTraceback()
      return reason
      
    def onFinished(result):
      reactor.stop()
#    df = ami.ping()
      
    df = ami.command('sip show peers')
#    print(ami.actionid())
    print(type(ami))
    print df
    df.addCallbacks(onResult, onError)
    df.addCallbacks(onFinished, onFinished)
      
    return df

#  amiDF = APPLICATION.amiSpecifier.login().addCallback(onConnect)
  amiDF = APPLICATION.amiSpecifier.login()
  amiDF.addCallback(onConnect)

  
def onConnect(ami):
  def onSuccess(result):
    print('Connect success. {}'.format(result))
    return None
  
  def onFail(reason):
    print('Connect failed. {}'.format(reason))
    return reason
  

def onDisconnect(ami):
  ami.logoff()
  reactor.stop()
  print('someting...')
 


def onResult(result):
  print result
#    return ami.logoff()

def onError(reason):  
  print reason.getTraceback()

def onFinished(result):
  reactor.stop()
  print('Finished!!')
  return

def resultSipShowPeers(result):
  for i in range(len(result)):
    
    # just continue first and last. Because there are menu and description messages.
    if i == 0 or i == (len(result) - 1):
      continue
    
#    print('peer {} : {}'.format(i, result[i]))
    
    # Let's get the sip's names.
    tmp = result[i].split()
#    print(tmp)
    sipPeerName = tmp[0].split('/')
    print sipPeerName[0]
    
    # insert cheach item into db
    ''' TODO:... Need insert.'''

  return None
   
def getSipPeers(ami):
  '''ami is an AMIProtocol class
  '''  
  
  print('sipShowpeers!!!!')
  df = ami.command('sip show peers')
  df.addCallbacks(resultSipShowPeers, onError)
  
#  df = ami.command('sip show peers')
#  df.addCallbacks(onResult, onError)

#  df.addCallbacks(onFinished, onFinished)
  return


class sipHandler:
  def __init__(self):
    dbConn = None
  
    
def main():
  amiDF = APPLICATION.amiSpecifier.login()
#  amiDF.addCallback(onConnect)
#  df = amiDF.command('sip show peers')
  amiDF.addCallback(getSipPeers)
#  amiDF.addCallbacks(onResult, onError)
  
  amiDF.addCallback(onDisconnect)
#  amiDF.logoff()
#  reactor.stop()
  

if __name__ == "__main__":
  logging.basicConfig()
  manager.log.setLevel(logging.DEBUG)
#  reactor.callWhenRunning(main)
#  reactor.run()
#  unittest.TestCase.callWhenRunning(main)
  reactor.callWhenRunning(main)
  reactor.run()
  

"""
from twisted.application import service, internet
from twisted.internet import reactor, defer
from starpy import manager, fastagi
import utilapplication
import menu
import os, logging, pprint, time

log = logging.getLogger('callduration')
APPLICATION = utilapplication.UtilApplication()

def main():
    def onConnect(ami):
        def onResult(result):
            print 'Result type:{}'.format(type(result))
            print result
            return ami.logoff()
        
        def onError(reason):
            print reason.getTraceback()
            return reason
        
        def onFinished(result):
            reactor.stop()
        
        df = ami.command('sip show peer 2000')
        print(type(df))
        print(type(ami))
        print ami
        df.addCallbacks(onResult, onError)
        df.addCallbacks(onFinished, onFinished)
        
        return df
    amiDF = APPLICATION.amiSpecifier.login().addCallback(onConnect)

if __name__ == "__main__":
    logging.basicConfig()
    manager.log.setLevel(logging.DEBUG)
    reactor.callWhenRunning(main)
    reactor.run()
    reactor.stop()
"""