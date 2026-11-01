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
#include <errno.h>
#include <stdlib.h>
static int oix_allocations_before_failure = -1;
static int
oix_fail_allocation (void)
{
  if (oix_allocations_before_failure < 0)
    return 0;
  if (oix_allocations_before_failure-- == 0)
    {
      errno = ENOMEM;
      return 1;
    }
  return 0;
}
static void *
oix_test_malloc (size_t n)
{
  return oix_fail_allocation () ? NULL : malloc (n);
}
static void *
oix_test_calloc (size_t n, size_t size)
{
  return oix_fail_allocation () ? NULL : calloc (n, size);
}
#if defined(__linux__) && (defined(__i386__) || defined(__x86_64__))
/* Intercept only an artificial fd; hardware tests still enter real KVM. */
static int oix_mock_result, oix_mock_errno, oix_mock_count;
static uint32_t oix_mock_reason = KVM_EXIT_IO;
static int
oix_test_kvm_run (mulib_kvm_vcpu_t *cpu, uint32_t *reason)
{
  if (cpu->fd != -123)
    return mulib_kvm_run (cpu, reason);
  ++oix_mock_count;
  errno = oix_mock_errno;
  *reason = oix_mock_reason;
  return oix_mock_result;
}
#define mulib_kvm_run oix_test_kvm_run
#endif
#define malloc oix_test_malloc
#define calloc oix_test_calloc
#define MULIB_IMPL_OIX
#include "mulib-oix.h"
#undef malloc
#undef calloc
#undef mulib_kvm_run
#include "oix_cpu.h"
#include <assert.h>
#include <stdio.h>

