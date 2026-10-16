#include "qscript/include/QScript-CFFI.h"
#include "qscript/include/QScript-Diagnostics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FFI:%d: %s (%s)\n",__LINE__,#x,qs_error(c)); exit(1); } } while (0)

static qs_status sum(const int64_t *args,size_t count,int64_t *result,void *user) {
    if (count!=2) return QS_ERROR;
    ++*(int *)user; *result=args[0]+args[1]; return QS_OK;
}
static qs_status failure(const int64_t *args,size_t count,int64_t *result,void *user) {
    (void)args; (void)count; (void)result; (void)user; return QS_ERROR;
}
struct callback_state { qs_context *context; qs_program *program; };
static qs_status into_script(const int64_t *args,size_t count,int64_t *result,void *user) {
    struct callback_state *s=(struct callback_state *)user;
    return qs_call(s->context,s->program,"twice",args,count,result);
}
static int64_t eval(qs_context *c,const char *source) {
    int64_t result=0;
    CHECK(qs_eval(c,source,strlen(source),&result)==QS_OK);
    return result;
}
int main(int argc,char **argv) {
    qs_context *c=qs_create(),*other=qs_create();
    int calls=0,engine;
    int64_t result=0;
    const char *source="fn answer() { return host_sum(20,22); } return answer();";
    qs_program *program;
    CHECK(c && other);
    CHECK(qs_register_function(NULL,"sum",2,sum,&calls)==QS_INVALID_ARGUMENT);
    CHECK(qs_register_function(c,"sum",17,sum,&calls)==QS_INVALID_ARGUMENT);
    CHECK(qs_register_function(c,"bad-name",2,sum,&calls)!=QS_OK);
    CHECK(qs_register_function(c,"host_sum",2,sum,&calls)==QS_OK);
    CHECK(qs_register_function(c,"host_sum",2,sum,&calls)==QS_ERROR);
    CHECK(qs_registered_function_count(c)==1);
    CHECK(!qs_compile(c,"return host_sum(1);",strlen("return host_sum(1);")));
    program=qs_compile(c,source,strlen(source)); CHECK(program);
    for (engine=QS_ENGINE_INTERPRETER;engine<=QS_ENGINE_JIT;++engine) {
        CHECK(qs_set_engine(c,engine)==QS_OK);
        CHECK(qs_run(c,program,&result)==QS_OK && result==42);
        CHECK(qs_call(c,program,"answer",NULL,0,&result)==QS_OK && result==42);
    }
    CHECK(calls==4);
    CHECK(qs_run(other,program,&result)==QS_ERROR);
    {
        const unsigned char *data=NULL; size_t size=0;
        CHECK(qs_emit(c,program,QS_BYTECODE,&data,&size)==QS_ERROR && !data && !size);
        CHECK(qs_emit(c,program,QS_NATIVE,&data,&size)==QS_ERROR);
        CHECK(qs_emit(c,program,QS_C_SOURCE,&data,&size)==QS_OK && size>0);
    }
    CHECK(qs_unregister_function(c,"host_sum")==QS_OK);
    CHECK(qs_register_function(c,"host_sum",2,sum,&calls)==QS_OK);
    CHECK(qs_run(c,program,&result)==QS_ERROR); /* no ID reuse */
    qs_program_destroy(program);
    CHECK(qs_register_function(c,"fail",0,failure,NULL)==QS_OK);
    CHECK(qs_eval(c,"return fail();",strlen("return fail();"),&result)==QS_ERROR);
    CHECK(strstr(qs_error(c),"host callback failed"));
    {
        struct callback_state state;
        source="fn twice(x) { return 2*x; }";
        state.context=other; state.program=qs_compile(other,source,strlen(source)); CHECK(state.program);
        CHECK(qs_register_function(c,"callback",1,into_script,&state)==QS_OK);
        CHECK(eval(c,"return callback(21);")==42);
        CHECK(qs_unregister_function(c,"callback")==QS_OK);
        qs_program_destroy(state.program);
    }
    {
        int64_t values[]={0,-1,INT64_MIN,INT64_MAX,42},decoded[5]={0};
        unsigned char wire[48],short_out[47]; size_t bytes=0,count=0,i;
        memset(short_out,0xab,sizeof short_out);
        CHECK(qs_values_encode(values,5,NULL,0,&bytes)==QS_OK && bytes==48);
        CHECK(qs_values_encode(values,5,short_out,sizeof short_out,&bytes)==QS_INVALID_ARGUMENT);
        for (i=0;i<sizeof short_out;++i) CHECK(short_out[i]==0xab);
        CHECK(qs_values_encode(values,5,wire,sizeof wire,&bytes)==QS_OK);
        CHECK(!memcmp(wire,"QSV1\5\0\0\0",8) && wire[23]==255 && wire[31]==128);
        CHECK(qs_values_decode(wire,bytes,NULL,0,&count)==QS_OK && count==5);
        CHECK(qs_values_decode(wire,bytes,decoded,5,&count)==QS_OK);
        CHECK(!memcmp(values,decoded,sizeof values));
        for (i=0;i<bytes;++i) CHECK(qs_values_decode(wire,i,decoded,5,&count)==QS_INVALID_ARGUMENT);
        wire[4]=255;
        CHECK(qs_values_decode(wire,bytes,decoded,5,&count)==QS_INVALID_ARGUMENT);
        CHECK(qs_values_encode(NULL,1,wire,sizeof wire,&bytes)==QS_INVALID_ARGUMENT);
        CHECK(qs_values_encode(NULL,0,wire,sizeof wire,&bytes)==QS_OK && bytes==8);
        CHECK(qs_values_decode(wire,bytes,NULL,0,&count)==QS_OK && count==0);
    }
    CHECK(!qs_shared_library_open(c,"/definitely/missing/qscript-ffi-library"));
    if (qs_cffi_available()) {
        qs_shared_library *lib;
        qs_cffi_function *f;
        qs_cffi_value args[4],value;
        int types[4]={QS_CFFI_I64,QS_CFFI_I64};
        void *address=NULL;
        CHECK(argc==2);
        lib=qs_shared_library_open(c,argv[1]); CHECK(lib);
        CHECK(qs_shared_library_symbol(c,lib,"fixture_add",&address)==QS_OK && address);
        CHECK(qs_shared_library_symbol(c,lib,"missing_symbol",&address)==QS_ERROR && !address);
        CHECK(!qs_cffi_function_create(c,lib,"fixture_add",99,types,2));
        types[0]=QS_CFFI_VOID;
        CHECK(!qs_cffi_function_create(c,lib,"fixture_add",QS_CFFI_I64,types,2));
        types[0]=QS_CFFI_I64;
        f=qs_cffi_function_create(c,lib,"fixture_add",QS_CFFI_I64,types,2); CHECK(f);
        memset(args,0,sizeof args); args[0].type=args[1].type=QS_CFFI_I64;
        args[0].data.i64=20; args[1].data.i64=22;
        CHECK(qs_cffi_call(c,f,args,2,&value)==QS_OK && value.type==QS_CFFI_I64 && value.data.i64==42);
        CHECK(qs_cffi_call(c,f,args,1,&value)==QS_ERROR);
        args[0].type=QS_CFFI_U64;
        CHECK(qs_cffi_call(c,f,args,2,&value)==QS_ERROR);
        CHECK(qs_cffi_bind(c,"native_add",f)==QS_OK);
        qs_cffi_function_destroy(f);
        CHECK(eval(c,"return native_add(20,22);")==42);
        {
            const char *names[]={"fixture_i8","fixture_u8","fixture_i16","fixture_u16","fixture_i32","fixture_u32","fixture_u64"};
            int scalar_types[]={QS_CFFI_I8,QS_CFFI_U8,QS_CFFI_I16,QS_CFFI_U16,QS_CFFI_I32,QS_CFFI_U32,QS_CFFI_U64};
            size_t n;
            for (n=0;n<7;++n) {
                int type=scalar_types[n]; int is_signed=type==QS_CFFI_I8 || type==QS_CFFI_I16 || type==QS_CFFI_I32;
                f=qs_cffi_function_create(c,lib,names[n],type,&type,1); CHECK(f);
                args[0].type=type;
                if (is_signed) args[0].data.i64=-123;
                else args[0].data.u64=type==QS_CFFI_U64 ? UINT64_MAX : 250;
                CHECK(qs_cffi_call(c,f,args,1,&value)==QS_OK && value.type==type);
                CHECK(is_signed ? value.data.i64==-123 : value.data.u64==args[0].data.u64);
                if (type==QS_CFFI_I8 || type==QS_CFFI_U8) {
                    args[0].data.i64=256;
                    CHECK(qs_cffi_call(c,f,args,1,&value)==QS_ERROR);
                }
                qs_cffi_function_destroy(f);
            }
        }
        types[0]=QS_CFFI_I8; types[1]=QS_CFFI_U16; types[2]=QS_CFFI_FLOAT; types[3]=QS_CFFI_DOUBLE;
        f=qs_cffi_function_create(c,lib,"fixture_mixed",QS_CFFI_DOUBLE,types,4); CHECK(f);
        args[0].type=types[0]; args[0].data.i64=-2;
        args[1].type=types[1]; args[1].data.u64=40;
        args[2].type=types[2]; args[2].data.f64=1.5;
        args[3].type=types[3]; args[3].data.f64=2.5;
        CHECK(qs_cffi_call(c,f,args,4,&value)==QS_OK && value.data.f64==42.0);
        qs_cffi_function_destroy(f);
        types[0]=QS_CFFI_FLOAT;
        f=qs_cffi_function_create(c,lib,"fixture_float",QS_CFFI_FLOAT,types,1); CHECK(f);
        CHECK(qs_cffi_bind(c,"double_float",f)==QS_OK);
        CHECK(eval(c,"return double_float(0x3f800000);")==0x40000000);
        qs_cffi_function_destroy(f);
        types[0]=QS_CFFI_POINTER;
        f=qs_cffi_function_create(c,lib,"fixture_pointer",QS_CFFI_POINTER,types,1); CHECK(f);
        args[0].type=QS_CFFI_POINTER; args[0].data.pointer=&calls;
        CHECK(qs_cffi_call(c,f,args,1,&value)==QS_OK && value.data.pointer==&calls);
        qs_cffi_function_destroy(f);
        {
            int32_t target=0;
            types[0]=QS_CFFI_POINTER; types[1]=QS_CFFI_I32;
            f=qs_cffi_function_create(c,lib,"fixture_write",QS_CFFI_VOID,types,2); CHECK(f);
            args[0].type=QS_CFFI_POINTER; args[0].data.pointer=&target;
            args[1].type=QS_CFFI_I32; args[1].data.i64=42;
            CHECK(qs_cffi_call(c,f,args,2,&value)==QS_OK && value.type==QS_CFFI_VOID && target==42);
            qs_cffi_function_destroy(f);
        }
        f=qs_cffi_function_create(c,lib,"fixture_zero",QS_CFFI_I32,NULL,0); CHECK(f);
        qs_shared_library_close(lib);
        CHECK(qs_cffi_call(c,f,NULL,0,&value)==QS_OK && value.data.i64==42);
        qs_cffi_function_destroy(f);
        CHECK(eval(c,"return native_add(19,23);")==42); /* binding retains library */
        CHECK(qs_cffi_enable(c)==QS_OK && qs_cffi_enable(c)==QS_OK);
        {
            int64_t input[2]={20,22},output[2]={0},buffer=0;
            unsigned char wire[24]; size_t bytes=0,count=0;
            source="use cffi; fn exchange(b) { let x=read(b,8,8)+read(b,16,8); write(b,8,8,x); return x; }";
            program=qs_compile(c,source,strlen(source)); CHECK(program);
            CHECK(qs_values_encode(input,2,wire,sizeof wire,&bytes)==QS_OK);
            CHECK(qs_cffi_buffer_create(c,wire,bytes,&buffer)==QS_OK && buffer>0);
            CHECK(qs_call(c,program,"exchange",&buffer,1,&result)==QS_OK && result==42);
            CHECK(qs_cffi_buffer_read(c,buffer,0,wire,sizeof wire)==QS_OK);
            CHECK(qs_values_decode(wire,sizeof wire,output,2,&count)==QS_OK && output[0]==42 && output[1]==22);
            CHECK(qs_cffi_buffer_write(c,buffer,SIZE_MAX,wire,1)==QS_INVALID_ARGUMENT);
            CHECK(qs_cffi_buffer_read(c,buffer,24,NULL,0)==QS_OK);
            CHECK(qs_cffi_buffer_write(c,buffer,8,wire+16,8)==QS_OK);
            CHECK(qs_cffi_release(c,buffer)==QS_OK);
            CHECK(qs_cffi_buffer_read(c,buffer,0,wire,1)==QS_ERROR);
            CHECK(qs_cffi_release(c,buffer)==QS_ERROR);
            CHECK(qs_cffi_buffer_create(c,NULL,SIZE_MAX,&buffer)==QS_ERROR && buffer==0);
            qs_program_destroy(program);
        }
        CHECK(eval(c,"use cffi; let b=alloc(8); write(b,0,8,-1); let r=read(b,0,8); release(b); return r;")==-1);
        CHECK(qs_eval(c,"use cffi; return read(alloc(1),0,8);",strlen("use cffi; return read(alloc(1),0,8);"),&result)==QS_ERROR);
        CHECK(strstr(qs_error(c),"out of bounds"));
        CHECK(qs_eval(c,"use cffi; let b=alloc(1); release(b); return byte(b,0);",strlen("use cffi; let b=alloc(1); release(b); return byte(b,0);"),&result)==QS_ERROR);
    } else CHECK(qs_cffi_enable(c)==QS_ERROR);
    qs_destroy(other); qs_destroy(c);
    puts("QScript C FFI tests passed");
    return 0;
}
