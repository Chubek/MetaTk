/* cc -std=c11 -Iinclude examples/modules/oix.c -lm -o oix
 * ./oix --output oix.img
 * ./oix --json config.json --output custom.img --run
 * Configuration and image generation require only C11/libc and MuLIB headers.
 * --run requires a Linux x86 host with accessible /dev/kvm. */
#define MULIB_IMPL_OIX
#define MULIB_IMPL_EMITTER
#define MULIB_IMPL_CRC32
#define MULIB_IMPL_KVM
#define MULIB_IMPL_JSON
#define MULIB_IMPL_YAML
#define MULIB_IMPL_TOML
#define MULIB_IMPL_CONFUSE
#define MULIB_IMPL_BASE64
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PARSER
#define MULIB_IMPL_PEG
#define MULIB_IMPL_AST
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_GLOB
#include "mulib-oix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
output (void *user, const void *bytes, size_t n)
{
  return fwrite (bytes, 1, n, (FILE *)user) == n;
}
static char *
read_config (const char *path, size_t *length)
{
  FILE *file = fopen (path, "rb");
  char *bytes;
  size_t n;
  if (!file)
    return NULL;
  bytes = (char *)malloc (1024U * 1024U + 1);
  if (!bytes)
    {
      fclose (file);
      return NULL;
    }
  n = fread (bytes, 1, 1024U * 1024U + 1, file);
  int failed = ferror (file);
  if (fclose (file))
    failed = 1;
  if (failed || n > 1024U * 1024U)
    {
      free (bytes);
      return NULL;
    }
  *length = n;
  return bytes;
}
int
main (int argc, char **argv)
{
  const char *path = NULL, *destination = NULL;
  int run = 0, exit_code = 1;
  mulib_oix_format_t format = MULIB_OIX_JSON;
  mulib_oix_config_t config;
  mulib_oix_image_t image = MULIB_OIX_IMAGE_INIT;
  mulib_oix_error_t error;
  mulib_oix_info_t info;
  mulib_oix_vm_t *vm = NULL;
  mulib_oix_status_t status;
  for (int j = 1; j < argc; ++j)
    {
      if (!strcmp (argv[j], "--run"))
        run = 1;
      else if (!strcmp (argv[j], "--output") && j + 1 < argc)
        destination = argv[++j];
      else if ((!strcmp (argv[j], "--json") || !strcmp (argv[j], "--yaml")
                || !strcmp (argv[j], "--toml")
                || !strcmp (argv[j], "--confuse"))
               && j + 1 < argc)
        {
          format = !strcmp (argv[j], "--json")   ? MULIB_OIX_JSON
                   : !strcmp (argv[j], "--yaml") ? MULIB_OIX_YAML
                   : !strcmp (argv[j], "--toml") ? MULIB_OIX_TOML
                                                 : MULIB_OIX_CONFUSE;
          path = argv[++j];
        }
      else
        {
          fprintf (stderr,
                   "usage: %s [--json|--yaml|--toml|--confuse FILE] [--output "
                   "IMAGE] [--run]\n",
                   argv[0]);
          goto done;
        }
    }
  if (path)
    {
      size_t length = 0;
      char *source = read_config (path, &length);
      if (!source)
        {
          fprintf (stderr, "cannot read configuration (maximum 1 MiB): %s\n",
                   path);
          goto done;
        }
      status = mulib_oix_compile_text (format, source, length, &image, &error);
      free (source);
    }
  else
    {
      mulib_oix_config_init (&config);
      status = mulib_oix_compile (&config, &image, &error);
    }
  if (status)
    goto failed;
  status = mulib_oix_validate (image.data, image.length, &info, &error);
  if (status)
    goto failed;
  fprintf (stderr,
           "%s: %zu-byte image, %zu-byte kernel, %zu task(s), %zu file(s)\n",
           info.name, info.image_bytes, info.kernel_bytes, info.tasks,
           info.files);
  if (destination)
    {
      FILE *file = fopen (destination, "wb");
      if (!file)
        {
          perror (destination);
          goto done;
        }
      int failed = fwrite (image.data, 1, image.length, file) != image.length;
      if (fclose (file))
        failed = 1;
      if (failed)
        {
          fprintf (stderr, "cannot write image: %s\n", destination);
          goto done;
        }
    }
  if (run)
    {
      mulib_oix_result_t result;
      vm = mulib_oix_vm_create (image.data, image.length, &error);
      if (!vm)
        goto failed;
      /* Seed console input, without opening a terminal or waiting for stdin.
       */
      status = mulib_oix_vm_feed (vm, "hello\n", 6, NULL, &error);
      if (status)
        goto failed;
      status = mulib_oix_vm_run (vm, 100000, output, stdout, &result, &error);
      if (status)
        goto failed;
      exit_code = (int)result.exit_code;
    }
  else
    exit_code = 0;
  goto done;
failed:
  fprintf (stderr, "OIX: %s (status %d, errno %d, offset %zu)\n",
           error.message, (int)error.code, error.system_error, error.offset);
done:
  mulib_oix_vm_destroy (vm);
  mulib_oix_image_free (&image);
  return exit_code;
}
