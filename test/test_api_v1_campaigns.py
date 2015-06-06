# test_api_campaign.py
#  Created on: Jun 5, 2015
#      Author: pchero


import pycurl
import cStringIO
import base64
import sys
import json

class olive_curl:    
    def https_send(self, url, auth, method, data):
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
            conn.setopt(pycurl.POST, 1)
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

class campaigns(olive_curl):
    def __init__(self, url, auth):
        self.url = url
        self.auth = auth

    def get_all(self):
        url = self.url + "/api/v1/campaigns"
        ret, res = self.https_send(url, self.auth, "GET", None)
        
        return ret, json.loads(res)
    
    def get(self, uuid):
        url = self.url + "/api/v1/campaigns/" + uuid
        
        ret, res = self.https_send(url, self.auth, "GET", None)
    
        return ret, json.loads(res)

    def create(self, data):
        url = self.url + "/api/v1/campaigns"
        
        ret, res = self.https_send(url, self.auth, "POST", data)
        
        return ret, json.loads(res)
        
    def delete(self, uuid):
        url = self.url + "/api/v1/campaigns/" + uuid
        
        ret, res = self.https_send(url, self.auth, "DELETE", None)
    
        return ret, res

    def update(self, uuid, data):
        url = self.url + "/api/v1/campaigns/" + uuid
        
        ret, res = self.https_send(url, self.auth, "PUT", data)
    
        return ret, json.loads(res)

    def updates(self, data):
        url = self.url + "/api/v1/campaigns"
        
        ret, res = self.https_send(url, self.auth, "PUT", data)
    
        return ret, json.loads(res)
            

def test_scenario_01():
    '''
    create -> delete -> get speficify -> get all
    '''
    camp = campaigns("https://localhost:8081", "admin:1234")
    
    # Create
    ret, res = camp.create('{"detail": "test api v1 campaign create.", "name":"test api v1 campaign"}')
    if ret != 200:
        raise Exception("Could not pass campaigns test")
    if res["result"] != 1:
        raise Exception("Could not pass campaign create")
    if res["message"]["detail"] != "test api v1 campaign create.":
        raise Exception("Could not pass campaign create")
    if res["message"]["name"] != "test api v1 campaign":
        raise Exception("Could not pass campaign create")
    
    # delete
    uuid = res["message"]["uuid"]
    ret, res = camp.delete(uuid)
    if ret != 200:
        raise Exception("Could not pass the test.")
    
    # get
    ret, res = camp.get(uuid)
    if ret != 200:
        raise Exception("Could not pass the test.")
    if res["message"] is not None:
        raise Exception("Could not pass the test.")
    
    # get all
    ret, res = camp.get_all()
    if ret != 200:
        raise Exception("Could not pass the test.")
    for i in res["message"]:
        if i["uuid"] == uuid:
            raise Exception("Could not pass the test.")


def test_scenario_02():
    '''
    create -> delete -> get speficify -> get all
    '''
    camp = campaigns("https://localhost:8081", "admin:1234")
    
    # Create
    ret, res = camp.create('{"detail": "test_camp_01_detail", "name":"test_camp_01"}')
    if ret != 200:
        raise Exception("Could not pass campaigns test")
    if res["result"] != 1:
        raise Exception("Could not pass campaign create")
    if res["message"]["detail"] != "test_camp_01_detail":
        raise Exception("Could not pass campaign create")
    if res["message"]["name"] != "test_camp_01":
        raise Exception("Could not pass campaign create")
    uuid1 = res["message"]["uuid"]

    # Create
    ret, res = camp.create('{"detail": "test_camp_02_detail", "name":"test_camp_02"}')
    if ret != 200:
        raise Exception("Could not pass campaigns test")
    if res["result"] != 1:
        raise Exception("Could not pass campaign create")
    if res["message"]["detail"] != "test_camp_02_detail":
        raise Exception("Could not pass campaign create")
    if res["message"]["name"] != "test_camp_02":
        raise Exception("Could not pass campaign create")
    uuid2 = res["message"]["uuid"]
 
    ret, res = camp.updates('[{"uuid":"%s", "detail":"test_camp_01_detail_change"}, {"uuid":"%s", "detail":"test_camp_02_detail_change"}]' 
                 % (uuid1, uuid2))
    if ret != 200:
        raise Exception("Could not pass the test")
    
    for i in res["message"]:
        if i["uuid"] == uuid1:
            if i["detail"] != "test_camp_01_detail_change":
                raise Exception("Could not pass the test.")

        if i["uuid"] == uuid2:
            if i["detail"] != "test_camp_02_detail_change":
                raise Exception("Could not pass the test.")

    ret, res = camp.delete(uuid1)
    if ret != 200:
        raise Exception("Could not pass the test.")
    
    ret, res = camp.delete(uuid2)
    if ret != 200:
        raise Exception("Could not pass the test.")
    
    
    # get
    ret, res = camp.get(uuid1)
    if ret != 200:
        raise Exception("Could not pass the test.")
    if res["message"] is not None:
        raise Exception("Could not pass the test.")

    ret, res = camp.get(uuid2)
    if ret != 200:
        raise Exception("Could not pass the test.")
    if res["message"] is not None:
        raise Exception("Could not pass the test.")

    
    # get all
    ret, res = camp.get_all()
    if ret != 200:
        raise Exception("Could not pass the test.")
    for i in res["message"]:
        if i["uuid"] == uuid1:
            raise Exception("Could not pass the test.")
        if i["uuid"] == uuid2:
            raise Exception("Could not pass the test.")

if __name__ == "__main__":
    test_scenario_01()
    test_scenario_02()
