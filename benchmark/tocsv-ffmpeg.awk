function flush() { if (t) { print "\"" t "\"," real "," user "," sys "," maxrss; t = "" } }
END { flush() }

/^===/ { flush(); t = $0; sub("^=* ", "", t); sub(" [^\(]*", "", t); }
/[0-9][0-9.]* real  *[0-9][0-9.]* user  *[0-9][0-9.]* sys/ { real = $1; user = $3; sys = $5 }
/[0-9][0-9]*  *maximum resident set size/ { maxrss = $1 }
