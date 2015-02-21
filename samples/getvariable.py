#! /usr/bin/env python
"""Demonstrate usage of getVariable on the agi interface...
"""
from twisted.internet import reactor
from starpy import fastagi, utilapplication
import logging, time, pprint

log = logging.getLogger('hellofastagi')

def envVars(agi):
    """Print out channel variables for display"""
    vars = [
        s.split(' -- ')[0].strip()
        for x in agi.getVarable.__doc__.splitlines
        if len(x.split(' -- ')) == 2
    ]
    for var in vars:
        yield var


def testFunction(agi):
    """Print out known AGI variables"""
    log.debug('testFunction')

if __name__ == "__main__":
    logging.basicConfig()
    APPLICATION = utilapplication.UtilApplication()
    APPLICATION.handleCallsFor('s', testFunction)
    APPLICATION.agiSpecifier.run(APPLICATION.dispatchIncomingCall)
    reactor.run()
 
