/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FEATURE_H
#define FEATURE_H
#include "NW.H"
#define FEATURE_MESSAGES(X) \
 X(FM_CONFLICT,"Destination exists") \
 X(FM_CHOICES,"O overwrite | A overwrite all | S skip | K skip all | Esc cancel") \
 X(FM_COMPARE,"%s: %lu B  %02u-%02u-%04u %02u:%02u") \
 X(FM_SOURCE,"Source") \
 X(FM_DEST,"Target") \
 X(FM_FREE,"Free %c:%s %c:%s | Selected %lu B%s") \
 X(FM_UNKNOWN,"unknown") \
 X(FM_DIRS," + folders (Ctrl-L totals)") \
 X(FM_PAGING,"Change directory page? Marks on this page will be cleared.") \
 X(FM_ARCHPAGE,"Directory paging is unavailable inside archives") \
 X(FM_SEARCH,"Find beneath directory (filename pattern)") \
 X(FM_SEARCHROOT,"Search beneath absolute directory") \
 X(FM_RESULT,"Search match") \
 X(FM_RESULTKEYS,"Enter opens location | N next match | Esc stops") \
 X(FM_SEARCHDONE,"Search finished: %lu matches") \
 X(FM_ATTR,"File attributes - R/H/S/A toggle; Enter saves") \
 X(FM_ATTRLINE,"Readonly [%c]  Hidden [%c]  System [%c]  Archive [%c]") \
 X(FM_ATTRFAIL,"Cannot change attributes") \
 X(FM_RETRY,"Transfer failed") \
 X(FM_RETRYKEYS,"R retry file | S skip file | Esc cancel remaining work") \
 X(FM_RETRYINFO,"Source: %.62s") \
 X(FM_OFFSET,"Page %lu | Ctrl-PgUp/PgDn change directory page") \
 X(FM_SEARCHING,"Searching filenames - Esc cancels") \
 X(FM_MATCHES,"%lu matches found") \
 X(FM_NODATE,"%s: %lu B  date unknown") \
 X(FM_SAMEFREE,"Free %c:%s | Selected %lu B%s")
#define FEATURE_ENUM(symbol,value) symbol,
enum { FEATURE_MESSAGES(FEATURE_ENUM) FM_COUNT };
#undef FEATURE_ENUM
#ifndef NW_HOST
const char NW_FAR *asset_feature_label(unsigned index);
#endif
void input_history_free(void);
char *feature_label(char *out, unsigned index);
int input_key(char *text, unsigned max, unsigned *cursor, int key);
void input_history_reset(unsigned channel, const char *draft);
int input_history(char *text, unsigned max, unsigned *cursor, unsigned channel, int key);
int input_complete(char *text, unsigned max, unsigned *cursor, const char *directory);
#endif
