#include <assert.h>
#include <string.h>
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_XML
#define MULIB_IMPL_XPATH
#include "mulib-xpath.h"

int main(void) {
    const char xml[]="<shop><book available='yes'>C</book><book>Other</book></shop>";
    mulib_xml_node_t *doc=mulib_xml_parse(xml,sizeof(xml)-1,NULL);
    mulib_xpath_result_t result={NULL,0};
    mulib_xml_error_t error;
    assert(doc);
    assert(mulib_xpath_query(doc,"/shop/book[@available='yes']/text()",&result,&error));
    assert(result.count==1&&!strcmp(mulib_xml_value(result.nodes[0]),"C"));
    mulib_xpath_result_free(&result);
    assert(mulib_xpath_query(doc,"//book[2]",&result,&error)&&result.count==1);
    mulib_xpath_result_free(&result);
    mulib_xml_destroy(doc);
    return 0;
}
