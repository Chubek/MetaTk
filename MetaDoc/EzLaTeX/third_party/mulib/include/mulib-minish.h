/**
 * @file mulib-minish.h
 * @brief A bounded command interpreter with quoting, variables, and callbacks.
 *
 * This is not a POSIX shell. No pipes, redirects, substitution, globbing,
 * assignments, or control-flow grammar are implemented. External execution is
 * disabled by default, both at build time and per interpreter. Define
 * MULIB_IMPL_MINISH, MULIB_IMPL_MAP, MULIB_IMPL_VEC, and MULIB_IMPL_STRBUF in
 * one translation unit. MULIB_MINISH_ENABLE_POSIX enables optional fork/execv
 * code.
 */
#ifndef MULIB_MINISH_H
#define MULIB_MINISH_H
#include "mulib-map.h"
#include "mulib-strbuf.h"
#include "mulib-vec.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_minish mulib_minish_t;
  /** @brief Return a command status in [0,255]; argv is borrowed for the call.
   */
  typedef int (*mulib_minish_command_fn) (mulib_minish_t *shell, size_t argc,
                                          const char *const *argv, void *user);
  /** @brief Write exactly length bytes; return 1 on success, 0 on failure. */
  typedef int (*mulib_minish_write_fn) (void *user, const char *text,
                                        size_t length);
  /** @brief Per-execution and persistent-state limits; zero is an actual zero.
   */
  typedef struct mulib_minish_limits
  {
    size_t input_bytes, word_bytes, words, commands, variables, registered;
  } mulib_minish_limits_t;
  /** @brief Interpreter errors, separate from a dispatched command's exit
   * status. */
  typedef enum mulib_minish_status
  {
    MULIB_MINISH_OK,
    MULIB_MINISH_INVALID_ARGUMENT,
    MULIB_MINISH_NO_MEMORY,
    MULIB_MINISH_SYNTAX,
    MULIB_MINISH_LIMIT,
    MULIB_MINISH_INVALID_NAME,
    MULIB_MINISH_NOT_FOUND,
    MULIB_MINISH_CALLBACK_ERROR,
    MULIB_MINISH_UNSUPPORTED,
    MULIB_MINISH_IO_ERROR,
    MULIB_MINISH_PROCESS_ERROR
  } mulib_minish_status_t;
  /** @brief Byte offset and zero-based command index; system_error is OS
   * errno. */
  typedef struct mulib_minish_error
  {
    mulib_minish_status_t status;
    size_t offset, command;
    int system_error;
  } mulib_minish_error_t;
  /** @brief Owned command-table entry; do not modify interpreter internals. */
  typedef struct mulib_minish_command
  {
    char *name;
    mulib_minish_command_fn function;
    void *user;
  } mulib_minish_command_t;
  /** @brief Owning interpreter; initialize once, free once, and do not copy.
   */
  struct mulib_minish
  {
    mulib_map_t variables;
    mulib_vec_t registered;
    mulib_minish_limits_t limits;
    mulib_minish_write_fn writer;
    void *writer_user;
    int last_status, exit_requested, exit_status;
    int allow_process, running;
  };

