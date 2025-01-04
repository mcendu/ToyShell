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
 * The shell mimicks the command-line interfaces shipped with almost every
 * desktop and server. A command is like a sentence, each word separated
 * by a space. The first word specifies the command to run; all later
 * words serve as parameters to said command. The parsing is simple compared
 * to desktop shells: there is no quoting mechanism whatsoever, and the
 * entire command has to be on a single line.
 *
 * The shell requires FreeRTOS to run. ESP32-based platforms ship with
 * FreeRTOS active by default. Elsewhere, like on AVR or UNO R4, the header
 * `<Arduino_FreeRTOS.h>` should be included in your sketch, and
 * `vTaskStartScheduler` should be called at the end of your `setup`
 * routine for the shell to actually start.
 */
#ifndef TOYSHELL_H
#define TOYSHELL_H

#include <stdatomic.h>
#include <stdlib.h>

#include <HardwareSerial.h>
#include <Stream.h>

#define SHELL_LINE_MAX 2048
#define SHELL_ARG_MAX 32

/**
 * The shell's event loop. Called by `Shell::begin()`. DO NOT CALL THIS
 * FUNCTION YOURSELF.
 */
void shellMain(void *parameters);

/**
 * A command accepted by the shell.
 */
struct Command {
  /**
   * The name of the command.
   *
   * While any character (other than a NUL byte) is acceptable in the
   * name of a command, non-ASCII characters should be avoided to prevent
   * issues arising from terminal control sequences or text encodings.
   *
   * Consider implementing a command named `help` that prints a list of
   * all commands, in case you or some other developer forgot the name
   * of some debugging function.
   */
  const char *name;
  /**
   * The entry point of a command.
   *
   * The calling convention is akin to the `main` function in normal C
   * programs. The list of parameters, including the name of the command, is
   * given in the parameter `argv`. The number of parameters is passed
   * in `argc`. The UART interface the shell is listening on is exposed
   * as the `serial` pointer; output should be written using `serial->print()`
   * and friends, and user input should be obtained from the same interface as
   * well.
   *
   * This field expects a function. If you want to implement a command with
   * the entry point `cmdHelp`, you should put `cmdHelp` instead of `cmdHelp()`
   * here.
   */
  int (*entry)(int argc, const char *const *argv, Stream *serial);
};

/**
 * A simple, interactive UART shell.
 */
class Shell {
private:
  Stream *stream;
  const Command *commands;
  size_t cmd_count;
  atomic_bool f_begin;
  atomic_bool f_end;

  char input[SHELL_LINE_MAX];
  char *argv[SHELL_ARG_MAX];

  void main();
  static void start(void *);
public:
  /**
   * Create a shell instance accepting the specified list of commands. The
   * list of commands must be sorted by name in dictionary order, or strange
   * bugs will arise. See also the documentation for `Command`.
   *
   * This form requires the number of commands to be passed in a parameter.
   */
  Shell(const Command *commands, size_t count)
      : stream(nullptr), commands(commands), cmd_count(count), f_begin(0), f_end(0) {}

  /**
   * Create a shell instance accepting the specified list of commands. The
   * list of commands must be sorted by name in dictionary order, or strange
   * bugs will arise. See also the documentation for `Command`.
   *
   * This form requires the list of commands to end with {nullptr, nullptr}.
   */
  Shell(const Command *commands)
      : stream(nullptr), commands(commands), f_begin(0), f_end(0) {
    cmd_count = 0;
    while (commands[cmd_count].name) {
      cmd_count += 1;
    }
  }

  Shell(Shell &other) = delete;

  /**
   * Stop the shell and free all associated resources.
   */
  ~Shell();

  /**
   * Start the shell and listen on the serial port specified. If no
   * serial port is specified, defaults to `Serial`.
   *
   * This method fails if it is already accepting commands; call `end`
   * before calling `begin` again to change port.
   */
  void begin(Stream &stream = Serial);

  /**
   * Stop accepting commands.
   */
  void end();
};

#endif