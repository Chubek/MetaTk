import "getopt"
val parsed := Getopt.parse [("verbose", false), ("name", true)] ["--verbose", "--name=ada", "file.ml"]
