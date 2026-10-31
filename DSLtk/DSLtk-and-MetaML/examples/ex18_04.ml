module Outer := struct
module Inner := struct
val answer := 42
end
end
Outer.Inner.answer
