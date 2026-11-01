/**
 * @file mulib-oix.h
 * @brief OIX: a tiny standalone x86 guest OS, configuration compiler and KVM
 * VMM. The generated kernel schedules cooperative accumulator-machine tasks,
 * serves read-only RAM files and console I/O. No Linux guest, BIOS, assembler,
 * external runtime libraries or host subprocesses are required to build/run an
 * image. Define MULIB_IMPL_OIX and explicit dependency switches; see
 * manual/mulib-oix.md.
 */
#ifndef MULIB_OIX_H
#define MULIB_OIX_H
#include "mulib-base64.h"
#include "mulib-confuse.h"
#include "mulib-crc32.h"
#include "mulib-emitter.h"
#include "mulib-json.h"
#include "mulib-kvm.h"
#include "mulib-toml.h"
#include "mulib-yaml.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
#define MULIB_OIX_MAX_FILES 64
#define MULIB_OIX_MAX_TASKS 16
#define MULIB_OIX_MAX_INSTRUCTIONS 256
#define MULIB_OIX_RAM_BYTES 65536U
  /** @brief Compiler/platform feature bits, not runtime KVM availability. */
  enum
  {
    MULIB_OIX_BUILD = 1,
    MULIB_OIX_KVM = 2,
    MULIB_OIX_CONFIG_FORMATS = 4
  };
  typedef enum mulib_oix_status
  {
    MULIB_OIX_OK,
    MULIB_OIX_INVALID,
    MULIB_OIX_FORMAT,
    MULIB_OIX_LIMIT,
    MULIB_OIX_MEMORY,
    MULIB_OIX_SYSTEM,
    MULIB_OIX_UNSUPPORTED,
    MULIB_OIX_INTERRUPTED,
    MULIB_OIX_HOST_ERROR,
    MULIB_OIX_GUEST_FAULT,
    MULIB_OIX_AGAIN
  } mulib_oix_status_t;
  /** @brief Static message, saved errno, config byte offset, and
   * task/instruction indices (SIZE_MAX when inapplicable). No pointers into
   * temporary input. */
  typedef struct mulib_oix_error
  {
    mulib_oix_status_t code;
    int system_error;
    size_t offset, task, instruction;
    const char *message;
  } mulib_oix_error_t;
  typedef enum mulib_oix_opcode
  {
    MULIB_OIX_WRITE = 1,
    MULIB_OIX_CAT,
    MULIB_OIX_SET,
    MULIB_OIX_ADD,
    MULIB_OIX_JUMP,
    MULIB_OIX_JNZ,
    MULIB_OIX_READ,
    MULIB_OIX_PUTC,
    MULIB_OIX_NUMBER,
    MULIB_OIX_YIELD,
    MULIB_OIX_EXIT,
    MULIB_OIX_SIZE,
    MULIB_OIX_BYTE,
    MULIB_OIX_LIST,
    MULIB_OIX_INFO
  } mulib_oix_opcode_t;
  /** @brief Borrowed instruction. WRITE uses data/length (binary);
   * CAT/SIZE/BYTE use file; SET/ADD use value; JUMP/JNZ use value as
   * instruction index; EXIT uses value (0..255). All unused fields must be
   * zero/NULL. READ blocks the task until input arrives. NUMBER prints the
   * unsigned 16-bit accumulator in decimal.
   */
  typedef struct mulib_oix_instruction
  {
    mulib_oix_opcode_t opcode;
    uint16_t value;
    const void *data;
    size_t length;
    const char *file;
  } mulib_oix_instruction_t;
  /** @brief Borrowed absolute guest filename (<=127 bytes) and arbitrary
   * bytes. */
  typedef struct mulib_oix_file
  {
    const char *name;
    const void *data;
    size_t length;
  } mulib_oix_file_t;
  /** @brief Borrowed task name (1..31 ASCII letters/digits/_.-) and code. */
  typedef struct mulib_oix_task
  {
    const char *name;
    const mulib_oix_instruction_t *code;
    size_t count;
  } mulib_oix_task_t;
  /** @brief Borrowed build configuration. Initialize before modifying.
   * Compilation copies all referents. Independent configurations/images may
   * build concurrently.
   */
  typedef struct mulib_oix_config
  {
    size_t size;
    const char *name, *banner;
    const mulib_oix_file_t *files;
    size_t file_count;
    const mulib_oix_task_t *tasks;
    size_t task_count;
    uint32_t reserved[4];
  } mulib_oix_config_t;
  /** @brief Owned serialized OIX image; initialize to {NULL,0}; do not copy
   * live objects. Bytes may be saved as-is. A CRC detects corruption, not
   * forgery. */
  typedef struct mulib_oix_image
  {
    unsigned char *data;
    size_t length;
  } mulib_oix_image_t;
#define MULIB_OIX_IMAGE_INIT { NULL, 0 }
  /** @brief Validated image metadata. name borrows input image until
   * freed/changed. */
  typedef struct mulib_oix_info
  {
    const char *name;
    size_t image_bytes, kernel_bytes, files, tasks;
    uint32_t ram_bytes;
  } mulib_oix_info_t;
  typedef enum mulib_oix_format
  {
    MULIB_OIX_JSON,
    MULIB_OIX_YAML,
    MULIB_OIX_TOML,
    MULIB_OIX_CONFUSE
  } mulib_oix_format_t;
  /** @brief Return API/image ABI revision 1. */
  unsigned mulib_oix_version (void);
  /** @brief Return compile-time feature bits; creation still probes /dev/kvm.
   */
  unsigned mulib_oix_features (void);
  /** @brief Defaults: OIX name/banner and one init task that exits
   * successfully. */
  void mulib_oix_config_init (mulib_oix_config_t *config);
  /** @brief Compile a native real-mode kernel plus programs/data into an owned
   * image. Output must be empty and is unchanged on failure. No tools are
   * spawned.
   */
  mulib_oix_status_t mulib_oix_compile (const mulib_oix_config_t *config,
                                        mulib_oix_image_t *out,
                                        mulib_oix_error_t *error);
  /** @brief Parse <=1 MiB config and compile. Binary files/text accept
   * data_base64. Confuse supports titled file/task sections and repeated
   * instruction sections. Full supported schema and format examples are in the
   * manual.
   */
  mulib_oix_status_t mulib_oix_compile_text (mulib_oix_format_t format,
                                             const char *source, size_t length,
                                             mulib_oix_image_t *out,
                                             mulib_oix_error_t *error);
  /** @brief Check header, CRCs, native kernel and canonical layout/records.
   * Does not authenticate images. info is optional and unchanged on failure.
   */
  mulib_oix_status_t mulib_oix_validate (const void *bytes, size_t length,
                                         mulib_oix_info_t *info,
                                         mulib_oix_error_t *error);
  /** @brief Free/reset image; NULL safe and idempotent. */
  void mulib_oix_image_free (mulib_oix_image_t *image);
  typedef struct mulib_oix_vm mulib_oix_vm_t;
  typedef enum mulib_oix_event_kind
  {
    MULIB_OIX_OUTPUT,
    MULIB_OIX_TICK,
    MULIB_OIX_INPUT,
    MULIB_OIX_HALTED
  } mulib_oix_event_kind_t;
  typedef struct mulib_oix_event
  {
    mulib_oix_event_kind_t kind;
    uint16_t value;
  } mulib_oix_event_t;
  typedef struct mulib_oix_result
  {
    uint64_t exits, ticks;
    int halted;
    unsigned exit_code;
  } mulib_oix_result_t;
  /** @brief Callback borrows output bytes during call. Return 1 for consumed,
   * 0 for failure. Never retained; do not reenter the VM from callbacks. */
  typedef int (*mulib_oix_output_fn) (void *user, const void *bytes,
                                      size_t length);
  /** @brief Copy a validated image into private 64 KiB guest RAM and
   * initialize one real-mode vCPU. Linux x86 KVM only; NULL on
   * unavailable/denied KVM or error. No fallback execution or host
   * filesystem/device passthrough. */
  mulib_oix_vm_t *mulib_oix_vm_create (const void *bytes, size_t length,
                                       mulib_oix_error_t *error);
  /** @brief Destroy guest/vCPU/RAM, NULL safe. Does not affect unrelated VMs.
   * All operations on one VM require external synchronization; no hidden
   * threads.
   */
  void mulib_oix_vm_destroy (mulib_oix_vm_t *vm);
  /** @brief Queue up to 4096 console bytes, copying input. written receives
   * the accepted prefix. Returns AGAIN when capacity is insufficient,
   * including a partial write; zero-length input succeeds. No terminal is
   * opened implicitly.
   */
  mulib_oix_status_t mulib_oix_vm_feed (mulib_oix_vm_t *vm, const void *bytes,
                                        size_t length, size_t *written,
                                        mulib_oix_error_t *error);
  /** @brief Process one KVM exit. EINTR/EAGAIN return INTERRUPTED for caller
   * retry; event unchanged then. Repeated calls after halt return HALTED.
   * Completion of emulated I/O occurs on the next KVM_RUN, per KVM's API. */
  mulib_oix_status_t mulib_oix_vm_step (mulib_oix_vm_t *vm,
                                        mulib_oix_event_t *event,
                                        mulib_oix_error_t *error);
  /** @brief Run at most exit_budget KVM exits; LIMIT preserves resumable
   * state. NULL output discards bytes. A failing callback consumes its output
   * event. This is an exit/work budget, not a wall-clock deadline. No EINTR
   * retry here. result is filled for every valid VM, even on failure. */
  mulib_oix_status_t mulib_oix_vm_run (mulib_oix_vm_t *vm, size_t exit_budget,
                                       mulib_oix_output_fn output, void *user,
                                       mulib_oix_result_t *result,
                                       mulib_oix_error_t *error);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OIX) && !defined(MULIB_OIX_IMPLEMENTED)
