import "stream"
val naturals := Stream.unfold (fn n -> Some (n, n + 1)) 0
val first_five := Stream.take 5 naturals
