/*
 * Copyright © 2024 Du Yijie.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the “Software”),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
/*
 * An interactive shell listening on UART, useful for debugging.
 *
 * This is the implementation. See `"Shell.h"` for documentation.
 */
#include "ToyShell.h"

#include <stdlib.h>
#include <string.h>

#if defined(ARDUINO_ARCH_ESP32)
// The ESP32 core puts FreeRTOS headers in a custom location. Make it happy.
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#else
// Include the usual FreeRTOS central header.
#include <Arduino_FreeRTOS.h>
#endif

Shell::~Shell() {
  end();
}

void Shell::begin(Stream &stream) {
  if (!f_begin) {
    this->stream = &stream;
    xTaskCreate(Shell::start, "shell", 4096, this, 1, nullptr);
  }
}

void Shell::end() {
  atomic_store(&f_end, 1);
  while (f_begin != 0)
    taskYIELD();
}

static void prompt(Stream &stream) {
  stream.setTimeout(20);
  stream.print("shell> ");
}

static int cmp(const void *k, const void *e) {
  const char *key = (const char *)k;
  const Command *entry = (const Command *)e;
  return strcmp(key, entry->name);
}

void Shell::main() {
  atomic_store(&f_begin, 1);

  int argc;
  size_t count;
  char *bufhead = input;
  char *end;
  char *space;
  char *i;

  prompt(*stream);

  while (f_end == 0) {
    // read input
    count = stream->readBytes(bufhead, SHELL_LINE_MAX - (bufhead - input));
    end = (char *)memchr(bufhead, '\n', count);
    if (!end) {
      bufhead += count;

      // discard buffer if overrun
      if (bufhead >= &input[SHELL_LINE_MAX]) {
        stream->print("\nshell: Command line too long; discarding\n");
        prompt(*stream);
        bufhead = input;
      }

      // continue to wait for input
      continue;
    }

    // split arguments
    *end = '\0';
    argc = 0;
    stream->printf("%s\n", input);

    for (i = input; i < end; i += strlen(i) + 1) {
      if (argc < SHELL_ARG_MAX) {
        argv[argc] = i;
        argc += 1;
      } else {
        stream->printf(
            "Too many arguments; discarding arguments after #%d\n",
            SHELL_ARG_MAX - 1);
        break;
      }

      space = (char *)memchr(i, ' ', end - i);
      if (space) {
        *space = '\0';
      }
    }

    // execute command
    if (argc == 0) continue;
    const Command *cmd = (const Command *)bsearch(
        argv[0], commands, cmd_count, sizeof(Command), cmp);
    if (cmd) {
      cmd->entry(argc, argv, stream);
    } else {
      stream->printf("shell: No such command: %s\n", argv[0]);
    }

    // prepare for next command
    bufhead += count;
    count = bufhead - (end + 1);
    if (count > 0) {
      memmove(input, end + 1, count);
    }

    bufhead = input + count;
    prompt(*stream);
  }

  // cleanup
  atomic_store(&f_end, 0);
  atomic_store(&f_begin, 0);
  vTaskDelete(NULL);
}

void Shell::start(void *parameters) {
  Shell *shell = (Shell *)parameters;
  shell->main();
}
