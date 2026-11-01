/**
 * @file mulib-svg.h
 * @brief Escaped SVG document/shape builder on the shared XML DOM.
 *
 * Finite double geometry, groups, rectangles, circles, lines, text and path
 * attributes. Path/CSS/paint semantics are caller-owned; no renderer or SVG
 * sanitizer. Define MULIB_IMPL_SVG plus XML, STRBUF and UTF8 switches.
 */
#ifndef MULIB_SVG_H
#define MULIB_SVG_H
#include "mulib-xml.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Create an owned SVG document with namespace, size and viewBox.
 * Width/height must be finite and positive; NULL on invalid input/allocation. */
mulib_xml_node_t *mulib_svg_document(double width,double height);
/** @brief Append a group; optional id is copied. Returns a borrowed node. */
mulib_xml_node_t *mulib_svg_group(mulib_xml_node_t *parent,const char *id);
/** @brief Append a rectangle; finite geometry, nonnegative size, optional fill. */
mulib_xml_node_t *mulib_svg_rect(mulib_xml_node_t *parent,double x,double y,double width,double height,const char *fill);
/** @brief Append a circle; finite center, nonnegative radius, optional fill. */
mulib_xml_node_t *mulib_svg_circle(mulib_xml_node_t *parent,double cx,double cy,double radius,const char *fill);
/** @brief Append a line; finite coordinates/nonnegative stroke width. */
mulib_xml_node_t *mulib_svg_line(mulib_xml_node_t *parent,double x1,double y1,double x2,double y2,const char *stroke,double stroke_width);
/** @brief Append copied, escaped text at finite coordinates. text must be non-NULL. */
mulib_xml_node_t *mulib_svg_text(mulib_xml_node_t *parent,double x,double y,const char *text,const char *fill);
/** @brief Append a path with copied data and optional fill/stroke attributes.
 * Data is escaped but its SVG path grammar is not validated. */
mulib_xml_node_t *mulib_svg_path(mulib_xml_node_t *parent,const char *data,const char *fill,const char *stroke);
/** @brief Serialize an SVG document/root through the shared XML serializer.
 * Returns malloc-owned text; optional length is zero on error. */
char *mulib_svg_serialize(const mulib_xml_node_t *document_or_svg,size_t *length);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_SVG_H */

