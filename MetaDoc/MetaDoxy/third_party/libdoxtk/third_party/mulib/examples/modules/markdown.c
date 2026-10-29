#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_XML
#define MULIB_IMPL_HTML
#define MULIB_IMPL_MARKDOWN
#include "mulib-markdown.h"

int main(void) {
    const char source[]="# Hello\n\nA **small** [link](/docs?a=1&b=2).\n\n- escaped <HTML>\n- `code`\n";
    mulib_xml_error_t error;
    size_t length;
    char *html=mulib_markdown_to_html(source,sizeof(source)-1,&length,&error);
    assert(html&&!error.message&&length==strlen(html));
    assert(strstr(html,"<strong>small</strong>")&&strstr(html,"&lt;HTML&gt;"));
    puts(html); free(html);
    return 0;
}
