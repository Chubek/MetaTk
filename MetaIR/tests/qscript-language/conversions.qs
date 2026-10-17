// Integer parsing and formatting, character codes, trimming and case.
print " -0x1F ".to_int();
print "+5".to_int();
print "0b101".to_int();
print "0o17".to_int();
print "-9223372036854775808".to_int();
print "9223372036854775807".to_int();
print (9.chr() ~ "5").to_int();
print (65.chr() ~ 66.chr()).len();
print 255.chr().at(0);
print 0.chr().len();
print (-7).str() ~ "|" ~ (-9223372036854775807 - 1).str();
print (9.chr() ~ " hi" ~ 13.chr() ~ 10.chr() ~ 11.chr()).trim() ~ "|";
print "MiXed 123".upper() ~ " " ~ "MiXed 123".lower();
print "abcabc".find("c", 3);
print "abc".find("", 2);
print "abc".find("z", 0);
print "abc".starts_with("") && "abc".ends_with("bc") && "abc".contains("");
return "42".to_int() + 1;
