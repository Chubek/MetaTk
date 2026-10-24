import "prelude"
import "weblets"
import "sequence"
val items := Sequence.map (fn x -> Weblets.element "li" [] [Weblets.text x]) ["one", "two", "three"]
val page := Weblets.page "Items" [Weblets.element "ul" [] items]
