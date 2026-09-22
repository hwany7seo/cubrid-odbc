#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <dlfcn.h>
#include <errno.h>
#include "odbc_test.h"

testcase_t *odbc_testcases;
int num_testcases = 0;
int case_num = 1;

/* the full log is written to a file, the console only shows what failed */
static FILE *console;
static FILE *log_reader = NULL;
static char log_file[PATHMAX];
static int verbose = 0;

static int find_dsn (char *dsn);
static int find_exec_dir (char *dir);
static int open_log_file (const char *dir);
static void report_failure (int idx, long from, long to);
static void dump_log_file (void);
static void show_usage (const char *progname);

int
main (int argc, char *argv[])
{
  int i, j, opt;
  int name_count = 0;
  char **names = NULL;
  int *name_matched = NULL;
  char *group = NULL;
  int group_matched = FALSE;
  char *log = NULL;
  char dsn[PATHMAX] = "";
  char exec_dir[PATHMAX];

  int run_count = 0;
  int unmatched_count = 0;
  int failed_count = 0;
  int failed_cases[MAX_TEST_CASES];

  console = stderr;

  while ((opt = getopt (argc, argv, "d:g:l:vh")) != -1)
    {
      switch (opt)
	{
	case 'd':
	  snprintf (dsn, sizeof (dsn), "%s", optarg);
	  break;
	case 'g':
	  group = optarg;
	  break;
	case 'l':
	  log = optarg;
	  break;
	case 'v':
	  verbose = 1;
	  break;
	case 'h':
	  show_usage (argv[0]);
	  return 0;
	default:
	  show_usage (argv[0]);
	  return 1;
	}
    }

  names = argv + optind;
  name_count = argc - optind;

  if (name_count > 0)
    {
      name_matched = (int *) calloc (name_count, sizeof (int));
      if (name_matched == NULL)
	{
	  fprintf (console, "error: out of memory\n");
	  return 1;
	}
    }

  if (find_exec_dir (exec_dir))
    {
      fprintf (console, "error: cannot find the directory of %s\n", argv[0]);
      return 1;
    }

  if (dsn[0] == '\0' && find_dsn (dsn))
    {
      return 1;
    }

  odbc_testcases = (testcase_t *) calloc (sizeof (testcase_t), MAX_TEST_CASES);
  if (odbc_testcases == NULL)
    {
      fprintf (console, "error: out of memory\n");
      return 1;
    }

  if (load_linux_odbc_testcases (exec_dir) < 0)
    {
      fprintf (console, "error: cannot load the testcases of %s\n", exec_dir);
      return 1;
    }

  snprintf (log_file, sizeof (log_file), "%s", log ? log : DEFAULT_LOG);
  if (open_log_file (log ? "." : exec_dir))
    {
      return 1;
    }

  fprintf (console, "DSN = %s\n", dsn);
  fprintf (console, "full log = %s\n", log_file);
  printf ("DSN = %s\n", dsn);

  for (i = 0; i < num_testcases; i++)
    {
      int rc;
      int matched;
      long from, to;

      if (group != NULL && strcmp (odbc_testcases[i].group, group) != 0)
	{
	  continue;
	}

      group_matched = TRUE;

      if (name_count > 0)
	{
	  for (j = 0, matched = FALSE; j < name_count; j++)
	    {
	      if (strcmp (odbc_testcases[i].name, names[j]) == 0)
		{
		  matched = TRUE;
		  name_matched[j] = TRUE;
		  break;
		}
	    }

	  if (matched == FALSE)
	    {
	      continue;
	    }
	}

      run_count++;

      if (!IS_LOADED (i))
	{
	  printf ("testcase #%d: %s is not loaded from %s\n", case_num, odbc_testcases[i].name, LINUXODBC_TESTLIB);
	  fprintf (console, "NOT LOADED #%d: %s/%s (no such symbol in %s)\n",
		   case_num, odbc_testcases[i].group, odbc_testcases[i].name, LINUXODBC_TESTLIB);
	  failed_cases[failed_count++] = i;
	  case_num++;
	  continue;
	}

      printf ("running testcase #%d: %s/%s\n", case_num, odbc_testcases[i].group, odbc_testcases[i].name);
      fflush (stdout);
      from = ftell (stdout);

      rc = (odbc_testcases[i].func) (case_num, dsn);

      fflush (stdout);
      to = ftell (stdout);

      if (rc != SQL_SUCCESS)
	{
	  printf ("testcase #%d: %s FAIL (retcode = %d)\n", case_num, odbc_testcases[i].name, rc);
	  failed_cases[failed_count++] = i;
	  report_failure (i, from, to);
	}

      case_num++;
    }

  /* a misspelled group or testcase would run nothing and look like a pass */
  for (j = 0; j < name_count; j++)
    {
      if (name_matched[j] == FALSE)
	{
	  printf ("error: no testcase named %s\n", names[j]);
	  fprintf (console, "error: no testcase named %s\n", names[j]);
	  unmatched_count++;
	}
    }

  if (group != NULL && group_matched == FALSE)
    {
      printf ("error: no testcase in the group %s\n", group);
      fprintf (console, "error: no testcase in the group %s\n", group);
    }

  if (run_count == 0)
    {
      printf ("error: no testcase was run\n");
      fprintf (console, "error: no testcase was run\n");
    }

  if (verbose)
    {
      dump_log_file ();
    }

  printf ("========== TEST SUMMARY ==========\n");
  printf ("total: %d, pass: %d, fail: %d\n", run_count, run_count - failed_count, failed_count);
  fprintf (console, "========== TEST SUMMARY ==========\n");
  fprintf (console, "total: %d, pass: %d, fail: %d\n", run_count, run_count - failed_count, failed_count);

  if (failed_count > 0)
    {
      printf ("Failed testcases:\n");
      fprintf (console, "Failed testcases:\n");

      for (j = 0; j < failed_count; j++)
	{
	  int idx = failed_cases[j];

	  printf ("  %s/%s\n", odbc_testcases[idx].group, odbc_testcases[idx].name);
	  fprintf (console, "  %s/%s\n", odbc_testcases[idx].group, odbc_testcases[idx].name);
	}
    }
  else if (run_count > 0 && unmatched_count == 0)
    {
      printf ("========== All testcases passed ==========\n");
      fprintf (console, "========== All testcases passed ==========\n");
    }

  fprintf (console, "full log = %s\n", log_file);
  fflush (stdout);

  if (log_reader != NULL)
    {
      fclose (log_reader);
    }

  free (odbc_testcases);
  free (name_matched);

  return (failed_count > 0 || unmatched_count > 0 || run_count == 0) ? 1 : 0;
}

