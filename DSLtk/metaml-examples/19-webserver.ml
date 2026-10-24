import "webserver"
val server := Webserver.create ()
val route := Webserver.text server "/" "hello"
