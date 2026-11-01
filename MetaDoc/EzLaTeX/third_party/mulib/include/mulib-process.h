/**
 * @file mulib-process.h
 * @brief POSIX argv spawning, close-on-exec pipes, waiting and forced reaping.
 * Define MULIB_IMPL_PROCESS in one C11 translation unit. No shell is invoked.
 */
#ifndef MULIB_PROCESS_H
#define MULIB_PROCESS_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_process
  {
    long pid;
  } mulib_process_t;
  typedef struct mulib_process_io
  {
    int stdin_fd, stdout_fd, stderr_fd;
  } mulib_process_io_t;
  typedef struct mulib_process_status
  {
    int exited, exit_code, term_signal;
  } mulib_process_status_t;
  enum
  {
    MULIB_PROCESS_OK = 0,
    MULIB_PROCESS_RUNNING = 1,
    MULIB_PROCESS_INVALID = -1,
    MULIB_PROCESS_NOMEM = -2,
    MULIB_PROCESS_SYSTEM = -3,
    MULIB_PROCESS_UNSUPPORTED = -4
  };
  /** Initialize an unused handle. Never overwrite a live child handle. */
  void mulib_process_init (mulib_process_t *process);
  /** Spawn file using PATH lookup, inherited environment and exactly argc
   * arguments. argv[0] must be provided. NULL io inherits standard
   * descriptors; -1 entries inherit individually. Configured descriptors must
   * be >=3 or equal their target. Parent retains descriptor ownership. out
   * must be initialized and unused. */
  int mulib_process_spawn (mulib_process_t *out, const char *file,
                           const char *const *argv, size_t argc,
                           const mulib_process_io_t *io);
  /** Create read/write endpoints >=3 with FD_CLOEXEC. Caller closes both. */
  int mulib_process_pipe (int descriptors[2]);
  /** Close one descriptor; does not retry EINTR (avoids closing reused
   * descriptors). */
  int mulib_process_close (int descriptor);
  /** Reap child, retrying EINTR. nohang != 0 returns RUNNING if still live.
   * Successful reap clears pid and writes status; RUNNING leaves status
   * untouched. */
  int mulib_process_wait (mulib_process_t *process,
                          mulib_process_status_t *status, int nohang);
  /** Send SIGKILL and synchronously reap (not graceful termination). */
  int mulib_process_terminate (mulib_process_t *process,
                               mulib_process_status_t *status);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_PROCESS) && !defined(MULIB_PROCESS_IMPLEMENTED)
#define MULIB_PROCESS_IMPLEMENTED
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
void
mulib_process_init (mulib_process_t *p)
{
  if (p)
    p->pid = 0;
}
#if !defined(MULIB_PROCESS_NO_POSIX)                                          \
    && (defined(__unix__) || defined(__APPLE__))
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
/* Strict ISO C mode may hide kill's POSIX prototype. Its POSIX signature uses
 * only types declared above; declaring it avoids changing feature-test macros
 * in an application header after system headers may already have been read. */