#define MULIB_OIX_IMPLEMENTED
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#if defined(__linux__) && (defined(__i386__) || defined(__x86_64__))
#define MULIB_OIX_NATIVE_ 1
#include <fcntl.h>
#include <linux/kvm.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif
#ifdef __cplusplus
extern "C"
{
#endif
#define MULIB_OIX_HEADER_ 64U
#define MULIB_OIX_LOAD_ 0x1000U
#define MULIB_OIX_END_ 0xe000U
#define MULIB_OIX_CAPACITY_                                                   \
  (MULIB_OIX_HEADER_ + MULIB_OIX_END_ - MULIB_OIX_LOAD_)
  static void
  mulib_oix_error_init_ (mulib_oix_error_t *e)
  {
    if (e)
      {
        memset (e, 0, sizeof (*e));
        e->task = e->instruction = SIZE_MAX;
        e->message = "ok";
      }
  }
  static mulib_oix_status_t
  mulib_oix_fail_ (mulib_oix_error_t *e, mulib_oix_status_t code,
                   const char *message)
  {
    if (e && e->code == MULIB_OIX_OK)
      {
        e->code = code;
        e->message = message;
        if (code == MULIB_OIX_SYSTEM)
          e->system_error = errno;
      }
    return code;
  }
  unsigned
  mulib_oix_version (void)
  {
    return 1;
  }
  unsigned
  mulib_oix_features (void)
  {
    unsigned flags = MULIB_OIX_BUILD | MULIB_OIX_CONFIG_FORMATS;
#ifdef MULIB_OIX_NATIVE_
    flags |= MULIB_OIX_KVM;
#endif
    return flags;
  }
  void
  mulib_oix_config_init (mulib_oix_config_t *c)
  {
    static const mulib_oix_instruction_t code[]
        = { { MULIB_OIX_EXIT, 0, NULL, 0, NULL } };
    static const mulib_oix_task_t task = { "init", code, 1 };
    if (c)
      {
        memset (c, 0, sizeof (*c));
        c->size = sizeof (*c);
        c->name = "OIX";
        c->banner = "OIX ready\n";
        c->tasks = &task;
        c->task_count = 1;
      }
  }
  void
  mulib_oix_image_free (mulib_oix_image_t *i)
  {
    if (i)
      {
        free (i->data);
        i->data = NULL;
        i->length = 0;
      }
  }
  static uint16_t
  mulib_oix_u16_ (const unsigned char *p)
  {
    return (uint16_t)((uint16_t)p[0] | (uint16_t)p[1] << 8);
  }
  static uint32_t
  mulib_oix_u32_ (const unsigned char *p)
  {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16
           | (uint32_t)p[3] << 24;
  }
  static void
  mulib_oix_store_ (unsigned char *p, uint32_t v, unsigned n)
  {
    for (unsigned j = 0; j < n; ++j)
      {
        p[j] = (unsigned char)v;
        v >>= 8;
      }
  }
  /* The entire freestanding OS kernel is assembled here, without a host
   * toolchain. 16-bit x86, DS=ES=SS=CS=0; fixed low-memory ABI at 0x500.
   * Programs are data, never native code. Each round-robin instruction issues
   * a host-visible tick.
   */
  typedef struct mulib_oix_assembly_
  {
    mulib_emitter_t emitter;
    size_t labels[64];
    struct
    {
      size_t at;
      unsigned label;
    } patches[128];
    size_t count;
    int failed;
  } mulib_oix_assembly_;
  static void
  mulib_oix_emit_ (mulib_oix_assembly_ *a, const char *bytes, size_t n)
  {
    if (mulib_emitter_bytes (&a->emitter, bytes, n))
      a->failed = 1;
  }
  static void
  mulib_oix_label_ (mulib_oix_assembly_ *a, unsigned label)
  {
    a->labels[label] = a->emitter.size;
  }
  static void
  mulib_oix_branch_ (mulib_oix_assembly_ *a, const char *opcode, size_t n,
                     unsigned label)
  {
    mulib_oix_emit_ (a, opcode, n);
    if (a->count == 128)
      {
        a->failed = 1;
        return;
      }
    a->patches[a->count].at = a->emitter.size;
    a->patches[a->count++].label = label;
    if (mulib_emitter_uint (&a->emitter, 0, 2))
      a->failed = 1;
  }
  static size_t
  mulib_oix_kernel_ (unsigned char *out, size_t capacity)
  {
    enum
    {
      SCHEDULE,
      INDEX,
      EXECUTE,
      WRITE,
      CAT,
      SET,
      ADD,
      JUMP,
      JNZ,
      READ,
      BLOCKED,
      PUTC,
      NUMBER,
      YIELD,
      EXIT,
      SIZE,
      BYTE,
      LIST,
      LIST_LOOP,
      LIST_END,
      INFO,
      TRAP,
      DONE,
      PUTCHAR,
      WRITE_FN,
      WRITE_LOOP,
      WRITE_END,
      NUMBER_FN,
      NUMBER_DIV,
      NUMBER_EMIT
    };
    mulib_oix_assembly_ a;
    memset (&a, 0, sizeof (a));
    for (size_t i = 0; i < 64; ++i)
      a.labels[i] = SIZE_MAX;
    if (mulib_emitter_init (&a.emitter, out, capacity, MULIB_EMITTER_LITTLE))
      return 0;
#define MULIB_OIX_B_(s) mulib_oix_emit_ (&a, s, sizeof (s) - 1)
#define MULIB_OIX_L_(l) mulib_oix_label_ (&a, l)
#define MULIB_OIX_J_(s, l) mulib_oix_branch_ (&a, s, sizeof (s) - 1, l)
    MULIB_OIX_B_ (
        "\xfa\xfc\x31\xc0\x8e\xd8\x8e\xc0\x8e\xd0\xbc\xf0\xff"); /* cli; cld;
                                                                    segments=0;
                                                                    sp=fff0 */
    MULIB_OIX_B_ ("\x8b\x36\x0c\x05\x8b\x0e\x0e\x05");           /* banner */
    MULIB_OIX_J_ ("\xe8", WRITE_FN);
    MULIB_OIX_L_ (SCHEDULE);
    MULIB_OIX_B_ ("\x83\x3e\x08\x05\x00"); /* live == 0 */
    MULIB_OIX_J_ ("\x0f\x84", DONE);
    MULIB_OIX_B_ ("\xa1\x06\x05\x3b\x06\x04\x05"); /* current < count */
    MULIB_OIX_J_ ("\x0f\x82", INDEX);
    MULIB_OIX_B_ ("\x31\xc0\xa3\x06\x05");
    MULIB_OIX_L_ (INDEX);
    MULIB_OIX_B_ (
        "\xff\x06\x06\x05\xc1\xe0\x04\x8b\x3e\x02\x05\x01\xc7"); /* DI =
                                                                    tasks+16*index;
                                                                    ++index */
    MULIB_OIX_B_ ("\x83\x7d\x08\x00"); /* skip exited tasks */
    MULIB_OIX_J_ ("\x0f\x85", SCHEDULE);
    MULIB_OIX_B_ (
        "\xba\xec\x00\x30\xc0\xee\xff\x06\x18\x05"); /* tick device; ++ticks */
    MULIB_OIX_B_ ("\x8b\x35\x3b\x75\x04");           /* SI=pc; pc < end */
    MULIB_OIX_J_ ("\x0f\x82", EXECUTE);
    MULIB_OIX_J_ ("\xe9", TRAP);
    MULIB_OIX_L_ (EXECUTE);
    MULIB_OIX_B_ (
        "\x8d\x44\x08\x89\x05\xad\x8b\x1c\x8b\x4c\x02"); /* pc+=8; opcode, BX
                                                            operand, CX aux */
    {
      static const unsigned labels[]
          = { WRITE,  CAT,   SET,  ADD,  JUMP, JNZ,  READ, PUTC,
              NUMBER, YIELD, EXIT, SIZE, BYTE, LIST, INFO };
      for (unsigned i = 0; i < 15; ++i)
        {
          MULIB_OIX_B_ ("\x3d");
          if (mulib_emitter_uint (&a.emitter, i + 1, 2))
            a.failed = 1;
          MULIB_OIX_J_ ("\x0f\x84", labels[i]);
        }
    }
    MULIB_OIX_J_ ("\xe9", TRAP);
    MULIB_OIX_L_ (WRITE);
    MULIB_OIX_B_ ("\x89\xde");
    MULIB_OIX_J_ ("\xe8", WRITE_FN);
    MULIB_OIX_J_ ("\xe9", SCHEDULE);
    MULIB_OIX_L_ (CAT);
    MULIB_OIX_B_ ("\xc1\xe3\x03\x03\x1e\x00\x05\x8b\x77\x04\x8b\x4f\x06");
    MULIB_OIX_J_ ("\xe8", WRITE_FN);
    MULIB_OIX_J_ ("\xe9", SCHEDULE);
    MULIB_OIX_L_ (SET);
    MULIB_OIX_B_ ("\x89\x5d\x06");
    MULIB_OIX_J_ ("\xe9", SCHEDULE);
    MULIB_OIX_L_ (ADD);
    MULIB_OIX_B_ ("\x01\x5d\x06");
    MULIB_OIX_J_ ("\xe9", SCHEDULE);
    MULIB_OIX_L_ (JUMP);
    MULIB_OIX_B_ ("\x89\x1d");
    MULIB_OIX_J_ ("\xe9", SCHEDULE);
    MULIB_OIX_L_ (JNZ);
    MULIB_OIX_B_ ("\x83\x7d\x06\x00");
    MULIB_OIX_J_ ("\x0f\x85", JUMP);
    MULIB_OIX_J_ ("\xe9", SCHEDULE);
    MULIB_OIX_L_ (READ);
    MULIB_OIX_B_ ("\xba\xea\x00\xec\x84\xc0");
    MULIB_OIX_J_ ("\x0f\x84", BLOCKED);
    MULIB_OIX_B_ ("\xba\xeb\x00\xec\x30\xe4\x89\x45\x06");
    MULIB_OIX_J_ ("\xe9", SCHEDULE);
    MULIB_OIX_L_ (BLOCKED);
    MULIB_OIX_B_ ("\x83\x2d\x08");
    MULIB_OIX_J_ ("\xe9", SCHEDULE);
    MULIB_OIX_L_ (PUTC);
    MULIB_OIX_B_ ("\x8b\x45\x06");
    MULIB_OIX_J_ ("\xe8", PUTCHAR);
    MULIB_OIX_J_ ("\xe9", SCHEDULE);
    MULIB_OIX_L_ (NUMBER);
    MULIB_OIX_B_ ("\x8b\x45\x06");
    MULIB_OIX_J_ ("\xe8", NUMBER_FN);
    MULIB_OIX_J_ ("\xe9", SCHEDULE);
    MULIB_OIX_L_ (YIELD);
    MULIB_OIX_J_ ("\xe9", SCHEDULE);
    MULIB_OIX_L_ (EXIT);
    MULIB_OIX_B_ ("\x89\x5d\x0c\xc7\x45\x08\x01\x00\xff\x0e\x08\x05\x85\xdb");
    MULIB_OIX_J_ ("\x0f\x84", SCHEDULE);
    MULIB_OIX_B_ ("\x83\x3e\x0a\x05\x00");
    MULIB_OIX_J_ ("\x0f\x85", SCHEDULE);
    MULIB_OIX_B_ ("\x89\x1e\x0a\x05");
    MULIB_OIX_J_ ("\xe9", SCHEDULE);
    MULIB_OIX_L_ (SIZE);
    MULIB_OIX_B_ ("\xc1\xe3\x03\x03\x1e\x00\x05\x8b\x47\x06\x89\x45\x06");
    MULIB_OIX_J_ ("\xe9", SCHEDULE);
    MULIB_OIX_L_ (BYTE);
    MULIB_OIX_B_ ("\xc1\xe3\x03\x03\x1e\x00\x05\x8b\x45\x06\x3b\x47\x06");
    MULIB_OIX_J_ ("\x0f\x83", TRAP);
    MULIB_OIX_B_ ("\x03\x47\x04\x89\xc3\x8a\x07\x30\xe4\x89\x45\x06");
    MULIB_OIX_J_ ("\xe9", SCHEDULE);
    MULIB_OIX_L_ (LIST);
    MULIB_OIX_B_ ("\x57\x31\xff");
    MULIB_OIX_L_ (LIST_LOOP);
    MULIB_OIX_B_ ("\x3b\x3e\x14\x05");
    MULIB_OIX_J_ ("\x0f\x83", LIST_END);
    MULIB_OIX_B_ ("\x89\xfb\xc1\xe3\x03\x03\x1e\x00\x05\x8b\x37\x8b\x4f\x02");
    MULIB_OIX_J_ ("\xe8", WRITE_FN);
    MULIB_OIX_B_ ("\xb0\x0a");
    MULIB_OIX_J_ ("\xe8", PUTCHAR);
    MULIB_OIX_B_ ("\x47");
    MULIB_OIX_J_ ("\xe9", LIST_LOOP);
    MULIB_OIX_L_ (LIST_END);
    MULIB_OIX_B_ ("\x5f");
    MULIB_OIX_J_ ("\xe9", SCHEDULE);
    MULIB_OIX_L_ (INFO);
    MULIB_OIX_B_ ("\x8b\x36\x10\x05\x8b\x0e\x12\x05");
    MULIB_OIX_J_ ("\xe8", WRITE_FN);
    MULIB_OIX_B_ ("\xb0\x0a");
    MULIB_OIX_J_ ("\xe8", PUTCHAR);
    MULIB_OIX_J_ ("\xe9", SCHEDULE);
    MULIB_OIX_L_ (TRAP);
    MULIB_OIX_B_ ("\xbb\xff\x00");
    MULIB_OIX_J_ ("\xe9", EXIT);
    MULIB_OIX_L_ (DONE);
    MULIB_OIX_B_ ("\xa1\x0a\x05\xba\xf4\x00\xef\xf4");
    MULIB_OIX_J_ ("\xe9", DONE);
    MULIB_OIX_L_ (PUTCHAR);
    MULIB_OIX_B_ ("\x52\xba\xe9\x00\xee\x5a\xc3");
    MULIB_OIX_L_ (WRITE_FN);
    MULIB_OIX_B_ ("\x85\xc9");
    MULIB_OIX_J_ ("\x0f\x84", WRITE_END);
    MULIB_OIX_L_ (WRITE_LOOP);
    MULIB_OIX_B_ ("\xac");
    MULIB_OIX_J_ ("\xe8", PUTCHAR);
    MULIB_OIX_B_ ("\x49");
    MULIB_OIX_J_ ("\x0f\x85", WRITE_LOOP);
    MULIB_OIX_L_ (WRITE_END);
    MULIB_OIX_B_ ("\xc3");
    MULIB_OIX_L_ (NUMBER_FN);
    MULIB_OIX_B_ ("\x53\x51\x52\x31\xc9\xbb\x0a\x00");
    MULIB_OIX_L_ (NUMBER_DIV);
    MULIB_OIX_B_ ("\x31\xd2\xf7\xf3\x52\x41\x85\xc0");
    MULIB_OIX_J_ ("\x0f\x85", NUMBER_DIV);
    MULIB_OIX_L_ (NUMBER_EMIT);
    MULIB_OIX_B_ ("\x58\x04\x30");
    MULIB_OIX_J_ ("\xe8", PUTCHAR);
    MULIB_OIX_B_ ("\x49");
    MULIB_OIX_J_ ("\x0f\x85", NUMBER_EMIT);
    MULIB_OIX_B_ ("\x5a\x59\x5b\xc3");
#undef MULIB_OIX_B_
#undef MULIB_OIX_L_
#undef MULIB_OIX_J_
    for (size_t i = 0; i < a.count; ++i)
      {
        size_t target = a.labels[a.patches[i].label], at = a.patches[i].at;
        if (target == SIZE_MAX
            || mulib_emitter_patch_uint (
                &a.emitter, at,
                (uint16_t)((uint16_t)target - (uint16_t)(at + 2)), 2))
          a.failed = 1;
      }
    return a.failed ? 0 : a.emitter.size;
  }
  static size_t
  mulib_oix_string_length_ (const char *s, size_t maximum)
  {
    size_t n = 0;
    if (!s)
      return maximum + 1;
    while (n <= maximum && s[n])
      ++n;
    return n;
  }
  static int
  mulib_oix_name_ (const char *s, int path)
  {
    size_t n = mulib_oix_string_length_ (s, path ? 127 : 31), start = 1;
    if (!n || n > (path ? 127U : 31U) || (path && (s[0] != '/' || n == 1)))
      return 0;
    for (size_t i = path ? 1 : 0; i < n; ++i)
      {
        unsigned char c = (unsigned char)s[i];
        if (path && c == '/')
          {
            size_t len = i - start;
            if (!len || (len == 1 && s[start] == '.')
                || (len == 2 && s[start] == '.' && s[start + 1] == '.'))
              return 0;
            start = i + 1;
            continue;
          }
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
              || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.'))
          return 0;
      }
    return !path
           || (start < n && !(n - start == 1 && s[start] == '.')
               && !(n - start == 2 && s[start] == '.' && s[start + 1] == '.'));
  }
  static int
  mulib_oix_file_index_ (const mulib_oix_config_t *c, const char *name)
  {
    if (!name)
      return -1;
    for (size_t j = 0; j < c->file_count; ++j)
      if (!strcmp (c->files[j].name, name))
        return (int)j;
    return -1;
  }
  static mulib_oix_status_t
  mulib_oix_config_check_ (const mulib_oix_config_t *c, mulib_oix_error_t *e)
  {
    if (!c || c->size != sizeof (*c) || !mulib_oix_name_ (c->name, 0)
        || mulib_oix_string_length_ (c->banner, 1024) > 1024
        || (!c->files && c->file_count) || !c->tasks || !c->task_count)
      return mulib_oix_fail_ (e, MULIB_OIX_INVALID,
                              "invalid configuration, name or banner");
    for (size_t j = 0; j < 4; ++j)
      if (c->reserved[j])
        return mulib_oix_fail_ (e, MULIB_OIX_INVALID,
                                "nonzero reserved field");
    if (c->file_count > 64 || c->task_count > 16)
      return mulib_oix_fail_ (e, MULIB_OIX_LIMIT,
                              "file or task limit exceeded");
    for (size_t j = 0; j < c->file_count; ++j)
      {
        const mulib_oix_file_t *f = &c->files[j];
        if (!mulib_oix_name_ (f->name, 1) || (!f->data && f->length))
          return mulib_oix_fail_ (e, MULIB_OIX_INVALID, "invalid guest file");
        if (f->length > 32768)
          return mulib_oix_fail_ (e, MULIB_OIX_LIMIT, "file exceeds 32 KiB");
        for (size_t k = 0; k < j; ++k)
          if (!strcmp (f->name, c->files[k].name))
            return mulib_oix_fail_ (e, MULIB_OIX_INVALID,
                                    "duplicate filename");
      }
    for (size_t j = 0; j < c->task_count; ++j)
      {
        const mulib_oix_task_t *t = &c->tasks[j];
        if (e)
          {
            e->task = j;
            e->instruction = SIZE_MAX;
          }
        if (!mulib_oix_name_ (t->name, 0) || !t->code || !t->count)
          return mulib_oix_fail_ (e, MULIB_OIX_INVALID, "invalid task");
        if (t->count > 256)
          return mulib_oix_fail_ (e, MULIB_OIX_LIMIT,
                                  "task exceeds 256 instructions");
        for (size_t k = 0; k < j; ++k)
          if (!strcmp (t->name, c->tasks[k].name))
            return mulib_oix_fail_ (e, MULIB_OIX_INVALID,
                                    "duplicate task name");
        for (size_t k = 0; k < t->count; ++k)
          {
            const mulib_oix_instruction_t *i = &t->code[k];
            int scalar
                = i->opcode == MULIB_OIX_SET || i->opcode == MULIB_OIX_ADD
                  || i->opcode == MULIB_OIX_JUMP || i->opcode == MULIB_OIX_JNZ
                  || i->opcode == MULIB_OIX_EXIT;
            int file = i->opcode == MULIB_OIX_CAT
                       || i->opcode == MULIB_OIX_SIZE
                       || i->opcode == MULIB_OIX_BYTE;
            if (e)
              e->instruction = k;
            if (i->opcode < 1 || i->opcode > 15 || (!scalar && i->value)
                || (i->opcode != MULIB_OIX_WRITE && (i->data || i->length))
                || (!file && i->file))
              return mulib_oix_fail_ (
                  e, MULIB_OIX_INVALID,
                  "invalid opcode or unused instruction operand");
            if (i->opcode == MULIB_OIX_WRITE
                && ((!i->data && i->length) || i->length > 4096))
              return mulib_oix_fail_ (
                  e, i->length > 4096 ? MULIB_OIX_LIMIT : MULIB_OIX_INVALID,
                  "invalid WRITE payload");
            if (file && mulib_oix_file_index_ (c, i->file) < 0)
              return mulib_oix_fail_ (e, MULIB_OIX_INVALID,
                                      "unknown guest file");
            if ((i->opcode == MULIB_OIX_JUMP || i->opcode == MULIB_OIX_JNZ)
                && i->value >= t->count)
              return mulib_oix_fail_ (e, MULIB_OIX_INVALID,
                                      "jump target outside task");
            if (i->opcode == MULIB_OIX_EXIT && i->value > 255)
              return mulib_oix_fail_ (e, MULIB_OIX_INVALID,
                                      "exit code exceeds 255");
          }
      }
    if (e)
      e->task = e->instruction = SIZE_MAX;
    return MULIB_OIX_OK;
  }
  static int
  mulib_oix_append_ (mulib_emitter_t *b, const void *p, size_t n,
                     uint16_t *address)
  {
    if (b->size > UINT16_MAX - MULIB_OIX_LOAD_ || n > b->capacity - b->size)
      return 0;
    if (address)
      *address = (uint16_t)(MULIB_OIX_LOAD_ + b->size);
    return mulib_emitter_bytes (b, p, n) == MULIB_EMITTER_OK;
  }
  mulib_oix_status_t
  mulib_oix_compile (const mulib_oix_config_t *c, mulib_oix_image_t *out,
                     mulib_oix_error_t *e)
  {
    unsigned char *bytes = NULL;
    mulib_emitter_t b;
    size_t kernel, file_table, task_table;
    uint16_t name, banner, ptr;
    mulib_oix_status_t status;
    mulib_oix_error_init_ (e);
    if (!out || out->data || out->length)
      return mulib_oix_fail_ (e, MULIB_OIX_INVALID,
                              "output image must be empty");
    status = mulib_oix_config_check_ (c, e);
    if (status)
      return status;
    bytes = (unsigned char *)calloc (1, MULIB_OIX_CAPACITY_);
    if (!bytes)
      return mulib_oix_fail_ (e, MULIB_OIX_MEMORY, "image allocation failed");
    (void)mulib_emitter_init (&b, bytes + 64, MULIB_OIX_CAPACITY_ - 64,
                              MULIB_EMITTER_LITTLE);
    kernel = mulib_oix_kernel_ (b.data, b.capacity);
    if (!kernel)
      {
        free (bytes);
        return mulib_oix_fail_ (e, MULIB_OIX_FORMAT,
                                "internal kernel assembly failed");
      }
    b.size = kernel;
    if (!mulib_oix_append_ (&b, c->name, strlen (c->name) + 1, &name)
        || !mulib_oix_append_ (&b, c->banner, strlen (c->banner), &banner)
        || mulib_emitter_reserve (&b, c->file_count * 8, &file_table))
      goto limit;
    for (size_t j = 0; j < c->file_count; ++j)
      {
        const mulib_oix_file_t *f = &c->files[j];
        size_t n = strlen (f->name);
        if (!mulib_oix_append_ (&b, f->name, n + 1, &ptr))
          goto limit;
        mulib_oix_store_ (b.data + file_table + j * 8, ptr, 2);
        mulib_oix_store_ (b.data + file_table + j * 8 + 2, (uint32_t)n, 2);
        if (!mulib_oix_append_ (&b, f->data, f->length, &ptr))
          goto limit;
        mulib_oix_store_ (b.data + file_table + j * 8 + 4, ptr, 2);
        mulib_oix_store_ (b.data + file_table + j * 8 + 6, (uint32_t)f->length,
                          2);
      }
    if (mulib_emitter_reserve (&b, c->task_count * 16, &task_table))
      goto limit;
    for (size_t j = 0; j < c->task_count; ++j)
      {
        const mulib_oix_task_t *t = &c->tasks[j];
        size_t code;
        if (!mulib_oix_append_ (&b, t->name, strlen (t->name) + 1, &ptr))
          goto limit;
        mulib_oix_store_ (b.data + task_table + j * 16 + 10, ptr, 2);
        if (mulib_emitter_reserve (&b, t->count * 8, &code))
          goto limit;
        mulib_oix_store_ (b.data + task_table + j * 16,
                          (uint32_t)(code + MULIB_OIX_LOAD_), 2);
        mulib_oix_store_ (b.data + task_table + j * 16 + 2,
                          (uint32_t)(code + MULIB_OIX_LOAD_), 2);
        mulib_oix_store_ (b.data + task_table + j * 16 + 4,
                          (uint32_t)(code + MULIB_OIX_LOAD_ + t->count * 8),
                          2);
        for (size_t k = 0; k < t->count; ++k)
          {
            const mulib_oix_instruction_t *i = &t->code[k];
            uint16_t operand = i->value, aux = 0;
            if (i->opcode == MULIB_OIX_WRITE)
              {
                if (!mulib_oix_append_ (&b, i->data, i->length, &operand))
                  goto limit;
                aux = (uint16_t)i->length;
              }
            else if (i->opcode == MULIB_OIX_CAT || i->opcode == MULIB_OIX_SIZE
                     || i->opcode == MULIB_OIX_BYTE)
              operand = (uint16_t)mulib_oix_file_index_ (c, i->file);
            else if (i->opcode == MULIB_OIX_JUMP || i->opcode == MULIB_OIX_JNZ)
              operand = (uint16_t)(MULIB_OIX_LOAD_ + code + i->value * 8U);
            mulib_oix_store_ (b.data + code + k * 8, (uint32_t)i->opcode, 2);
            mulib_oix_store_ (b.data + code + k * 8 + 2, operand, 2);
            mulib_oix_store_ (b.data + code + k * 8 + 4, aux, 2);
          }
      }
    memcpy (bytes, "OIX\0", 4);
    mulib_oix_store_ (bytes + 4, 1, 2);
    mulib_oix_store_ (bytes + 6, 64, 2);
    mulib_oix_store_ (bytes + 8, 1, 4);
    mulib_oix_store_ (bytes + 12, 65536, 4);
    mulib_oix_store_ (bytes + 16, 0x1000, 4);
    mulib_oix_store_ (bytes + 20, 0x1000, 4);
    mulib_oix_store_ (bytes + 24, 0xfff0, 4);
    mulib_oix_store_ (bytes + 28, (uint32_t)b.size, 4);
    mulib_oix_store_ (bytes + 32, mulib_crc32 (b.data, b.size), 4);
    mulib_oix_store_ (bytes + 36, 7, 4);
    mulib_oix_store_ (bytes + 40, (uint32_t)c->file_count, 2);
    mulib_oix_store_ (bytes + 42, (uint32_t)c->task_count, 2);
    mulib_oix_store_ (bytes + 44, (uint32_t)(task_table + MULIB_OIX_LOAD_), 2);
    mulib_oix_store_ (bytes + 46, (uint32_t)(file_table + MULIB_OIX_LOAD_), 2);
    mulib_oix_store_ (bytes + 48, name, 2);
    mulib_oix_store_ (bytes + 50, banner, 2);
    mulib_oix_store_ (bytes + 52, (uint32_t)strlen (c->banner), 2);
    mulib_oix_store_ (bytes + 54, (uint32_t)kernel, 2);
    mulib_oix_store_ (bytes + 56, 1, 4);
    mulib_oix_store_ (bytes + 60, mulib_crc32 (bytes, 60), 4);
    out->data = bytes;
    out->length = 64 + b.size;
    return MULIB_OIX_OK;
  limit:
    free (bytes);
    return mulib_oix_fail_ (
        e, MULIB_OIX_LIMIT,
        "image exceeds the guest's 52 KiB kernel/data region");
  }
  /* Cursor-based canonical validation prevents aliases to kernel/stack or
   * mutable task metadata. CRC-valid but malformed images must never reach
   * KVM. */
  static int
  mulib_oix_range_ (size_t at, size_t n, size_t end)
  {
    return at <= end && n <= end - at;
  }
  static int
  mulib_oix_image_name_ (const unsigned char *p, size_t end, size_t *cursor,
                         int path)
  {
    size_t at = *cursor, n = 0, maximum = path ? 127 : 31;
    if (at > end)
      return 0;
    while (n <= maximum && n < end - at && p[at + n])
      ++n;
    if (n > maximum || !mulib_oix_range_ (at, n + 1, end)
        || !mulib_oix_name_ ((const char *)p + at, path))
      return 0;
    *cursor = at + n + 1;
    return 1;
  }
  mulib_oix_status_t
  mulib_oix_validate (const void *data, size_t n, mulib_oix_info_t *info,
                      mulib_oix_error_t *e)
  {
    const unsigned char *h = (const unsigned char *)data, *p;
    unsigned char kernel[2048];
    size_t kn, files, tasks, ft, tt, cursor, end, name;
    mulib_oix_info_t result;
    mulib_oix_error_init_ (e);
    if (!data || n < 64 || n > MULIB_OIX_CAPACITY_)
      goto invalid;
    if (memcmp (h, "OIX\0", 4) || mulib_oix_u16_ (h + 4) != 1
        || mulib_oix_u16_ (h + 6) != 64 || mulib_oix_u32_ (h + 8) != 1
        || mulib_oix_u32_ (h + 12) != 65536
        || mulib_oix_u32_ (h + 16) != 0x1000
        || mulib_oix_u32_ (h + 20) != 0x1000
        || mulib_oix_u32_ (h + 24) != 0xfff0
        || mulib_oix_u32_ (h + 28) != n - 64 || mulib_oix_u32_ (h + 36) != 7
        || mulib_oix_u32_ (h + 56) != 1
        || mulib_oix_u32_ (h + 60) != mulib_crc32 (h, 60)
        || mulib_oix_u32_ (h + 32) != mulib_crc32 (h + 64, n - 64))
      goto invalid;
    files = mulib_oix_u16_ (h + 40);
    tasks = mulib_oix_u16_ (h + 42);
    if (files > 64 || !tasks || tasks > 16)
      goto invalid;
    p = h + 64;
    end = n - 64;
    kn = mulib_oix_kernel_ (kernel, sizeof (kernel));
    if (!kn || mulib_oix_u16_ (h + 54) != kn || !mulib_oix_range_ (0, kn, end)
        || memcmp (p, kernel, kn))
      goto invalid;
    cursor = kn;
    name = cursor;
    if (mulib_oix_u16_ (h + 48) != cursor + 0x1000
        || !mulib_oix_image_name_ (p, end, &cursor, 0))
      goto invalid;
    if (mulib_oix_u16_ (h + 50) != cursor + 0x1000
        || mulib_oix_u16_ (h + 52) > 1024
        || !mulib_oix_range_ (cursor, mulib_oix_u16_ (h + 52), end))
      goto invalid;
    for (size_t j = 0; j < mulib_oix_u16_ (h + 52); ++j)
      if (!p[cursor + j])
        goto invalid;
    cursor += mulib_oix_u16_ (h + 52);
    ft = cursor;
    if (mulib_oix_u16_ (h + 46) != ft + 0x1000
        || !mulib_oix_range_ (ft, files * 8, end))
      goto invalid;
    cursor += files * 8;
    for (size_t j = 0; j < files; ++j)
      {
        const unsigned char *f = p + ft + j * 8;
        size_t start = cursor;
        if (mulib_oix_u16_ (f) != cursor + 0x1000
            || !mulib_oix_image_name_ (p, end, &cursor, 1)
            || mulib_oix_u16_ (f + 2) != cursor - start - 1
            || mulib_oix_u16_ (f + 4) != cursor + 0x1000
            || mulib_oix_u16_ (f + 6) > 32768
            || !mulib_oix_range_ (cursor, mulib_oix_u16_ (f + 6), end))
          goto invalid;
        for (size_t k = 0; k < j; ++k)
          if (!strcmp ((const char *)p + start,
                       (const char *)p + mulib_oix_u16_ (p + ft + k * 8)
                           - 0x1000))
            goto invalid;
        cursor += mulib_oix_u16_ (f + 6);
      }
    tt = cursor;
    if (mulib_oix_u16_ (h + 44) != tt + 0x1000
        || !mulib_oix_range_ (tt, tasks * 16, end))
      goto invalid;
    cursor += tasks * 16;
    for (size_t j = 0; j < tasks; ++j)
      {
        const unsigned char *t = p + tt + j * 16;
        size_t start = cursor, base, limit, count;
        if (mulib_oix_u16_ (t + 10) != cursor + 0x1000
            || !mulib_oix_image_name_ (p, end, &cursor, 0))
          goto invalid;
        for (size_t k = 0; k < j; ++k)
          if (!strcmp ((const char *)p + start,
                       (const char *)p + mulib_oix_u16_ (p + tt + k * 16 + 10)
                           - 0x1000))
            goto invalid;
        base = mulib_oix_u16_ (t + 2);
        limit = mulib_oix_u16_ (t + 4);
        if (base != cursor + 0x1000 || mulib_oix_u16_ (t) != base
            || limit <= base || (limit - base) % 8 || (limit - base) / 8 > 256
            || !mulib_oix_range_ (cursor, limit - base, end)
            || mulib_oix_u16_ (t + 6) || mulib_oix_u16_ (t + 8)
            || mulib_oix_u16_ (t + 12) || mulib_oix_u16_ (t + 14))
          goto invalid;
        count = (limit - base) / 8;
        cursor += limit - base;
        for (size_t k = 0; k < count; ++k)
          {
            const unsigned char *i = p + base - 0x1000 + k * 8;
            unsigned op = mulib_oix_u16_ (i), operand = mulib_oix_u16_ (i + 2),
                     aux = mulib_oix_u16_ (i + 4);
            if (op < 1 || op > 15 || mulib_oix_u16_ (i + 6)
                || (op != MULIB_OIX_WRITE && aux))
              goto invalid;
            if (op == MULIB_OIX_WRITE)
              {
                if (operand != cursor + 0x1000 || aux > 4096
                    || !mulib_oix_range_ (cursor, aux, end))
                  goto invalid;
                cursor += aux;
              }
            else if (op == MULIB_OIX_CAT || op == MULIB_OIX_SIZE
                     || op == MULIB_OIX_BYTE)
              {
                if (operand >= files)
                  goto invalid;
              }
            else if (op == MULIB_OIX_JUMP || op == MULIB_OIX_JNZ)
              {
                if (operand < base || operand >= limit || (operand - base) % 8)
                  goto invalid;
              }
            else if (op == MULIB_OIX_EXIT)
              {
                if (operand > 255)
                  goto invalid;
              }
            else if (op != MULIB_OIX_SET && op != MULIB_OIX_ADD && operand)
              goto invalid;
          }
      }
    if (cursor != end)
      goto invalid;
    result.name = (const char *)p + name;
    result.image_bytes = n;
    result.kernel_bytes = kn;
    result.files = files;
    result.tasks = tasks;
    result.ram_bytes = 65536;
    if (info)
      *info = result;
    return MULIB_OIX_OK;
  invalid:
    return mulib_oix_fail_ (e, MULIB_OIX_FORMAT,
                            "invalid, corrupt or noncanonical OIX image");
  }
  static int
  mulib_oix_keys_ (const mulib_json_t *o, const char *const *keys,
                   size_t count)
  {
    if (mulib_json_typeof (o) != MULIB_JSON_OBJECT)
      return 0;
    for (size_t j = 0; j < mulib_json_size (o); ++j)
      {
        size_t n, k;
        const char *s = mulib_json_key (o, j, &n);
        for (k = 0; k < count; ++k)
          if (strlen (keys[k]) == n && !memcmp (s, keys[k], n))
            break;
        if (k == count)
          return 0;
      }
    return 1;
  }
  static const char *
  mulib_oix_text_ (const mulib_json_t *v)
  {
    size_t n;
    const char *s = mulib_json_text (v, &n);
    return s && !memchr (s, 0, n) ? s : NULL;
  }
  static int
  mulib_oix_numeric_ (const mulib_json_t *v, uint16_t *out)
  {
    double n = mulib_json_numeric (v);
    if (mulib_json_typeof (v) != MULIB_JSON_NUMBER || n < 0 || n > 65535
        || floor (n) != n)
      return 0;
    *out = (uint16_t)n;
    return 1;
  }
  /* Confuse maps titled sections onto the same schema; duplicate scalar keys
   * are rejected rather than silently choosing the first or last definition.
   */
  static mulib_json_t *
  mulib_oix_confuse_scalar_ (const mulib_confuse_t *v)
  {
    if (v->kind == MULIB_CONFUSE_STRING)
      return mulib_json_string (v->as.string);
    if (v->kind == MULIB_CONFUSE_INTEGER && v->as.integer >= 0
        && v->as.integer <= 65535)
      return mulib_json_number ((double)v->as.integer);
    return NULL;
  }
  static mulib_json_t *
  mulib_oix_confuse_object_ (const mulib_confuse_t *section, int task)
  {
    mulib_json_t *o = mulib_json_new (MULIB_JSON_OBJECT), *code = NULL;
    if (!o)
      return NULL;
    if (section->title)
      {
        mulib_json_t *name = mulib_json_string (section->title);
        if (!name || !mulib_json_set (o, "name", name))
          {
            mulib_json_destroy (name);
            goto fail;
          }
      }
    for (const mulib_confuse_t *v = section->child; v; v = v->next)
      {
        mulib_json_t *value;
        if (task && v->kind == MULIB_CONFUSE_SECTION
            && !strcmp (v->name, "instruction") && !v->title)
          {
            if (!code)
              {
                if (mulib_json_get (o, "code"))
                  goto fail;
                code = mulib_json_new (MULIB_JSON_ARRAY);
                if (!code)
                  goto fail;
                if (!mulib_json_set (o, "code", code))
                  {
                    mulib_json_destroy (code);
                    goto fail;
                  }
              }
            value = mulib_oix_confuse_object_ (v, 0);
            if (!value || !mulib_json_append (code, value))
              {
                mulib_json_destroy (value);
                goto fail;
              }
          }
        else
          {
            if (mulib_json_get (o, v->name))
              goto fail;
            value = mulib_oix_confuse_scalar_ (v);
            if (!value || !mulib_json_set (o, v->name, value))
              {
                mulib_json_destroy (value);
                goto fail;
              }
          }
      }
    return o;
  fail:
    mulib_json_destroy (o);
    return NULL;
  }
  static mulib_json_t *
  mulib_oix_confuse_root_ (const mulib_confuse_t *root)
  {
    mulib_json_t *o = mulib_json_new (MULIB_JSON_OBJECT);
    if (!o)
      return NULL;
    for (const mulib_confuse_t *v = root->child; v; v = v->next)
      {
        mulib_json_t *value;
        if (v->kind == MULIB_CONFUSE_SECTION && v->title
            && (!strcmp (v->name, "file") || !strcmp (v->name, "task")))
          {
            int task = !strcmp (v->name, "task");
            const char *key = task ? "tasks" : "files";
            mulib_json_t *array = mulib_json_get (o, key);
            if (!array)
              {
                array = mulib_json_new (MULIB_JSON_ARRAY);
                if (!array)
                  goto fail;
                if (!mulib_json_set (o, key, array))
                  {
                    mulib_json_destroy (array);
                    goto fail;
                  }
              }
            if (mulib_json_typeof (array) != MULIB_JSON_ARRAY)
              goto fail;
            value = mulib_oix_confuse_object_ (v, task);
            if (!value || !mulib_json_append (array, value))
              {
                mulib_json_destroy (value);
                goto fail;
              }
          }
        else
          {
            if (mulib_json_get (o, v->name))
              goto fail;
            value = mulib_oix_confuse_scalar_ (v);
            if (!value || !mulib_json_set (o, v->name, value))
              {
                mulib_json_destroy (value);
                goto fail;
              }
          }
      }
    return o;
  fail:
    mulib_json_destroy (o);
    return NULL;
  }
  typedef struct mulib_oix_text_config_
  {
    mulib_oix_file_t files[64];
    mulib_oix_task_t tasks[16];
    mulib_oix_instruction_t code[16 * 256];
    void *decoded[64 + 16 * 256];
    size_t decoded_count;
    int allocation_failed;
  } mulib_oix_text_config_;
  static int
  mulib_oix_binary_ (mulib_oix_text_config_ *t, const mulib_json_t *object,
                     const char *plain, const void **data, size_t *length)
  {
    const mulib_json_t *a = mulib_json_get (object, plain),
                       *b = mulib_json_get (object, "data_base64");
    if (!!a == !!b)
      return 0;
    if (a)
      {
        *data = mulib_json_text (a, length);
        return *data != NULL;
      }
    {
      size_t n, written = 0;
      const char *source = mulib_json_text (b, &n);
      void *bytes;
      if (!source || n > 65536 || t->decoded_count == 64 + 16 * 256)
        return 0;
      bytes = malloc (n ? n : 1);
      if (!bytes)
        {
          t->allocation_failed = 1;
          return 0;
        }
      if (!mulib_base64_decode (source, n, (unsigned char *)bytes, n,
                                &written))
        {
          free (bytes);
          return 0;
        }
      t->decoded[t->decoded_count++] = bytes;
      *data = bytes;
      *length = written;
      return 1;
    }
  }
  static mulib_oix_status_t
  mulib_oix_document_ (const mulib_json_t *document, mulib_oix_image_t *out,
                       mulib_oix_error_t *e)
  {
    static const char *const root_keys[]
        = { "name", "banner", "files", "tasks" };
    static const char *const file_keys[] = { "name", "data", "data_base64" };
    static const char *const task_keys[] = { "name", "code" };
    static const char *const scalar_keys[] = { "op", "value" },
                             *const jump_keys[] = { "op", "target" };
    static const char *const write_keys[] = { "op", "text", "data_base64" },
                             *const file_op_keys[] = { "op", "file" };
    static const char *const bare_keys[] = { "op" };
    static const char *const opnames[]
        = { "write",  "cat",   "set",  "add",  "jump", "jnz",  "read", "putc",
            "number", "yield", "exit", "size", "byte", "list", "info" };
    mulib_oix_config_t c;
    mulib_oix_text_config_ *t = NULL;
    const mulib_json_t *value, *files, *tasks;
    mulib_oix_status_t status = MULIB_OIX_INVALID;
    size_t used = 0;
    mulib_oix_config_init (&c);
    if (!mulib_oix_keys_ (document, root_keys, 4))
      goto invalid;
    value = mulib_json_get (document, "name");
    if (value)
      {
        c.name = mulib_oix_text_ (value);
        if (!c.name)
          goto invalid;
      }
    value = mulib_json_get (document, "banner");
    if (value)
      {
        c.banner = mulib_oix_text_ (value);
        if (!c.banner)
          goto invalid;
      }
    t = (mulib_oix_text_config_ *)calloc (1, sizeof (*t));
    if (!t)
      return mulib_oix_fail_ (e, MULIB_OIX_MEMORY,
                              "configuration allocation failed");
    files = mulib_json_get (document, "files");
    tasks = mulib_json_get (document, "tasks");
    if ((files && mulib_json_typeof (files) != MULIB_JSON_ARRAY)
        || (tasks && mulib_json_typeof (tasks) != MULIB_JSON_ARRAY))
      goto invalid;
    c.file_count = mulib_json_size (files);
    c.files = t->files;
    if (c.file_count > 64 || mulib_json_size (tasks) > 16)
      {
        status = MULIB_OIX_LIMIT;
        goto invalid;
      }
    for (size_t j = 0; j < c.file_count; ++j)
      {
        const mulib_json_t *f = mulib_json_at (files, j);
        if (!mulib_oix_keys_ (f, file_keys, 3)
            || !(t->files[j].name
                 = mulib_oix_text_ (mulib_json_get (f, "name")))
            || !mulib_oix_binary_ (t, f, "data", &t->files[j].data,
                                   &t->files[j].length))
          goto invalid;
      }
    if (tasks)
      {
        c.tasks = t->tasks;
        c.task_count = mulib_json_size (tasks);
      }
    for (size_t j = 0; tasks && j < c.task_count; ++j)
      {
        const mulib_json_t *task = mulib_json_at (tasks, j),
                           *code = mulib_json_get (task, "code");
        if (e)
          {
            e->task = j;
            e->instruction = SIZE_MAX;
          }
        if (!mulib_oix_keys_ (task, task_keys, 2)
            || !(t->tasks[j].name
                 = mulib_oix_text_ (mulib_json_get (task, "name")))
            || mulib_json_typeof (code) != MULIB_JSON_ARRAY)
          goto invalid;
        t->tasks[j].count = mulib_json_size (code);
        t->tasks[j].code = t->code + used;
        if (t->tasks[j].count > 256)
          {
            status = MULIB_OIX_LIMIT;
            goto invalid;
          }
        for (size_t k = 0; k < t->tasks[j].count; ++k)
          {
            const mulib_json_t *instruction = mulib_json_at (code, k);
            const char *op
                = mulib_oix_text_ (mulib_json_get (instruction, "op"));
            unsigned index;
            mulib_oix_instruction_t *i = &t->code[used++];
            if (e)
              e->instruction = k;
            if (!op)
              goto invalid;
            for (index = 0; index < 15 && strcmp (op, opnames[index]); ++index)
              {
              }
            if (index == 15)
              goto invalid;
            i->opcode = (mulib_oix_opcode_t)(index + 1);
            if (i->opcode == MULIB_OIX_WRITE)
              {
                if (!mulib_oix_keys_ (instruction, write_keys, 3)
                    || !mulib_oix_binary_ (t, instruction, "text", &i->data,
                                           &i->length))
                  goto invalid;
              }
            else if (i->opcode == MULIB_OIX_CAT || i->opcode == MULIB_OIX_SIZE
                     || i->opcode == MULIB_OIX_BYTE)
              {
                if (!mulib_oix_keys_ (instruction, file_op_keys, 2)
                    || !(i->file = mulib_oix_text_ (
                             mulib_json_get (instruction, "file"))))
                  goto invalid;
              }
            else if (i->opcode == MULIB_OIX_JUMP || i->opcode == MULIB_OIX_JNZ)
              {
                if (!mulib_oix_keys_ (instruction, jump_keys, 2)
                    || !mulib_oix_numeric_ (
                        mulib_json_get (instruction, "target"), &i->value))
                  goto invalid;
              }
            else if (i->opcode == MULIB_OIX_SET || i->opcode == MULIB_OIX_ADD
                     || i->opcode == MULIB_OIX_EXIT)
              {
                if (!mulib_oix_keys_ (instruction, scalar_keys, 2))
                  goto invalid;
                value = mulib_json_get (instruction, "value");
                if (value && !mulib_oix_numeric_ (value, &i->value))
                  goto invalid;
              }
            else if (!mulib_oix_keys_ (instruction, bare_keys, 1))
              goto invalid;
          }
      }
    status = mulib_oix_compile (&c, out, e);
    goto done;
  invalid:
    if (t && t->allocation_failed)
      status = MULIB_OIX_MEMORY;
    (void)mulib_oix_fail_ (
        e, status,
        status == MULIB_OIX_LIMIT ? "configuration resource limit"
        : status == MULIB_OIX_MEMORY
            ? "binary configuration allocation failed"
            : "invalid OIX configuration schema or operand");
  done:
    if (t)
      {
        for (size_t j = 0; j < t->decoded_count; ++j)
          free (t->decoded[j]);
        free (t);
      }
    return status;
  }
  mulib_oix_status_t
  mulib_oix_compile_text (mulib_oix_format_t format, const char *source,
                          size_t n, mulib_oix_image_t *out,
                          mulib_oix_error_t *e)
  {
    mulib_json_t *document = NULL;
    const char *message = NULL;
    size_t offset = 0;
    mulib_oix_status_t status;
    mulib_oix_error_init_ (e);
    if ((!source && n) || !out || out->data || out->length
        || format < MULIB_OIX_JSON || format > MULIB_OIX_CONFUSE)
      return mulib_oix_fail_ (e, MULIB_OIX_INVALID,
                              "invalid configuration input or output");
    if (n > 1024U * 1024U)
      return mulib_oix_fail_ (e, MULIB_OIX_LIMIT,
                              "configuration exceeds 1 MiB");
    if (!source)
      source = "";
    if (format == MULIB_OIX_JSON)
      {
        mulib_json_error_t error = { 0, 0, 0, NULL };
        document = mulib_json_parse (source, n, &error);
        message = error.message;
        offset = error.offset;
      }
    else if (format == MULIB_OIX_YAML)
      {
        mulib_yaml_error_t error = { 0, 0, 0, NULL };
        document = mulib_yaml_parse (source, n, &error);
        message = error.message;
        offset = error.offset;
      }
    else if (format == MULIB_OIX_TOML)
      {
        mulib_toml_error_t error = { 0, 0, 0, NULL };
        document = mulib_toml_parse (source, n, &error);
        message = error.message;
        offset = error.offset;
      }
    else
      {
        mulib_parser_error_t error;
        mulib_confuse_t *conf;
        memset (&error, 0, sizeof (error));
        conf = mulib_confuse_parse (source, n, &error);
        if (conf)
          {
            document = mulib_oix_confuse_root_ (conf);
            mulib_confuse_destroy (conf);
          }
        message = error.message;
        offset = error.offset;
      }
    if (!document)
      {
        if (e)
          e->offset = offset;
        return mulib_oix_fail_ (
            e, MULIB_OIX_FORMAT,
            message ? message : "configuration parsing/conversion failed");
      }
    status = mulib_oix_document_ (document, out, e);
    mulib_json_destroy (document);
    return status;
  }
  struct mulib_oix_vm
  {
    mulib_kvm_t device;
    mulib_kvm_vm_t machine;
    mulib_kvm_vcpu_t cpu;
    void *ram;
    unsigned char input[4096];
    size_t head, count;
    mulib_oix_result_t result;
    int faulted;
  };
  void
  mulib_oix_vm_destroy (mulib_oix_vm_t *v)
  {
    if (!v)
      return;
    (void)mulib_kvm_vcpu_close (&v->cpu);
    (void)mulib_kvm_vm_close (&v->machine);
    (void)mulib_kvm_close (&v->device);
#ifdef MULIB_OIX_NATIVE_
    if (v->ram)
      (void)munmap (v->ram, 65536);
#endif
    free (v);
  }
#ifdef MULIB_OIX_NATIVE_
  static int
  mulib_oix_ioctl_ (int fd, unsigned long request, uintptr_t argument)
  {
    int rc;
    do
      {
        rc = ioctl (fd, request, argument);
      }
    while (rc < 0 && errno == EINTR);
    return rc;
  }
  static void
  mulib_oix_segment_ (struct kvm_segment *s, int code)
  {
    memset (s, 0, sizeof (*s));
    s->limit = 65535;
    s->type = (uint8_t)(code ? 11 : 3);
    s->present = 1;
    s->s = 1;
  }
#endif
  mulib_oix_vm_t *
  mulib_oix_vm_create (const void *bytes, size_t n, mulib_oix_error_t *e)
  {
    mulib_oix_error_init_ (e);
    if (mulib_oix_validate (bytes, n, NULL, e))
      return NULL;
#ifdef MULIB_OIX_NATIVE_
    {
      const unsigned char *h = (const unsigned char *)bytes;
      mulib_oix_vm_t *v = (mulib_oix_vm_t *)calloc (1, sizeof (*v));
      struct kvm_sregs sregs;
      struct kvm_regs regs;
      int rc, zero, cap = 0, system_error;
      if (!v)
        {
          mulib_oix_fail_ (e, MULIB_OIX_MEMORY, "VM allocation failed");
          return NULL;
        }
      v->device.fd = v->machine.fd = v->cpu.fd = -1;
      rc = mulib_kvm_open (&v->device, NULL);
      if (rc)
        {
          system_error = errno;
          mulib_oix_fail_ (e,
                           rc == MULIB_KVM_ENOTSUP || system_error == ENOENT
                                   || system_error == ENODEV
                               ? MULIB_OIX_UNSUPPORTED
                               : MULIB_OIX_SYSTEM,
                           "KVM is unavailable, denied or incompatible");
          if (e)
            e->system_error = system_error;
          goto fail;
        }
      if (mulib_kvm_vm_create (&v->device, &v->machine))
        goto system;
      if (mulib_kvm_check (&v->device, KVM_CAP_SET_TSS_ADDR, &cap))
        goto system;
      if (cap
          && mulib_oix_ioctl_ (v->machine.fd, KVM_SET_TSS_ADDR,
                               UINT32_C (0xfffbd000))
                 < 0)
        goto system;
      zero = open ("/dev/zero", O_RDWR);
      if (zero < 0)
        goto system;
      v->ram
          = mmap (NULL, 65536, PROT_READ | PROT_WRITE, MAP_PRIVATE, zero, 0);
      system_error = errno;
      (void)close (zero);
      errno = system_error;
      if (v->ram == MAP_FAILED)
        {
          v->ram = NULL;
          goto system;
        }
      memset (v->ram, 0, 65536);
      memcpy ((unsigned char *)v->ram + 0x1000, h + 64, n - 64);
      {
        unsigned char *g = (unsigned char *)v->ram + 0x500;
        mulib_oix_store_ (g, mulib_oix_u16_ (h + 46), 2);
        mulib_oix_store_ (g + 2, mulib_oix_u16_ (h + 44), 2);
        mulib_oix_store_ (g + 4, mulib_oix_u16_ (h + 42), 2);
        mulib_oix_store_ (g + 8, mulib_oix_u16_ (h + 42), 2);
        mulib_oix_store_ (g + 12, mulib_oix_u16_ (h + 50), 2);
        mulib_oix_store_ (g + 14, mulib_oix_u16_ (h + 52), 2);
        mulib_oix_store_ (g + 16, mulib_oix_u16_ (h + 48), 2);
        mulib_oix_store_ (g + 18,
                          (uint32_t)strlen ((const char *)h + 64
                                            + mulib_oix_u16_ (h + 48)
                                            - 0x1000),
                          2);
        mulib_oix_store_ (g + 20, mulib_oix_u16_ (h + 40), 2);
      }
      if (mulib_kvm_memory (&v->machine, 0, 0, v->ram, 65536, 0)
          || mulib_kvm_vcpu_create (&v->device, &v->machine, 0, &v->cpu))
        goto system;
      if (mulib_oix_ioctl_ (v->cpu.fd, KVM_GET_SREGS, (uintptr_t)&sregs) < 0)
        goto system;
      mulib_oix_segment_ (&sregs.cs, 1);
      mulib_oix_segment_ (&sregs.ds, 0);
      mulib_oix_segment_ (&sregs.es, 0);
      mulib_oix_segment_ (&sregs.fs, 0);
      mulib_oix_segment_ (&sregs.gs, 0);
      mulib_oix_segment_ (&sregs.ss, 0);
      sregs.cr0 = 0x10;
      sregs.cr3 = sregs.cr4 = sregs.efer = 0;
      sregs.idt.base = 0;
      sregs.idt.limit = 0x3ff;
      sregs.gdt.base = 0;
      sregs.gdt.limit = 0;
      if (mulib_oix_ioctl_ (v->cpu.fd, KVM_SET_SREGS, (uintptr_t)&sregs) < 0)
        goto system;
      memset (&regs, 0, sizeof (regs));
      regs.rip = 0x1000;
      regs.rsp = 0xfff0;
      regs.rflags = 2;
      if (mulib_oix_ioctl_ (v->cpu.fd, KVM_SET_REGS, (uintptr_t)&regs) < 0)
        goto system;
      return v;
    system:
      mulib_oix_fail_ (e, MULIB_OIX_SYSTEM, "KVM setup failed");
    fail:
      system_error = errno;
      mulib_oix_vm_destroy (v);
      errno = system_error;
      return NULL;
    }
#else
  mulib_oix_fail_ (e, MULIB_OIX_UNSUPPORTED,
                   "OIX KVM execution requires Linux x86");
  return NULL;
#endif
  }
  mulib_oix_status_t
  mulib_oix_vm_feed (mulib_oix_vm_t *v, const void *bytes, size_t n,
                     size_t *written, mulib_oix_error_t *e)
  {
    const unsigned char *p = (const unsigned char *)bytes;
    size_t accepted;
    mulib_oix_error_init_ (e);
    if (written)
      *written = 0;
    if (!v || (!bytes && n) || v->faulted || v->result.halted)
      return mulib_oix_fail_ (e, MULIB_OIX_INVALID,
                              "invalid or stopped VM input");
    accepted = n < 4096 - v->count ? n : 4096 - v->count;
    for (size_t j = 0; j < accepted; ++j)
      v->input[(v->head + v->count + j) % 4096] = p[j];
    v->count += accepted;
    if (written)
      *written = accepted;
    return accepted == n ? MULIB_OIX_OK
                         : mulib_oix_fail_ (e, MULIB_OIX_AGAIN,
                                            "console input queue full");
  }
#ifdef MULIB_OIX_NATIVE_
  static mulib_oix_status_t
  mulib_oix_io_ (mulib_oix_vm_t *v, mulib_oix_event_t *event,
                 mulib_oix_error_t *e)
  {
    struct kvm_run *run = (struct kvm_run *)v->cpu.run;
    mulib_oix_event_t next;
    unsigned char *data;
    uint64_t offset = run->io.data_offset;
    if (run->io.count != 1 || offset > v->cpu.run_size
        || run->io.size > v->cpu.run_size - (size_t)offset)
      goto invalid;
    data = (unsigned char *)run + (size_t)offset;
    next.value = 0;
    if (run->io.direction == KVM_EXIT_IO_OUT)
      {
        if (run->io.port == 0xe9 && run->io.size == 1)
          {
            next.kind = MULIB_OIX_OUTPUT;
            next.value = *data;
          }
        else if (run->io.port == 0xec && run->io.size == 1)
          {
            next.kind = MULIB_OIX_TICK;
            if (v->result.ticks != UINT64_MAX)
              ++v->result.ticks;
          }
        else if (run->io.port == 0xf4 && run->io.size == 2)
          {
            next.kind = MULIB_OIX_HALTED;
            next.value = mulib_oix_u16_ (data);
            if (next.value > 255)
              goto invalid;
            v->result.halted = 1;
            v->result.exit_code = next.value;
          }
        else
          goto invalid;
      }
    else if (run->io.direction == KVM_EXIT_IO_IN && run->io.size == 1)
      {
        next.kind = MULIB_OIX_INPUT;
        if (run->io.port == 0xea)
          *data = (unsigned char)(v->count != 0);
        else if (run->io.port == 0xeb)
          {
            *data = v->count ? v->input[v->head] : 0;
            if (v->count)
              {
                v->head = (v->head + 1) % 4096;
                --v->count;
              }
          }
        else
          goto invalid;
        next.value = *data;
      }
    else
      goto invalid;
    *event = next;
    return MULIB_OIX_OK;
  invalid:
    v->faulted = 1;
    return mulib_oix_fail_ (e, MULIB_OIX_GUEST_FAULT,
                            "unsupported or out-of-bounds guest I/O");
  }
#endif
  mulib_oix_status_t
  mulib_oix_vm_step (mulib_oix_vm_t *v, mulib_oix_event_t *event,
                     mulib_oix_error_t *e)
  {
    mulib_oix_error_init_ (e);
    if (!v || !event)
      return mulib_oix_fail_ (e, MULIB_OIX_INVALID, "invalid VM/event");
    if (v->faulted)
      return mulib_oix_fail_ (e, MULIB_OIX_GUEST_FAULT, "VM has faulted");
    if (v->result.halted)
      {
        event->kind = MULIB_OIX_HALTED;
        event->value = (uint16_t)v->result.exit_code;
        return MULIB_OIX_OK;
      }
#ifdef MULIB_OIX_NATIVE_
    {
      uint32_t reason = 0;
      int rc = mulib_kvm_run (&v->cpu, &reason);
      if (rc == MULIB_KVM_INTERRUPTED)
        return mulib_oix_fail_ (e, MULIB_OIX_INTERRUPTED,
                                "KVM execution interrupted; retry explicitly");
      if (rc)
        return mulib_oix_fail_ (e, MULIB_OIX_SYSTEM, "KVM_RUN failed");
      if (v->result.exits != UINT64_MAX)
        ++v->result.exits;
      if (reason == KVM_EXIT_IO)
        return mulib_oix_io_ (v, event, e);
      v->faulted = 1;
      return mulib_oix_fail_ (
          e, MULIB_OIX_GUEST_FAULT,
          "unexpected guest exit (HLT, exception, MMIO or shutdown)");
    }
#else
  return mulib_oix_fail_ (e, MULIB_OIX_UNSUPPORTED,
                          "OIX KVM execution requires Linux x86");
#endif
  }
  mulib_oix_status_t
  mulib_oix_vm_run (mulib_oix_vm_t *v, size_t budget,
                    mulib_oix_output_fn output, void *user,
                    mulib_oix_result_t *result, mulib_oix_error_t *e)
  {
    mulib_oix_status_t status = MULIB_OIX_OK;
    mulib_oix_error_init_ (e);
    if (!v)
      return mulib_oix_fail_ (e, MULIB_OIX_INVALID, "NULL VM");
    if (v->faulted)
      status = mulib_oix_fail_ (e, MULIB_OIX_GUEST_FAULT, "VM has faulted");
    for (size_t j = 0; !status && !v->result.halted && j < budget; ++j)
      {
        mulib_oix_event_t event;
        status = mulib_oix_vm_step (v, &event, e);
        if (!status && event.kind == MULIB_OIX_OUTPUT && output)
          {
            unsigned char byte = (unsigned char)event.value;
            if (output (user, &byte, 1) != 1)
              status = mulib_oix_fail_ (e, MULIB_OIX_HOST_ERROR,
                                        "output callback failed");
          }
      }
    if (!status && !v->result.halted)
      status = mulib_oix_fail_ (e, MULIB_OIX_LIMIT,
                                "KVM exit budget exhausted; VM is resumable");
    if (result)
      *result = v->result;
    return status;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
