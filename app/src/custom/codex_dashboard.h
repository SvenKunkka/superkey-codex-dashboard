#ifndef CODEX_DASHBOARD_H
#define CODEX_DASHBOARD_H

#include <stdbool.h>

/* Creates the fixed three-panel Codex companion display. */
int codex_dashboard_init(void);
void codex_dashboard_deinit(void);

/* Accepts the `sys_set codex_* <value>` commands received over USB CDC. */
bool codex_dashboard_handle_command(const char *key, const char *value);

#endif