#if defined(MULIB_IMPL_SVG) && !defined(MULIB_SVG_IMPLEMENTED)
#define MULIB_SVG_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <locale.h>
#ifdef __cplusplus
extern "C" {
#endif
/* printf's locale decimal separator is normalized without changing the locale. */
static int mulib_svg_number_internal(char *out,size_t capacity,double value) {
    char raw[128]; int n; const char *decimal; char *at;
    if (!isfinite(value)) return 0;
    n=snprintf(raw,sizeof(raw),"%.17g",value);
    if (n<0||(size_t)n>=sizeof(raw)) return 0;
    decimal=localeconv()->decimal_point;
    at=decimal&&*decimal?strstr(raw,decimal):NULL;
    if (at&&strcmp(decimal,".")) {
        size_t skip=strlen(decimal);
        *at='.'; memmove(at+1,at+skip,strlen(at+skip)+1);
    }
    if (strlen(raw)>=capacity) return 0;
    strcpy(out,raw); return 1;
}
static int mulib_svg_number_attr_internal(mulib_xml_node_t *node,const char *name,double value) {
    char text[128]; return mulib_svg_number_internal(text,sizeof(text),value)&&mulib_xml_set_attr(node,name,text);
}
static int mulib_svg_optional_internal(mulib_xml_node_t *node,const char *name,const char *value) { return !value||mulib_xml_set_attr(node,name,value); }
static mulib_xml_node_t *mulib_svg_finish_internal(mulib_xml_node_t *parent,mulib_xml_node_t *node,int ok) {
    if (!ok||!mulib_xml_append(parent,node)) { mulib_xml_destroy(node); return NULL; }
    return node;
}
mulib_xml_node_t *mulib_svg_document(double width,double height) {
    mulib_xml_node_t *doc,*svg; char w[128],h[128]; mulib_strbuf_t view;
    if (width<=0||height<=0||!mulib_svg_number_internal(w,sizeof(w),width)||!mulib_svg_number_internal(h,sizeof(h),height)) return NULL;
    doc=mulib_xml_new(MULIB_XML_DOCUMENT,NULL); svg=mulib_xml_new(MULIB_XML_ELEMENT,"svg");
    mulib_strbuf_init(&view);
    if (!doc||!svg||!mulib_strbuf_append(&view,"0 0 ")||!mulib_strbuf_append(&view,w)||!mulib_strbuf_putc(&view,' ')||!mulib_strbuf_append(&view,h)||
        !mulib_xml_set_attr(svg,"xmlns","http://www.w3.org/2000/svg")||!mulib_xml_set_attr(svg,"width",w)||!mulib_xml_set_attr(svg,"height",h)||
        !mulib_xml_set_attr(svg,"viewBox",view.data)||!mulib_xml_append(doc,svg)) {
        mulib_xml_destroy(svg); mulib_xml_destroy(doc); mulib_strbuf_free(&view); return NULL;
    }
    mulib_strbuf_free(&view); return doc;
}
mulib_xml_node_t *mulib_svg_group(mulib_xml_node_t *parent,const char *id) {
    mulib_xml_node_t *n=mulib_xml_new(MULIB_XML_ELEMENT,"g");
    if (!n) return NULL;
    return mulib_svg_finish_internal(parent,n,mulib_svg_optional_internal(n,"id",id));
}
mulib_xml_node_t *mulib_svg_rect(mulib_xml_node_t *parent,double x,double y,double w,double h,const char *fill) {
    mulib_xml_node_t *n;
    if (w<0||h<0) return NULL;
    n=mulib_xml_new(MULIB_XML_ELEMENT,"rect"); if (!n) return NULL;
    return mulib_svg_finish_internal(parent,n,mulib_svg_number_attr_internal(n,"x",x)&&mulib_svg_number_attr_internal(n,"y",y)&&
        mulib_svg_number_attr_internal(n,"width",w)&&mulib_svg_number_attr_internal(n,"height",h)&&mulib_svg_optional_internal(n,"fill",fill));
}
mulib_xml_node_t *mulib_svg_circle(mulib_xml_node_t *parent,double cx,double cy,double radius,const char *fill) {
    mulib_xml_node_t *n;
    if (radius<0) return NULL;
    n=mulib_xml_new(MULIB_XML_ELEMENT,"circle"); if (!n) return NULL;
    return mulib_svg_finish_internal(parent,n,mulib_svg_number_attr_internal(n,"cx",cx)&&mulib_svg_number_attr_internal(n,"cy",cy)&&
        mulib_svg_number_attr_internal(n,"r",radius)&&mulib_svg_optional_internal(n,"fill",fill));
}
mulib_xml_node_t *mulib_svg_line(mulib_xml_node_t *parent,double x1,double y1,double x2,double y2,const char *stroke,double width) {
    mulib_xml_node_t *n;
    if (width<0) return NULL;
    n=mulib_xml_new(MULIB_XML_ELEMENT,"line"); if (!n) return NULL;
    return mulib_svg_finish_internal(parent,n,mulib_svg_number_attr_internal(n,"x1",x1)&&mulib_svg_number_attr_internal(n,"y1",y1)&&
        mulib_svg_number_attr_internal(n,"x2",x2)&&mulib_svg_number_attr_internal(n,"y2",y2)&&mulib_svg_optional_internal(n,"stroke",stroke)&&
        mulib_svg_number_attr_internal(n,"stroke-width",width));
}
mulib_xml_node_t *mulib_svg_text(mulib_xml_node_t *parent,double x,double y,const char *text,const char *fill) {
    mulib_xml_node_t *n,*t;
    if (!text) return NULL;
    n=mulib_xml_new(MULIB_XML_ELEMENT,"text"); if (!n) return NULL;
    t=mulib_xml_new(MULIB_XML_TEXT,text);
    if (!t) { mulib_xml_destroy(n); return NULL; }
    if (!mulib_xml_append(n,t)) { mulib_xml_destroy(t); mulib_xml_destroy(n); return NULL; }
    return mulib_svg_finish_internal(parent,n,mulib_svg_number_attr_internal(n,"x",x)&&mulib_svg_number_attr_internal(n,"y",y)&&mulib_svg_optional_internal(n,"fill",fill));
}
mulib_xml_node_t *mulib_svg_path(mulib_xml_node_t *parent,const char *data,const char *fill,const char *stroke) {
    mulib_xml_node_t *n;
    if (!data) return NULL;
    n=mulib_xml_new(MULIB_XML_ELEMENT,"path"); if (!n) return NULL;
    return mulib_svg_finish_internal(parent,n,mulib_xml_set_attr(n,"d",data)&&mulib_svg_optional_internal(n,"fill",fill)&&mulib_svg_optional_internal(n,"stroke",stroke));
}
char *mulib_svg_serialize(const mulib_xml_node_t *node,size_t *length) {
    const mulib_xml_node_t *root=mulib_xml_type(node)==MULIB_XML_DOCUMENT?mulib_xml_root(node):node;
    if (length) *length=0;
    if (!root||!mulib_xml_name(root)||strcmp(mulib_xml_name(root),"svg")) return NULL;
    return mulib_xml_serialize(node,length);
}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
