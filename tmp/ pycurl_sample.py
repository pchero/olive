import pycurl
import cStringIO
import base64

headers = { 'Authorization' : 'Basic %s' % base64.b64encode("admin:1234") }

response = cStringIO.StringIO()
conn = pycurl.Curl()

conn.setopt(pycurl.VERBOSE, 1)
conn.setopt(pycurl.HTTPHEADER, ["%s: %s" % t for t in headers.items()])

conn.setopt(pycurl.URL, "https://localhost:8081/api/v1/campaigns")
conn.setopt(pycurl.POST, 1)

conn.setopt(pycurl.SSL_VERIFYPEER, False)
conn.setopt(pycurl.SSL_VERIFYHOST, False)

conn.setopt(pycurl.WRITEFUNCTION, response.write)

#post_body = "foobar"
#conn.setopt(pycurl.POSTFIELDS, post_body)
conn.setopt(pycurl.HTTPGET, 1)


conn.perform()

http_code = conn.getinfo(pycurl.HTTP_CODE)
if http_code is 200:
   print response.getvalue()
