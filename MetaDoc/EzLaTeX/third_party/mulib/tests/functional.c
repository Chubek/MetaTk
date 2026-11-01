#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "mulib-sexpr.h"
#include "mulib-microtcl.h"
#include "mulib-manpage.h"
#include "mulib-argparse.h"
#include "mulib-base64.h"
#include "mulib-hex.h"
#include "mulib-utf8.h"
#include "mulib-url.h"
#include "mulib-glob.h"
#include "mulib-rle.h"
#include "mulib-roman.h"
#include "mulib-fraction.h"
#include "mulib-levenshtein.h"
#include "mulib-search.h"
#include "mulib-matrix.h"
#include "mulib-graph.h"
#include "mulib-ini.h"
#include "mulib-csv.h"
#include "mulib-bloom.h"
#include "mulib-uuid.h"
#include "mulib-geometry.h"
#include "mulib-date.h"
#include "mulib-buffer.h"
#include "mulib-queue.h"
#include "mulib-deque.h"
#include "mulib-histogram.h"
#include "mulib-popcount.h"
#include "mulib-prime.h"
#include "mulib-interpolate.h"
#include "mulib-fs.h"
#include "mulib-escape.h"
#include "mulib-clamp.h"
#include "mulib-adler32.h"
#include "mulib-color.h"
#include "mulib-logger.h"
#include "mulib-strbuf.h"
#include "mulib-replace.h"
#include "mulib-split.h"
#include "mulib-text.h"
#include "mulib-semver.h"
#include "mulib-rng.h"
#include "mulib-bounds.h"
#include "mulib-stats.h"
#include "mulib-timer.h"
#include "mulib-path.h"
#include "mulib-list.h"
#include "mulib-grid.h"
#include "mulib-bitmap.h"
#include "mulib-heap.h"
#include "mulib-math.h"

