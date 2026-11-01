#include <assert.h>
#include <string.h>
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_XML
#define MULIB_IMPL_CSS
#include "mulib-css.h"

int main(void) {
    const char xml[]="<main><p class='notice'>Hello</p></main>";
    const char css[]="main > p.notice { color: red; color: blue !important; }";
    mulib_xml_node_t *doc=mulib_xml_parse(xml,sizeof(xml)-1,NULL);
    mulib_css_sheet_t *sheet=mulib_css_parse(css,sizeof(css)-1,NULL);
    mulib_xml_node_t *p;
    const mulib_css_style_t *style;
    assert(doc&&sheet&&mulib_css_rule_count(sheet)==1);
    p=mulib_xml_first(mulib_xml_root(doc));
    assert(mulib_css_matches(p,mulib_css_selector(sheet,0))==1);
    style=mulib_css_declarations(sheet,0);
    assert(!strcmp(mulib_css_get(style,"COLOR"),"blue"));
    mulib_css_destroy(sheet); mulib_xml_destroy(doc);
    return 0;
}