/*
 * load_linux_odbc_testcases - read the testcase list generated by cmake and
 *                             bind every name to its function in the test library
 *   return: the number of loaded testcases, -1 on error
 *   dir(in): directory holding TESTCASE_LIST and LINUXODBC_TESTLIB
 */
int
load_linux_odbc_testcases (const char *dir)
{
  FILE *fp;
  char path[PATHMAX];
  char line[PATHMAX];
  int loaded = 0;
  void *dh;

  snprintf (path, sizeof (path), "%s/%s", dir, TESTCASE_LIST);
  fp = fopen (path, "r");
  if (fp == NULL)
    {
      fprintf (console, "error: cannot open %s (%s)\n", path, strerror (errno));
      return -1;
    }

  snprintf (path, sizeof (path), "%s/%s", dir, LINUXODBC_TESTLIB);
  dh = dlopen (path, RTLD_LAZY);
  if (dh == NULL)
    {
      fprintf (console, "error: cannot open %s (%s)\n", path, dlerror ());
      fclose (fp);
      return -1;
    }

  while (fgets (line, sizeof (line), fp) != NULL)
    {
      char *group, *name, *p;

      p = strchr (line, '\n');
      if (p)
	{
	  *p = '\0';
	}

      if (line[0] == '\0' || line[0] == '#')
	{
	  continue;
	}

      /* "<group> <testcase name>" */
      group = line;
      name = strchr (line, ' ');
      if (name == NULL)
	{
	  continue;
	}

      *name++ = '\0';

      if (strncmp (name, CASE_PREFIX, strlen (CASE_PREFIX)) != 0 || testcase_exists (name))
	{
	  continue;
	}

      if (num_testcases >= MAX_TEST_CASES)
	{
	  fprintf (console, "error: too many testcases (max = %d)\n", MAX_TEST_CASES);
	  break;
	}

      snprintf (odbc_testcases[num_testcases].group, NAMEMAX, "%s", group);
      snprintf (odbc_testcases[num_testcases].name, NAMEMAX, "%s", name);
      odbc_testcases[num_testcases++].func = dlsym (dh, name);
      loaded++;
    }

  fclose (fp);

  return loaded;
}

