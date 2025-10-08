/**
 * @file cli_handler.h
 * @brief Command-line interface handler
 * 
 * Provides interactive serial console for system monitoring,
 * configuration, and testing.
 */

#ifndef CLI_HANDLER_H
#define CLI_HANDLER_H

/**
 * @brief Initialize CLI console
 * 
 * Configures UART, console subsystem, and linenoise for interactive
 * command-line interface. Registers all available commands and
 * displays welcome banner.
 * 
 * @note Must be called before starting CLI task
 */
void cli_init(void);

/**
 * @brief CLI task
 * @param pvParameters Task parameters (unused)
 * 
 * Main CLI loop that:
 * - Displays prompt
 * - Reads user input with line editing and history
 * - Parses and executes commands
 * - Displays command output and errors
 * 
 * @note Runs continuously, processing user input
 */
void cli_task(void *pvParameters);

#endif