#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_XML
#include "mulib-xml.h"

int main(void) {
    const char input[]="<catalog><book id='1'>C &amp; XML</book></catalog>";
    mulib_xml_error_t error;
    mulib_xml_node_t *doc=mulib_xml_parse(input,sizeof(input)-1,&error);
    mulib_xml_node_t *root,*book,*text;
    char *output; size_t length;
    assert(doc&&!error.message);
    root=mulib_xml_root(doc);
    book=mulib_xml_new(MULIB_XML_ELEMENT,"book");
    text=mulib_xml_new(MULIB_XML_TEXT,"<safe> & owned");
    assert(book&&text&&mulib_xml_set_attr(book,"id","2"));
    assert(mulib_xml_append(book,text)&&mulib_xml_append(root,book));
    output=mulib_xml_serialize(doc,&length);
    assert(output&&length==strlen(output)&&strstr(output,"&lt;safe&gt; &amp; owned"));
    puts(output);
    free(output); mulib_xml_destroy(doc);
    return 0;
}