static int ini_cb(void*user,const char*section,const char*k,const char*v) {
  int *n=user; assert(!strcmp(section,"foo")&&!strcmp(k,"a")&&!strcmp(v,"4")); ++*n; return 1;
}
static int csv_cb(void*user,const char*const*f,size_t n) {
  int *rows=user; assert(n==3); assert(!strcmp(f[0],"A")&&!strcmp(f[1],"B,C")&&!strcmp(f[2],"D\"E"));++*rows;return 1;
}
static void log_cb(void*user,mulib_logger_level_t lvl,const char*message){int *n=user;assert(lvl==MULIB_LOG_WARN&&!strcmp(message,"alert"));++*n;}
int main(void){
  const char*end=NULL;
  mulib_sexpr_t *sexpr=mulib_sexpr_parse("(hello (world 1) \"two words\")",&end);
  assert(sexpr&&!*end&&sexpr->kind==MULIB_SEXPR_LIST);
  char *txt=mulib_sexpr_render(sexpr);
  assert(txt&&!strcmp(txt,"(hello (world 1) \"two words\")")); free(txt);mulib_sexpr_destroy(sexpr);
  assert(!mulib_sexpr_parse("(unfinished",NULL));
  mulib_microtcl_t *t=mulib_microtcl_loadall();assert(t);
  assert(mulib_microtcl_eval(t,"set x 1; incr x 3; set y \"$x!\""));
  assert(!strcmp(mulib_microtcl_get(t,"x"),"4"));assert(!strcmp(mulib_microtcl_result(t),"4!"));
  assert(mulib_microtcl_eval(t,"expr 3 + 4")&&!strcmp(mulib_microtcl_result(t),"7"));
  assert(!mulib_microtcl_eval(t,"bogus foo"));mulib_microtcl_destroy(t);
  mulib_manpage_t *mp=mulib_manpage_linux(); assert(mp);
  assert(mulib_manpage_set(mp,"FOO","1","foo [options]","Tool description"));
  txt=mulib_manpage_render(mp);assert(txt&&strstr(txt,".TH FOO 1")&&strstr(txt,".SH DESCRIPTION"));free(txt);mulib_manpage_destroy(mp);
  char *argv[]={"prog","--out=result","-v","1","src","--","--anything"};
  mulib_argparse_t*a=mulib_argparse_load(); assert(a&&mulib_argparse_parse(a,7,argv));
  assert(!strcmp(mulib_argparse_get(a,"out"),"result")&&!strcmp(mulib_argparse_get(a,"v"),"1"));
  assert(a->positional_count==2&&!strcmp(a->positionals[1],"--anything"));mulib_argparse_destroy(a);
  char buf[128];unsigned char raw[128]; size_t written=0;
  assert(mulib_base64_encode("hello",5,buf,sizeof buf)==8&&!strcmp(buf,"aGVsbG8="));
  assert(mulib_base64_decode(buf,8,raw,sizeof raw,&written)&&written==5&&!memcmp(raw,"hello",5));
  assert(!mulib_base64_decode("a!!!",4,raw,sizeof raw,&written));
  assert(mulib_hex_encode("abc",3,buf,sizeof buf)==6&&!strcmp(buf,"616263"));
  assert(mulib_hex_decode(buf,6,raw,sizeof raw)&&!memcmp(raw,"abc",3));
  uint32_t cp;size_t used;
  assert(mulib_utf8_decode((const unsigned char*)"\xF0\x9F\x98\x80",4,&cp,&used)&&cp==0x1f600&&used==4);
  assert(!mulib_utf8_validate((const unsigned char*)"\xC0\x80",2));
  assert(mulib_utf8_encode(0x1f600,raw,sizeof raw)==4);
  assert(mulib_url_encode((const unsigned char*)"a b",3,buf,sizeof buf)==5&&!strcmp(buf,"a%20b"));
  assert(mulib_url_decode(buf,5,raw,sizeof raw,&written)&&written==3&&!memcmp(raw,"a b",3));
  assert(mulib_glob_match("hello*.?","helloworld.c")&&!mulib_glob_match("*.c","a.cpp"));
  unsigned char runs[]={1,1,1,2,2,2,2,3};
  size_t rlen=mulib_rle_encode(runs,sizeof runs,raw,sizeof raw);
  unsigned char restored[16];assert(rlen&&mulib_rle_decode(raw,rlen,restored,sizeof restored,&written)&&written==8&&!memcmp(runs,restored,8));
  assert(mulib_roman_format(1994,buf,sizeof buf)&&!strcmp(buf,"MCMXCIV"));
  assert(mulib_roman_parse("MCMXCIV")==1994&&mulib_roman_parse("IIII")==0);
  mulib_fraction_t f,g,h;assert(mulib_fraction_make(2,4,&f)&&f.num==1&&f.den==2);
  assert(mulib_fraction_make(1,6,&g)&&mulib_fraction_add(f,g,&h)&&h.num==2&&h.den==3);
  assert(mulib_levenshtein_distance("kitten","sitting")==3);
  assert(mulib_search_kmp("abcdef",6,"cde",3)==2);
  mulib_matrix_t A,B,C={0};assert(mulib_matrix_init(&A,1,2)&&mulib_matrix_init(&B,2,1));
  A.data[0]=3;A.data[1]=4;B.data[0]=5;B.data[1]=6;
  assert(mulib_matrix_multiply(&A,&B,&C)&&C.data[0]==39);mulib_matrix_free(&A);mulib_matrix_free(&B);mulib_matrix_free(&C);
  mulib_graph_t graph;assert(mulib_graph_init(&graph,3));assert(mulib_graph_edge(&graph,0,1,1));assert(mulib_graph_edge(&graph,1,2,1));
  size_t order[3];assert(mulib_graph_bfs(&graph,0,order,3)==3&&order[0]==0&&order[2]==2);mulib_graph_free(&graph);
  int calls=0;assert(mulib_ini_parse("[foo]\na=4\n",ini_cb,&calls)&&calls==1);
  calls=0;assert(mulib_csv_parse("A,\"B,C\",\"D\"\"E\"\n",csv_cb,&calls)&&calls==1);
  mulib_bloom_t bloom;assert(mulib_bloom_init(&bloom,1000,5));mulib_bloom_add(&bloom,"hello",5);assert(mulib_bloom_test(&bloom,"hello",5));mulib_bloom_free(&bloom);
  mulib_uuid_t id;assert(mulib_uuid_parse("550e8400-e29b-41d4-a716-446655440000",&id));char idbuf[37];mulib_uuid_format(&id,idbuf);assert(!strcmp(idbuf,"550e8400-e29b-41d4-a716-446655440000"));
  mulib_geometry_point_t p[]={{0,0},{2,0},{0,2}};assert(mulib_geometry_polygon_area(p,3)==2.0);
  assert(mulib_date_is_leap(2000)&&!mulib_date_is_leap(1900)&&mulib_date_valid(2024,2,29));
  mulib_buffer_t bb;mulib_buffer_init(&bb);assert(mulib_buffer_append(&bb,"abcdef",6));assert(mulib_buffer_remove(&bb,1,2)&&bb.length==4&&!memcmp(bb.data,"adef",4));mulib_buffer_free(&bb);
  mulib_deque_t dq;mulib_deque_init(&dq,sizeof(int));int x=5,y=6,z=0;assert(mulib_deque_push_back(&dq,&x)&&mulib_deque_push_front(&dq,&y)&&mulib_deque_pop_back(&dq,&z)&&z==5);mulib_deque_free(&dq);
  mulib_histogram_t hist;assert(mulib_histogram_init(&hist,4,0,4)&&mulib_histogram_add(&hist,4.0)&&hist.bins[3]==1);mulib_histogram_free(&hist);
  assert(mulib_popcount_u64(15)==4&&mulib_popcount_clz(1)==63&&mulib_popcount_ctz(8)==3);
  assert(mulib_prime_is_prime(97)&&mulib_prime_next(98)==101);
  assert(mulib_interpolate_clamped(0,10,2)==10);
  assert(mulib_escape_c("A\n",buf,sizeof buf)==3&&!strcmp(buf,"A\\n"));assert(mulib_escape_unc(buf,(char*)raw,sizeof raw)&&!strcmp((char*)raw,"A\n"));
  assert(mulib_clamp_add_i64(INT64_MAX,1)==INT64_MAX&&mulib_clamp_sub_i64(INT64_MIN,1)==INT64_MIN);
  assert(mulib_adler32("Wikipedia",9)==0x11e60398u);
  mulib_color_hsv_t hsv=mulib_color_to_hsv((mulib_color_rgb_t){1,0,0});assert(fabs(hsv.h)<1e-9);
  calls=0;mulib_logger_t l;mulib_logger_init(&l,log_cb,&calls,MULIB_LOG_WARN);
  assert(mulib_logger_write(&l,MULIB_LOG_DEBUG,"skip"));assert(mulib_logger_write(&l,MULIB_LOG_WARN,"alert")&&calls==1);
  mulib_strbuf_t sb;mulib_strbuf_init(&sb);assert(mulib_strbuf_append(&sb,"hello")&&mulib_strbuf_putc(&sb,'!')&&!strcmp(sb.data,"hello!"));mulib_strbuf_free(&sb);
  txt=mulib_replace_all("abcabc","ab","q");assert(txt&&!strcmp(txt,"qcqc"));free(txt);
  char text[]="A,B,C";char*fields[3];assert(mulib_split_inplace(text,',',fields,3)==3&&!strcmp(fields[2],"C"));
  char whitespace[]="  hi  ";assert(!strcmp(mulib_text_trim(whitespace),"hi"));
  mulib_semver_t sv;assert(mulib_semver_parse("1.2.3",&sv)&&sv.patch==3);
  mulib_rng_t rng;mulib_rng_seed(&rng,7);uint64_t first=mulib_rng_next(&rng);mulib_rng_seed(&rng,7);assert(first==mulib_rng_next(&rng));
  assert(mulib_bounds_intersects((mulib_bounds_t){0,0,1,1},(mulib_bounds_t){1,1,2,2}));
  mulib_stats_t stats;mulib_stats_init(&stats);mulib_stats_add(&stats,2);mulib_stats_add(&stats,4);assert(stats.mean==3&&mulib_stats_variance(&stats)==2);
  mulib_timer_t timer;assert(mulib_timer_start(&timer));assert(isfinite(mulib_timer_elapsed(&timer)));
  assert(!strcmp(mulib_path_basename("/a/b"),"b"));
  mulib_list_node_t root,node;mulib_list_init(&root);assert(mulib_list_empty(&root));mulib_list_append(&root,&node);assert(!mulib_list_empty(&root));mulib_list_remove(&node);assert(mulib_list_empty(&root));
  mulib_grid_t grid;assert(mulib_grid_init(&grid,2,2,sizeof(int)));int *cell=mulib_grid_at(&grid,1,1);assert(cell);*cell=7;assert(*(int*)mulib_grid_at(&grid,1,1)==7);mulib_grid_free(&grid);
  mulib_bitmap_t bitmap;assert(mulib_bitmap_init(&bitmap,4,4));assert(mulib_bitmap_put(&bitmap,3,3,1)&&mulib_bitmap_get(&bitmap,3,3)==1);mulib_bitmap_free(&bitmap);
  mulib_heap_t heap;mulib_heap_init(&heap);assert(mulib_heap_push(&heap,4)&&mulib_heap_push(&heap,-3));int64_t val;assert(mulib_heap_pop(&heap,&val)&&val==-3);mulib_heap_free(&heap);
  assert(mulib_math_gcd(12,20)==4);
  puts("functional checks passed"); return 0;
}