extern int kill (pid_t, int);
#if defined(__linux__) && defined(O_CLOEXEC)
extern int pipe2 (int[2], int);
#endif
extern char **environ;
int
mulib_process_spawn (mulib_process_t *out, const char *file,
                     const char *const *argv, size_t argc,
                     const mulib_process_io_t *io)
{
  posix_spawn_file_actions_t actions;
  char **args;
  pid_t pid;
  size_t i;
  int rc, fds[3] = { -1, -1, -1 };
  if (!out || out->pid || !file || !*file || !argv || !argc
      || argc > SIZE_MAX / sizeof (*args) - 1)
    return MULIB_PROCESS_INVALID;
  for (i = 0; i < argc; ++i)
    if (!argv[i])
      return MULIB_PROCESS_INVALID;
  if (io)
    {
      fds[0] = io->stdin_fd;
      fds[1] = io->stdout_fd;
      fds[2] = io->stderr_fd;
    }
  for (i = 0; i < 3; ++i)
    if (fds[i] < -1 || (fds[i] >= 0 && fds[i] < 3 && fds[i] != (int)i))
      return MULIB_PROCESS_INVALID;
  args = (char **)malloc ((argc + 1) * sizeof (*args));
  if (!args)
    return MULIB_PROCESS_NOMEM;
  for (i = 0; i < argc; ++i)
    args[i] = (char *)argv[i];
  args[argc] = NULL;
  rc = posix_spawn_file_actions_init (&actions);
  if (rc)
    {
      free (args);
      errno = rc;
      return MULIB_PROCESS_SYSTEM;
    }
  for (i = 0; i < 3 && !rc; ++i)
    if (fds[i] >= 0)
      rc = posix_spawn_file_actions_adddup2 (&actions, fds[i], (int)i);
  /* All dup actions precede closes, so one source can feed multiple streams.
   */
  for (i = 0; i < 3 && !rc; ++i)
    if (fds[i] >= 3)
      {
        size_t j;
        for (j = 0; j < i && fds[j] != fds[i]; ++j)
          {
          }
        if (j == i)
          rc = posix_spawn_file_actions_addclose (&actions, fds[i]);
      }
  if (!rc)
    rc = posix_spawnp (&pid, file, &actions, NULL, args, environ);
  (void)posix_spawn_file_actions_destroy (&actions);
  free (args);
  if (rc)
    {
      errno = rc;
      return MULIB_PROCESS_SYSTEM;
    }
  out->pid = (long)pid;
  return MULIB_PROCESS_OK;
}
int
mulib_process_pipe (int descriptors[2])
{
  int fds[2], i;
  if (!descriptors)
    return MULIB_PROCESS_INVALID;
  descriptors[0] = descriptors[1] = -1;
#if defined(__linux__) && defined(O_CLOEXEC)
  if (pipe2 (fds, O_CLOEXEC))
    return MULIB_PROCESS_SYSTEM;
#else
  if (pipe (fds))
    return MULIB_PROCESS_SYSTEM;
#endif
  for (i = 0; i < 2; ++i)
    {
      if (fds[i] < 3)
        {
#if defined(F_DUPFD_CLOEXEC)
          int replacement = fcntl (fds[i], F_DUPFD_CLOEXEC, 3);
#else
          int replacement = fcntl (fds[i], F_DUPFD, 3);
#endif
          if (replacement < 0)
            goto fail;
          (void)close (fds[i]);
          fds[i] = replacement;
        }
      if (fcntl (fds[i], F_SETFD, FD_CLOEXEC) < 0)
        goto fail;
    }
  descriptors[0] = fds[0];
  descriptors[1] = fds[1];
  return MULIB_PROCESS_OK;
fail:
  {
    int saved = errno;
    (void)close (fds[0]);
    (void)close (fds[1]);
    errno = saved;
  }
  return MULIB_PROCESS_SYSTEM;
}
int
mulib_process_close (int fd)
{
  if (fd < 0)
    return MULIB_PROCESS_INVALID;
  return close (fd) ? MULIB_PROCESS_SYSTEM : MULIB_PROCESS_OK;
}
int
mulib_process_wait (mulib_process_t *p, mulib_process_status_t *status,
                    int nohang)
{
  int raw;
  pid_t rc;
  if (!p || p->pid <= 0 || !status || (long)(pid_t)p->pid != p->pid)
    return MULIB_PROCESS_INVALID;
  do
    {
      rc = waitpid ((pid_t)p->pid, &raw, nohang ? WNOHANG : 0);
    }
  while (rc < 0 && errno == EINTR);
  if (!rc)
    return MULIB_PROCESS_RUNNING;
  if (rc < 0)
    {
      /* A competing reaper violates ownership, but do not retain a stale PID
       * that a later terminate could accidentally use after PID reuse. */
      if (errno == ECHILD)
        p->pid = 0;
      return MULIB_PROCESS_SYSTEM;
    }
  status->exited = WIFEXITED (raw) ? 1 : 0;
  status->exit_code = status->exited ? WEXITSTATUS (raw) : 0;
  status->term_signal = WIFSIGNALED (raw) ? WTERMSIG (raw) : 0;
  p->pid = 0;
  return MULIB_PROCESS_OK;
}
int
mulib_process_terminate (mulib_process_t *p, mulib_process_status_t *status)
{
  int rc;
  if (!p || p->pid <= 0 || !status || (long)(pid_t)p->pid != p->pid)
    return MULIB_PROCESS_INVALID;
  /* Reap an already exited child, or invalidate an externally reaped handle,
   * before using its numeric PID as a signal target. */
  rc = mulib_process_wait (p, status, 1);
  if (rc != MULIB_PROCESS_RUNNING)
    return rc;
  if (kill ((pid_t)p->pid, SIGKILL) && errno != ESRCH)
    return MULIB_PROCESS_SYSTEM;
  return mulib_process_wait (p, status, 0);
}
#else
int
mulib_process_spawn (mulib_process_t *p, const char *f, const char *const *a,
                     size_t n, const mulib_process_io_t *io)
{
  (void)p;
  (void)f;
  (void)a;
  (void)n;
  (void)io;
  return MULIB_PROCESS_UNSUPPORTED;
}
int
mulib_process_pipe (int fds[2])
{
  if (fds)
    fds[0] = fds[1] = -1;
  return MULIB_PROCESS_UNSUPPORTED;
}
int
mulib_process_close (int fd)
{
  (void)fd;
  return MULIB_PROCESS_UNSUPPORTED;
}
int
mulib_process_wait (mulib_process_t *p, mulib_process_status_t *s, int n)
{
  (void)p;
  (void)s;
  (void)n;
  return MULIB_PROCESS_UNSUPPORTED;
}
int
mulib_process_terminate (mulib_process_t *p, mulib_process_status_t *s)
{
  (void)p;
  (void)s;
  return MULIB_PROCESS_UNSUPPORTED;
}
#endif
#endif
