import http.client, json
c=http.client.HTTPConnection('localhost',8000,timeout=10)
c.request('POST','/mcp',body=json.dumps({'jsonrpc':'2.0','id':1,'method':'initialize','params':{'protocolVersion':'2025-03-26','capabilities':{},'clientInfo':{'name':'CodeBuddy','version':'1'}}}),headers={'Content-Type':'application/json','Accept':'application/json, text/event-stream'})
r=c.getresponse(); print(r.status); print(r.read().decode()[:2000])
