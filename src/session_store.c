#include "session_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int state_dir(char *path, int path_size)
{
#if defined(KRYON_NATIVE_PLAN9)
    const char *home = getenv("home");
#else
    const char *xdg = getenv("XDG_STATE_HOME");
    const char *home = getenv("HOME");
#endif

    if(path == NULL || path_size <= 0)
        return 0;
#if defined(KRYON_NATIVE_PLAN9)
    if(home != NULL && home[0] != '\0')
        snprintf(path, (size_t)path_size, "%s/lib/kapsule", home);
    else
        return 0;
#else
    if(xdg != NULL && xdg[0] != '\0')
        snprintf(path, (size_t)path_size, "%s/kapsule", xdg);
    else if(home != NULL && home[0] != '\0')
        snprintf(path, (size_t)path_size, "%s/.local/state/kapsule", home);
    else
        return 0;
#endif
    return 1;
}

int session_store_state_path(char *path, int path_size)
{
    char dir[1024];
    int len;

    if(path == NULL || path_size <= 0 || !state_dir(dir, sizeof(dir)))
        return 0;
    len = (int)strlen(dir);
    if(len <= 0 || len + 8 >= path_size)
        return 0;
    snprintf(path, (size_t)path_size, "%s/session", dir);
    return 1;
}

static void ensure_state_dir(void)
{
    char dir[1024];
    char partial[1024];
    int i;

    if(state_dir(dir, sizeof(dir)))
        snprintf(partial, sizeof(partial), "%s", dir);
    else
        return;
    for(i = 1; partial[i] != '\0'; i++) {
        if(partial[i] == '/') {
            partial[i] = '\0';
            mkdir(partial, 0700);
            partial[i] = '/';
        }
    }
    mkdir(partial, 0700);
}

int session_store_save(const Session *sessions, int session_count, int active)
{
    char path[1024];
    FILE *file;
    int i;

    if(sessions == NULL || session_count <= 0 ||
       !session_store_state_path(path, sizeof(path)))
        return 0;
    ensure_state_dir();
    file = fopen(path, "w");
    if(file == NULL)
        return 0;
    fprintf(file, "active=%d\n", active);
    for(i = 0; i < session_count; i++) {
        char cwd[1024];
        char line[8192];
        SessionRecord record;
        const Session *session = &sessions[i];

        memset(&record, 0, sizeof(record));
        session_current_cwd(session, cwd, sizeof(cwd));
        snprintf(record.cwd, sizeof(record.cwd), "%s", cwd);
        snprintf(record.shell, sizeof(record.shell), "%s", session->shell);
        snprintf(record.title, sizeof(record.title), "%s",
                 session->title_override ? session_title(session) : "");
        snprintf(record.command, sizeof(record.command), "%s",
                 session->command);
        record.title_override = session->title_override ? 1 : 0;
        record.scroll_offset = session->scroll_offset;
        if(FormatTerminalPaneSessionRecord(line, (int)sizeof(line), record) >
           0)
            fprintf(file, "%s\n", line);
    }
    fclose(file);
    return 1;
}

int session_store_load(SessionRecord *records, int max_records, int *active)
{
    char path[1024];
    char line[4096];
    FILE *file;
    int restored = 0;

    if(active != NULL)
        *active = 0;
    if(records == NULL || max_records <= 0 ||
       !session_store_state_path(path, sizeof(path)))
        return 0;
    file = fopen(path, "r");
    if(file == NULL)
        return 0;
    while(fgets(line, sizeof(line), file) != NULL && restored < max_records) {
        SessionRecord *record = &records[restored];

        if(ParseTerminalPaneSessionActive(line, active))
            continue;
        if(ParseTerminalPaneSessionRecord(line, record))
            restored++;
    }
    fclose(file);
    return restored;
}
