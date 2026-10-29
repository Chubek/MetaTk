#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_XML
#define MULIB_IMPL_HTML
#include "mulib-html.h"

int main(void) {
    mulib_xml_node_t *doc=mulib_html_document("A & B");
    mulib_xml_node_t *body,*link;
    char *html;
    assert(doc);
    body=mulib_html_body(doc);
    assert(mulib_html_add(body,"h1","<Hello> & welcome"));
    link=mulib_html_add(body,"a","MuLIB");
    assert(link&&mulib_xml_set_attr(link,"href","/docs?a=1&b=2"));
    assert(mulib_html_add(body,"br",NULL));
    html=mulib_html_serialize(doc,NULL);
    assert(html&&strstr(html,"&lt;Hello&gt;")&&strstr(html,"<br>"));
    puts(html);
    free(html); mulib_xml_destroy(doc);
    return 0;
}
