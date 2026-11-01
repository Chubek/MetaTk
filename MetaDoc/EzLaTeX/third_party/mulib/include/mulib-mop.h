/** @file mulib-mop.h
 * @brief Borrowed class descriptors, inherited methods and checked dispatch.
 * Define MULIB_IMPL_MOP once. No allocations or global registry.
 */
#ifndef MULIB_MOP_H
#define MULIB_MOP_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Return API revision 1. */
  unsigned mulib_mop_version (void);
  typedef struct mulib_mop_object mulib_mop_object_t;
  /** @brief Callback borrows self, argument and output; status is
   * application-defined. */
  typedef int (*mulib_mop_method_fn) (mulib_mop_object_t *self,
                                      const void *argument, void *out);
  typedef struct mulib_mop_method
  {
    const char *name;
    mulib_mop_method_fn call;
  } mulib_mop_method_t;
  /** @brief Immutable descriptors and method/name storage must outlive all
   * objects. Parent chains are acyclic and at most 64 classes deep. Derived
   * methods override.
   */
  typedef struct mulib_mop_class
  {
    const char *name;
    const struct mulib_mop_class *parent;
    const mulib_mop_method_t *methods;
    size_t method_count;
  } mulib_mop_class_t;
  /** @brief Borrowed class and user payload; caller manages lifetime and
   * synchronization. */
  struct mulib_mop_object
  {
    const mulib_mop_class_t *type;
    void *data;
  };
  /** @brief Check the full descriptor chain, including cycles and duplicate
   * methods. */
  int mulib_mop_class_valid (const mulib_mop_class_t *type);
  /** @brief Initialize borrowed object; unchanged on invalid descriptor or
   * NULL output. */
  int mulib_mop_init (mulib_mop_object_t *object,
                      const mulib_mop_class_t *type, void *data);
  /** @brief Return 1 for identity or an ancestor, else 0 (including invalid
   * chains). */
  int mulib_mop_is_a (const mulib_mop_object_t *object,
                      const mulib_mop_class_t *type);
  /** @brief Borrow the nearest named method; NULL for absent/invalid
   * descriptors.
   */
  const mulib_mop_method_t *mulib_mop_lookup (const mulib_mop_class_t *type,
                                              const char *name);
  /** @brief Return 1 if dispatched, 0 if invalid/absent. Optional status
   * receives callback return. */
  int mulib_mop_call (mulib_mop_object_t *object, const char *name,
                      const void *argument, void *out, int *status);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MOP) && !defined(MULIB_MOP_IMPLEMENTED)
#define MULIB_MOP_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  unsigned
  mulib_mop_version (void)
  {
    return 1;
  }
  int
  mulib_mop_class_valid (const mulib_mop_class_t *type)
  {
    const mulib_mop_class_t *seen[64];
    size_t depth = 0;
    if (!type)
      return 0;
    for (; type; type = type->parent)
      {
        if (depth == 64 || !type->name || !*type->name
            || (!type->methods && type->method_count)
            || type->method_count > 1024)
          return 0;
        for (size_t i = 0; i < depth; ++i)
          if (seen[i] == type)
            return 0;
        seen[depth++] = type;
        for (size_t i = 0; i < type->method_count; ++i)
          {
            const mulib_mop_method_t *m = &type->methods[i];
            if (!m->name || !*m->name || !m->call)
              return 0;
            for (size_t j = 0; j < i; ++j)
              if (!strcmp (m->name, type->methods[j].name))
                return 0;
          }
      }
    return 1;
  }
  int
  mulib_mop_init (mulib_mop_object_t *o, const mulib_mop_class_t *t,
                  void *data)
  {
    if (!o || !mulib_mop_class_valid (t))
      return 0;
    o->type = t;
    o->data = data;
    return 1;
  }
  int
  mulib_mop_is_a (const mulib_mop_object_t *o, const mulib_mop_class_t *type)
  {
    if (!o || !type || !mulib_mop_class_valid (o->type))
      return 0;
    for (const mulib_mop_class_t *t = o->type; t; t = t->parent)
      if (t == type)
        return 1;
    return 0;
  }
  const mulib_mop_method_t *
  mulib_mop_lookup (const mulib_mop_class_t *t, const char *name)
  {
    if (!name || !mulib_mop_class_valid (t))
      return NULL;
    for (; t; t = t->parent)
      for (size_t i = 0; i < t->method_count; ++i)
        if (!strcmp (t->methods[i].name, name))
          return &t->methods[i];
    return NULL;
  }
  int
  mulib_mop_call (mulib_mop_object_t *o, const char *name, const void *arg,
                  void *out, int *status)
  {
    const mulib_mop_method_t *m = o ? mulib_mop_lookup (o->type, name) : NULL;
    int result;
    if (!m)
      return 0;
    result = m->call (o, arg, out);
    if (status)
      *status = result;
    return 1;
  }
#ifdef __cplusplus
}
#endif
#endif
