/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2011  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/arc/arc.h>
#include <grub/arc/console.h>
#include <grub/term.h>
#include <grub/terminfo.h>

/* FIXME: use unicode.  */

static int
readkey (struct grub_term_input *term __attribute__ ((unused)))
{
  unsigned long count;
  char chr;

  if (GRUB_ARC_FIRMWARE_VECTOR->get_read_status (GRUB_ARC_STDIN))
    return -1;

  if (GRUB_ARC_FIRMWARE_VECTOR->read (GRUB_ARC_STDIN, &chr, 1, &count))
    return -1;
  if (!count)
    return -1;
  return chr;
}

static void
put (struct grub_term_output *term __attribute__ ((unused)), const int c)
{
  unsigned long count;
  char chr = c;

  GRUB_ARC_FIRMWARE_VECTOR->write (GRUB_ARC_STDOUT, &chr, 1, &count);
}

static struct grub_terminfo_output_state grub_console_terminfo_output;

static int
check_is_serial (void)
{
  static int is_serial = -1;

  if (is_serial != -1)
    return is_serial;

  const char *consout = 0;

  /* Check for serial. It works unless user manually overrides ConsoleOut
     variable. If he does there is nothing we can do. Fortunately failure
     isn't critical.
  */
  if (GRUB_ARC_SYSTEM_PARAMETER_BLOCK->firmware_vector_length
      >= ((char *) (&GRUB_ARC_FIRMWARE_VECTOR->getenvironmentvariable + 1)
	  - (char *) GRUB_ARC_FIRMWARE_VECTOR)
      && GRUB_ARC_FIRMWARE_VECTOR->getenvironmentvariable)
    consout = GRUB_ARC_FIRMWARE_VECTOR->getenvironmentvariable ("ConsoleOut");
  if (!consout)
    return is_serial = 0;
  if (consout[0] == '\0')
    return is_serial = 0;

  const char *ptr = consout + grub_strlen (consout) - 1;
  int i;
  /*
    Recognize:
    serial(N)
    serial(N)other(M)
   */
  for (i = 0; i < 2; i++)
    {
      if (*ptr != ')')
	return is_serial = 0;
      ptr--;
      for (; ptr >= consout && grub_isdigit (*ptr); ptr--);
      if (ptr < consout)
	return is_serial = 0;
      if (*ptr != '(')
	return is_serial = 0;
      if (ptr >= consout + sizeof ("serial") - 1
	  && grub_memcmp (ptr - (sizeof ("serial") - 1),
			  "serial", sizeof ("serial") - 1) == 0)
	return is_serial = 1;
      if (!(ptr >= consout + sizeof ("other") - 1
	    && grub_memcmp (ptr - (sizeof ("other") - 1),
			    "other", sizeof ("other") - 1) == 0))
	return is_serial = 0;
      ptr -= sizeof ("other");
    }
  return 0;
}
    
static void
set_console_dimensions (void)
{
  struct grub_arc_display_status *info = NULL;

  if (check_is_serial ())
    {
      grub_console_terminfo_output.width = 80;
      grub_console_terminfo_output.height = 24;
      return;
    }

  if (GRUB_ARC_SYSTEM_PARAMETER_BLOCK->firmware_vector_length
      >= ((char *) (&GRUB_ARC_FIRMWARE_VECTOR->getdisplaystatus + 1)
	  - (char *) GRUB_ARC_FIRMWARE_VECTOR)
      && GRUB_ARC_FIRMWARE_VECTOR->getdisplaystatus)
    info = GRUB_ARC_FIRMWARE_VECTOR->getdisplaystatus (GRUB_ARC_STDOUT);
  if (info)
    {
      grub_console_terminfo_output.width = info->w + 1;
      grub_console_terminfo_output.height = info->h + 1;
    }
}

static grub_err_t
grub_console_init_output (struct grub_term_output *term)
{
  set_console_dimensions ();
  grub_terminfo_output_init (term);

  return 0;
}

static struct grub_terminfo_input_state grub_console_terminfo_input =
  {
    .readkey = readkey
  };

static struct grub_terminfo_output_state grub_console_terminfo_output =
  {
    .put = put,
    .width = 80,
    .height = 20
  };

static struct grub_term_input grub_console_term_input =
  {
    .name = "console",
    .init = grub_terminfo_input_init,
    .getkey = grub_terminfo_getkey,
    .data = &grub_console_terminfo_input
  };

static struct grub_term_output grub_console_term_output =
  {
    .name = "console",
    .init = grub_console_init_output,
    .putchar = grub_terminfo_putchar,
    .getxy = grub_terminfo_getxy,
    .getwh = grub_terminfo_getwh,
    .gotoxy = grub_terminfo_gotoxy,
    .cls = grub_terminfo_cls,
    .setcolorstate = grub_terminfo_setcolorstate,
    .setcursor = grub_terminfo_setcursor,
    .flags = GRUB_TERM_CODE_TYPE_ASCII,
    .data = &grub_console_terminfo_output,
  };

void
grub_console_init_early (void)
{
  grub_term_register_input ("console", &grub_console_term_input);
  grub_term_register_output ("console", &grub_console_term_output);
}

void
grub_console_init_lately (void)
{
  grub_terminfo_init ();
  if (check_is_serial ())
    grub_terminfo_output_register (&grub_console_term_output, "vt100");
  else
    grub_terminfo_output_register (&grub_console_term_output, "arc");
}
