// Integers and text print in program order.
print 1;
print "two";
print [3];
print 4;
let s = "";
for (my i in [5, 6]) { s = s ~ i.str(); print i; }
print s;
return 0;
