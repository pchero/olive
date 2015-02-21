#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#  connect.py
#  
#  Copyright 2014 Sungtae Kim <pchero21@gmail.com>
#  
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#  
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#  
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, 
#  MA 02110-1301, USA.
#  
#  

from starpy import manager

g_amiId='admin'
g_amiPass='amp111'
g_amiIp='192.168.200.30'
g_amiPort=5038

class connAst:
    def login(self):
        def on_login_success(self, ami):
            self.ami_factory.ping().addCallback(ping_response)
            return ami
        
        def on_login_error(self, reason):
            print('Failed to log into AMI')
            return reason
        
        def ping_response(self, ami):
            print('Got a ping response!')
            return ami
        
        self.ami_factory = manager.AMIFactory(g_amiId, g_amiPass)
        self.ami_factory.login(g_amiIp, g_amiPort).addCallbacks(on_login_success, on_login_error)

def main():
    connect = connAst()
    
    connect.login()
    
    return 0

if __name__ == '__main__':
	main()

