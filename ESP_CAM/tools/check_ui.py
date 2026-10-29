import time
import urllib.request

time.sleep(2)
html = urllib.request.urlopen("http://192.168.6.221/", timeout=8).read().decode("utf-8", "replace")
print("max100", 'max="100"' in html)
print("value60", 'value="60"' in html)
print("old49", 'max="49"' in html)
