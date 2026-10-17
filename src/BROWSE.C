/* SPDX-License-Identifier: GPL-3.0-only */
/* Read-only archive panes. Only listings and navigation state are cached;
 * payloads stream through the existing checked codecs on view/extract. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __TURBOC__
#include <alloc.h>
#include <dos.h>
#endif
#include "BROWSE.H"
typedef struct {
    void NW_FAR *allocation;
    char source[NW_PATH], prefix[NW_PATH];
    unsigned type;
} Browser;
static Browser NW_FAR *views[2];
static unsigned slot(const Panel *p) { return p == &panes[1]; }
static void from_far(char *out, const char NW_FAR *in)
{ while ((*out++ = *in++) != 0) {} }
static void to_far(char NW_FAR *out, const char *in)
{ while ((*out++ = *in++) != 0) {} }
static void NW_FAR *allocate(unsigned bytes, void NW_FAR **original)
{
#ifdef __TURBOC__
    *original = farmalloc((unsigned long)bytes+15UL);
    if (!*original) return NULL;
    return MK_FP(FP_SEG(*original)+(FP_OFF(*original)>>4),FP_OFF(*original)&15);
#else
    *original = malloc(bytes); return *original;
#endif
}
static void release(void NW_FAR *original)
{
#ifdef __TURBOC__
    farfree(original);
#else
    free(original);
#endif
}
int browse_type(const char *name)
{ return name_match("*.ZIP",name) ? 1 : name_match("*.ARJ",name) ? 2 : 0; }
int browse_active(const Panel *p) { return views[slot(p)] != NULL; }
void browse_release(Panel *p)
{
    Browser NW_FAR *b = views[slot(p)];
    if (b) { release(b->allocation); views[slot(p)] = NULL; }
}
int browse_source(const Panel *p, char *path)
{
    Browser NW_FAR *b = views[slot(p)];
    if (!b) return 0;
    from_far(path,b->source); return 1;
}
int browse_home(const Panel *p, char *path)
{
    char *cut;
    if (!browse_source(p,path)) { strcpy(path,p->path); return 1; }
    cut = strrchr(path,SEP);
    if (!cut) return 0;
    if (cut == path || (cut == path+2 && path[1] == ':')) cut[1] = 0;
    else *cut = 0;
    return 1;
}
static int walk(Browser NW_FAR *b, ArchiveVisit visit, void *context)
{
    char source[NW_PATH];
    from_far(source,b->source);
    return b->type == 1 ? zip_walk(source,visit,context) : arj_walk(source,visit,context);
}
typedef struct {
    Entry NW_FAR *entries;
    unsigned count;
    int truncated;
    char prefix[NW_PATH];
} Listing;
static int list_member(Archive *a, Member *m, ArchiveUnpack unpack, void *context)
{
    Listing *l = context;
    Entry entry;
    char name[13];
    const char *rest, *cut;
    unsigned length, i, n = (unsigned)strlen(l->prefix);
    int directory;
    (void)a; (void)unpack;
    if (strncmp(m->name,l->prefix,n)) return 1;
    rest = m->name+n;
    if (!*rest) return 1;
    cut = strchr(rest,SEP); length = cut ? (unsigned)(cut-rest) : (unsigned)strlen(rest);
    if (!length || length > 12) return archive_message(ARC_BROWSER_PATH);
    memcpy(name,rest,length); name[length] = 0;
    directory = cut != NULL || m->directory;
    for (i = 1; i < l->count; ++i) {
        unsigned j = 0;
        while (name[j] && name[j] == l->entries[i].name[j]) ++j;
        if (!name[j] && !l->entries[i].name[j]) return 1;
    }
    if (l->count == NW_FILES) { l->truncated = 1; return 1; }
    entry = m->info; strcpy(entry.name,name); entry.marked = 0;
    if (directory) { entry.attr = NW_DIR; entry.size = 0; if (cut) entry.date = entry.time = 0; }
    /* Direct far assignments of whole structs are not portable to Turbo C. */
    {
        unsigned j;
        const unsigned char *in = (const unsigned char *)&entry;
        unsigned char NW_FAR *out = (unsigned char NW_FAR *)&l->entries[l->count++];
        for (j = 0; j < sizeof(Entry); ++j) out[j] = in[j];
    }
    return 1;
}
static int load(Panel *p, Browser NW_FAR *b, const char *prefix, const char *selected)
{
    Listing l;
    void NW_FAR *memory;
    char source[NW_PATH], display[NW_PATH];
    unsigned i, j;
    int ok;
    from_far(source,b->source);
    if (strlen(source)+(*prefix ? 1+strlen(prefix)-1 : 0) >= NW_PATH)
        return archive_message(ARC_BROWSER_PATH);
    strcpy(display,source);
    if (*prefix) {
        if (!path_join(display,source,prefix)) return 0;
        display[strlen(display)-1] = 0;
    }
    memset(&l,0,sizeof(l)); strcpy(l.prefix,prefix);
    l.entries = allocate((unsigned)(NW_FILES*sizeof(Entry)),&memory);
    if (!l.entries) return archive_message(ARC_BROWSER_MEMORY);
    /* The parent exists even at archive root and leads to the containing disk folder. */
    for (j = 0; j < sizeof(Entry); ++j) ((unsigned char NW_FAR *)l.entries)[j] = 0;
    l.entries[0].name[0] = '.'; l.entries[0].name[1] = '.'; l.entries[0].attr = NW_DIR;
    l.count = 1;
    ok = walk(b,list_member,&l);
    if (ok) {
        for (i = 0; i < l.count; ++i) {
            unsigned char *out = (unsigned char *)&p->files[i];
            const unsigned char NW_FAR *in = (const unsigned char NW_FAR *)&l.entries[i];
            for (j = 0; j < sizeof(Entry); ++j) out[j] = in[j];
        }
        p->count = l.count; p->truncated = l.truncated; p->marks = p->cursor = p->top = 0;
        strcpy(p->path,display); to_far(b->prefix,prefix);
        panel_sort(p,p->sort,p->reverse);
        if (selected) for (i = 0; i < (unsigned)p->count; ++i)
            if (!strcmp(selected,p->files[i].name)) { p->cursor = i; break; }
        ++p->revision;
    }
    release(memory); return ok;
}
int browse_open(Panel *p, const char *source)
{
    Browser NW_FAR *b;
    void NW_FAR *memory;
    int ok;
    if (browse_active(p)) return archive_message(ARC_BROWSER_NESTED);
    if (strlen(source) >= NW_PATH || !strrchr(source,SEP) || !browse_type(source))
        return archive_message(ARC_BROWSER_PATH);
    b = allocate(sizeof(Browser),&memory);
    if (!b) return archive_message(ARC_BROWSER_MEMORY);
    b->allocation = memory; b->type = browse_type(source);
    to_far(b->source,source); b->prefix[0] = 0;
    ok = load(p,b,"",NULL);
    if (ok) views[slot(p)] = b; else release(memory);
    return ok;
}
int browse_load(Panel *p)
{
    char prefix[NW_PATH], selected[13];
    Browser NW_FAR *b = views[slot(p)];
    if (!b) return panel_load(p);
    from_far(prefix,b->prefix); selected[0] = 0;
    if (p->cursor >= 0 && p->cursor < p->count) strcpy(selected,p->files[p->cursor].name);
    return load(p,b,prefix,selected);
}
int browse_enter(Panel *p, const char *name)
{
    Browser NW_FAR *b = views[slot(p)];
    char prefix[NW_PATH], next[NW_PATH], selected[13], home[NW_PATH], source[NW_PATH], *cut;
    unsigned i;
    if (!b) return panel_enter(p,name);
    from_far(prefix,b->prefix);
    if (!strcmp(name,"..")) {
        if (!*prefix) {
            browse_source(p,source);
            if (!browse_home(p,home)) return archive_message(ARC_BROWSER_PATH);
            cut = strrchr(source,SEP); strncpy(selected,cut+1,12); selected[12] = 0;
            strcpy(p->path,home);
            if (!panel_load(p)) {
                browse_release(p); p->count = p->cursor = p->top = p->marks = p->truncated = 0;
                ++p->revision; return 0;
            }
            browse_release(p);
            for (i = 0; i < (unsigned)p->count; ++i) if (!strcmp(p->files[i].name,selected)) { p->cursor = i; break; }
            return 1;
        }
        prefix[strlen(prefix)-1] = 0; cut = strrchr(prefix,SEP);
        strcpy(selected,cut ? cut+1 : prefix);
        if (cut) cut[1] = 0; else prefix[0] = 0;
        return load(p,b,prefix,selected);
    }
    for (i = 0; i < (unsigned)p->count; ++i)
        if (!strcmp(name,p->files[i].name) && (p->files[i].attr & NW_DIR)) break;
    if (i == (unsigned)p->count || !valid_name(name)) return archive_message(ARC_BROWSER_PATH);
    if (strlen(prefix)+strlen(name)+1 >= NW_PATH) return archive_message(ARC_BROWSER_PATH);
    strcpy(next,prefix); strcat(next,name); i = (unsigned)strlen(next); next[i] = SEP; next[i+1] = 0;
    return load(p,b,next,NULL);
}
typedef struct {
    Panel *panel;
    char prefix[NW_PATH], target[NW_PATH];
    const char *temporary;
    unsigned long files, bytes;
    int measure, view, found, created;
} Selection;
static int selected_member(Archive *a, Member *m, ArchiveUnpack unpack, void *context)
{
    Selection *s = context;
    Panel *p = s->panel;
    const char *rest;
    unsigned i, n = (unsigned)strlen(s->prefix), length;
    int fd, ok;
    if (strncmp(m->name,s->prefix,n)) return 1;
    rest = m->name+n;
    if (!*rest) return 1;
    for (i = 0; i < (unsigned)p->count; ++i) {
        Entry *e = &p->files[i];
        if (!strcmp(e->name,"..") || (s->view ? i != (unsigned)p->cursor : p->marks ? !e->marked : i != (unsigned)p->cursor)) continue;
        length = (unsigned)strlen(e->name);
        if (strncmp(rest,e->name,length) ||
            (rest[length] && (!(e->attr & NW_DIR) || rest[length] != SEP))) continue;
        if (s->view) {
            if (m->directory || strcmp(rest,e->name)) continue;
            if (s->found) return archive_message(ARC_BROWSER_PATH);
            s->found = 1;
            fd = fs_create(s->temporary);
            if (fd < 0) { fs_error("Create view temporary"); return 0; }
            s->created = 1;
            ok = unpack(a,m,fd);
            if (fs_close(fd) < 0 && ok) { fs_error("Close view temporary"); ok = 0; }
            return ok;
        }
        memmove(m->name,rest,strlen(rest)+1); strcpy(a->target,s->target);
        if (s->measure) {
            if (!archive_directories(a,m,0)) return 0;
            if (!m->directory) { ++s->files; s->bytes += m->size; }
            return 1;
        }
        return archive_extract_member(a,m,unpack);
    }
    return 1;
}
int browse_copy(Panel *p, const char *target, int measure, unsigned long *files, unsigned long *bytes)
{
    Selection s;
    Browser NW_FAR *b = views[slot(p)];
    int ok;
    if (!b) return archive_message(ARC_BROWSER_PATH);
    memset(&s,0,sizeof(s)); s.panel = p; s.measure = measure;
    from_far(s.prefix,b->prefix);
    if (!fs_canonical(target,s.target)) return archive_message(ARC_BROWSER_DESTINATION);
    ok = walk(b,selected_member,&s);
    if (ok && measure) { *files = s.files; *bytes = s.bytes; }
    return ok;
}
int browse_view(Panel *p, const char *temporary)
{
    Selection s;
    Browser NW_FAR *b = views[slot(p)];
    int ok;
    if (!b || !p->count || (p->files[p->cursor].attr & NW_DIR)) return archive_message(ARC_BROWSER_PATH);
    memset(&s,0,sizeof(s)); s.panel = p; s.view = 1; s.temporary = temporary;
    from_far(s.prefix,b->prefix);
    ok = walk(b,selected_member,&s);
    if (!ok && s.created) operation_discard(temporary);
    if (ok && !s.found) return archive_message(ARC_BROWSER_PATH);
    return ok;
}
