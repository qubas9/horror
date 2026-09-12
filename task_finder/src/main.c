#include "raylib.h"
#include "score.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>

#include <sys/stat.h>

#ifdef _WIN32
    #define popen _popen
    #define pclose _pclose
    #define strcasecmp _stricmp
    #define strncasecmp _strnicmp
    #define NULL_DEV "2>nul"
    __declspec(dllimport) unsigned long __stdcall GetModuleFileNameA(void *hModule, char *lpFilename, unsigned long nSize);
    __declspec(dllimport) unsigned long __stdcall GetFullPathNameA(const char *lpFileName, unsigned long nBufferLength, char *lpBuffer, char **lpFilePart);
#else
    #include <unistd.h>
    #include <strings.h>
    #define NULL_DEV "2>/dev/null"
#endif

#define MAX_TASKS 256
#define MAX_WORKERS 32
#define MAX_STR 256
#define MAX_URL 512
#define MAX_CONTENT (1024 * 64)

// -----------------------------------------------------------------------------
// DATOVÉ STRUKTURY
// -----------------------------------------------------------------------------
typedef struct {
    char name[MAX_STR];
    char branch[MAX_STR];
    char github_url[MAX_URL];
    char task_master[MAX_STR];
    bool is_task_master_free;

    bool requires_worker;
    bool is_worker_free;
    char workers[MAX_WORKERS][MAX_STR];
    int worker_count;

    char status[64];
    bool is_available;

    int deadline_year;
    int deadline_month;
    int deadline_day;
    bool has_deadline;
    char deadline_raw[64];
    int days_to_deadline;

    bool has_subtasks;
    float score;
} Task;

typedef struct {
    Task items[MAX_TASKS];
    int count;
} TaskList;

typedef enum {
    SCREEN_LOGIN = 0,
    SCREEN_ACTIVE_TASK,
    SCREEN_FREE_TASKS
} AppScreen;

