#include "MetaVect-Scene.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"scene check failed at line %d: %s\n",__LINE__,#x);return 1;}} while(0)
int main(void){
    char error[512];
    const char *source="metavect-scene 1 200 100; stroke 0.1 0.2 0.3; fill 1 0 0; width 2; rect 10 20 30 40; circle 100 50 20; path M 0 0 C 1 2 3 4 5 6 Z;";
    mv_scene *scene=mv_scene_parse(source,strlen(source),error,sizeof(error));CHECK(scene);CHECK(!*error);
    mv_scene_view v=mv_scene_get_view(scene);CHECK(v.width==200&&v.height==100&&v.count==3);
    CHECK(v.paths[0].count==5&&v.paths[0].commands[4].op==MV_CLOSE);
    CHECK(v.paths[0].commands[2].values[0]==40&&v.paths[0].commands[2].values[1]==60);
    CHECK(v.paths[0].stroke.red==0.1&&v.paths[0].fill.red==1&&v.paths[0].stroke_width==2);
    CHECK(v.paths[1].count==6&&v.paths[1].commands[1].op==MV_CUBIC);
    CHECK(v.paths[2].commands[1].values[5]==6);
    mv_scene_destroy(scene);mv_scene_destroy(NULL);CHECK(mv_scene_get_view(NULL).count==0);
    const char *bad[]={"", "metavect-scene 2 10 10;", "metavect-scene 1 nan 10;", "metavect-scene 1 0 10;",
        "metavect-scene 1 1e-99 10;", "metavect-scene 1 10 10", "metavect-scene 1 10 10; fill 2 0 0;",
        "metavect-scene 1 10 10; rect 0 0 -1 1;", "metavect-scene 1 10 10; path L 1 2;",
        "metavect-scene 1 10 10; path M 1 2 Z L 3 4;", "metavect-scene 1 10 10; path M 1 2 C 3 4;",
        "metavect-scene 1 10 10; width inf;", "metavect-scene 1 10 10; path ;", "metavect-scene 1 10 10; script x;",
        "metavect-scene 1 10 10; rect 1,2 3 4 5;", "metavect-scene 1 10 10; circle 0 0 1000001;"};
    for(size_t i=0;i<sizeof(bad)/sizeof(*bad);++i){CHECK(!mv_scene_parse(bad[i],strlen(bad[i]),error,sizeof(error)));CHECK(*error);}
    CHECK(!mv_scene_parse(NULL,0,error,sizeof(error)));
    CHECK(!mv_scene_parse(source,strlen(source),NULL,1));
    CHECK(!mv_scene_parse(source,strlen(source)+1,error,sizeof(error))); /* embedded NUL */
    char tiny='x';CHECK(!mv_scene_parse("",0,&tiny,1));CHECK(tiny==0);
    CHECK(!mv_scene_parse(source,4*1024*1024+1,error,sizeof(error)));
    scene=mv_scene_parse("metavect-scene 1 10 10;",23,NULL,0);CHECK(scene);mv_scene_destroy(scene);
    puts("Scene C ABI tests passed");return 0;
}
