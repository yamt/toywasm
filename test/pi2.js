load('test/pi.js')
print("pi = " + pi);
if (pi < 3.1415 || pi > 3.1416)
   throw new Error("unexpected pi value")
quit()
