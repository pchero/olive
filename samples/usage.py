
from starpy import manager
#usernmae, password
f = manager.AMIFactory(sys.argv[1], sys.argv[2])
df = f.login('server', port)

self.ami.originate(
    self.callbackChannel,
    self.ourContext, id(self), 1,
    timeout = 15,


def onChannelHangup(ami, event):
    """Deal with the hangup of an event"""
    if event['uniqueid'] == self.uniqueChannelId:
        log.info("""AMI Detected close of our channel: %s""", self.uniqueChannelId)
        self.stopTime = time.time()
        # give the user a few seconds to put down the hand-set
        reactor.callLater(2, df.callback, event)
        self.ami.deregisterEvent('Hangup', onChannelHangup)

delf.ami.registerEvent('Hangup', onChannelHangup)
return df.addCallback(self.onHangup, callbacks=5)
