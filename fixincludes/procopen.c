/*
 *  server.c  Set up and handle communications with a server process.
 *
 *  Server Handling copyright 1992-1999, 2004 The Free Software Foundation
 *
 *  Please review: $(src-dir)/SPL-README for Licencing info.
 */

#include "fixlib.h"
#include "server.h"

STATIC const char* def_args[] =
{ (char *) NULL, (char *) NULL };

/*
 *  chain_open
 *
 *  Given an FD for an inferior process to use as stdin,
 *  start that process and return a NEW FD that that process
 *  will use for its stdout.  Requires the argument vector
 *  for the new process and, optionally, a pointer to a place
 *  to store the child's process id.
 */
int
chain_open (int stdin_fd, tCC** pp_args, pid_t* p_child)
{
  t_fd_pair stdout_pair;
  pid_t ch_id;
  tCC *pz_cmd;

  stdout_pair.read_fd = stdout_pair.write_fd = -1;

  /*
   *  Create a pipe it will be the child process' stdout,
   *  and the parent will read from it.
   */
  if (pipe ((int *) &stdout_pair) < 0)
    {
      if (p_child != (pid_t *) NULL)
        *p_child = NOPROCESS;
      return -1;
    }

  /*
   *  If we did not get an arg list, use the default
   */
  if (pp_args == (tCC **) NULL)
    pp_args = def_args;

  /*
   *  If the arg list does not have a program,
   *  assume the "SHELL" from the environment, or, failing
   *  that, then sh.  Set argv[0] to whatever we decided on.
   */
  if (pz_cmd = *pp_args,
      (pz_cmd == (char *) NULL) || (*pz_cmd == '\0'))
    {

      pz_cmd = getenv ("SHELL");
      if (pz_cmd == (char *) NULL)
        pz_cmd = "sh";
    }

#ifdef DEBUG_PRINT
  printf ("START:  %s\n", pz_cmd);
  {
    int idx = 0;
    
    while (pp_args[++idx] != (char *) NULL)
      printf ("  ARG %2d:  %s\n", idx, pp_args[idx]);
  }
#endif

  /*
   *  Call fork() and see which process we become
   */
  ch_id = fork ();
  switch (ch_id)
    {
    case NOPROCESS:             /* parent - error in call */
      close (stdout_pair.read_fd);
      close (stdout_pair.write_fd);
      if (p_child != (pid_t *) NULL)
        *p_child = NOPROCESS;
      return -1;

    default:                    /* parent - return opposite FD's */
      if (p_child != (pid_t *) NULL)
        *p_child = ch_id;
#ifdef DEBUG_PRINT
      printf ("for pid %d:  stdin from %d, stdout to %d\n"
              "for parent:  read from %d\n",
              ch_id, stdin_fd, stdout_pair.write_fd, stdout_pair.read_fd);
#endif
      close (stdin_fd);
      close (stdout_pair.write_fd);
      return stdout_pair.read_fd;

    case NULLPROCESS:           /* child - continue processing */
      break;
    }

  /*
   *  Close the pipe end handed back to the parent process
   */
  close (stdout_pair.read_fd);

  /*
   *  Close our current stdin and stdout
   */
  close (STDIN_FILENO);
  close (STDOUT_FILENO);

  /*
   *  Make the fd passed in the stdin, and the write end of
   *  the new pipe become the stdout.
   */
  dup2 (stdout_pair.write_fd, STDOUT_FILENO);
  dup2 (stdin_fd, STDIN_FILENO);

  if (*pp_args == (char *) NULL)
    *pp_args = pz_cmd;

  execvp (pz_cmd, (char**)pp_args);
  fprintf (stderr, "Error %d:  Could not execvp( '%s', ... ):  %s\n",
           errno, pz_cmd, xstrerror (errno));
  exit (EXIT_PANIC);
}


/*
 *  proc2_open
 *
 *  Given a pointer to an argument vector, start a process and
 *  place its stdin and stdout file descriptors into an fd pair
 *  structure.  The "write_fd" connects to the inferior process
 *  stdin, and the "read_fd" connects to its stdout.  The calling
 *  process should write to "write_fd" and read from "read_fd".
 *  The return value is the process id of the created process.
 */
pid_t
proc2_open (t_fd_pair* p_pair, tCC** pp_args)
{
  pid_t ch_id;

  /*  Create a bi-directional pipe.  Writes on 0 arrive on 1 and vice
     versa, so the parent and child processes will read and write to
     opposite FD's.  */
  if (pipe ((int *) p_pair) < 0)
    return NOPROCESS;

  p_pair->read_fd = chain_open (p_pair->read_fd, pp_args, &ch_id);
  if (ch_id == NOPROCESS)
    close (p_pair->write_fd);

  return ch_id;
}


/*
 *  proc2_fopen
 *
 *  Identical to "proc2_open()", except that the "fd"'s are
 *  "fdopen(3)"-ed into file pointers instead.
 */
pid_t
proc2_fopen (t_pf_pair* pf_pair, tCC** pp_args)
{
  t_fd_pair fd_pair;
  pid_t ch_id = proc2_open (&fd_pair, pp_args);

  if (ch_id == NOPROCESS)
    return ch_id;

  pf_pair->pf_read = fdopen (fd_pair.read_fd, "r");
  pf_pair->pf_write = fdopen (fd_pair.write_fd, "w");
  return ch_id;
}