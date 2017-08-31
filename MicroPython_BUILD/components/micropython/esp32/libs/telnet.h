/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef TELNET_H_
#define TELNET_H_

#include "sdkconfig.h"

#ifdef CONFIG_MICROPY_USE_TELNET

#include <stdbool.h>

#define TELNET_USER_PASS_LEN_MAX	32
#define TELNET_DEF_USER             "micro"
#define TELNET_DEF_PASS             "python"
#define TELNET_DEF_TIMEOUT_MS       300000	// 5 minutes
#define TELNET_MUTEX_TIMEOUT_MS       1000

typedef enum {
    E_TELNET_STE_DISABLED = 0,
    E_TELNET_STE_START,
    E_TELNET_STE_LISTEN,
    E_TELNET_STE_CONNECTED,
    E_TELNET_STE_LOGGED_IN
} telnet_state_t;


char telnet_user[TELNET_USER_PASS_LEN_MAX + 1];
char telnet_pass[TELNET_USER_PASS_LEN_MAX + 1];

/******************************************************************************
 DECLARE PUBLIC FUNCTIONS
 ******************************************************************************/

void telnet_init (void);
void telnet_deinit (void);
bool telnet_run (void);
void telnet_tx_strn (const char *str, int len);
bool telnet_rx_any (void);
bool telnet_loggedin (void);
int  telnet_rx_char (void);
bool telnet_enable (void);
bool telnet_disable (void);
bool telnet_isenabled (void);
bool telnet_reset (void);
int telnet_getstate();

#endif

#endif /* TELNET_H_ */
