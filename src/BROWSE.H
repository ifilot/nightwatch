/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef BROWSE_H
#define BROWSE_H
#include "ARCHIVE.H"
int browse_type(const char *name);
int browse_active(const Panel *p);
int browse_open(Panel *p, const char *source);
int browse_load(Panel *p);
int browse_enter(Panel *p, const char *name);
void browse_release(Panel *p);
/* Physical containing directory for settings and leaving a virtual pane. */
int browse_home(const Panel *p, char *path);
int browse_source(const Panel *p, char *path);
int browse_copy(Panel *p, const char *target, int measure,
                unsigned long *files, unsigned long *bytes);
int browse_view(Panel *p, const char *temporary);
#endif
