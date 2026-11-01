#define MULIB_IMPL_DATALOG
#include "mulib-datalog.h"
#include <stdio.h>
static int print_path(void *user, const char *const *tuple, size_t arity) {
    (void)user;
    if (arity == 2) printf("path(%s, %s)\n", tuple[0], tuple[1]);
    return 0;
}
int main(void) {
    mulib_datalog_t *db = mulib_datalog_create(NULL, NULL);
    mulib_datalog_term_t ab[] = {{"alice",0},{"bob",0}}, bc[] = {{"bob",0},{"carol",0}};
    mulib_datalog_term_t xy[] = {{"X",1},{"Y",1}}, xz[] = {{"X",1},{"Z",1}}, zy[] = {{"Z",1},{"Y",1}};
    mulib_datalog_atom_t a = {"edge",ab,2}, b = {"edge",bc,2}, head = {"path",xy,2};
    mulib_datalog_atom_t base = {"edge",xy,2}, body[] = {{"path",xz,2},{"edge",zy,2}};
    size_t count; int failed;
    if (!db) return 1;
    failed = mulib_datalog_fact(db,&a) || mulib_datalog_fact(db,&b) ||
        mulib_datalog_rule(db,&head,&base,1) || mulib_datalog_rule(db,&head,body,2) ||
        mulib_datalog_saturate(db) || mulib_datalog_query(db,&head,print_path,NULL,&count);
    mulib_datalog_destroy(db); return failed ? 1 : 0;
}