int
testcase_exists (char *casename)
{
  int i;

  for (i = 0; i < num_testcases; i++)
    {
      if (strcmp (odbc_testcases[i].name, casename) == 0)
	{
	  return TRUE;
	}
    }

  return FALSE;
}

/*
 * open_log_file - send everything the testcases print to the log file so that
 *                 the console is left for the failures only
 */
static int
open_log_file (const char *dir)
{
  char path[PATHMAX];

  if (log_file[0] != '/')
    {
      if (snprintf (path, sizeof (path), "%s/%s", dir, log_file) >= (int) sizeof (path))
	{
	  fprintf (console, "error: the path of the log file is too long\n");
	  return 1;
	}

      snprintf (log_file, sizeof (log_file), "%s", path);
    }

  if (freopen (log_file, "w", stdout) == NULL)
    {
      fprintf (console, "error: cannot open %s (%s)\n", log_file, strerror (errno));
      return 1;
    }

  log_reader = fopen (log_file, "r");
  if (log_reader == NULL)
    {
      fprintf (console, "error: cannot read %s (%s)\n", log_file, strerror (errno));
      return 1;
    }

  return 0;
}

/*
 * report_failure - print the failed checks of a testcase to the console
 *   idx(in): testcase index
 *   from(in), to(in): the part of the log file written by the testcase
 */
static void
report_failure (int idx, long from, long to)
{
  char line[PATHMAX];

  fprintf (console, "FAIL #%d: %s/%s\n", case_num, odbc_testcases[idx].group, odbc_testcases[idx].name);

  if (log_reader == NULL || from < 0 || to < from)
    {
      return;
    }

  if (fseek (log_reader, from, SEEK_SET) != 0)
    {
      return;
    }

  while (ftell (log_reader) < to && fgets (line, sizeof (line), log_reader) != NULL)
    {
      if (strstr (line, "NOK") != NULL)
	{
	  fprintf (console, "    %s", line);
	}
    }
}

static void
dump_log_file (void)
{
  char line[PATHMAX];

  if (log_reader == NULL)
    {
      return;
    }

  fflush (stdout);
  rewind (log_reader);

  fprintf (console, "========== FULL LOG (%s) ==========\n", log_file);
  while (fgets (line, sizeof (line), log_reader) != NULL)
    {
      fputs (line, console);
    }
}

/*
 * find_exec_dir - directory of the running odbc_test, it holds the test
 *                 library and the testcase list built together with it
 */
static int
find_exec_dir (char *dir)
{
  char path[PATHMAX];
  ssize_t len;

  len = readlink ("/proc/self/exe", path, sizeof (path) - 1);
  if (len <= 0)
    {
      return 1;
    }

  path[len] = '\0';
  snprintf (dir, PATHMAX, "%s", dirname (path));

  return 0;
}

static int
find_dsn (char *dsn)
{
  FILE *fp;
  char buf[PATHMAX];
  char *p;

  if (dsn == NULL)
    {
      return 1;
    }

  fp = fopen (DSNFILE, "r");
  if (fp == NULL || fgets (buf, PATHMAX, fp) == NULL)
    {
      strcpy (dsn, DEFAULT_DSN);
      if (fp)
	{
	  fclose (fp);
	}
      return 0;
    }

  p = strchr (buf, '\n');
  if (p)
    {
      *p = '\0';
    }

  strcpy (dsn, buf);

  fclose (fp);

  return 0;
}

static void
show_usage (const char *progname)
{
  printf ("Usage: %s [OPTIONS] [TESTCASE ...]\n", progname);
  printf (" OPTIONS\n");
  printf ("  -d DSN    DSN to test (default: %s of %s, or %s)\n", DSNFILE, "the current directory", DEFAULT_DSN);
  printf ("  -g GROUP  run the testcases of testcases/GROUP only (default: all groups)\n");
  printf ("  -l FILE   write the full log to FILE (default: %s)\n", DEFAULT_LOG);
  printf ("  -v        print the full log to the console, too\n");
  printf ("  -h        show this help message and exit\n");
  printf ("\n");
  printf (" TESTCASE\n");
  printf ("  name of the testcase to run, e.g. sql_tablesw (default: all testcases)\n");
  printf ("\n");
  printf (" EXAMPLES\n");
  printf ("  %s -d link2u                       # run every testcase with the DSN link2u\n", progname);
  printf ("  %s -g default-spec                 # run the testcases of testcases/default-spec\n", progname);
  printf ("  %s sql_tablesw sql_connectw        # run the given testcases only\n", progname);
  printf ("\n");
}
