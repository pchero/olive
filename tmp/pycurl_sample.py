import pycurl
import cStringIO
import base64
import sys


def https_send(url, auth, method, data):
    '''
    @param url: Access url.
    @param auth: Authorization info.
    @param method: [GET, POST, PUT, DELETE]
    @param data: Send data
    
    @return: code, get_data.
    '''
    
    if url is None:
        raise Exception("No url to send.")
    
    headers = None 
    if auth is not None:
        headers = { 'Authorization' : 'Basic %s' % base64.b64encode(auth) }
    
    response = cStringIO.StringIO()
    conn = pycurl.Curl()
    
    if headers is not None:
        conn.setopt(pycurl.HTTPHEADER, ["%s: %s" % t for t in headers.items()])
    
    conn.setopt(pycurl.URL, url)
    
    # set no ssl certification check    
    conn.setopt(pycurl.SSL_VERIFYPEER, False)
    conn.setopt(pycurl.SSL_VERIFYHOST, False)
    
    # set response buffer
    conn.setopt(pycurl.WRITEFUNCTION, response.write)
    
    # set request buffer
    if data is not None:
        conn.setopt(pycurl.POSTFIELDS, data)

    # set method    
    if method == "POST":
        conn.setopt(pycurl.HTTPPOST, 1)
    elif method == "GET":
        conn.setopt(pycurl.HTTPGET, 1)
    elif method == "PUT":
        conn.setopt(pycurl.CUSTOMREQUEST, "PUT")
    elif method == "DELETE":
        conn.setopt(pycurl.CUSTOMREQUEST, "DELETE")
    else:
        raise Exception("Unsupported method.")

    # perform
    conn.perform()
    
    # return    
    return conn.getinfo(pycurl.HTTP_CODE), response.getvalue()

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print( "Usage:")
        print("    %s <url> <auth> <method> <data>" % (sys.argv[0]))
        raise "Wrong usage."
    
    code, res = https_send(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4])
    
    print "Get result. code[%s], res[%s]" % (code, res)
    
    
    