// -----------------------------------------------------------------------------
// POMOCNÉ FUNKCE PRO TEXT A DATUM
// -----------------------------------------------------------------------------
static char* TrimWhitespace(char *str) {
    if (!str) return NULL;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

static bool StrCaseContains(const char *haystack, const char *needle) {
    if (!haystack || !needle) return false;
    size_t h_len = strlen(haystack);
    size_t n_len = strlen(needle);
    if (n_len > h_len) return false;
    for (size_t i = 0; i <= h_len - n_len; i++) {
        if (strncasecmp(haystack + i, needle, n_len) == 0) return true;
    }
    return false;
}

static bool NamesEqual(const char *a, const char *b) {
    if (!a || !b) return false;
    char buf_a[MAX_STR];
    char buf_b[MAX_STR];
    strncpy(buf_a, a, sizeof(buf_a) - 1);
    buf_a[sizeof(buf_a) - 1] = '\0';
    strncpy(buf_b, b, sizeof(buf_b) - 1);
    buf_b[sizeof(buf_b) - 1] = '\0';
    char *ta = TrimWhitespace(buf_a);
    char *tb = TrimWhitespace(buf_b);
    return (strcasecmp(ta, tb) == 0);
}

// Odstrani ceskou diakritiku z UTF-8 retezce pro ciste vykresleni ve standardnim Raylib fontu
static void StripDiacritics(const char *src, char *dst, size_t dst_len) {
    if (!src || !dst || dst_len == 0) return;
    size_t s = 0, d = 0;
    while (src[s] && d < dst_len - 1) {
        unsigned char c1 = (unsigned char)src[s];
        if (c1 < 128) {
            dst[d++] = src[s++];
        } else if (c1 == 0xC3 && src[s + 1]) {
            unsigned char c2 = (unsigned char)src[s + 1];
            s += 2;
            switch (c2) {
                case 0xA1: dst[d++] = 'a'; break; // á
                case 0x81: dst[d++] = 'A'; break; // Á
                case 0xA9: dst[d++] = 'e'; break; // é
                case 0x89: dst[d++] = 'E'; break; // É
                case 0xAD: dst[d++] = 'i'; break; // í
                case 0x8D: dst[d++] = 'I'; break; // Í
                case 0xB3: dst[d++] = 'o'; break; // ó
                case 0x93: dst[d++] = 'O'; break; // Ó
                case 0xBA: dst[d++] = 'u'; break; // ú
                case 0x9A: dst[d++] = 'U'; break; // Ú
                case 0xBD: dst[d++] = 'y'; break; // ý
                case 0x9D: dst[d++] = 'Y'; break; // Ý
                default: dst[d++] = '?'; break;
            }
        } else if (c1 == 0xC4 && src[s + 1]) {
            unsigned char c2 = (unsigned char)src[s + 1];
            s += 2;
            switch (c2) {
                case 0x8D: dst[d++] = 'c'; break; // č
                case 0x8C: dst[d++] = 'C'; break; // Č
                case 0x8F: dst[d++] = 'd'; break; // ď
                case 0x8E: dst[d++] = 'D'; break; // Ď
                case 0x9B: dst[d++] = 'e'; break; // ě
                case 0x9A: dst[d++] = 'E'; break; // Ě
                default: dst[d++] = '?'; break;
            }
        } else if (c1 == 0xC5 && src[s + 1]) {
            unsigned char c2 = (unsigned char)src[s + 1];
            s += 2;
            switch (c2) {
                case 0x88: dst[d++] = 'n'; break; // ň
                case 0x87: dst[d++] = 'N'; break; // Ň
                case 0x99: dst[d++] = 'r'; break; // ř
                case 0x98: dst[d++] = 'R'; break; // Ř
                case 0xA1: dst[d++] = 's'; break; // š
                case 0xA0: dst[d++] = 'S'; break; // Š
                case 0xA5: dst[d++] = 't'; break; // ť
                case 0xA4: dst[d++] = 'T'; break; // Ť
                case 0xAF: dst[d++] = 'u'; break; // ů
                case 0xAE: dst[d++] = 'U'; break; // Ů
                case 0xBE: dst[d++] = 'z'; break; // ž
                case 0xBD: dst[d++] = 'Z'; break; // Ž
                default: dst[d++] = '?'; break;
            }
        } else {
            s++;
        }
    }
    dst[d] = '\0';
}

// Výpočet kalendářních dnů od počátku juliánského letopočtu (pro přesný rozdíl dnů)
static long DateToDays(int y, int m, int d) {
    if (m <= 2) {
        y -= 1;
        m += 12;
    }
    return 365L * y + y / 4 - y / 100 + y / 400 + (153 * (m + 1)) / 5 + d - 1;
}

static int CalculateDaysToDeadline(int d_year, int d_month, int d_day) {
    time_t t = time(NULL);
    struct tm *today = localtime(&t);
    int t_year = today->tm_year + 1900;
    int t_month = today->tm_mon + 1;
    int t_day = today->tm_mday;

    long d_days = DateToDays(d_year, d_month, d_day);
    long t_days = DateToDays(t_year, t_month, t_day);

    return (int)(d_days - t_days);
}

static bool ParseDeadline(const char *str, int *out_y, int *out_m, int *out_d) {
    if (!str || strlen(str) == 0) return false;
    if (StrCaseContains(str, "placeholder")) return false;

    int y = 0, m = 0, d = 0;
    // Formáty: yy/MM/DD, yyyy/MM/DD, yy-MM-DD, yyyy-MM-DD, DD.MM.YYYY
    if (sscanf(str, "%d/%d/%d", &y, &m, &d) == 3 ||
        sscanf(str, "%d-%d-%d", &y, &m, &d) == 3) {
        if (y < 100) y += 2000;
        if (m >= 1 && m <= 12 && d >= 1 && d <= 31) {
            *out_y = y;
            *out_m = m;
            *out_d = d;
            return true;
        }
    } else if (sscanf(str, "%d.%d.%d", &d, &m, &y) == 3) {
        if (y < 100) y += 2000;
        if (m >= 1 && m <= 12 && d >= 1 && d <= 31) {
            *out_y = y;
            *out_m = m;
            *out_d = d;
            return true;
        }
    }
    return false;
}

// -----------------------------------------------------------------------------
// GIT FUNKCE
// -----------------------------------------------------------------------------
static char g_git_cmd[512] = "git";
static bool g_git_available = false;

static bool TestGitCommand(const char *cmd_to_test) {
    char check_cmd[1024];
#ifdef _WIN32
    snprintf(check_cmd, sizeof(check_cmd), "\"\"%s\" --version\" %s", cmd_to_test, NULL_DEV);
#else
    snprintf(check_cmd, sizeof(check_cmd), "\"%s\" --version %s", cmd_to_test, NULL_DEV);
#endif
    FILE *p = popen(check_cmd, "r");
    if (!p) return false;
    char buf[128] = "";
    bool ok = (fgets(buf, sizeof(buf), p) != NULL);
    pclose(p);
    return ok;
}

static void DetectGitBinary(void) {
    if (TestGitCommand("git")) {
        strcpy(g_git_cmd, "git");
        g_git_available = true;
        return;
    }

#ifdef _WIN32
    const char *common_paths[] = {
        "C:\\Program Files\\Git\\cmd\\git.exe",
        "C:\\Program Files\\Git\\bin\\git.exe",
        "C:\\Program Files (x86)\\Git\\cmd\\git.exe",
        "C:\\Program Files (x86)\\Git\\bin\\git.exe",
        NULL
    };

    for (int i = 0; common_paths[i]; i++) {
        if (TestGitCommand(common_paths[i])) {
            strncpy(g_git_cmd, common_paths[i], sizeof(g_git_cmd) - 1);
            g_git_cmd[sizeof(g_git_cmd) - 1] = '\0';
            g_git_available = true;
            return;
        }
    }

    const char *localappdata = getenv("LOCALAPPDATA");
    if (localappdata) {
        char p1[512], p2[512];
        snprintf(p1, sizeof(p1), "%s\\Programs\\Git\\cmd\\git.exe", localappdata);
        if (TestGitCommand(p1)) {
            strncpy(g_git_cmd, p1, sizeof(g_git_cmd) - 1);
            g_git_cmd[sizeof(g_git_cmd) - 1] = '\0';
            g_git_available = true;
            return;
        }
        snprintf(p2, sizeof(p2), "%s\\Programs\\Git\\bin\\git.exe", localappdata);
        if (TestGitCommand(p2)) {
            strncpy(g_git_cmd, p2, sizeof(g_git_cmd) - 1);
            g_git_cmd[sizeof(g_git_cmd) - 1] = '\0';
            g_git_available = true;
            return;
        }
    }

    const char *userprofile = getenv("USERPROFILE");
    if (userprofile) {
        char p_scoop[512];
        snprintf(p_scoop, sizeof(p_scoop), "%s\\scoop\\shims\\git.exe", userprofile);
        if (TestGitCommand(p_scoop)) {
            strncpy(g_git_cmd, p_scoop, sizeof(g_git_cmd) - 1);
            g_git_cmd[sizeof(g_git_cmd) - 1] = '\0';
            g_git_available = true;
            return;
        }
    }
#endif

    g_git_available = false;
}

static bool ExecuteGit(const char *repo_dir, const char *args, char *output, size_t max_len) {
    char cmd[2048];
    if (repo_dir && strlen(repo_dir) > 0) {
#ifdef _WIN32
        snprintf(cmd, sizeof(cmd), "\"\"%s\" -c safe.directory=* -C \"%s\" %s\" %s", g_git_cmd, repo_dir, args, NULL_DEV);
#else
        snprintf(cmd, sizeof(cmd), "\"%s\" -C \"%s\" %s %s", g_git_cmd, repo_dir, args, NULL_DEV);
#endif
    } else {
#ifdef _WIN32
        snprintf(cmd, sizeof(cmd), "\"\"%s\" -c safe.directory=* %s\" %s", g_git_cmd, args, NULL_DEV);
#else
        snprintf(cmd, sizeof(cmd), "\"%s\" %s %s", g_git_cmd, args, NULL_DEV);
#endif
    }

    FILE *pipe = popen(cmd, "r");
    if (!pipe) return false;

    size_t total = 0;
    output[0] = '\0';
    while (total < max_len - 1 && fgets(output + total, (int)(max_len - total), pipe) != NULL) {
        total = strlen(output);
    }

    pclose(pipe);
    return (total > 0);
}

static bool DirHasGit(const char *dir) {
    if (!dir || strlen(dir) == 0) return false;
    char path[1024];
    snprintf(path, sizeof(path), "%s/.git", dir);
    struct stat st;
    return (stat(path, &st) == 0);
}

static bool WalkUpForGit(const char *start_dir, char *out_repo, size_t max_len) {
    if (!start_dir || strlen(start_dir) == 0) return false;
    char current[1024] = "";
#ifdef _WIN32
    if (!GetFullPathNameA(start_dir, sizeof(current), current, NULL)) {
        strncpy(current, start_dir, sizeof(current) - 1);
        current[sizeof(current) - 1] = '\0';
    }
#else
    char *res = realpath(start_dir, NULL);
    if (res) {
        snprintf(current, sizeof(current), "%s", res);
        free(res);
    } else {
        snprintf(current, sizeof(current), "%s", start_dir);
    }
#endif

    while (strlen(current) > 0) {
        if (DirHasGit(current)) {
            snprintf(out_repo, max_len, "%s", current);
            return true;
        }

        char *last_slash = strrchr(current, '/');
#ifdef _WIN32
        char *last_bslash = strrchr(current, '\\');
        if (last_bslash && (!last_slash || last_bslash > last_slash)) {
            last_slash = last_bslash;
        }
#endif
        if (!last_slash || last_slash == current) {
            break;
        }
        *last_slash = '\0';
    }
    return false;
}

static bool GetExecutableDir(char *out_dir, size_t max_len) {
    out_dir[0] = '\0';
#ifdef _WIN32
    char exe_path[1024] = "";
    if (GetModuleFileNameA(NULL, exe_path, sizeof(exe_path)) > 0) {
        char *last_slash = strrchr(exe_path, '\\');
        if (!last_slash) last_slash = strrchr(exe_path, '/');
        if (last_slash) {
            *last_slash = '\0';
            snprintf(out_dir, max_len, "%s", exe_path);
            return true;
        }
    }
#else
    char exe_path[1024] = "";
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len > 0) {
        exe_path[len] = '\0';
        char *last_slash = strrchr(exe_path, '/');
        if (last_slash) {
            *last_slash = '\0';
            snprintf(out_dir, max_len, "%s", exe_path);
            return true;
        }
    }
#endif
    return false;
}

