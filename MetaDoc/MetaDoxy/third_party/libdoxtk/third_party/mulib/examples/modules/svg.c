#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_XML
#define MULIB_IMPL_SVG
#include "mulib-svg.h"

int main(void) {
    mulib_xml_node_t *doc=mulib_svg_document(200,100);
    mulib_xml_node_t *root,*group;
    char *svg;
    assert(doc); root=mulib_xml_root(doc);
    group=mulib_svg_group(root,"drawing"); assert(group);
    assert(mulib_svg_rect(group,5,5,190,90,"white"));
    assert(mulib_svg_circle(group,50,50,20,"blue"));
    assert(mulib_svg_path(group,"M80 80 L120 20 L160 80 Z","none","black"));
    assert(mulib_svg_text(group,90,55,"A < B & C","black"));
    svg=mulib_svg_serialize(doc,NULL);
    assert(svg&&strstr(svg,"&lt; B &amp; C"));
    puts(svg); free(svg); mulib_xml_destroy(doc);
    return 0;
}
