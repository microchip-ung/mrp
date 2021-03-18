// Copyright (c) 2020 Microchip Technology Inc. and its subsidiaries.
// SPDX-License-Identifier: (GPL-2.0)

#ifndef PRINT_H
#define PRINT_H

#include <syslog.h>

void print_set_level(int level);
void print_set_verbose(int value);

#ifdef __GNUC__
__attribute__ ((format (printf, 2, 3)))
#endif
void print(int level, char const *format, ...);

#define pr_emerg(x...)   print(LOG_EMERG, x)
#define pr_alert(x...)   print(LOG_ALERT, x)
#define pr_crit(x...)    print(LOG_CRIT, x)
#define pr_err(x...)     print(LOG_ERR, x)
#define pr_warning(x...) print(LOG_WARNING, x)
#define pr_notice(x...)  print(LOG_NOTICE, x)
#define pr_info(x...)    print(LOG_INFO, x)
#define pr_debug(x...)   print(LOG_DEBUG, x)

#endif /* PRINT_H */