static void FindRepoRoot(char *out_repo, size_t max_len) {
    char found[1024] = "";

    // 1. Walk up from current working directory
    if (WalkUpForGit(".", found, sizeof(found))) {
        snprintf(out_repo, max_len, "%s", found);
        return;
    }

    // 2. Walk up from executable directory
    char exe_dir[1024] = "";
    if (GetExecutableDir(exe_dir, sizeof(exe_dir))) {
        if (WalkUpForGit(exe_dir, found, sizeof(found))) {
            snprintf(out_repo, max_len, "%s", found);
            return;
        }
    }

    // 3. Fallback: ask git rev-parse (if git is available)
    char buf[512] = "";
    if (ExecuteGit(".", "rev-parse --show-toplevel", buf, sizeof(buf))) {
        char *trimmed = TrimWhitespace(buf);
        if (trimmed && strlen(trimmed) > 0) {
            snprintf(out_repo, max_len, "%s", trimmed);
            return;
        }
    }

    if (strlen(exe_dir) > 0 && ExecuteGit(exe_dir, "rev-parse --show-toplevel", buf, sizeof(buf))) {
        char *trimmed = TrimWhitespace(buf);
        if (trimmed && strlen(trimmed) > 0) {
            snprintf(out_repo, max_len, "%s", trimmed);
            return;
        }
    }

    // 4. Ultimate fallback
    if (strlen(exe_dir) > 0) {
        snprintf(out_repo, max_len, "%s", exe_dir);
    } else {
        snprintf(out_repo, max_len, ".");
    }
}

static void GetGitHubBaseUrl(const char *repo_dir, char *out_url, size_t max_len) {
    out_url[0] = '\0';
    char raw[512] = "";
    if (!ExecuteGit(repo_dir, "remote get-url origin", raw, sizeof(raw))) {
        ExecuteGit(repo_dir, "config --get remote.origin.url", raw, sizeof(raw));
    }
    char *url = TrimWhitespace(raw);
    if (!url || strlen(url) == 0) return;

    // SSH formát: git@github.com:Owner/Repo.git
    char owner[128] = "", repo[128] = "";
    if (sscanf(url, "git@github.com:%127[^/]/%127s", owner, repo) == 2) {
        char *dot = strstr(repo, ".git");
        if (dot) *dot = '\0';
        snprintf(out_url, max_len, "https://github.com/%s/%s", owner, repo);
        return;
    }

    // HTTPS formát: https://github.com/Owner/Repo.git
    if (StrCaseContains(url, "github.com/")) {
        char *after = strstr(url, "github.com/");
        if (after) {
            after += strlen("github.com/");
            if (sscanf(after, "%127[^/]/%127s", owner, repo) == 2) {
                char *dot = strstr(repo, ".git");
                if (dot) *dot = '\0';
                snprintf(out_url, max_len, "https://github.com/%s/%s", owner, repo);
                return;
            }
        }
    }

    // Jiné URL bez .git
    strncpy(out_url, url, max_len - 1);
    char *dot = strstr(out_url, ".git");
    if (dot) *dot = '\0';
}