#define MULIB_MINISH_MAX_NAME 128u
  /** @brief Default limits: 64KiB input, 16KiB word, 256 words/variables, 128
   * commands/registrations. */
  mulib_minish_limits_t mulib_minish_default_limits (void);
  /** @brief Initialize empty state with stdout output; no allocation is
   * performed. */
  void mulib_minish_init (mulib_minish_t *shell);
  /** @brief Release owned variables/command names; safe on an initialized
   * empty shell. */
  void mulib_minish_free (mulib_minish_t *shell);
  /** @brief Change limits while idle; shrinking below persistent state is
   * rejected. */
  mulib_minish_status_t
  mulib_minish_set_limits (mulib_minish_t *shell,
                           const mulib_minish_limits_t *limits);
  /** @brief Set a borrowed output callback/user; NULL restores stdout. */
  void mulib_minish_set_writer (mulib_minish_t *shell,
                                mulib_minish_write_fn writer, void *user);
  /** @brief Enable exact-path execv while idle; enabling without POSIX support
   * is unsupported. */
  mulib_minish_status_t mulib_minish_allow_process (mulib_minish_t *shell,
                                                    int enabled);
  /** @brief Copy a named variable's value; valid names are ASCII
   * [A-Za-z_][A-Za-z0-9_]*. */
  mulib_minish_status_t mulib_minish_set (mulib_minish_t *shell,
                                          const char *name, const char *value);
  /** @brief Return a borrowed value, or NULL if absent/invalid; no environment
   * fallback. */
  const char *mulib_minish_get (const mulib_minish_t *shell, const char *name);
  /** @brief Remove a variable, succeeding also if it is absent. */
  mulib_minish_status_t mulib_minish_unset (mulib_minish_t *shell,
                                            const char *name);
  /** @brief Register/replace a callback; command names also allow '-' after
   * the first byte. */
  mulib_minish_status_t
  mulib_minish_register (mulib_minish_t *shell, const char *name,
                         mulib_minish_command_fn function, void *user);
  /** @brief Remove a registered command; returns 1 if removed, 0 if
   * absent/invalid. */
  int mulib_minish_unregister (mulib_minish_t *shell, const char *name);
  /**
   * @brief Parse and execute commands sequentially; retain variables and last
   * status.
   * @details Supports ;/newline separators, comments at word start,
   * concatenated single/double/unquoted segments, backslash escapes, $NAME,
   * ${NAME}, and $?. Expansion never splits words or evaluates syntax;
   * undefined variables yield an empty string. Builtins: echo, set NAME VALUE,
   * unset NAME, true, false, exit [0..255]. Usage errors have command
   * status 2. Engine errors stop execution; earlier commands' side effects
   * remain. Recursion on the same shell is rejected. Exit stops execution and
   * future execute calls until exit_requested is cleared.
   */
  mulib_minish_status_t mulib_minish_execute (mulib_minish_t *shell,
                                              const char *source,
                                              size_t length,
                                              mulib_minish_error_t *error);
  /** @brief Return a static status description. */
  const char *mulib_minish_status_string (mulib_minish_status_t status);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_MINISH_H */

