# test_api_campaign.py
#  Created on: Jun 5, 2015
#      Author: pchero


import pycurl
import cStringIO
import base64
import sys
import json

class olive_curl:    
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

class campaigns:
    def __init__(self, url, auth):
        self.url = url
        self.auth = auth
        
    def create(self, data):
        url = "https://localhost:8081/api/v1/campaigns"
        auth = "admin:1234"
        
        ret, res = https_send(url, auth, "POST", data)
        if ret != 200:
            raise Exception("Could not pass campaign create test")
        
        j_res = json.dump(res)
        if j_res is None:
            raise Exception("Could not pass campaign create test")
        
        if j_res["detail"] != "test api v1 campaign create.":
            raise Exception("Could not pass campaign create test")
    
        if j_res["name"] != "test api v1 campaign create.":
            raise Exception("test api v1 campaign")
        
        uuid = j_res["uuid"]
        
        # test delete campaign
        url = "https://localhost:8081/api/v1/campaigns/%s" % uuid
        ret, res = https_send(url, auth, "DELETE", None)
        if ret != 200:
            raise Exception("Could not pass campaign delete test")

if __name__ == "__main__":
    auth = "admin:1234"
    
    # test get campaign list
    url = "https://localhost:8081/api/v1/campaigns"
    ret, res = https_send(rul, auth, "GET", None)
    if ret != 200:
        raise Exception("Could not pass campaigns test")
    
    # test create new campaign
    url = "https://localhost:8081/api/v1/campaigns"
    data = '{"detail": "test api v1 campaign create.", "name":"test api v1 campaign"}'
    ret, res = https_send(url, auth, "POST", data)
    if ret != 200:
        raise Exception("Could not pass campaign create test")
    
    j_res = json.dump(res)
    if j_res is None:
        raise Exception("Could not pass campaign create test")
    
    if j_res["detail"] != "test api v1 campaign create.":
        raise Exception("Could not pass campaign create test")

    if j_res["name"] != "test api v1 campaign create.":
        raise Exception("test api v1 campaign")
    
    uuid = j_res["uuid"]
    
    # test delete campaign
    url = "https://localhost:8081/api/v1/campaigns/%s" % uuid
    ret, res = https_send(url, auth, "DELETE", None)
    if ret != 200:
        raise Exception("Could not pass campaign delete test")

    
