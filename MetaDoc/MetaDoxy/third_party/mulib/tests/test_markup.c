#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Exercise allocation failures in every transitive implementation, too. */
static size_t markup_budget=SIZE_MAX;
static int markup_allow(void) {
    if (markup_budget==SIZE_MAX) return 1;
    if (!markup_budget) return 0;
    markup_budget--; return 1;
}
static void *markup_malloc(size_t n) { return markup_allow()?malloc(n):NULL; }
static void *markup_calloc(size_t n,size_t size) { return markup_allow()?calloc(n,size):NULL; }
static void *markup_realloc(void *p,size_t n) { return markup_allow()?realloc(p,n):NULL; }
#define malloc markup_malloc
#define calloc markup_calloc
#define realloc markup_realloc
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_GRAPH
#define MULIB_IMPL_XML
#define MULIB_IMPL_XPATH
#define MULIB_IMPL_HTML
#define MULIB_IMPL_CSS
#define MULIB_IMPL_MARKDOWN
#define MULIB_IMPL_SVG
#define MULIB_IMPL_GRAPHVIZ
#include "mulib-xml.h"
#include "mulib-xpath.h"
#include "mulib-html.h"
#include "mulib-css.h"
#include "mulib-markdown.h"
#include "mulib-svg.h"
#include "mulib-graphviz.h"
#undef malloc
#undef calloc
#undef realloc

