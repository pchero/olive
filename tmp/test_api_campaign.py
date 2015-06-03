# test_api_campaign.py
#  Created on: May 31, 2015
#      Author: pchero

import sys
import pycurl
from StringIO import StringIO

def print_usage():
    print "%s <uri>\n" % (sys.argv[0])
    return


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print_usage()
        return False
    
    curl = pycurl.Curl()
    curl.setopt()
    
    buffer = StringIO()
    c = pycurl.Curl()
    c.setopt(c.URL, 'http://pycurl.sourceforge.net/')
    c.setopt(c.WRITEDATA, buffer)
    c.perform()
    c.close()
    
    body = buffer.getvalue()
    # Body is a string in some encoding.
    # In Python 2, we can print it without knowing what the encoding is.
    print(body)

