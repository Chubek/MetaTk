#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_PERLPOD
#include "mulib-perlpod.h"
#include <stdio.h>
#include <stdlib.h>
int main(void) {
    const char pod[] = "=head1 Example\n\nA B<small> POD renderer with C<escaped E<lt> markup>.\n\n=over\n=item *\nUseful output.\n=back\n=cut\n";
    mulib_perlpod_error_t error; size_t length;
    char *html = mulib_perlpod_render(pod,sizeof pod-1,MULIB_PERLPOD_HTML,NULL,&length,&error);
    if (!html) { fprintf(stderr,"%zu:%zu: %s\n",error.line,error.column,error.message); return 1; }
    if (fwrite(html,1,length,stdout) != length) { free(html); return 1; }
    free(html); return 0;
}
