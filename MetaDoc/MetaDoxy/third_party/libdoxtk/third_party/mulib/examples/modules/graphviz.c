#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_GRAPH
#define MULIB_IMPL_GRAPHVIZ
#include "mulib-graphviz.h"

int main(void) {
    mulib_graph_t graph;
    const char *labels[]={"start","quoted \"finish\""};
    const mulib_graphviz_attr_t nodes[]={ {"shape","box"} };
    mulib_graphviz_options_t options={0};
    char *dot;
    assert(mulib_graph_init(&graph,2)&&mulib_graph_edge(&graph,0,1,1));
    options.name="example"; options.labels=labels; options.label_count=2;
    options.node_attributes=nodes; options.node_attribute_count=1;
    dot=mulib_graphviz_dot(&graph,&options,NULL);
    assert(dot&&strstr(dot,"\"n0\" -> \"n1\""));
    puts(dot); free(dot); mulib_graph_free(&graph);
    return 0;
}