#define INS(op, v) { MULIB_OIX_##op, v, NULL, 0, NULL }
#define TEXT(s) { MULIB_OIX_WRITE, 0, s, sizeof (s) - 1, NULL }
#define FILE_OP(op, f) { MULIB_OIX_##op, 0, NULL, 0, f }
static void
expected (oix_cpu *cpu, const void *bytes, size_t length, unsigned code)
{
  cpu_run (cpu, 1000000);
  assert (cpu->halted);
  assert (cpu->exit_code == code);
  assert (cpu->output_size == length);
  assert (!memcmp (cpu->output, bytes, length));
}
static void
native_kernel (void)
{
  mulib_oix_config_t c;
  mulib_oix_image_t image = MULIB_OIX_IMAGE_INIT;
  mulib_oix_info_t info;
  oix_cpu cpu;
  mulib_oix_config_init (&c);
  assert (!mulib_oix_compile (&c, &image, NULL));
  assert (!mulib_oix_validate (image.data, image.length, &info, NULL));
  assert (info.ram_bytes == 65536 && info.files == 0 && info.tasks == 1);
  assert (info.kernel_bytes < 1024 && info.image_bytes < 1024);
  cpu_init (&cpu, &image);
  expected (&cpu, "OIX ready\n", 10, 0);
  mulib_oix_image_free (&image);
  mulib_oix_image_free (&image);
  mulib_oix_image_free (NULL);
  {
    const mulib_oix_instruction_t first[]
        = { TEXT ("A"), INS (YIELD, 0), TEXT ("C"), INS (EXIT, 7) };
    const mulib_oix_instruction_t second[]
        = { TEXT ("B"), TEXT ("D"), INS (EXIT, 9) };
    const mulib_oix_task_t tasks[]
        = { { "first", first, 4 }, { "second", second, 3 } };
    c.banner = "";
    c.tasks = tasks;
    c.task_count = 2;
    assert (!mulib_oix_compile (&c, &image, NULL));
    cpu_init (&cpu, &image);
    expected (&cpu, "ABDC", 4, 9);
    assert (cpu.ticks == 7);
    mulib_oix_image_free (&image);
  }
  {
    const mulib_oix_instruction_t code[]
        = { INS (SET, 3), INS (NUMBER, 0),  INS (ADD, 65535),
            INS (JNZ, 1), INS (SET, 65535), INS (NUMBER, 0),
            INS (ADD, 1), INS (NUMBER, 0),  INS (JUMP, 10),
            TEXT ("BAD"), INS (EXIT, 0) };
    const mulib_oix_task_t task
        = { "loop", code, sizeof (code) / sizeof (code[0]) };
    c.tasks = &task;
    c.task_count = 1;
    assert (!mulib_oix_compile (&c, &image, NULL));
    cpu_init (&cpu, &image);
    expected (&cpu, "321655350", 9, 0);
    mulib_oix_image_free (&image);
  }
  {
    const unsigned char file_data[] = { 0, 255, 'X' };
    const mulib_oix_file_t file = { "/data/raw", file_data, 3 };
    const mulib_oix_instruction_t code[] = { FILE_OP (SIZE, "/data/raw"),
                                             INS (NUMBER, 0),
                                             INS (SET, 1),
                                             FILE_OP (BYTE, "/data/raw"),
                                             INS (NUMBER, 0),
                                             FILE_OP (CAT, "/data/raw"),
                                             INS (LIST, 0),
                                             INS (INFO, 0),
                                             INS (SET, 3),
                                             FILE_OP (BYTE, "/data/raw"),
                                             TEXT ("BAD") };
    const mulib_oix_task_t task
        = { "files", code, sizeof (code) / sizeof (code[0]) };
    const unsigned char output[]
        = { '3', '2', '5', '5', 0,   255,  'X', '/', 'd', 'a', 't',
            'a', '/', 'r', 'a', 'w', '\n', 'O', 'I', 'X', '\n' };
    c.tasks = &task;
    c.files = &file;
    c.file_count = 1;
    assert (!mulib_oix_compile (&c, &image, NULL));
    cpu_init (&cpu, &image);
    expected (&cpu, output, sizeof (output), 255);
    mulib_oix_image_free (&image);
    c.files = NULL;
    c.file_count = 0;
  }
  {
    const mulib_oix_instruction_t code[]
        = { INS (READ, 0), INS (PUTC, 0), INS (READ, 0), INS (PUTC, 0),
            INS (EXIT, 0) };
    const mulib_oix_task_t task = { "console", code, 5 };
    c.tasks = &task;
    assert (!mulib_oix_compile (&c, &image, NULL));
    cpu_init (&cpu, &image);
    cpu_run (&cpu, 20000);
    assert (!cpu.halted && cpu.ticks > 10 && cpu.output_size == 0);
    cpu.input[0] = 0;
    cpu.input_size = 1;
    cpu_run (&cpu, 20000);
    assert (!cpu.halted && cpu.output_size == 1 && cpu.output[0] == 0);
    cpu.input[1] = '!';
    cpu.input_size = 2;
    expected (&cpu, "\0!", 2, 0);
    mulib_oix_image_free (&image);
  }
  {
    const mulib_oix_instruction_t code[] = { TEXT ("fall") };
    const mulib_oix_task_t task = { "fall", code, 1 };
    c.tasks = &task;
    assert (!mulib_oix_compile (&c, &image, NULL));
    cpu_init (&cpu, &image);
    expected (&cpu, "fall", 4, 255);
    mulib_oix_image_free (&image);
  }
}
static void
formats (void)
{
  const char *sources[] = {
    "{\"name\":\"Tiny\",\"banner\":\"\",\"files\":[{\"name\":\"/"
    "a\",\"data_base64\":\"AP8=\"}],\"tasks\":[{\"name\":\"init\",\"code\":[{"
    "\"op\":\"cat\",\"file\":\"/"
    "a\"},{\"op\":\"write\",\"text\":\"ok\"},{\"op\":\"exit\",\"value\":4}]}]"
    "}",
    "name: Tiny\nbanner: \"\"\nfiles:\n  - name: /a\n    data_base64: "
    "AP8=\ntasks:\n  - name: init\n    code:\n      - op: cat\n        file: "
    "/a\n      - op: write\n        text: ok\n      - op: exit\n        "
    "value: 4\n",
    "name = \"Tiny\"\nbanner = \"\"\n[[files]]\nname = \"/a\"\ndata_base64 = "
    "\"AP8=\"\n[[tasks]]\nname = \"init\"\ncode = [{op=\"cat\",file=\"/a\"}, "
    "{op=\"write\",text=\"ok\"}, {op=\"exit\",value=4}]\n",
    "name = \"Tiny\"\nbanner = \"\"\nfile \"/a\" { data_base64 = \"AP8=\" "
    "}\ntask \"init\" { instruction { op=cat file=\"/a\" } instruction { "
    "op=write text=ok } instruction { op=exit value=4 } }"
  };
  mulib_oix_image_t reference = MULIB_OIX_IMAGE_INIT;
  for (unsigned f = 0; f < 4; ++f)
    {
      mulib_oix_image_t image = MULIB_OIX_IMAGE_INIT;
      mulib_oix_error_t error;
      oix_cpu cpu;
      mulib_oix_status_t s
          = mulib_oix_compile_text ((mulib_oix_format_t)f, sources[f],
                                    strlen (sources[f]), &image, &error);
      if (s)
        fprintf (stderr, "OIX format %u: %s at %zu task %zu instruction %zu\n",
                 f, error.message, error.offset, error.task,
                 error.instruction);
      assert (!s);
      assert (!mulib_oix_validate (image.data, image.length, NULL, NULL));
      cpu_init (&cpu, &image);
      expected (&cpu, "\0\xffok", 4, 4);
      if (!f)
        reference = image;
      else
        {
          assert (image.length == reference.length
                  && !memcmp (image.data, reference.data, image.length));
          mulib_oix_image_free (&image);
        }
    }
  mulib_oix_image_free (&reference);
  {
    const char *bad[]
        = { "",
            "[]",
            "{",
            "{\"unknown\":1}",
            "{\"tasks\":[]}",
            "{\"files\":[{\"name\":\"/"
            "a\",\"data\":\"x\",\"data_base64\":\"YQ==\"}]}",
            "{\"files\":[{\"name\":\"/a\",\"data_base64\":\"!\"}]}",
            "{\"tasks\":[{\"name\":\"t\",\"code\":[{\"op\":\"unknown\"}]}]}",
            "{\"tasks\":[{\"name\":\"t\",\"code\":[{\"op\":\"set\",\"value\":"
            "1.5}]}]}",
            "{\"tasks\":[{\"name\":\"t\",\"code\":[{\"op\":\"yield\","
            "\"value\":0}]}]}",
            "{\"tasks\":[{\"name\":\"t\",\"code\":[{\"op\":\"jump\","
            "\"target\":1}]}]}" };
    for (size_t j = 0; j < sizeof (bad) / sizeof (bad[0]); ++j)
      {
        mulib_oix_image_t image = MULIB_OIX_IMAGE_INIT;
        mulib_oix_error_t error;
        assert (mulib_oix_compile_text (MULIB_OIX_JSON, bad[j],
                                        strlen (bad[j]), &image, &error));
        assert (error.code && error.message && !image.data && !image.length);
      }
    const char *bad_conf = "task init { code=7 instruction { op=exit } }";
    assert (mulib_oix_compile_text (MULIB_OIX_CONFUSE, bad_conf,
                                    strlen (bad_conf), &reference, NULL));
    assert (
        mulib_oix_compile_text (MULIB_OIX_JSON, NULL, 0, &reference, NULL));
    assert (mulib_oix_compile_text (MULIB_OIX_JSON, "", 1024U * 1024U + 1,
                                    &reference, NULL)
            == MULIB_OIX_LIMIT);
  }
}
static void
boundaries (void)
{
  mulib_oix_config_t config;
  mulib_oix_image_t image = MULIB_OIX_IMAGE_INIT;
  mulib_oix_instruction_t code[256];
  mulib_oix_task_t tasks[16];
  mulib_oix_file_t files[64];
  char task_names[16][16], file_names[64][16];
  unsigned char data[32768];
  oix_cpu cpu;
  memset (code, 0, sizeof (code));
  memset (data, 'x', sizeof (data));
  for (size_t j = 0; j < 256; ++j)
    code[j].opcode = MULIB_OIX_YIELD;
  code[255].opcode = MULIB_OIX_EXIT;
  for (unsigned j = 0; j < 16; ++j)
    {
      snprintf (task_names[j], sizeof (task_names[j]), "task%u", j);
      tasks[j].name = task_names[j];
      tasks[j].code = code;
      tasks[j].count = 256;
    }
  for (unsigned j = 0; j < 64; ++j)
    {
      snprintf (file_names[j], sizeof (file_names[j]), "/file%u", j);
      files[j].name = file_names[j];
      files[j].data = NULL;
      files[j].length = 0;
    }
  mulib_oix_config_init (&config);
  config.banner = "";
  config.tasks = tasks;
  config.task_count = 16;
  config.files = files;
  config.file_count = 64;
  assert (!mulib_oix_compile (&config, &image, NULL));
  cpu_init (&cpu, &image);
  expected (&cpu, "", 0, 0);
  assert (cpu.ticks == 4096);
  mulib_oix_image_free (&image);
  config.tasks = NULL;
  config.task_count = 0;
  assert (mulib_oix_compile (&config, &image, NULL) == MULIB_OIX_INVALID);
  config.tasks = tasks;
  config.task_count = 1;
  tasks[0].count = 1;
  code[0].opcode = MULIB_OIX_EXIT;
  files[0].data = data;
  files[0].length = sizeof (data);
  assert (!mulib_oix_compile (&config, &image, NULL));
  assert (!mulib_oix_validate (image.data, image.length, NULL, NULL));
  mulib_oix_image_free (&image);
  files[1].data = data;
  files[1].length = sizeof (data);
  assert (mulib_oix_compile (&config, &image, NULL) == MULIB_OIX_LIMIT
          && !image.data);
  files[1].data = NULL;
  files[1].length = 0;
  config.file_count = 2;
  files[1].name = files[0].name;
  assert (mulib_oix_compile (&config, &image, NULL) == MULIB_OIX_INVALID);
  files[1].name = "/empty";
  config.task_count = 2;
  tasks[1].name = tasks[0].name;
  assert (mulib_oix_compile (&config, &image, NULL) == MULIB_OIX_INVALID);
  tasks[1].name = "other";
  tasks[1].code = NULL;
  mulib_oix_error_t error;
  assert (mulib_oix_compile (&config, &image, &error) == MULIB_OIX_INVALID);
  assert (error.task == 1 && error.instruction == SIZE_MAX);
  const char *source = "{\"banner\":\"\",\"files\":[{\"name\":\"/"
                       "empty\",\"data_base64\":\"\"}],\"tasks\":[{\"name\":"
                       "\"empty\",\"code\":[{\"op\":\"write\",\"text\":\"\"},{"
                       "\"op\":\"write\",\"data_base64\":\"\"},{\"op\":"
                       "\"cat\",\"file\":\"/empty\"},{\"op\":\"exit\"}]}]}";
  assert (!mulib_oix_compile_text (MULIB_OIX_JSON, source, strlen (source),
                                   &image, NULL));
  cpu_init (&cpu, &image);
  expected (&cpu, "", 0, 0);
  mulib_oix_image_free (&image);
}
static void
rehash (mulib_oix_image_t *i)
{
  mulib_oix_store_ (i->data + 32, mulib_crc32 (i->data + 64, i->length - 64),
                    4);
  mulib_oix_store_ (i->data + 60, mulib_crc32 (i->data, 60), 4);
}
static void
validation (void)
{
  mulib_oix_config_t c;
  mulib_oix_image_t image = MULIB_OIX_IMAGE_INIT, copy = MULIB_OIX_IMAGE_INIT;
  mulib_oix_error_t error;
  mulib_oix_info_t info;
  char payload[] = "copied";
  mulib_oix_instruction_t code[]
      = { TEXT ("hello"), INS (JUMP, 2), INS (EXIT, 0) };
  mulib_oix_task_t task = { "init", code, 3 };
  mulib_oix_file_t file = { "/a", payload, 6 };
  mulib_oix_config_init (&c);
  c.files = &file;
  c.file_count = 1;
  c.tasks = &task;
  assert (!mulib_oix_compile (&c, &image, &error));
  payload[0] = 'X';
  assert (!mulib_oix_validate (image.data, image.length, &info, NULL));
  copy.data = (unsigned char *)malloc (image.length + 1);
  assert (copy.data);
  copy.length = image.length;
  for (size_t j = 0; j < image.length; ++j)
    assert (mulib_oix_validate (image.data, j, NULL, NULL)
            == MULIB_OIX_FORMAT);
  for (size_t j = 0; j < image.length; ++j)
    {
      memcpy (copy.data, image.data, image.length);
      copy.data[j] ^= 128;
      assert (mulib_oix_validate (copy.data, copy.length, NULL, NULL)
              == MULIB_OIX_FORMAT);
    }
  memcpy (copy.data, image.data, image.length);
  copy.data[copy.length] = 0;
  assert (mulib_oix_validate (copy.data, copy.length + 1, NULL, NULL)
          == MULIB_OIX_FORMAT);
  {
    size_t tt = 64 + cpu_word (image.data + 44) - 0x1000,
           ft = 64 + cpu_word (image.data + 46) - 0x1000;
    size_t base = 64 + cpu_word (image.data + tt + 2) - 0x1000;
    const size_t mutations[]
        = { 64,     tt,     tt + 6, tt + 8,   tt + 12,  tt + 14,     ft,
            ft + 2, ft + 4, base,   base + 2, base + 6, base + 8 + 2 };
    for (size_t j = 0; j < sizeof (mutations) / sizeof (mutations[0]); ++j)
      {
        memcpy (copy.data, image.data, image.length);
        copy.data[mutations[j]] ^= 1;
        rehash (&copy);
        assert (mulib_oix_validate (copy.data, copy.length, NULL, NULL)
                == MULIB_OIX_FORMAT);
      }
    assert (image.data[64 + cpu_word (image.data + ft + 4) - 0x1000] == 'c');
  }
  assert (mulib_oix_compile (&c, &image, NULL) == MULIB_OIX_INVALID);
  mulib_oix_image_free (&image);
  mulib_oix_image_free (&copy);
  code[1].value = 3;
  assert (mulib_oix_compile (&c, &image, &error) == MULIB_OIX_INVALID);
  assert (error.task == 0 && error.instruction == 1 && !image.data);
  code[1].value = 2;
  c.reserved[0] = 1;
  assert (mulib_oix_compile (&c, &image, NULL) == MULIB_OIX_INVALID);
  c.reserved[0] = 0;
  c.file_count = 65;
  assert (mulib_oix_compile (&c, &image, NULL) == MULIB_OIX_LIMIT);
  c.file_count = 1;
  file.name = "/../a";
  assert (mulib_oix_compile (&c, &image, NULL) == MULIB_OIX_INVALID);
  file.name = "/a";
  file.length = 32769;
  assert (mulib_oix_compile (&c, &image, NULL) == MULIB_OIX_LIMIT);
  file.length = 6;
  code[0].length = 4097;
  assert (mulib_oix_compile (&c, &image, NULL) == MULIB_OIX_LIMIT);
  code[0].length = 5;
  task.count = 257;
  assert (mulib_oix_compile (&c, &image, NULL) == MULIB_OIX_LIMIT);
  assert (mulib_oix_compile (NULL, &image, NULL) == MULIB_OIX_INVALID);
  assert (mulib_oix_vm_create (NULL, 0, &error) == NULL
          && error.code == MULIB_OIX_FORMAT);
}
#ifdef MULIB_OIX_NATIVE_
static void
host_devices (void)
{
  union
  {
    struct kvm_run align;
    unsigned char bytes[sizeof (struct kvm_run) + 16];
  } buffer;
  mulib_oix_vm_t v;
  mulib_oix_event_t event = { MULIB_OIX_HALTED, 42 };
  mulib_oix_error_t error;
  struct kvm_run *run = (struct kvm_run *)buffer.bytes;
  size_t written;
  unsigned char input[4100];
  memset (input, 'x', sizeof (input));
  memset (&v, 0, sizeof (v));
  memset (&buffer, 0, sizeof (buffer));
  v.cpu.run = buffer.bytes;
  v.cpu.run_size = sizeof (buffer);
  mulib_oix_error_init_ (&error);
  assert (mulib_oix_vm_feed (&v, input, sizeof (input), &written, &error)
              == MULIB_OIX_AGAIN
          && written == 4096);
  assert (!mulib_oix_vm_feed (&v, NULL, 0, &written, NULL) && written == 0);
  run->io.count = 1;
  run->io.data_offset = sizeof (struct kvm_run);
  run->io.direction = KVM_EXIT_IO_IN;
  run->io.size = 1;
  run->io.port = 0xea;
  assert (!mulib_oix_io_ (&v, &event, NULL) && event.kind == MULIB_OIX_INPUT
          && event.value == 1 && v.count == 4096);
  run->io.port = 0xeb;
  for (size_t j = 0; j < 4096; ++j)
    assert (!mulib_oix_io_ (&v, &event, NULL) && event.value == 'x');
  assert (!v.count);
  assert (!mulib_oix_vm_feed (&v, "\0!", 2, &written, NULL));
  assert (!mulib_oix_io_ (&v, &event, NULL) && event.value == 0);
  assert (!mulib_oix_io_ (&v, &event, NULL) && event.value == '!');
  run->io.direction = KVM_EXIT_IO_OUT;
  run->io.port = 0xe9;
  buffer.bytes[sizeof (struct kvm_run)] = 'z';
  assert (!mulib_oix_io_ (&v, &event, NULL) && event.kind == MULIB_OIX_OUTPUT
          && event.value == 'z');
  run->io.port = 0xec;
  assert (!mulib_oix_io_ (&v, &event, NULL) && v.result.ticks == 1);
  run->io.port = 0xf4;
  run->io.size = 2;
  cpu_store (buffer.bytes + sizeof (struct kvm_run), 7);
  assert (!mulib_oix_io_ (&v, &event, NULL) && v.result.halted
          && v.result.exit_code == 7);
  assert (!mulib_oix_vm_step (&v, &event, NULL) && event.value == 7);
  assert (!mulib_oix_vm_run (&v, 0, NULL, NULL, NULL, NULL));
  assert (mulib_oix_vm_feed (&v, "x", 1, &written, NULL) == MULIB_OIX_INVALID);
  v.result.halted = 0;
  assert (mulib_oix_vm_run (&v, 0, NULL, NULL, NULL, NULL) == MULIB_OIX_LIMIT);
  run->io.data_offset = UINT64_MAX;
  event.value = 42;
  assert (mulib_oix_io_ (&v, &event, NULL) == MULIB_OIX_GUEST_FAULT
          && event.value == 42);
  assert (mulib_oix_vm_step (&v, &event, NULL) == MULIB_OIX_GUEST_FAULT);
}
#endif
static void
allocation_failures (void)
{
  mulib_oix_config_t c;
  mulib_oix_image_t image = MULIB_OIX_IMAGE_INIT;
  const char *source
      = "{\"files\":[{\"name\":\"/a\",\"data_base64\":\"YQ==\"}]}";
  mulib_oix_config_init (&c);
  oix_allocations_before_failure = 0;
  assert (mulib_oix_compile (&c, &image, NULL) == MULIB_OIX_MEMORY
          && !image.data);
  for (int j = 0; j < 3; ++j)
    {
      oix_allocations_before_failure = j;
      assert (mulib_oix_compile_text (MULIB_OIX_JSON, source, strlen (source),
                                      &image, NULL)
              == MULIB_OIX_MEMORY);
      assert (!image.data && !image.length);
    }
  oix_allocations_before_failure = -1;
  assert (!mulib_oix_compile (&c, &image, NULL));
  oix_allocations_before_failure = 0;
  mulib_oix_error_t error;
  assert (!mulib_oix_vm_create (image.data, image.length, &error));
#ifdef MULIB_OIX_NATIVE_
  assert (error.code == MULIB_OIX_MEMORY);
#else
  assert (error.code == MULIB_OIX_UNSUPPORTED);
#endif
  oix_allocations_before_failure = -1;
  mulib_oix_image_free (&image);
}
#ifdef MULIB_OIX_NATIVE_
static int
failing_output (void *user, const void *bytes, size_t length)
{
  size_t *calls = (size_t *)user;
  assert (length == 1 && *(const unsigned char *)bytes == 'z');
  ++*calls;
  return 0;
}
static void
run_errors (void)
{
  union
  {
    struct kvm_run align;
    unsigned char bytes[sizeof (struct kvm_run) + 16];
  } buffer;
  struct kvm_run *run = (struct kvm_run *)buffer.bytes;
  mulib_oix_vm_t vm;
  mulib_oix_result_t result;
  mulib_oix_event_t event = { MULIB_OIX_TICK, 99 };
  mulib_oix_error_t error;
  size_t calls = 0;
  memset (&buffer, 0, sizeof (buffer));
  memset (&vm, 0, sizeof (vm));
  vm.cpu.fd = -123;
  vm.cpu.run = buffer.bytes;
  vm.cpu.run_size = sizeof (buffer);
  run->io.count = 1;
  run->io.data_offset = sizeof (struct kvm_run);
  run->io.direction = KVM_EXIT_IO_OUT;
  run->io.port = 0xe9;
  run->io.size = 1;
  buffer.bytes[sizeof (struct kvm_run)] = 'z';
  oix_mock_result = MULIB_KVM_INTERRUPTED;
  oix_mock_errno = EINTR;
  oix_mock_count = 0;
  assert (mulib_oix_vm_run (&vm, 50, NULL, NULL, &result, &error)
          == MULIB_OIX_INTERRUPTED);
  assert (oix_mock_count == 1 && result.exits == 0 && !result.halted);
  assert (mulib_oix_vm_step (&vm, &event, &error) == MULIB_OIX_INTERRUPTED
          && event.value == 99);
  oix_mock_result = MULIB_KVM_EIO;
  oix_mock_errno = EBADF;
  assert (mulib_oix_vm_step (&vm, &event, &error) == MULIB_OIX_SYSTEM
          && error.system_error == EBADF && event.value == 99);
  oix_mock_result = 0;
  oix_mock_errno = 0;
  assert (mulib_oix_vm_run (&vm, 2, NULL, NULL, &result, &error)
              == MULIB_OIX_LIMIT
          && result.exits == 2);
  assert (mulib_oix_vm_run (&vm, 5, failing_output, &calls, &result, &error)
          == MULIB_OIX_HOST_ERROR);
  assert (calls == 1 && result.exits == 3 && !result.halted);
  run->io.port = 0xf4;
  run->io.size = 2;
  cpu_store (buffer.bytes + sizeof (struct kvm_run), 12);
  assert (!mulib_oix_vm_run (&vm, 1, NULL, NULL, &result, &error));
  assert (result.halted && result.exit_code == 12 && result.exits == 4);
  vm.result.halted = 0;
  oix_mock_reason = KVM_EXIT_MMIO;
  assert (mulib_oix_vm_run (&vm, 1, NULL, NULL, &result, &error)
              == MULIB_OIX_GUEST_FAULT
          && vm.faulted);
  int previous = oix_mock_count;
  assert (mulib_oix_vm_step (&vm, &event, &error) == MULIB_OIX_GUEST_FAULT
          && oix_mock_count == previous);
  oix_mock_reason = KVM_EXIT_IO;
}
#endif
static int
console (void *user, const void *bytes, size_t n)
{
  return fwrite (bytes, 1, n, (FILE *)user) == n;
}
static void
hardware (void)
{
  if (!getenv ("MULIB_OIX_TEST_KVM"))
    {
      puts ("OIX hardware test skipped (set MULIB_OIX_TEST_KVM=1 to require "
            "KVM)");
      return;
    }
  mulib_oix_config_t c;
  mulib_oix_image_t image = MULIB_OIX_IMAGE_INIT;
  mulib_oix_error_t error;
  mulib_oix_result_t result;
  mulib_oix_vm_t *vm;
  mulib_oix_config_init (&c);
  assert (!mulib_oix_compile (&c, &image, NULL));
  vm = mulib_oix_vm_create (image.data, image.length, &error);
  if (!vm)
    fprintf (stderr, "OIX KVM create: %s (errno %d)\n", error.message,
             error.system_error);
  assert (vm);
  mulib_oix_image_free (&image);
  assert (!mulib_oix_vm_run (vm, 1000, console, stdout, &result, &error));
  assert (result.halted && !result.exit_code);
  mulib_oix_vm_destroy (vm);
}
int
main (void)
{
  assert (mulib_oix_version () == 1
          && (mulib_oix_features () & MULIB_OIX_BUILD));
  native_kernel ();
  formats ();
  validation ();
  boundaries ();
  allocation_failures ();
#ifdef MULIB_OIX_NATIVE_
  host_devices ();
  run_errors ();
#endif
  hardware ();
  mulib_oix_vm_destroy (NULL);
  puts ("OIX tests passed");
  return 0;
}
