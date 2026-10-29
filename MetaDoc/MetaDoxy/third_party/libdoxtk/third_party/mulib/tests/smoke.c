#include <assert.h>
#define MULIB_IMPL_VEC
#define MULIB_IMPL_QUEUE
#define MULIB_IMPL_HEAP
#define MULIB_IMPL_MATH
#define MULIB_IMPL_VARINT
#define MULIB_IMPL_CRC32
#define MULIB_IMPL_BITSET
#define MULIB_IMPL_DSU
#include "mulib-vec.h"
#include "mulib-queue.h"
#include "mulib-heap.h"
#include "mulib-math.h"
#include "mulib-varint.h"
#include "mulib-crc32.h"
#include "mulib-bitset.h"
#include "mulib-dsu.h"
int main(void){
 int x=12,y=0;mulib_vec_t v;mulib_vec_init(&v,sizeof x);assert(mulib_vec_push(&v,&x));assert(*(int*)mulib_vec_at(&v,0)==12);mulib_vec_free(&v);
 mulib_queue_t q;mulib_queue_init(&q,sizeof x);assert(mulib_queue_push(&q,&x));assert(mulib_queue_pop(&q,&y)&&y==12);mulib_queue_free(&q);
 mulib_heap_t h;mulib_heap_init(&h);assert(mulib_heap_push(&h,3));assert(mulib_heap_push(&h,-1));int64_t t;assert(mulib_heap_pop(&h,&t)&&t==-1);mulib_heap_free(&h);
 assert(mulib_math_gcd(54,24)==6);
 unsigned char b[10];size_t n=mulib_varint_encode(1234567,b),used=0;uint64_t v64=0;assert(mulib_varint_decode(b,n,&v64,&used)&&used==n&&v64==1234567);
 assert(mulib_crc32("123456789",9)==0xcbf43926u);
 mulib_bitset_t bs;assert(mulib_bitset_init(&bs,40));assert(mulib_bitset_set(&bs,39,1));assert(mulib_bitset_count(&bs)==1);mulib_bitset_free(&bs);
 mulib_dsu_t d;assert(mulib_dsu_init(&d,3));assert(mulib_dsu_union(&d,0,2));assert(mulib_dsu_find(&d,0)==mulib_dsu_find(&d,2));mulib_dsu_free(&d);
 return 0;
}
