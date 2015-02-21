#! /usr/bin/env python


def login(self):

  def on_login_success(self, ami):
      self.ami_factory.ping().addCallback(ping_response)
      return ami

  def on_login_error(self, reason):
      print "Failed to log into AMI"
      return reason

  def ping_response(self, ami):
      print "Got a ping response!"
      return ami

  self.ami_factory = manager.AMIFactory("user", "mysecret")
  self.ami_factory.login("127.0.0.1", 5038).addCallbacks(on_login_success, on_login_error)

login(self)
