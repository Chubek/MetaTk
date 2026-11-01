# mulib-visitor

Define `MULIB_IMPL_VISITOR`. Traversal itself requires only AST declarations.

`mulib_visitor_walk(root,callback,user)` visits nodes in preorder. A callback
returns `MULIB_VISIT_DESCEND`, `MULIB_VISIT_SKIP` (skip this node's descendants),
or `MULIB_VISIT_STOP`. The walk returns 1 complete, 0 stopped, or -1 for invalid
arguments or an invalid callback result. Sibling traversal continues after SKIP.

Traversal borrows the tree and uses no allocation or recursive calls. It stays
within the given root, even if that root is attached. Do not mutate the tree
inside callbacks. Use listeners when paired enter/leave callbacks are needed.
