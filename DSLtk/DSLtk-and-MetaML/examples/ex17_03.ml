type Answer := | Missing | Found Int
match Found 42 with | Found n -> n | Missing -> 0