// -----------------------------------------------------------------------------
// PARSOVÁNÍ SOUBORU TASK.MD
// -----------------------------------------------------------------------------
static void ParseTaskMd(const char *content, const char *branch_name, const char *github_base, Task *task) {
    memset(task, 0, sizeof(Task));
    strncpy(task->branch, branch_name, sizeof(task->branch) - 1);
    strcpy(task->name, branch_name); // Výchozí název odpovídá větvi

    if (github_base && strlen(github_base) > 0) {
        snprintf(task->github_url, sizeof(task->github_url), "%s/tree/%s", github_base, branch_name);
    } else {
        snprintf(task->github_url, sizeof(task->github_url), "#branch-%s", branch_name);
    }

    enum {
        SEC_NONE = 0,
        SEC_NAME,
        SEC_DESCRIPTION,
        SEC_TASK_MASTER,
        SEC_WORKER,
        SEC_STATUS,
        SEC_DEADLINE,
        SEC_PARENT,
        SEC_SUBTASKS,
        SEC_REQUIRED
    } cur_sec = SEC_NONE;

    bool worker_sec_present = false;
    bool worker_has_free_dash = false;
    int worker_lines_count = 0;

    const char *p = content;
    char line[512];

    while (*p) {
        size_t idx = 0;
        while (*p && *p != '\n' && *p != '\r' && idx < sizeof(line) - 1) {
            line[idx++] = *p++;
        }
        line[idx] = '\0';
        while (*p == '\r' || *p == '\n') p++;

        char *t = TrimWhitespace(line);
        if (strlen(t) == 0) continue;

        // Hlavičky sekcí (# SECTION)
        if (t[0] == '#') {
            char *hdr = TrimWhitespace(t + 1);
            if (strncasecmp(hdr, "NAME", 4) == 0) cur_sec = SEC_NAME;
            else if (strncasecmp(hdr, "DESCRIPTION", 11) == 0) cur_sec = SEC_DESCRIPTION;
            else if (strncasecmp(hdr, "TASK MASTER", 11) == 0) cur_sec = SEC_TASK_MASTER;
            else if (strncasecmp(hdr, "WORKER", 6) == 0) {
                cur_sec = SEC_WORKER;
                worker_sec_present = true;
            }
            else if (strncasecmp(hdr, "STATUS", 6) == 0) cur_sec = SEC_STATUS;
            else if (strncasecmp(hdr, "DEADLINE", 8) == 0) cur_sec = SEC_DEADLINE;
            else if (strncasecmp(hdr, "PARENT", 6) == 0) cur_sec = SEC_PARENT;
            else if (strncasecmp(hdr, "SUBTASK", 7) == 0) cur_sec = SEC_SUBTASKS;
            else if (strncasecmp(hdr, "REQUIRED", 8) == 0) cur_sec = SEC_REQUIRED;
            else cur_sec = SEC_NONE;
            continue;
        }

        // Obsah sekce
        switch (cur_sec) {
            case SEC_NAME:
                if (!StrCaseContains(t, "placeholder")) {
                    StripDiacritics(t, task->name, sizeof(task->name));
                }
                break;
            case SEC_TASK_MASTER:
                if (!StrCaseContains(t, "placeholder") && strcmp(t, "...") != 0 && strcmp(t, "---") != 0) {
                    if (strlen(task->task_master) == 0) {
                        StripDiacritics(t, task->task_master, sizeof(task->task_master));
                    }
                }
                break;
            case SEC_WORKER:
                worker_lines_count++;
                if (strcmp(t, "---") == 0) {
                    worker_has_free_dash = true;
                } else if (!StrCaseContains(t, "placeholder") && strcmp(t, "...") != 0) {
                    if (task->worker_count < MAX_WORKERS) {
                        StripDiacritics(t, task->workers[task->worker_count++], MAX_STR);
                    }
                }
                break;
            case SEC_STATUS:
                if (strlen(task->status) == 0) {
                    strncpy(task->status, t, sizeof(task->status) - 1);
                }
                break;
            case SEC_DEADLINE:
                if (!task->has_deadline) {
                    strncpy(task->deadline_raw, t, sizeof(task->deadline_raw) - 1);
                    int y, m, d;
                    if (ParseDeadline(t, &y, &m, &d)) {
                        task->deadline_year = y;
                        task->deadline_month = m;
                        task->deadline_day = d;
                        task->has_deadline = true;
                    }
                }
                break;
            case SEC_SUBTASKS:
                if (!StrCaseContains(t, "placeholder") && strcmp(t, "...") != 0) {
                    if (strstr(t, "[") && strstr(t, "]")) {
                        task->has_subtasks = true;
                    }
                }
                break;
            default:
                break;
        }
    }

    // Vyhodnocení:
    // 1. Task Master je volný, pokud je pole prázdné
    task->is_task_master_free = (strlen(task->task_master) == 0);

    // 2. Sekce WORKER (vytváří ji pouze Task Master, pokud je potřeba)
    task->requires_worker = worker_sec_present;
    if (worker_sec_present) {
        // Volná pozice se označuje "---" nebo prázdnou sekcí
        if (worker_has_free_dash || worker_lines_count == 0 || (task->worker_count == 0 && worker_lines_count > 0)) {
            task->is_worker_free = true;
        } else {
            task->is_worker_free = false;
        }
    } else {
        task->is_worker_free = false;
    }

    // 3. Status je "available"
    task->is_available = (strcasecmp(task->status, "available") == 0);

    // 4. Dny do deadline
    if (task->has_deadline) {
        task->days_to_deadline = CalculateDaysToDeadline(task->deadline_year, task->deadline_month, task->deadline_day);
    } else {
        task->days_to_deadline = 999999;
    }

    // 5. Výpočet skóre pomocí funkce z score.h
    task->score = CalculateScore(task->days_to_deadline);
}

// Načtení všech tasků napříč větvemi
static void LoadAllTasks(const char *repo_dir, const char *github_base, TaskList *list) {
    list->count = 0;

    char branches_raw[1024 * 32] = "";
    ExecuteGit(repo_dir, "for-each-ref --format=\"%(refname)\" refs/heads/ refs/remotes/", branches_raw, sizeof(branches_raw));

    char seen_branches[MAX_TASKS][MAX_STR];
    int seen_count = 0;

    char *line = strtok(branches_raw, "\r\n");
    while (line && list->count < MAX_TASKS) {
        char *t = TrimWhitespace(line);
        size_t t_len = strlen(t);

        // Přeskočit pouze symbolický odkaz HEAD (např. refs/remotes/origin/HEAD)
        bool is_head_symref = false;
        if (t_len >= 5 && strcmp(t + t_len - 5, "/HEAD") == 0) {
            is_head_symref = true;
        }

        if (t_len > 0 && !is_head_symref) {
            char branch_clean[MAX_STR] = "";
            if (strncmp(t, "refs/heads/", 11) == 0) {
                strncpy(branch_clean, t + 11, sizeof(branch_clean) - 1);
            } else if (strncmp(t, "refs/remotes/", 13) == 0) {
                // Přeskočit např. "refs/remotes/origin/"
                char *slash = strchr(t + 13, '/');
                if (slash) {
                    strncpy(branch_clean, slash + 1, sizeof(branch_clean) - 1);
                } else {
                    strncpy(branch_clean, t + 13, sizeof(branch_clean) - 1);
                }
            }

            // Deduplikace větví
            bool seen = false;
            for (int i = 0; i < seen_count; i++) {
                if (strcmp(seen_branches[i], branch_clean) == 0) {
                    seen = true;
                    break;
                }
            }

            if (!seen && strlen(branch_clean) > 0 && seen_count < MAX_TASKS) {
                int s_idx = seen_count++;
                snprintf(seen_branches[s_idx], sizeof(seen_branches[s_idx]), "%s", branch_clean);

                char show_cmd[512];
                snprintf(show_cmd, sizeof(show_cmd), "show \"%s:TASK.md\"", t);

                char *file_content = (char *)malloc(MAX_CONTENT);
                if (file_content) {
                    file_content[0] = '\0';
                    if (ExecuteGit(repo_dir, show_cmd, file_content, MAX_CONTENT)) {
                        ParseTaskMd(file_content, branch_clean, github_base, &list->items[list->count++]);
                    }
                    free(file_content);
                }
            }
        }
        line = strtok(NULL, "\r\n");
    }

    // Pokud v Gitu nic nebylo (čerstvý repo před commitem), zkusíme lokální TASK.md
    if (list->count == 0) {
        char local_path[512];
        snprintf(local_path, sizeof(local_path), "%s/TASK.md", repo_dir);
        FILE *f = fopen(local_path, "r");
        if (f) {
            char *file_content = (char *)malloc(MAX_CONTENT);
            if (file_content) {
                size_t n = fread(file_content, 1, MAX_CONTENT - 1, f);
                file_content[n] = '\0';
                ParseTaskMd(file_content, "master", github_base, &list->items[list->count++]);
                free(file_content);
            }
            fclose(f);
        }
    }
}

