#ifndef METAAS_H
#define METAAS_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define METAAS_ABI_VERSION 1u
typedef struct mas_context mas_context;
typedef struct mas_image mas_image;
enum { MAS_OK=0, MAS_ERROR=1, MAS_INVALID_ARGUMENT=2 };
/* Opaque C11 API. Contexts are caller-owned, non-reentrant, and not thread-safe.
 * Images own their data and survive context destruction. Returned pointers are
 * borrowed until the owning context's next operation or image destruction.
 * NULL destroy is harmless; no exception crosses this interface. */
mas_context *mas_create(void);
void mas_destroy(mas_context *);
const char *mas_error(const mas_context *);
int mas_load(mas_context *,const char *spec_path);
int mas_set_target(mas_context *,size_t target);
int mas_set_origin(mas_context *,uint64_t address);
int mas_set_limits(mas_context *,size_t source_bytes,size_t output_bytes);
/* Copies a complete source-operand-order -> encoder-parameter mapping. */
int mas_bind(mas_context *,const char *encoder,const char *const *parameters,size_t count);
int mas_assemble(mas_context *,const char *source,size_t length,const char *file,mas_image **out);
void mas_image_destroy(mas_image *);
size_t mas_image_section_count(const mas_image *);
int mas_image_section(const mas_image *,size_t index,const char **name,
                      const unsigned char **data,size_t *size,uint64_t *address);
/* Encoded buffers belong to the context; binary refuses unresolved/ISA relocs. */
int mas_image_binary(mas_context *,const mas_image *,const unsigned char **data,size_t *size);
int mas_image_json(mas_context *,const mas_image *,const char **data,size_t *size);
#ifdef __cplusplus
}
#endif
#endif
