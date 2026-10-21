external make_pair : int -> int * string = "make_pair"
let () = assert (make_pair 42 = (42, "ok"))