// -----------------------------------------------------------------------------
// KONTROLA AKTIVNÍHO ÚKOLU A TŘÍDĚNÍ VOLNÝCH ÚKOLŮ
// -----------------------------------------------------------------------------
static int CompareTasksDescending(const void *a, const void *b) {
    const Task *ta = (const Task *)a;
    const Task *tb = (const Task *)b;
    if (tb->score > ta->score) return 1;
    if (tb->score < ta->score) return -1;
    return 0;
}

// -----------------------------------------------------------------------------
// TERMINAL MODE (CLI FALLBACK)
// -----------------------------------------------------------------------------
static void RunCliMode(TaskList *all_tasks, const char *initial_user) {
    char user[MAX_STR] = "";
    if (initial_user && strlen(initial_user) > 0) {
        strncpy(user, initial_user, sizeof(user) - 1);
    } else {
        printf("\n============================================================\n");
        printf("           TASK FINDER - TASK SELECTION\n");
        printf("============================================================\n");
        printf("Enter your name: ");
        if (!fgets(user, sizeof(user), stdin)) return;
    }
    char *user_name = TrimWhitespace(user);
    if (strlen(user_name) == 0) {
        printf("No name entered.\n");
        return;
    }

    // 1. Check active task
    int active_count = 0;
    static Task active_tasks[MAX_TASKS];
    static char active_roles[MAX_TASKS][64];

    for (int i = 0; i < all_tasks->count; i++) {
        Task *t = &all_tasks->items[i];
        bool has_active = false;
        char role[64] = "";

        for (int w = 0; w < t->worker_count; w++) {
            if (NamesEqual(t->workers[w], user_name)) {
                has_active = true;
                strcpy(role, "WORKER");
                break;
            }
        }

        if (!has_active && strlen(t->task_master) > 0) {
            if (NamesEqual(t->task_master, user_name) && !t->has_subtasks) {
                has_active = true;
                strcpy(role, "TASK MASTER (Direct Execution)");
            }
        }

        if (has_active) {
            active_tasks[active_count] = *t;
            strcpy(active_roles[active_count], role);
            active_count++;
        }
    }

    if (active_count > 0) {
        printf("\n============================================================\n");
        printf("  ACTIVE TASK ASSIGNED!\n");
        printf("============================================================\n");
        for (int i = 0; i < active_count; i++) {
            Task *t = &active_tasks[i];
            printf("• Task:       %s\n", t->name);
            printf("  Your Role:  %s\n", active_roles[i]);
            printf("  Branch:     %s\n", t->branch);
            printf("  GitHub:     %s\n", t->github_url);
            if (t->has_deadline) {
                if (t->days_to_deadline > 0)
                    printf("  Deadline:   %04d/%02d/%02d (%d days left)\n", t->deadline_year, t->deadline_month, t->deadline_day, t->days_to_deadline);
                else if (t->days_to_deadline == 0)
                    printf("  Deadline:   TODAY!\n");
                else
                    printf("  Deadline:   %04d/%02d/%02d (OVERDUE: %d days!)\n", t->deadline_year, t->deadline_month, t->deadline_day, -t->days_to_deadline);
            } else {
                printf("  Deadline:   None\n");
            }
            printf("------------------------------------------------------------\n");
        }
        printf("Complete your active task before picking new tasks.\n\n");
        return;
    }

    // 2. No active task -> find available free tasks
    static Task free_tasks[MAX_TASKS];
    int free_count = 0;

    for (int i = 0; i < all_tasks->count; i++) {
        Task *t = &all_tasks->items[i];
        if (!t->is_available) continue;

        bool is_free = t->is_task_master_free || (t->requires_worker && t->is_worker_free);
        if (is_free) {
            free_tasks[free_count++] = *t;
        }
    }

    if (free_count == 0) {
        printf("\nNo available tasks found.\n\n");
        return;
    }

    qsort(free_tasks, free_count, sizeof(Task), CompareTasksDescending);

    printf("\n============================================================\n");
    printf("  NO ACTIVE TASK - AVAILABLE TASKS\n");
    printf("============================================================\n");
    printf("Found %d available tasks (sorted by score descending):\n\n", free_count);

    for (int i = 0; i < free_count; i++) {
        Task *t = &free_tasks[i];
        printf("%d. %s  [Score: %.1f]\n", i + 1, t->name, t->score);
        printf("   • Open role:     ");
        if (t->is_task_master_free && (t->requires_worker && t->is_worker_free)) {
            printf("TASK MASTER & WORKER\n");
        } else if (t->is_task_master_free) {
            printf("TASK MASTER\n");
        } else {
            printf("WORKER\n");
        }
        printf("   • Git branch:    %s\n", t->branch);
        printf("   • GitHub link:   %s\n", t->github_url);
        if (t->has_deadline) {
            if (t->days_to_deadline > 0)
                printf("   • Deadline:      %04d/%02d/%02d (%d days left)\n", t->deadline_year, t->deadline_month, t->deadline_day, t->days_to_deadline);
            else if (t->days_to_deadline == 0)
                printf("   • Deadline:      TODAY!\n");
            else
                printf("   • Deadline:      %04d/%02d/%02d (OVERDUE: %d days!)\n", t->deadline_year, t->deadline_month, t->deadline_day, -t->days_to_deadline);
        } else {
            printf("   • Deadline:      None\n");
        }
        printf("------------------------------------------------------------\n");
    }
    printf("\n");
}