#if defined(MULIB_IMPL_MINISH) && !defined(MULIB_MINISH_IMPLEMENTED)
#define MULIB_MINISH_IMPLEMENTED
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef MULIB_MINISH_ENABLE_POSIX
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
#ifdef __cplusplus
extern "C"
{
#endif
  static int
  mulib_minish__letter (unsigned char c)
  {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
  }
  static int
  mulib_minish__name (const char *name, int command)
  {
    size_t i;
    if (!name || !mulib_minish__letter ((unsigned char)name[0]))
      return 0;
    for (i = 1; i <= MULIB_MINISH_MAX_NAME; ++i)
      {
        unsigned char c = (unsigned char)name[i];
        if (!c)
          return 1;
        if (!mulib_minish__letter (c) && !(c >= '0' && c <= '9')
            && !(command && c == '-'))
          return 0;
      }
    return 0;
  }
  static int
  mulib_minish__builtin (const char *name)
  {
    return !strcmp (name, "echo") || !strcmp (name, "set")
           || !strcmp (name, "unset") || !strcmp (name, "true")
           || !strcmp (name, "false") || !strcmp (name, "exit");
  }
  static int
  mulib_minish__stdout (void *user, const char *text, size_t length)
  {
    (void)user;
    return fwrite (text, 1, length, stdout) == length;
  }
  static mulib_minish_status_t
  mulib_minish__error (mulib_minish_error_t *e, mulib_minish_status_t status,
                       size_t offset)
  {
    e->status = status;
    e->offset = offset;
    return status;
  }
  mulib_minish_limits_t
  mulib_minish_default_limits (void)
  {
    mulib_minish_limits_t limits = { 65536, 16384, 256, 128, 256, 128 };
    return limits;
  }
  void
  mulib_minish_init (mulib_minish_t *s)
  {
    if (!s)
      return;
    memset (s, 0, sizeof (*s));
    mulib_map_init (&s->variables);
    mulib_vec_init (&s->registered, sizeof (mulib_minish_command_t));
    s->limits = mulib_minish_default_limits ();
    s->writer = mulib_minish__stdout;
  }
  void
  mulib_minish_free (mulib_minish_t *s)
  {
    size_t i;
    if (!s)
      return;
    for (i = 0; i < s->variables.length; ++i)
      free (s->variables.items[i].value);
    mulib_map_free (&s->variables);
    for (i = 0; i < s->registered.length; ++i)
      free (((mulib_minish_command_t *)s->registered.data)[i].name);
    mulib_vec_free (&s->registered);
    mulib_minish_init (s);
  }
  mulib_minish_status_t
  mulib_minish_set_limits (mulib_minish_t *s,
                           const mulib_minish_limits_t *limits)
  {
    size_t i;
    if (!s || !limits || s->running)
      return MULIB_MINISH_INVALID_ARGUMENT;
    if (limits->word_bytes == SIZE_MAX
        || limits->words >= SIZE_MAX / sizeof (char *)
        || limits->registered > SIZE_MAX / sizeof (mulib_minish_command_t)
        || limits->variables > SIZE_MAX / sizeof (mulib_map_entry_t))
      return MULIB_MINISH_LIMIT;
    if (limits->variables < s->variables.length
        || limits->registered < s->registered.length)
      return MULIB_MINISH_LIMIT;
    for (i = 0; i < s->variables.length; ++i)
      if (strlen ((const char *)s->variables.items[i].value)
          > limits->word_bytes)
        return MULIB_MINISH_LIMIT;
    s->limits = *limits;
    return MULIB_MINISH_OK;
  }
  void
  mulib_minish_set_writer (mulib_minish_t *s, mulib_minish_write_fn writer,
                           void *user)
  {
    if (s)
      {
        s->writer = writer ? writer : mulib_minish__stdout;
        s->writer_user = user;
      }
  }
  mulib_minish_status_t
  mulib_minish_allow_process (mulib_minish_t *s, int enabled)
  {
    if (!s || s->running)
      return MULIB_MINISH_INVALID_ARGUMENT;
#ifndef MULIB_MINISH_ENABLE_POSIX
    if (enabled)
      return MULIB_MINISH_UNSUPPORTED;
#endif
    s->allow_process = enabled != 0;
    return MULIB_MINISH_OK;
  }
  mulib_minish_status_t
  mulib_minish_set (mulib_minish_t *s, const char *name, const char *value)
  {
    size_t length;
    void *old = NULL;
    char *copy;
    int exists;
    if (!s || !value)
      return MULIB_MINISH_INVALID_ARGUMENT;
    if (!mulib_minish__name (name, 0))
      return MULIB_MINISH_INVALID_NAME;
    exists = mulib_map_get (&s->variables, name, &old);
    if (!exists && s->variables.length >= s->limits.variables)
      return MULIB_MINISH_LIMIT;
    for (length = 0; length <= s->limits.word_bytes && value[length]; ++length)
      {
      }
    if (length > s->limits.word_bytes)
      return MULIB_MINISH_LIMIT;
    copy = (char *)malloc (length + 1);
    if (!copy)
      return MULIB_MINISH_NO_MEMORY;
    memcpy (copy, value, length + 1);
    if (!mulib_map_set (&s->variables, name, copy))
      {
        free (copy);
        return MULIB_MINISH_NO_MEMORY;
      }
    free (old);
    return MULIB_MINISH_OK;
  }
  const char *
  mulib_minish_get (const mulib_minish_t *s, const char *name)
  {
    void *value = NULL;
    if (!s || !mulib_minish__name (name, 0))
      return NULL;
    return mulib_map_get (&s->variables, name, &value) ? (const char *)value
                                                       : NULL;
  }
  mulib_minish_status_t
  mulib_minish_unset (mulib_minish_t *s, const char *name)
  {
    void *old = NULL;
    if (!s)
      return MULIB_MINISH_INVALID_ARGUMENT;
    if (!mulib_minish__name (name, 0))
      return MULIB_MINISH_INVALID_NAME;
    if (mulib_map_get (&s->variables, name, &old))
      {
        mulib_map_remove (&s->variables, name);
        free (old);
      }
    return MULIB_MINISH_OK;
  }
  mulib_minish_status_t
  mulib_minish_register (mulib_minish_t *s, const char *name,
                         mulib_minish_command_fn function, void *user)
  {
    size_t i, n;
    mulib_minish_command_t command;
    if (!s || !function)
      return MULIB_MINISH_INVALID_ARGUMENT;
    if (!mulib_minish__name (name, 1) || mulib_minish__builtin (name))
      return MULIB_MINISH_INVALID_NAME;
    for (i = 0; i < s->registered.length; ++i)
      {
        mulib_minish_command_t *entry
            = (mulib_minish_command_t *)mulib_vec_at (&s->registered, i);
        if (!strcmp (entry->name, name))
          {
            entry->function = function;
            entry->user = user;
            return MULIB_MINISH_OK;
          }
      }
    if (s->registered.length >= s->limits.registered)
      return MULIB_MINISH_LIMIT;
    n = strlen (name);
    command.name = (char *)malloc (n + 1);
    if (!command.name)
      return MULIB_MINISH_NO_MEMORY;
    memcpy (command.name, name, n + 1);
    command.function = function;
    command.user = user;
    if (!mulib_vec_push (&s->registered, &command))
      {
        free (command.name);
        return MULIB_MINISH_NO_MEMORY;
      }
    return MULIB_MINISH_OK;
  }
  int
  mulib_minish_unregister (mulib_minish_t *s, const char *name)
  {
    size_t i;
    if (!s || !name)
      return 0;
    for (i = 0; i < s->registered.length; ++i)
      {
        mulib_minish_command_t *entries
            = (mulib_minish_command_t *)s->registered.data;
        if (!strcmp (entries[i].name, name))
          {
            free (entries[i].name);
            memmove (entries + i, entries + i + 1,
                     (s->registered.length - i - 1) * sizeof (*entries));
            --s->registered.length;
            return 1;
          }
      }
    return 0;
  }
  static mulib_minish_status_t
  mulib_minish__append (mulib_minish_t *s, mulib_strbuf_t *b, const char *text,
                        size_t length, mulib_minish_error_t *e, size_t offset)
  {
    if (length > s->limits.word_bytes - b->length)
      return mulib_minish__error (e, MULIB_MINISH_LIMIT, offset);
    if (!mulib_strbuf_append_n (b, text, length))
      return mulib_minish__error (e, MULIB_MINISH_NO_MEMORY, offset);
    return MULIB_MINISH_OK;
  }
  static mulib_minish_status_t
  mulib_minish__expand (mulib_minish_t *s, const char *text, size_t length,
                        size_t *cursor, mulib_strbuf_t *word,
                        mulib_minish_error_t *e)
  {
    size_t start = *cursor, i = start + 1, begin, n;
    char name[MULIB_MINISH_MAX_NAME + 1], number[16];
    const char *value;
    if (i == length)
      {
        *cursor = i;
        return mulib_minish__append (s, word, "$", 1, e, start);
      }
    if (text[i] == '?')
      {
        int written = snprintf (number, sizeof (number), "%d", s->last_status);
        *cursor = i + 1;
        return mulib_minish__append (s, word, number, (size_t)written, e,
                                     start);
      }
    if (text[i] == '{')
      {
        begin = ++i;
        while (i < length && text[i] != '}')
          ++i;
        if (i == length)
          return mulib_minish__error (e, MULIB_MINISH_SYNTAX, start);
        n = i - begin;
        if (!n)
          return mulib_minish__error (e, MULIB_MINISH_SYNTAX, start);
        if (n > MULIB_MINISH_MAX_NAME)
          return mulib_minish__error (e, MULIB_MINISH_LIMIT, start);
        memcpy (name, text + begin, n);
        name[n] = '\0';
        if (!mulib_minish__name (name, 0))
          return mulib_minish__error (e, MULIB_MINISH_UNSUPPORTED, start);
        *cursor = i + 1;
      }
    else if (mulib_minish__letter ((unsigned char)text[i]))
      {
        begin = i++;
        while (i < length
               && (mulib_minish__letter ((unsigned char)text[i])
                   || (text[i] >= '0' && text[i] <= '9')))
          ++i;
        n = i - begin;
        if (n > MULIB_MINISH_MAX_NAME)
          return mulib_minish__error (e, MULIB_MINISH_LIMIT, start);
        memcpy (name, text + begin, n);
        name[n] = '\0';
        *cursor = i;
      }
    else
      {
        if (text[i] == '(' || text[i] == '$' || text[i] == '@'
            || text[i] == '*' || text[i] == '#' || text[i] == '!'
            || text[i] == '-' || (text[i] >= '0' && text[i] <= '9'))
          return mulib_minish__error (e, MULIB_MINISH_UNSUPPORTED, start);
        *cursor = i;
        return mulib_minish__append (s, word, "$", 1, e, start);
      }
    value = mulib_minish_get (s, name);
    return mulib_minish__append (s, word, value ? value : "",
                                 value ? strlen (value) : 0, e, start);
  }
  static int
  mulib_minish__space (char c)
  {
    return c == ' ' || c == '\t' || c == '\r' || c == '\v' || c == '\f';
  }
  static void
  mulib_minish__words_free (mulib_vec_t *words)
  {
    size_t i;
    for (i = 0; i < words->length; ++i)
      free (((char **)words->data)[i]);
    mulib_vec_free (words);
  }
  static mulib_minish_status_t
  mulib_minish__parse (mulib_minish_t *s, const char *text, size_t length,
                       size_t *cursor, size_t *command_start,
                       mulib_vec_t *words, mulib_minish_error_t *e)
  {
    size_t i = *cursor;
    while (i < length)
      {
        mulib_strbuf_t word;
        char quote = 0;
        int active = 0;
        size_t quote_start = i;
        while (i < length && mulib_minish__space (text[i]))
          ++i;
        if (i == length)
          break;
        if (text[i] == '#')
          {
            while (i < length && text[i] != '\n')
              ++i;
            if (i < length)
              ++i;
            break;
          }
        if (text[i] == ';' || text[i] == '\n')
          {
            ++i;
            break;
          }
        if (!words->length)
          *command_start = i;
        mulib_strbuf_init (&word);
        while (i < length)
          {
            char c = text[i];
            mulib_minish_status_t status;
            if (quote == '\'')
              {
                if (c == '\'')
                  {
                    quote = 0;
                    ++i;
                    continue;
                  }
              }
            else
              {
                if (quote == '"' && c == '"')
                  {
                    quote = 0;
                    ++i;
                    continue;
                  }
                if (!quote)
                  {
                    if (mulib_minish__space (c) || c == '\n' || c == ';')
                      break;
                    if (!active && c == '#')
                      {
                        while (i < length && text[i] != '\n')
                          ++i;
                        break;
                      }
                    if (c == '\'' || c == '"')
                      {
                        active = 1;
                        quote = c;
                        quote_start = i++;
                        continue;
                      }
                    if (c == '|' || c == '&' || c == '<' || c == '>'
                        || c == '(' || c == ')')
                      {
                        mulib_strbuf_free (&word);
                        return mulib_minish__error (
                            e, MULIB_MINISH_UNSUPPORTED, i);
                      }
                  }
                if (c == '`')
                  {
                    mulib_strbuf_free (&word);
                    return mulib_minish__error (e, MULIB_MINISH_UNSUPPORTED,
                                                i);
                  }
                if (c == '\\')
                  {
                    if (i + 1 == length)
                      {
                        mulib_strbuf_free (&word);
                        return mulib_minish__error (e, MULIB_MINISH_SYNTAX, i);
                      }
                    ++i;
                    if (text[i] == '\n')
                      {
                        ++i;
                        continue;
                      }
                    c = text[i];
                  }
                else if (c == '$')
                  {
                    active = 1;
                    status
                        = mulib_minish__expand (s, text, length, &i, &word, e);
                    if (status != MULIB_MINISH_OK)
                      {
                        mulib_strbuf_free (&word);
                        return status;
                      }
                    continue;
                  }
              }
            active = 1;
            status = mulib_minish__append (s, &word, &c, 1, e, i);
            if (status != MULIB_MINISH_OK)
              {
                mulib_strbuf_free (&word);
                return status;
              }
            ++i;
          }
        if (quote)
          {
            mulib_strbuf_free (&word);
            return mulib_minish__error (e, MULIB_MINISH_SYNTAX, quote_start);
          }
        if (!active)
          {
            mulib_strbuf_free (&word);
            continue;
          }
        if (words->length >= s->limits.words)
          {
            mulib_strbuf_free (&word);
            return mulib_minish__error (e, MULIB_MINISH_LIMIT, i);
          }
        if (!word.data && !mulib_strbuf_append (&word, ""))
          {
            mulib_strbuf_free (&word);
            return mulib_minish__error (e, MULIB_MINISH_NO_MEMORY, i);
          }
        if (!mulib_vec_push (words, &word.data))
          {
            mulib_strbuf_free (&word);
            return mulib_minish__error (e, MULIB_MINISH_NO_MEMORY, i);
          }
      }
    *cursor = i;
    return MULIB_MINISH_OK;
  }

#ifdef MULIB_MINISH_ENABLE_POSIX
  static mulib_minish_status_t
  mulib_minish__process (char *const *argv, int *command_status,
                         mulib_minish_error_t *e)
  {
    int pipes[2], saved, wait_status, child_error = 0;
    size_t received = 0;
    pid_t pid, waited;
    if (pipe (pipes) < 0)
      {
        e->system_error = errno;
        return MULIB_MINISH_PROCESS_ERROR;
      }
    if (fcntl (pipes[1], F_SETFD, FD_CLOEXEC) < 0)
      {
        saved = errno;
        close (pipes[0]);
        close (pipes[1]);
        e->system_error = saved;
        return MULIB_MINISH_PROCESS_ERROR;
      }
    pid = fork ();
    if (pid < 0)
      {
        saved = errno;
        close (pipes[0]);
        close (pipes[1]);
        e->system_error = saved;
        return MULIB_MINISH_PROCESS_ERROR;
      }
    if (!pid)
      {
        const char *bytes;
        size_t sent = 0;
        close (pipes[0]);
        execv (argv[0], argv);
        saved = errno;
        bytes = (const char *)&saved;
        while (sent < sizeof (saved))
          {
            ssize_t written
                = write (pipes[1], bytes + sent, sizeof (saved) - sent);
            if (written > 0)
              sent += (size_t)written;
            else if (written < 0 && errno == EINTR)
              continue;
            else
              break;
          }
        _exit (saved == ENOENT ? 127 : 126);
      }
    close (pipes[1]);
    saved = 0;
    while (received < sizeof (child_error))
      {
        ssize_t got = read (pipes[0], (char *)&child_error + received,
                            sizeof (child_error) - received);
        if (got > 0)
          received += (size_t)got;
        else if (got < 0 && errno == EINTR)
          continue;
        else
          {
            if (got < 0)
              saved = errno;
            break;
          }
      }
    close (pipes[0]);
    do
      {
        waited = waitpid (pid, &wait_status, 0);
      }
    while (waited < 0 && errno == EINTR);
    if (waited < 0)
      saved = errno;
    if (received && received != sizeof (child_error))
      saved = EIO;
    if (saved || received)
      {
        e->system_error = saved ? saved : child_error;
        return MULIB_MINISH_PROCESS_ERROR;
      }
    *command_status
        = WIFEXITED (wait_status)
              ? WEXITSTATUS (wait_status)
              : (WIFSIGNALED (wait_status) ? 128 + WTERMSIG (wait_status)
                                           : 125);
    if (*command_status > 255)
      *command_status = 255;
    return MULIB_MINISH_OK;
  }
#endif

  static mulib_minish_status_t
  mulib_minish__dispatch (mulib_minish_t *s, mulib_vec_t *words,
                          int *command_status, mulib_minish_error_t *e)
  {
    char **argv = (char **)words->data;
    size_t argc = words->length, i;
    *command_status = 0;
    if (!strcmp (argv[0], "echo"))
      {
        for (i = 1; i < argc; ++i)
          {
            if ((i > 1 && !s->writer (s->writer_user, " ", 1))
                || !s->writer (s->writer_user, argv[i], strlen (argv[i])))
              return MULIB_MINISH_IO_ERROR;
          }
        if (!s->writer (s->writer_user, "\n", 1))
          return MULIB_MINISH_IO_ERROR;
        return MULIB_MINISH_OK;
      }
    if (!strcmp (argv[0], "set") || !strcmp (argv[0], "unset"))
      {
        mulib_minish_status_t status;
        int set = !strcmp (argv[0], "set");
        if (argc != (size_t)(set ? 3 : 2))
          {
            *command_status = 2;
            return MULIB_MINISH_OK;
          }
        status = set ? mulib_minish_set (s, argv[1], argv[2])
                     : mulib_minish_unset (s, argv[1]);
        if (status == MULIB_MINISH_INVALID_NAME)
          {
            *command_status = 2;
            return MULIB_MINISH_OK;
          }
        return status;
      }
    if (!strcmp (argv[0], "true") || !strcmp (argv[0], "false"))
      {
        *command_status = argc == 1 ? !strcmp (argv[0], "false") : 2;
        return MULIB_MINISH_OK;
      }
    if (!strcmp (argv[0], "exit"))
      {
        unsigned value = (unsigned)s->last_status;
        if (argc > 2 || (argc == 2 && !argv[1][0]))
          {
            *command_status = 2;
            return MULIB_MINISH_OK;
          }
        if (argc == 2)
          {
            const unsigned char *p = (const unsigned char *)argv[1];
            value = 0;
            while (*p)
              {
                if (*p < '0' || *p > '9' || value > 25
                    || value * 10 + (*p - '0') > 255)
                  {
                    *command_status = 2;
                    return MULIB_MINISH_OK;
                  }
                value = value * 10 + (*p++ - '0');
              }
          }
        s->exit_requested = 1;
        s->exit_status = (int)value;
        *command_status = (int)value;
        return MULIB_MINISH_OK;
      }
    for (i = 0; i < s->registered.length; ++i)
      {
        mulib_minish_command_t command
            = ((mulib_minish_command_t *)s->registered.data)[i];
        if (!strcmp (command.name, argv[0]))
          {
            *command_status = command.function (
                s, argc, (const char *const *)argv, command.user);
            return *command_status >= 0 && *command_status <= 255
                       ? MULIB_MINISH_OK
                       : MULIB_MINISH_CALLBACK_ERROR;
          }
      }
#ifdef MULIB_MINISH_ENABLE_POSIX
    if (s->allow_process)
      {
        if (!mulib_vec_reserve (words, argc + 1))
          return MULIB_MINISH_NO_MEMORY;
        argv = (char **)words->data;
        argv[argc] = NULL;
        return mulib_minish__process (argv, command_status, e);
      }
#else
  (void)e;
#endif
    return MULIB_MINISH_NOT_FOUND;
  }

  mulib_minish_status_t
  mulib_minish_execute (mulib_minish_t *s, const char *text, size_t length,
                        mulib_minish_error_t *error)
  {
    mulib_minish_error_t local;
    mulib_minish_error_t *e = error ? error : &local;
    size_t cursor = 0, commands = 0, i;
    e->status = MULIB_MINISH_OK;
    e->offset = 0;
    e->command = 0;
    e->system_error = 0;
    if (!s || (!text && length) || s->running)
      return mulib_minish__error (e, MULIB_MINISH_INVALID_ARGUMENT, 0);
    if (length > s->limits.input_bytes)
      return mulib_minish__error (e, MULIB_MINISH_LIMIT, 0);
    for (i = 0; i < length; ++i)
      if (!text[i])
        return mulib_minish__error (e, MULIB_MINISH_SYNTAX, i);
    s->running = 1;
    while (cursor < length && !s->exit_requested)
      {
        mulib_vec_t words;
        mulib_minish_status_t status;
        size_t start = cursor;
        int command_status;
        mulib_vec_init (&words, sizeof (char *));
        status = mulib_minish__parse (s, text, length, &cursor, &start, &words,
                                      e);
        if (status == MULIB_MINISH_OK && words.length)
          {
            if (commands >= s->limits.commands)
              status = MULIB_MINISH_LIMIT;
            else
              status = mulib_minish__dispatch (s, &words, &command_status, e);
            if (status == MULIB_MINISH_OK)
              {
                s->last_status = command_status;
                ++commands;
              }
            else
              mulib_minish__error (e, status, start);
          }
        mulib_minish__words_free (&words);
        if (status != MULIB_MINISH_OK)
          {
            e->command = commands;
            s->running = 0;
            return status;
          }
      }
    s->running = 0;
    e->offset = length;
    e->command = commands;
    return MULIB_MINISH_OK;
  }

  const char *
  mulib_minish_status_string (mulib_minish_status_t s)
  {
    switch (s)
      {
      case MULIB_MINISH_OK:
        return "success";
      case MULIB_MINISH_INVALID_ARGUMENT:
        return "invalid argument or recursive execution";
      case MULIB_MINISH_NO_MEMORY:
        return "allocation failed";
      case MULIB_MINISH_SYNTAX:
        return "malformed command or embedded NUL";
      case MULIB_MINISH_LIMIT:
        return "interpreter limit exceeded";
      case MULIB_MINISH_INVALID_NAME:
        return "invalid or reserved name";
      case MULIB_MINISH_NOT_FOUND:
        return "command not found; process execution disabled";
      case MULIB_MINISH_CALLBACK_ERROR:
        return "callback status outside 0..255";
      case MULIB_MINISH_UNSUPPORTED:
        return "unsupported shell grammar or platform option";
      case MULIB_MINISH_IO_ERROR:
        return "output callback failed";
      case MULIB_MINISH_PROCESS_ERROR:
        return "process launch or wait failed";
      default:
        return "unknown shell status";
      }
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