static void markup_bad_xml(const char *s) {
    mulib_xml_error_t error;
    mulib_xml_node_t *doc=mulib_xml_parse(s,strlen(s),&error);
    assert(!doc&&error.message&&error.offset<=strlen(s));
}
static void test_xml(void) {
    const char source[]="\xef\xbb\xbf<?xml version='1.0' encoding='UTF-8' standalone='yes'?>\r\n<!--before--><r a='&quot;&apos;&lt;&gt;&amp;&#65;&#x1f642;' b='x\r\n\t y'>&lt;&amp;&#13;\r\n<![CDATA[x<y&z]]><!--ok--><n/></r>\n";
    const char *bad[]={""," ","<r>","<r></s>","<r/><s/>","text<r/>","<r/>tail","<r a='x' a='y'/>",
        "<r a='x'b='y'/>","<r a=x/>","<r a='<x'/>","<r>&unknown;</r>","<r>&amp</r>","<r>&#;</r>",
        "<r>&#x;</r>","<r>&#0;</r>","<r>&#xD800;</r>","<r>&#1114112;</r>","<r>&#999999999999999999;</r>",
        "<r>&#xFFFE;</r>","<r>]]></r>","<r><!--a--b--></r>","<r><!--a---></r>","<r><![CDATA[x</r>",
        "<!DOCTYPE r><r/>","<!DOCTYPE r [<!ENTITY e SYSTEM 'file:///etc/passwd'>]><r>&e;</r>","<?pi x?><r/>",
        "<r><?pi x?></r>"," <?xml version='1.0'?><r/>","<?xml version='1.1'?><r/>","<?xml encoding='UTF-8'?><r/>",
        "<?xml version='1.0' encoding='ISO-8859-1'?><r/>","<1bad/>","<r / >","</r>","<r/ >"," &#32;<r/>","<r/> &#x20;"};
    mulib_xml_error_t error; mulib_xml_node_t *doc,*root,*n,*other; char *output; size_t length,i;
    doc=mulib_xml_parse(source,sizeof(source)-1,&error); assert(doc&&!error.message);
    root=mulib_xml_root(doc); assert(root&&!strcmp(mulib_xml_name(root),"r"));
    assert(!strcmp(mulib_xml_attr(root,"a"),"\"'<>&A\xf0\x9f\x99\x82"));
    assert(!strcmp(mulib_xml_attr(root,"b"),"x   y"));
    n=mulib_xml_first(root); assert(!strcmp(mulib_xml_value(n),"<&\r\n"));
    assert(mulib_xml_type(mulib_xml_next(n))==MULIB_XML_CDATA);
    assert(mulib_xml_attr_count(root)==2&&!strcmp(mulib_xml_attr_name(root,0),"a"));
    assert(!mulib_xml_attr_name(root,2)&&!mulib_xml_attr_value(NULL,0));
    assert(mulib_xml_set_attr(root,"a",mulib_xml_attr(root,"a")));
    assert(!mulib_xml_set_attr(root,"bad name","x")&&!mulib_xml_set_attr(root,"x","\1"));
    output=mulib_xml_serialize(doc,&length); assert(output&&length==strlen(output));
    assert(strstr(output,"&#13;")&&strstr(output,"<![CDATA[x<y&z]]>"));
    other=mulib_xml_parse(output,length,&error); assert(other); free(output); mulib_xml_destroy(other);
    n=mulib_xml_new(MULIB_XML_ELEMENT,"child"); assert(n&&mulib_xml_append(root,n));
    assert(!mulib_xml_append(root,n)&&!mulib_xml_append(n,root)&&!mulib_xml_append(n,doc));
    assert(mulib_xml_detach(n)==n&&!mulib_xml_parent(n)&&!mulib_xml_next(n));
    assert(mulib_xml_append(root,n)); mulib_xml_destroy(n); /* attached destruction detaches */
    assert(!mulib_xml_new(MULIB_XML_COMMENT,"bad--comment")&&!mulib_xml_new(MULIB_XML_CDATA,"]]>"));
    mulib_xml_destroy(doc);
    for (i=0;i<sizeof(bad)/sizeof(bad[0]);i++) markup_bad_xml(bad[i]);
    { const char nul[]={'<','r','>',0,'<','/','r','>'}; assert(!mulib_xml_parse(nul,sizeof(nul),&error)); }
    markup_bad_xml("<r>\xc0\x80</r>"); markup_bad_xml("<r>\xed\xa0\x80</r>");
    assert(!mulib_xml_parse(NULL,1,&error)&&!mulib_xml_parse(NULL,0,&error));
    doc=mulib_xml_new(MULIB_XML_DOCUMENT,NULL); root=mulib_xml_new(MULIB_XML_ELEMENT,"r"); assert(doc&&root&&mulib_xml_append(doc,root));
    n=root;
    for (i=0;i<1024;i++) { other=mulib_xml_new(MULIB_XML_ELEMENT,"n"); assert(other&&mulib_xml_append(n,other)); n=other; }
    length=123; assert(!mulib_xml_serialize(doc,&length)&&length==0); mulib_xml_destroy(doc);
    { mulib_strbuf_t b; mulib_strbuf_init(&b);
      for (i=0;i<MULIB_XML_DEPTH_LIMIT+1;i++) assert(mulib_strbuf_append(&b,"<x>"));
      for (i=0;i<MULIB_XML_DEPTH_LIMIT+1;i++) assert(mulib_strbuf_append(&b,"</x>"));
      assert(!mulib_xml_parse(b.data,b.length,&error)); mulib_strbuf_free(&b); }
    { mulib_strbuf_t b; mulib_strbuf_init(&b);
      for (i=0;i<MULIB_XML_DEPTH_LIMIT;i++) assert(mulib_strbuf_append(&b,"<x>"));
      for (i=0;i<MULIB_XML_DEPTH_LIMIT;i++) assert(mulib_strbuf_append(&b,"</x>"));
      doc=mulib_xml_parse(b.data,b.length,&error); assert(doc);
      output=mulib_xml_serialize(doc,NULL); assert(output); free(output); mulib_xml_destroy(doc); mulib_strbuf_free(&b); }
    { const char *reference="<r>&#000000000000000000000000000000000000000000065;</r>";
      doc=mulib_xml_parse(reference,strlen(reference),&error); assert(doc); mulib_xml_destroy(doc); }
}
static void test_xpath_css(void) {
    const char *source="<root><group id='g' class='outer wide'><item id='a' class='hot' k='x'>one</item><item id='b'>two</item><group><item id='c' k='x'><![CDATA[three]]></item></group></group><group><item id='d'>four</item></group></root>";
    const char *bad_paths[]={"","//","/root/","/root///x","//item[0]","//item[-1]","//item[@]","//item[1+1]","//item[@x=1]","//item[text()]","//item[@x='a'","//item[99999999999999999999999999999]","//count()","//@id","//..","/root | //item"};
    const char *bad_selectors[]={"","a,","a >","a + b","a ~ b","a:hover","[x^=a]","[x=]","a,,b","a > > b","a\\b","[x='a]","a/**/b"};
    const char *bad_styles[]={"color","color:",": red","bad name: x","x: (a]","x: a}","x: 'unterminated","x: /* unterminated","x: \\"};
    const char declarations[]="/*a*/ COLOR: red; content: \"a;:/*keep*/\"; margin: calc(1px + var(--x)); color: blue ! IMPORTANT; color: green; --X: v; --x: w;";
    const char rules[]="/*rules*/ group > item.hot, #d { color: red; content: '};'; } [k='x'] { x: calc(1 + 2); }";
    mulib_xml_node_t *doc=mulib_xml_parse(source,strlen(source),NULL),*a,*c; mulib_xpath_result_t r={NULL,0}; mulib_xml_error_t error; size_t i;
    mulib_css_style_t *style; mulib_css_sheet_t *sheet;
    assert(doc);
    assert(mulib_xpath_query(doc,"/root/group/item[1]",&r,&error)&&r.count==2);
    a=r.nodes[0]; assert(!strcmp(mulib_xml_attr(a,"id"),"a")); mulib_xpath_result_free(&r);
    assert(mulib_xpath_query(doc,"//item[1]",&r,&error)&&r.count==3); mulib_xpath_result_free(&r);
    assert(mulib_xpath_query(doc,"//item[@k='x'][2]",&r,&error)&&r.count==0); mulib_xpath_result_free(&r);
    assert(mulib_xpath_query(doc,"//item[@k][text()='three']",&r,&error)&&r.count==1); c=r.nodes[0]; mulib_xpath_result_free(&r);
    assert(mulib_xpath_query(c,"../item/text()",&r,&error)&&r.count==1&&mulib_xml_type(r.nodes[0])==MULIB_XML_CDATA); mulib_xpath_result_free(&r);
    assert(mulib_xpath_query(a,".",&r,&error)&&r.count==1&&r.nodes[0]==a); mulib_xpath_result_free(&r);
    assert(mulib_xpath_query(a,"/",&r,&error)&&r.count==1&&r.nodes[0]==doc); mulib_xpath_result_free(&r);
    assert(mulib_xpath_query(doc,"//node()/..",&r,&error)&&r.count==9&&r.nodes[0]==doc); mulib_xpath_result_free(&r);
    assert(mulib_xpath_query(doc,"//missing/child",&r,&error)&&r.count==0); mulib_xpath_result_free(&r);
    { mulib_xml_node_t *detached=mulib_xml_new(MULIB_XML_ELEMENT,"root"); assert(detached);
      assert(!mulib_xpath_query(detached,"/root",&r,&error)&&error.message); mulib_xml_destroy(detached); }
    for (i=0;i<sizeof(bad_paths)/sizeof(bad_paths[0]);i++) assert(!mulib_xpath_query(doc,bad_paths[i],&r,&error)&&error.message&&!r.nodes&&!r.count);
    assert(mulib_css_matches(a,"root > group.outer item.hot[k='x']#a")==1);
    assert(mulib_css_matches(a,"group.wide > item, #missing")==1);
    assert(mulib_css_matches(c,"root > group.outer item")==1);
    assert(mulib_css_matches(c,"root > group.outer > item")==0);
    assert(mulib_css_matches(c,"item[k=x]")==1&&mulib_css_matches(c,"item[k=y]")==0);
    assert(mulib_css_matches(a,"item.HOT")==0&&mulib_css_matches(a,"*")==1);
    for (i=0;i<sizeof(bad_selectors)/sizeof(bad_selectors[0]);i++) assert(mulib_css_matches(a,bad_selectors[i])==-1);
    style=mulib_css_parse_declarations(declarations,sizeof(declarations)-1,&error); assert(style&&!error.message&&mulib_css_count(style)==7);
    assert(!strcmp(mulib_css_get(style,"CoLoR"),"blue")&&mulib_css_important(style,3));
    assert(!strcmp(mulib_css_value(style,1),"\"a;:/*keep*/\""));
    assert(!strcmp(mulib_css_get(style,"--X"),"v")&&!strcmp(mulib_css_get(style,"--x"),"w"));
    assert(!mulib_css_get(style,"missing")&&!mulib_css_name(style,100)); mulib_css_style_destroy(style);
    for (i=0;i<sizeof(bad_styles)/sizeof(bad_styles[0]);i++) { style=mulib_css_parse_declarations(bad_styles[i],strlen(bad_styles[i]),&error); assert(!style&&error.message); }
    sheet=mulib_css_parse(rules,sizeof(rules)-1,&error); assert(sheet&&!error.message&&mulib_css_rule_count(sheet)==2);
    assert(mulib_css_matches(a,mulib_css_selector(sheet,0))==1&&!strcmp(mulib_css_get(mulib_css_declarations(sheet,0),"content"),"'};'")); mulib_css_destroy(sheet);
    { const char *bad_sheets[]={"@import 'x';","a { x: 1;", "a { b { x: 1; } }", "a { x: 1; }}", "a:hover { x: 1; }"};
      for (i=0;i<sizeof(bad_sheets)/sizeof(bad_sheets[0]);i++) assert(!mulib_css_parse(bad_sheets[i],strlen(bad_sheets[i]),&error)&&error.message); }
    mulib_xml_destroy(doc);
}
static void test_html_markdown_svg(void) {
    mulib_xml_node_t *doc=mulib_html_document("A < B & C"),*body,*node,*bad,*parsed; char *text; size_t n;
    mulib_xml_error_t error;
    const char md[]="# Heading\r\n\r\n**bold** *em* _under_ `x<&` [link](https://x.test/?a=1&b=2)\nnext\n\n- one\n- two\n1. three\n\n---\n\n```c\n<x>&\n```\n<script>x</script>\n";
    assert(doc); body=mulib_html_body(doc); assert(body);
    node=mulib_html_add(body,"p","<script> & \"hi\""); assert(node&&mulib_xml_set_attr(node,"class","a\"&b"));
    assert(mulib_html_add(body,"br",NULL)&&!mulib_html_add(body,"br","x")&&!mulib_html_add(body,"script","x")&&!mulib_html_add(body,"P","x"));
    text=mulib_html_serialize(doc,&n); assert(text&&n==strlen(text)&&strstr(text,"<!doctype html>\n")&&strstr(text,"&lt;script&gt; &amp;")&&strstr(text,"class=\"a&quot;&amp;b\"")&&strstr(text,"<br>")); free(text);
    bad=mulib_xml_new(MULIB_XML_ELEMENT,"script"); assert(bad&&mulib_xml_append(body,bad)); assert(!mulib_html_serialize(doc,&n)&&n==0); mulib_xml_destroy(bad); mulib_xml_destroy(doc);
    text=mulib_markdown_to_html(md,sizeof(md)-1,&n,&error); assert(text&&!error.message&&n==strlen(text));
    assert(strstr(text,"<h1>Heading</h1>\n")&&strstr(text,"<strong>bold</strong> <em>em</em> <em>under</em> <code>x&lt;&amp;</code>"));
    assert(strstr(text,"href=\"https://x.test/?a=1&amp;b=2\"")&&strstr(text,"</a> next</p>\n"));
    assert(strstr(text,"<ul>\n<li>one</li>\n<li>two</li>\n</ul>\n<ol>\n<li>three</li>\n</ol>"));
    assert(strstr(text,"<pre><code class=\"language-c\">&lt;x&gt;&amp;\n</code></pre>")&&strstr(text,"&lt;script&gt;x&lt;/script&gt;")); free(text);
    { const char *s="[x](javascript:alert(1)) [x](data:text/html,x) [ok](/a) \\*literal\\* **open";
      text=mulib_markdown_to_html(s,strlen(s),NULL,&error); assert(text&&!strstr(text,"href=\"javascript")&&!strstr(text,"href=\"data:")&&strstr(text,"href=\"/a\"")&&strstr(text,"*literal* **open")); free(text); }
    text=mulib_markdown_to_html("*a **b** c*",11,NULL,&error); assert(text&&!strcmp(text,"<p><em>a <strong>b</strong> c</em></p>\n")); free(text);
    { const char *literal="``not code`` ![not image](/path)";
      text=mulib_markdown_to_html(literal,strlen(literal),NULL,&error); assert(text&&!strstr(text,"<code>")&&!strstr(text,"<a ")); free(text); }
    assert(!mulib_markdown_to_html("```\nx",5,&n,&error)&&error.message&&n==0);
    assert(!mulib_markdown_to_html("\xff",1,&n,&error)&&error.message);
    text=mulib_markdown_to_html(NULL,0,&n,&error); assert(text&&n==0); free(text);
    doc=mulib_svg_document(100,50); assert(doc); body=mulib_xml_root(doc);
    assert(mulib_svg_rect(body,1.5,2,20,10,"red"));
    node=mulib_svg_group(body,"g\"&"); assert(node&&mulib_svg_circle(node,5,5,3,"blue"));
    assert(mulib_svg_line(node,0,0,20,10,"black",1)&&mulib_svg_text(node,2,3,"<hello> & \xf0\x9f\x99\x82","black"));
    assert(mulib_svg_path(body,"M0 0 L10 10","none","red"));
    assert(!mulib_svg_rect(body,0,0,-1,1,NULL)&&!mulib_svg_circle(body,NAN,1,2,NULL)&&!mulib_svg_line(body,0,0,1,1,NULL,INFINITY));
    assert(!mulib_svg_document(0,10)&&!mulib_svg_document(INFINITY,10));
    text=mulib_svg_serialize(doc,&n); assert(text&&strstr(text,"viewBox=\"0 0 100 50\"")&&strstr(text,"&lt;hello&gt; &amp;")&&strstr(text,"id=\"g&quot;&amp;\""));
    parsed=mulib_xml_parse(text,n,&error); assert(parsed); mulib_xml_destroy(parsed); free(text); mulib_xml_destroy(doc);
}
static void test_graphviz(void) {
    mulib_graph_t g; mulib_graphviz_options_t options={0}; char *text; size_t n;
    const char *labels[]={"a\"; evil -> x; //","line\nslash\\N","<b>literal</b>"};
    mulib_graphviz_attr_t attr[]={ {"rankdir","LR"} },nodes[]={ {"shape","box"} };
    assert(mulib_graph_init(&g,3)&&mulib_graph_edge(&g,0,1,1)&&mulib_graph_edge(&g,2,2,1));
    options.name="g\"raph"; options.labels=labels; options.label_count=3;
    options.graph_attributes=attr; options.graph_attribute_count=1; options.node_attributes=nodes; options.node_attribute_count=1;
    text=mulib_graphviz_dot(&g,&options,&n); assert(text&&n==strlen(text)&&strstr(text,"digraph \"g\\\"raph\"")&&strstr(text,"\"n0\" -> \"n1\";")&&strstr(text,"\"n2\" -> \"n2\";"));
    assert(strstr(text,"a\\\"; evil -> x; //")&&strstr(text,"line\\nslash\\\\N")&&strstr(text,"\"rankdir\"=\"LR\"")); free(text);
    options.label_count=4; assert(!mulib_graphviz_dot(&g,&options,&n)&&n==0); options.label_count=3;
    attr[0].value="bad\1"; assert(!mulib_graphviz_dot(&g,&options,NULL));
    mulib_graph_free(&g);
    assert(mulib_graph_init(&g,0)); text=mulib_graphviz_dot(&g,NULL,NULL); assert(text&&!strcmp(text,"digraph \"G\" {\n}\n")); free(text); mulib_graph_free(&g);
}
static void markup_expect_html(const mulib_xml_node_t *node,const char *expected) {
    size_t length; char *output=mulib_html_serialize(node,&length);
    assert(output&&length==strlen(expected)&&!strcmp(output,expected));
    free(output);
}
static void test_markup_audit_regressions(void) {
    mulib_xml_node_t *doc,*root,*node,*parsed,*body;
    mulib_xml_error_t error; char *output; size_t length,i;
    /* Document-level CR must remain legal whitespace, not a character reference.
     * Normal XML newline normalization still applies when parsing it back. */
    doc=mulib_xml_new(MULIB_XML_DOCUMENT,NULL); root=mulib_xml_new(MULIB_XML_ELEMENT,"r");
    node=mulib_xml_new(MULIB_XML_TEXT,"\r"); assert(doc&&root&&node);
    assert(mulib_xml_append(doc,node)&&mulib_xml_append(doc,root));
    output=mulib_xml_serialize(doc,&length); assert(output&&!strcmp(output,"\r<r/>"));
    parsed=mulib_xml_parse(output,length,&error); assert(parsed&&!error.message);
    assert(!strcmp(mulib_xml_value(mulib_xml_first(parsed)),"\n"));
    free(output); mulib_xml_destroy(parsed);
    node=mulib_xml_new(MULIB_XML_TEXT,"\r"); assert(node&&mulib_xml_append(root,node));
    node=mulib_xml_new(MULIB_XML_TEXT,"\t \r\n"); assert(node&&mulib_xml_append(doc,node));
    output=mulib_xml_serialize(doc,&length); assert(output&&!strcmp(output,"\r<r>&#13;</r>\t \r\n"));
    parsed=mulib_xml_parse(output,length,&error); assert(parsed&&!error.message);
    assert(!strcmp(mulib_xml_value(mulib_xml_first(mulib_xml_root(parsed))),"\r"));
    assert(!strcmp(mulib_xml_value(mulib_xml_next(mulib_xml_root(parsed))),"\t \n"));
    free(output); mulib_xml_destroy(parsed); mulib_xml_destroy(doc);

    /* A delimiter inside an intact link or code span is not an outer closer. */
    { static const struct { const char *source,*expected; } cases[]={
        {"_a [b](/c_d) e_","<p><em>a <a href=\"/c_d\">b</a> e</em></p>\n"},
        {"*before [label](/some*path) after*","<p><em>before <a href=\"/some*path\">label</a> after</em></p>\n"},
        {"**before [label](/some**path) after**","<p><strong>before <a href=\"/some**path\">label</a> after</strong></p>\n"},
        {"__before [label](/some__path) after__","<p><strong>before <a href=\"/some__path\">label</a> after</strong></p>\n"},
        {"[a `]` b](/path)","<p><a href=\"/path\">a <code>]</code> b</a></p>\n"},
        {"[a `x``]y` *b*](/path)","<p><a href=\"/path\">a <code>x``]y</code> <em>b</em></a></p>\n"},
        {"[a \\] b](/path)","<p><a href=\"/path\">a ] b</a></p>\n"},
        {"*a [b `]` c](/d*e) f*","<p><em>a <a href=\"/d*e\">b <code>]</code> c</a> f</em></p>\n"},
        {"*a `x``*y` b*","<p><em>a <code>x``*y</code> b</em></p>\n"},
        {"_a [b](javascript:c_d) e_","<p><em>a [b](javascript:c_d) e</em></p>\n"},
        {"*a [b](/c*d) e","<p>*a <a href=\"/c*d\">b</a> e</p>\n"}
      };
      for (i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
          output=mulib_markdown_to_html(cases[i].source,strlen(cases[i].source),&length,&error);
          assert(output&&!error.message&&length==strlen(cases[i].expected)&&!strcmp(output,cases[i].expected)); free(output);
      }
    }

    /* DOT's nonexistent \\t escape loses tabs. Preserve a real tab inside quotes,
     * while a caller's literal backslash-t must still have its slash escaped. */
    { mulib_strbuf_t quote; mulib_graph_t graph; const char *labels[]={"A\tB","A\\tB"};
      mulib_graphviz_options_t options={0};
      mulib_strbuf_init(&quote); assert(mulib_graphviz_quote(&quote,"A\tB",3));
      assert(quote.length==5&&!strcmp(quote.data,"\"A\tB\"")); mulib_strbuf_free(&quote);
      assert(mulib_graphviz_quote(&quote,"A\\tB",4)); assert(!strcmp(quote.data,"\"A\\\\tB\"")); mulib_strbuf_free(&quote);
      assert(mulib_graph_init(&graph,2)); options.labels=labels; options.label_count=2;
      output=mulib_graphviz_dot(&graph,&options,&length);
      assert(output&&length==strlen(output)&&strstr(output,"\"n0\" [\"label\"=\"A\tB\"];")&&strstr(output,"\"n1\" [\"label\"=\"A\\\\tB\"];"));
      free(output); mulib_graph_free(&graph);
    }

    /* HTML parsers remove one initial LF from pre/textarea/listing. Empty text
     * emits nothing, but a real element/comment before LF prevents stripping. */
    doc=mulib_html_document("initial LF"); assert(doc); body=mulib_html_body(doc); assert(body);
    node=mulib_html_add(body,"textarea","\nx"); assert(node); markup_expect_html(node,"<textarea>\n\nx</textarea>");
    node=mulib_html_add(body,"pre","\nx"); assert(node); markup_expect_html(node,"<pre>\n\nx</pre>");
    node=mulib_html_add(body,"listing","\nx"); assert(node); markup_expect_html(node,"<listing>\n\nx</listing>");
    node=mulib_html_add(body,"textarea",""); assert(node&&mulib_xml_append(node,mulib_xml_new(MULIB_XML_TEXT,"\nx")));
    markup_expect_html(node,"<textarea>\n\nx</textarea>");
    node=mulib_html_add(body,"pre",NULL); assert(node);
    assert(mulib_xml_append(node,mulib_xml_new(MULIB_XML_CDATA,""))&&mulib_xml_append(node,mulib_xml_new(MULIB_XML_TEXT,""))&&
           mulib_xml_append(node,mulib_xml_new(MULIB_XML_CDATA,"\n<x>")));
    markup_expect_html(node,"<pre>\n\n&lt;x&gt;</pre>");
    node=mulib_html_add(body,"pre","\n\nx"); assert(node); markup_expect_html(node,"<pre>\n\n\nx</pre>");
    node=mulib_html_add(body,"pre","\n"); assert(node); markup_expect_html(node,"<pre>\n\n</pre>");
    node=mulib_html_add(body,"pre",NULL); assert(node); markup_expect_html(node,"<pre></pre>");
    assert(mulib_xml_append(node,mulib_xml_new(MULIB_XML_COMMENT,"prefix"))&&mulib_xml_append(node,mulib_xml_new(MULIB_XML_TEXT,"\nx")));
    markup_expect_html(node,"<pre><!--prefix-->\nx</pre>");
    node=mulib_html_add(body,"pre",""); assert(node&&mulib_html_add(node,"span","")&&mulib_xml_append(node,mulib_xml_new(MULIB_XML_TEXT,"\nx")));
    markup_expect_html(node,"<pre><span></span>\nx</pre>");
    node=mulib_html_add(body,"textarea","x"); assert(node&&mulib_xml_append(node,mulib_xml_new(MULIB_XML_TEXT,"\nx")));
    markup_expect_html(node,"<textarea>x\nx</textarea>");
    node=mulib_html_add(body,"textarea","\r\nx"); assert(node); markup_expect_html(node,"<textarea>&#13;\nx</textarea>");
    mulib_xml_destroy(doc);
}
static void test_allocation_failures(void) {
    const char xml[]="<root a='1'><item x='&amp;'>text</item><item/></root>";
    const char css[]="root > item[x] {color: red; content: 'x;y';} #a {x: calc(1 + 2)}";
    const char markdown[]="# Heading\n\nA **bold** [link](https://x)\n\n- a\n- b\n\n```c\nx<&\n```\n";
    size_t i; int success=0;
    for (i=0;i<256;i++) { mulib_xml_node_t *doc; markup_budget=i; doc=mulib_xml_parse(xml,sizeof(xml)-1,NULL); if (doc) { mulib_xml_destroy(doc); success=1; break; } }
    markup_budget=SIZE_MAX; assert(success);
    { mulib_xml_node_t *doc=mulib_xml_parse(xml,sizeof(xml)-1,NULL); assert(doc); success=0;
      for (i=0;i<128;i++) { mulib_xpath_result_t result={NULL,0}; markup_budget=i; if (mulib_xpath_query(doc,"//item[@x]",&result,NULL)) { mulib_xpath_result_free(&result); success=1; break; } assert(!result.nodes); }
      markup_budget=SIZE_MAX; assert(success); mulib_xml_destroy(doc); }
    success=0;
    for (i=0;i<256;i++) { mulib_css_sheet_t *sheet; markup_budget=i; sheet=mulib_css_parse(css,sizeof(css)-1,NULL); if (sheet) { mulib_css_destroy(sheet); success=1; break; } }
    markup_budget=SIZE_MAX; assert(success); success=0;
    for (i=0;i<128;i++) { char *out; markup_budget=i; out=mulib_markdown_to_html(markdown,sizeof(markdown)-1,NULL,NULL); if (out) { free(out); success=1; break; } }
    markup_budget=SIZE_MAX; assert(success); success=0;
    for (i=0;i<128;i++) { mulib_xml_node_t *doc; markup_budget=i; doc=mulib_html_document("title"); if (doc) { char *out=mulib_html_serialize(doc,NULL); mulib_xml_destroy(doc); if (out) { free(out); success=1; break; } } }
    markup_budget=SIZE_MAX; assert(success); success=0;
    for (i=0;i<128;i++) { mulib_xml_node_t *doc; markup_budget=i; doc=mulib_svg_document(10,20); if (doc) { int added=mulib_svg_text(mulib_xml_root(doc),1,2,"text","red")!=NULL; mulib_xml_destroy(doc); if (added) { success=1; break; } } }
    markup_budget=SIZE_MAX; assert(success);
    { mulib_graph_t g; assert(mulib_graph_init(&g,2)&&mulib_graph_edge(&g,0,1,1)); success=0;
      for (i=0;i<64;i++) { char *out; markup_budget=i; out=mulib_graphviz_dot(&g,NULL,NULL); if (out) { free(out); success=1; break; } }
      markup_budget=SIZE_MAX; assert(success); mulib_graph_free(&g); }
    { mulib_xml_node_t *node=mulib_xml_new(MULIB_XML_ELEMENT,"n"); assert(node&&mulib_xml_set_attr(node,"x","old"));
      markup_budget=0; assert(!mulib_xml_set_attr(node,"x","new")); markup_budget=SIZE_MAX;
      assert(!strcmp(mulib_xml_attr(node,"x"),"old")); mulib_xml_destroy(node); }
}
static void test_malformed_corpus(void) {
    uint32_t state=7; size_t i,j; char input[97];
    const char alphabet[]="<>/!?=-'\"&;#[]().*`_ abcdef0123\n\r\t";
    mulib_xml_node_t *context=mulib_xml_new(MULIB_XML_ELEMENT,"context"); assert(context);
    for (i=0;i<1200;i++) {
        size_t n=i%96; mulib_xml_node_t *doc; mulib_css_style_t *style; mulib_css_sheet_t *sheet; char *out;
        for (j=0;j<n;j++) { state=state*1664525u+1013904223u; input[j]=alphabet[state%(sizeof(alphabet)-1)]; } input[n]=0;
        doc=mulib_xml_parse(input,n,NULL); if (doc) { out=mulib_xml_serialize(doc,NULL); assert(out); free(out); mulib_xml_destroy(doc); }
        style=mulib_css_parse_declarations(input,n,NULL); mulib_css_style_destroy(style);
        sheet=mulib_css_parse(input,n,NULL); mulib_css_destroy(sheet);
        out=mulib_markdown_to_html(input,n,NULL,NULL); free(out);
        assert(mulib_css_matches(NULL,input)>=-1);
        { mulib_xpath_result_t result={NULL,0};
          (void)mulib_xpath_query(context,input,&result,NULL); mulib_xpath_result_free(&result); }
    }
    mulib_xml_destroy(context);
}
int main(void) {
    test_xml(); test_xpath_css(); test_html_markdown_svg(); test_graphviz();
    test_markup_audit_regressions(); test_allocation_failures(); test_malformed_corpus();
    return 0;
}