// -----------------------------------------------------------------------------
// HLAVNÍ GRAFICKÉ ROZHRANÍ (RAYLIB)
// -----------------------------------------------------------------------------
int main(int argc, char **argv) {
    char repo_dir[512] = "";
    bool force_cli = false;
    const char *cli_user = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--cli") == 0 || strcmp(argv[i], "-c") == 0) {
            force_cli = true;
        } else if ((strcmp(argv[i], "--repo") == 0 || strcmp(argv[i], "-r") == 0) && i + 1 < argc) {
            strncpy(repo_dir, argv[++i], sizeof(repo_dir) - 1);
        } else if (argv[i][0] != '-') {
            cli_user = argv[i];
            force_cli = true;
        }
    }

    DetectGitBinary();

    if (strlen(repo_dir) == 0) {
        FindRepoRoot(repo_dir, sizeof(repo_dir));
    }

    char github_base[MAX_URL] = "";
    GetGitHubBaseUrl(repo_dir, github_base, sizeof(github_base));

    static TaskList all_tasks;
    LoadAllTasks(repo_dir, github_base, &all_tasks);

    // Write diagnostic log for easy troubleshooting
    FILE *log_f = fopen("task_finder_log.txt", "w");
    if (log_f) {
        fprintf(log_f, "Git binary: %s (available: %s)\n", g_git_cmd, g_git_available ? "YES" : "NO");
        fprintf(log_f, "Detected repo: %s\n", repo_dir);
        fprintf(log_f, "Found tasks: %d\n", all_tasks.count);
        for (int i = 0; i < all_tasks.count; i++) {
            fprintf(log_f, "  [%d] Branch: %s, Name: %s, Status: %s, TM: %s, Available: %s\n",
                    i + 1, all_tasks.items[i].branch, all_tasks.items[i].name,
                    all_tasks.items[i].status, all_tasks.items[i].task_master,
                    all_tasks.items[i].is_available ? "YES" : "NO");
        }
        fclose(log_f);
    }

    if (force_cli) {
        RunCliMode(&all_tasks, cli_user);
        return 0;
    }

    const int screenWidth = 920;
    const int screenHeight = 640;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(screenWidth, screenHeight, "Task Finder - Horror Project");

    if (!IsWindowReady()) {
        printf("[INFO] Graphical window could not be opened, falling back to CLI mode...\n");
        FILE *err_f = fopen("task_finder_error.log", "w");
        if (err_f) {
            fprintf(err_f, "Graphical window failed to initialize (OpenGL/driver issue). Running in fallback mode.\n");
            fclose(err_f);
        }
        RunCliMode(&all_tasks, cli_user);
        return 0;
    }

    SetTargetFPS(60);

    AppScreen current_screen = SCREEN_LOGIN;
    char username_input[MAX_STR] = "";
    int username_len = 0;

    // Nalezené aktivní úkoly uživatele
    static Task active_tasks[MAX_TASKS];
    static char active_roles[MAX_TASKS][64];
    int active_task_count = 0;

    // Volné úkoly
    static Task free_tasks[MAX_TASKS];
    int free_task_count = 0;

    float scroll_y = 0.0f;

    // Barvy UI
    Color colBg        = (Color){ 20, 22, 28, 255 };
    Color colCard      = (Color){ 32, 36, 46, 255 };
    Color colCardHover = (Color){ 42, 48, 62, 255 };
    Color colAccent    = (Color){ 52, 120, 246, 255 };
    Color colAccentHov = (Color){ 70, 140, 255, 255 };
    Color colGreen     = (Color){ 40, 167, 69, 255 };
    Color colCyan      = (Color){ 23, 162, 184, 255 };
    Color colGold      = (Color){ 245, 175, 25, 255 };
    Color colRed       = (Color){ 220, 53, 69, 255 };

    while (!WindowShouldClose()) {
        // Drag & Drop repo directory support
        if (IsFileDropped()) {
            FilePathList dropped = LoadDroppedFiles();
            if (dropped.count > 0) {
                char new_repo[512] = "";
                if (WalkUpForGit(dropped.paths[0], new_repo, sizeof(new_repo))) {
                    strncpy(repo_dir, new_repo, sizeof(repo_dir) - 1);
                } else {
                    strncpy(repo_dir, dropped.paths[0], sizeof(repo_dir) - 1);
                }
                repo_dir[sizeof(repo_dir) - 1] = '\0';
                GetGitHubBaseUrl(repo_dir, github_base, sizeof(github_base));
                LoadAllTasks(repo_dir, github_base, &all_tasks);
                current_screen = SCREEN_LOGIN;
            }
            UnloadDroppedFiles(dropped);
        }

        // Logika obrazovek
        if (current_screen == SCREEN_LOGIN) {
            int key = GetCharPressed();
            while (key > 0) {
                if ((key >= 32) && (key <= 125) && (username_len < MAX_STR - 1)) {
                    username_input[username_len++] = (char)key;
                    username_input[username_len] = '\0';
                }
                key = GetCharPressed();
            }

            if (IsKeyPressed(KEY_BACKSPACE)) {
                if (username_len > 0) {
                    username_input[--username_len] = '\0';
                }
            }

            bool submit = IsKeyPressed(KEY_ENTER);
            Rectangle btnCheck = { screenWidth / 2.0f - 110, 360, 220, 44 };
            Vector2 mouse = GetMousePosition();
            bool btnHover = CheckCollisionPointRec(mouse, btnCheck);

            if (btnHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                submit = true;
            }

            if (submit && username_len > 0) {
                // Vyhodnocení
                active_task_count = 0;
                free_task_count = 0;
                scroll_y = 0.0f;

                // 1. Kontrola, zda má člověk aktivní úkol
                for (int i = 0; i < all_tasks.count; i++) {
                    Task *t = &all_tasks.items[i];
                    bool has_active = false;
                    char role[64] = "";

                    // Je workerem?
                    for (int w = 0; w < t->worker_count; w++) {
                        if (NamesEqual(t->workers[w], username_input)) {
                            has_active = true;
                            strcpy(role, "WORKER");
                            break;
                        }
                    }

                    // Je Task Masterem bez subtasku?
                    if (!has_active && strlen(t->task_master) > 0) {
                        if (NamesEqual(t->task_master, username_input) && !t->has_subtasks) {
                            has_active = true;
                            strcpy(role, "TASK MASTER (Direct Execution)");
                        }
                    }

                    if (has_active) {
                        active_tasks[active_task_count] = *t;
                        strcpy(active_roles[active_task_count], role);
                        active_task_count++;
                    }
                }

                if (active_task_count > 0) {
                    current_screen = SCREEN_ACTIVE_TASK;
                } else {
                    // 2. Nemá aktivní task -> najdeme volné tasky
                    for (int i = 0; i < all_tasks.count; i++) {
                        Task *t = &all_tasks.items[i];
                        if (!t->is_available) continue;

                        bool is_free = t->is_task_master_free || (t->requires_worker && t->is_worker_free);
                        if (is_free) {
                            free_tasks[free_task_count++] = *t;
                        }
                    }

                    // Seřadíme sestupně podle skóre
                    qsort(free_tasks, free_task_count, sizeof(Task), CompareTasksDescending);
                    current_screen = SCREEN_FREE_TASKS;
                }
            }
        } else {
            // Scrollování myší
            float wheel = GetMouseWheelMove();
            scroll_y += wheel * 35.0f;
            if (scroll_y > 0.0f) scroll_y = 0.0f;
        }

        // VYKRESLOVÁNÍ
        BeginDrawing();
        ClearBackground(colBg);

        if (current_screen == SCREEN_LOGIN) {
            // LOGIN SCREEN
            DrawText("TASK FINDER", screenWidth / 2 - MeasureText("TASK FINDER", 34) / 2, 130, 34, RAYWHITE);
            DrawText("Git Branch Task Management & Finder", screenWidth / 2 - MeasureText("Git Branch Task Management & Finder", 16) / 2, 175, 16, LIGHTGRAY);

            Rectangle cardRec = { screenWidth / 2.0f - 240, 220, 480, 210 };
            DrawRectangleRounded(cardRec, 0.08f, 6, colCard);

            DrawText("Enter your name:", cardRec.x + 30, cardRec.y + 25, 18, RAYWHITE);

            Rectangle inputRec = { cardRec.x + 30, cardRec.y + 60, cardRec.width - 60, 42 };
            DrawRectangleRounded(inputRec, 0.1f, 4, (Color){ 22, 24, 30, 255 });
            DrawRectangleRoundedLines(inputRec, 0.1f, 4, colAccent);

            if (username_len == 0) {
                DrawText("e.g. Pepa, Jan...", (int)inputRec.x + 12, (int)inputRec.y + 12, 18, DARKGRAY);
            } else {
                DrawText(username_input, (int)inputRec.x + 12, (int)inputRec.y + 12, 18, RAYWHITE);
            }

            // Cursor blinking
            if (((int)(GetTime() * 2)) % 2 == 0) {
                int txtW = MeasureText(username_input, 18);
                DrawRectangle((int)inputRec.x + 14 + txtW, (int)inputRec.y + 10, 2, 22, colAccent);
            }

            Rectangle btnCheck = { screenWidth / 2.0f - 110, cardRec.y + 130, 220, 44 };
            Vector2 mouse = GetMousePosition();
            bool btnHover = CheckCollisionPointRec(mouse, btnCheck);
            DrawRectangleRounded(btnCheck, 0.15f, 4, btnHover ? colAccentHov : colAccent);
            const char *btnTxt = "Check Tasks";
            DrawText(btnTxt, (int)(btnCheck.x + btnCheck.width / 2 - MeasureText(btnTxt, 18) / 2), (int)(btnCheck.y + 13), 18, RAYWHITE);

            // Repository info at bottom
            char info_buf[512];
            if (!g_git_available) {
                snprintf(info_buf, sizeof(info_buf), "Git executable not found in PATH or standard install locations!");
                DrawText(info_buf, screenWidth / 2 - MeasureText(info_buf, 14) / 2, screenHeight - 35, 14, (Color){ 220, 80, 80, 255 });
            } else if (all_tasks.count > 0) {
                snprintf(info_buf, sizeof(info_buf), "Repository: %s (%d task branches)", repo_dir, all_tasks.count);
                DrawText(info_buf, screenWidth / 2 - MeasureText(info_buf, 14) / 2, screenHeight - 35, 14, DARKGRAY);
            } else {
                snprintf(info_buf, sizeof(info_buf), "Repository: %s (0 task branches)  [Tip: Drag & drop folder here]", repo_dir);
                DrawText(info_buf, screenWidth / 2 - MeasureText(info_buf, 14) / 2, screenHeight - 35, 14, (Color){ 220, 150, 50, 255 });
            }

        } else if (current_screen == SCREEN_ACTIVE_TASK) {
            // SCREEN: ACTIVE TASK ASSIGNED
            Rectangle topBar = { 0, 0, (float)screenWidth, 65 };
            DrawRectangleRec(topBar, (Color){ 26, 30, 38, 255 });
            DrawText("TASK FINDER", 25, 20, 24, RAYWHITE);

            // Button: Change User
            Rectangle btnBack = { screenWidth - 210.0f, 15, 185, 36 };
            Vector2 mouse = GetMousePosition();
            bool backHov = CheckCollisionPointRec(mouse, btnBack);
            DrawRectangleRounded(btnBack, 0.15f, 4, backHov ? colCardHover : colCard);
            DrawText("Change User", (int)(btnBack.x + btnBack.width / 2 - MeasureText("Change User", 16) / 2), (int)btnBack.y + 10, 16, RAYWHITE);
            if (backHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                current_screen = SCREEN_LOGIN;
            }

            float curY = 90 + scroll_y;

            // Warning title
            DrawText("ACTIVE TASK ASSIGNED", 40, (int)curY, 22, colGold);
            curY += 35;
            DrawText("Complete your active task before picking new tasks.", 40, (int)curY, 15, LIGHTGRAY);
            curY += 35;

            for (int i = 0; i < active_task_count; i++) {
                Task *t = &active_tasks[i];
                Rectangle card = { 40, curY, (float)screenWidth - 80, 160 };
                DrawRectangleRounded(card, 0.05f, 6, colCard);

                // Name
                DrawText(t->name, (int)card.x + 25, (int)card.y + 20, 22, RAYWHITE);

                // Role tag
                Rectangle roleTag = { card.x + 25, card.y + 55, (float)MeasureText(active_roles[i], 14) + 16, 24 };
                DrawRectangleRounded(roleTag, 0.3f, 4, (Color){ 200, 130, 10, 255 });
                DrawText(active_roles[i], (int)roleTag.x + 8, (int)roleTag.y + 5, 14, RAYWHITE);

                // Branch
                char branchTxt[256];
                snprintf(branchTxt, sizeof(branchTxt), "Branch: %s", t->branch);
                DrawText(branchTxt, (int)roleTag.x + (int)roleTag.width + 15, (int)card.y + 58, 15, LIGHTGRAY);

                // Deadline
                char dlTxt[256];
                if (t->has_deadline) {
                    if (t->days_to_deadline > 0) {
                        snprintf(dlTxt, sizeof(dlTxt), "Deadline: %04d/%02d/%02d (%d days left)", t->deadline_year, t->deadline_month, t->deadline_day, t->days_to_deadline);
                        DrawText(dlTxt, (int)card.x + 25, (int)card.y + 100, 16, (Color){ 200, 200, 200, 255 });
                    } else if (t->days_to_deadline == 0) {
                        DrawText("Deadline: TODAY!", (int)card.x + 25, (int)card.y + 100, 16, colRed);
                    } else {
                        snprintf(dlTxt, sizeof(dlTxt), "Deadline: %04d/%02d/%02d (OVERDUE: %d days!)", t->deadline_year, t->deadline_month, t->deadline_day, -t->days_to_deadline);
                        DrawText(dlTxt, (int)card.x + 25, (int)card.y + 100, 16, colRed);
                    }
                } else {
                    DrawText("Deadline: None", (int)card.x + 25, (int)card.y + 100, 16, DARKGRAY);
                }

                // Button: Open on GitHub
                Rectangle btnGit = { card.x + card.width - 230, card.y + card.height / 2 - 20, 205, 42 };
                bool gitHov = CheckCollisionPointRec(mouse, btnGit);
                DrawRectangleRounded(btnGit, 0.15f, 4, gitHov ? colAccentHov : colAccent);
                const char *gtTxt = "Open on GitHub";
                DrawText(gtTxt, (int)(btnGit.x + btnGit.width / 2 - MeasureText(gtTxt, 16) / 2), (int)btnGit.y + 12, 16, RAYWHITE);

                if (gitHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    OpenURL(t->github_url);
                }

                curY += card.height + 20;
            }

        } else if (current_screen == SCREEN_FREE_TASKS) {
            // SCREEN: AVAILABLE FREE TASKS
            Rectangle topBar = { 0, 0, (float)screenWidth, 65 };
            DrawRectangleRec(topBar, (Color){ 26, 30, 38, 255 });
            DrawText("TASK FINDER", 25, 20, 24, RAYWHITE);

            // Button: Change User
            Rectangle btnBack = { screenWidth - 210.0f, 15, 185, 36 };
            Vector2 mouse = GetMousePosition();
            bool backHov = CheckCollisionPointRec(mouse, btnBack);
            DrawRectangleRounded(btnBack, 0.15f, 4, backHov ? colCardHover : colCard);
            DrawText("Change User", (int)(btnBack.x + btnBack.width / 2 - MeasureText("Change User", 16) / 2), (int)btnBack.y + 10, 16, RAYWHITE);
            if (backHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                current_screen = SCREEN_LOGIN;
            }

            float curY = 85 + scroll_y;

            DrawText("NO ACTIVE TASK", 40, (int)curY, 22, colGreen);
            curY += 32;

            char subTxt[256];
            snprintf(subTxt, sizeof(subTxt), "Available tasks (%d) sorted by score (descending):", free_task_count);
            DrawText(subTxt, 40, (int)curY, 15, LIGHTGRAY);
            curY += 35;

            if (free_task_count == 0) {
                DrawText("No available tasks found.", 40, (int)curY + 20, 18, DARKGRAY);
            }

            for (int i = 0; i < free_task_count; i++) {
                Task *t = &free_tasks[i];
                Rectangle card = { 40, curY, (float)screenWidth - 80, 115 };
                bool cardHover = CheckCollisionPointRec(mouse, card);
                DrawRectangleRounded(card, 0.06f, 6, cardHover ? colCardHover : colCard);

                // Index and title
                char title[MAX_STR];
                snprintf(title, sizeof(title), "%d. %s", i + 1, t->name);
                DrawText(title, (int)card.x + 20, (int)card.y + 15, 20, RAYWHITE);

                // Open role tags
                float tagX = card.x + 20;
                if (t->is_task_master_free) {
                    const char *tagTxt = "OPEN: TASK MASTER";
                    int tagW = MeasureText(tagTxt, 13) + 16;
                    Rectangle tagRec = { tagX, card.y + 45, (float)tagW, 22 };
                    DrawRectangleRounded(tagRec, 0.3f, 4, colCyan);
                    DrawText(tagTxt, (int)tagRec.x + 8, (int)tagRec.y + 4, 13, RAYWHITE);
                    tagX += tagW + 10;
                }
                if (t->requires_worker && t->is_worker_free) {
                    const char *tagTxt = "OPEN: WORKER";
                    int tagW = MeasureText(tagTxt, 13) + 16;
                    Rectangle tagRec = { tagX, card.y + 45, (float)tagW, 22 };
                    DrawRectangleRounded(tagRec, 0.3f, 4, colGreen);
                    DrawText(tagTxt, (int)tagRec.x + 8, (int)tagRec.y + 4, 13, RAYWHITE);
                    tagX += tagW + 10;
                }

                // Branch
                char branchTxt[256];
                snprintf(branchTxt, sizeof(branchTxt), "Branch: %s", t->branch);
                DrawText(branchTxt, (int)tagX + 5, (int)card.y + 48, 14, LIGHTGRAY);

                // Bottom row: Deadline and Score
                char dlBuf[256];
                if (t->has_deadline) {
                    if (t->days_to_deadline > 0) {
                        snprintf(dlBuf, sizeof(dlBuf), "Deadline: %04d/%02d/%02d (%d days left)", t->deadline_year, t->deadline_month, t->deadline_day, t->days_to_deadline);
                        DrawText(dlBuf, (int)card.x + 20, (int)card.y + 80, 14, (Color){ 200, 200, 200, 255 });
                    } else if (t->days_to_deadline == 0) {
                        DrawText("Deadline: TODAY!", (int)card.x + 20, (int)card.y + 80, 14, colRed);
                    } else {
                        snprintf(dlBuf, sizeof(dlBuf), "Deadline: %04d/%02d/%02d (OVERDUE: %d days!)", t->deadline_year, t->deadline_month, t->deadline_day, -t->days_to_deadline);
                        DrawText(dlBuf, (int)card.x + 20, (int)card.y + 80, 14, colRed);
                    }
                } else {
                    DrawText("Deadline: None", (int)card.x + 20, (int)card.y + 80, 14, DARKGRAY);
                }

                char scoreBuf[64];
                snprintf(scoreBuf, sizeof(scoreBuf), "Score: %.1f", t->score);
                DrawText(scoreBuf, (int)card.x + 360, (int)card.y + 80, 14, colGold);

                // Button: Open on GitHub
                Rectangle btnGit = { card.x + card.width - 210, card.y + card.height / 2 - 19, 190, 38 };
                bool gitHov = CheckCollisionPointRec(mouse, btnGit);
                DrawRectangleRounded(btnGit, 0.15f, 4, gitHov ? colAccentHov : colAccent);
                const char *gtTxt = "Open on GitHub";
                DrawText(gtTxt, (int)(btnGit.x + btnGit.width / 2 - MeasureText(gtTxt, 15) / 2), (int)btnGit.y + 11, 15, RAYWHITE);

                if (gitHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    OpenURL(t->github_url);
                }

                curY += card.height + 15;
            }
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
