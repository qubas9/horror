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
    #define GET_EXIT_CODE(s) (s)
#else
    #include <unistd.h>
    #include <strings.h>
    #include <sys/wait.h>
    #define NULL_DEV "2>/dev/null"
    #define GET_EXIT_CODE(s) (WIFEXITED(s) ? WEXITSTATUS(s) : (s))
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
    char description[512];
    float score;
} Task;

typedef struct {
    Task items[MAX_TASKS];
    int count;
} TaskList;

typedef enum {
    SCREEN_LOGIN = 0,
    SCREEN_ACTIVE_TASK,
    SCREEN_FREE_TASKS,
    SCREEN_CREATE_PROJECT,
    SCREEN_MANAGE_TASK
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

static int RunGit(const char *repo_dir, const char *args, char *output, size_t max_len) {
    char cmd[2048];
    if (repo_dir && strlen(repo_dir) > 0) {
#ifdef _WIN32
        snprintf(cmd, sizeof(cmd), "\"\"%s\" -c safe.directory=* -C \"%s\" %s\" 2>&1", g_git_cmd, repo_dir, args);
#else
        snprintf(cmd, sizeof(cmd), "\"%s\" -C \"%s\" %s 2>&1", g_git_cmd, repo_dir, args);
#endif
    } else {
#ifdef _WIN32
        snprintf(cmd, sizeof(cmd), "\"\"%s\" -c safe.directory=* %s\" 2>&1", g_git_cmd, args);
#else
        snprintf(cmd, sizeof(cmd), "\"%s\" %s 2>&1", g_git_cmd, args);
#endif
    }

    FILE *pipe = popen(cmd, "r");
    if (!pipe) {
        if (output && max_len > 0) output[0] = '\0';
        return -1;
    }

    size_t total = 0;
    if (output && max_len > 0) {
        output[0] = '\0';
        while (total < max_len - 1 && fgets(output + total, (int)(max_len - total), pipe) != NULL) {
            total = strlen(output);
        }
    } else {
        char dummy[256];
        while (fgets(dummy, sizeof(dummy), pipe) != NULL);
    }

    int status = pclose(pipe);
    return GET_EXIT_CODE(status);
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
            case SEC_DESCRIPTION:
                if (!StrCaseContains(t, "placeholder") && strcmp(t, "...") != 0) {
                    char clean_desc[512] = "";
                    StripDiacritics(t, clean_desc, sizeof(clean_desc));
                    if (strlen(task->description) > 0) {
                        size_t dlen = strlen(task->description);
                        if (dlen + strlen(clean_desc) + 2 < sizeof(task->description)) {
                            strcat(task->description, " ");
                            strcat(task->description, clean_desc);
                        }
                    } else {
                        snprintf(task->description, sizeof(task->description), "%s", clean_desc);
                    }
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

    static char branches_raw[1024 * 32];
    branches_raw[0] = '\0';
    ExecuteGit(repo_dir, "for-each-ref --format=\"%(refname)\" refs/heads/ refs/remotes/", branches_raw, sizeof(branches_raw));

    static char seen_branches[MAX_TASKS][MAX_STR];
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
// HELPER FUNKCE PRO CLAIM A VYTVORENI PROJEKTU
// -----------------------------------------------------------------------------
static void AppendStr(char *out, size_t out_max, const char *src) {
    if (!out || !src) return;
    size_t cur = strlen(out);
    if (cur >= out_max - 1) return;
    snprintf(out + cur, out_max - cur, "%s", src);
}

static void GenerateBranchSlug(const char *name, char *out_slug, size_t max_len) {
    if (!name || !out_slug || max_len < 10) return;
    strcpy(out_slug, "task/");
    size_t out_idx = 5;
    bool last_was_dash = false;

    for (size_t i = 0; name[i] && out_idx < max_len - 2; i++) {
        unsigned char c = (unsigned char)name[i];
        if (isalnum(c)) {
            out_slug[out_idx++] = (char)tolower(c);
            last_was_dash = false;
        } else if (c == ' ' || c == '-' || c == '_' || c == '/') {
            if (!last_was_dash && out_idx > 5) {
                out_slug[out_idx++] = '-';
                last_was_dash = true;
            }
        }
    }
    while (out_idx > 5 && out_slug[out_idx - 1] == '-') {
        out_idx--;
    }
    if (out_idx == 5) {
        snprintf(out_slug, max_len, "task/new-task");
    } else {
        out_slug[out_idx] = '\0';
    }
}

static bool UpdateTaskMdClaim(const char *content, const char *user_name, bool claim_tm, char *out, size_t out_max) {
    if (!content || !user_name || !out || out_max == 0) return false;
    out[0] = '\0';

    enum {
        SEC_OTHER = 0,
        SEC_NAME,
        SEC_TM,
        SEC_WORKER,
        SEC_STATUS
    } cur_sec = SEC_OTHER;

    bool tm_handled = false;
    bool worker_handled = false;
    bool has_tm_sec = false;
    bool has_worker_sec = false;

    const char *p = content;
    char line[1024];

    while (*p) {
        size_t idx = 0;
        while (*p && *p != '\n' && *p != '\r' && idx < sizeof(line) - 1) {
            line[idx++] = *p++;
        }
        line[idx] = '\0';
        while (*p == '\r' || *p == '\n') p++;

        char line_copy[1024];
        strncpy(line_copy, line, sizeof(line_copy) - 1);
        line_copy[sizeof(line_copy) - 1] = '\0';
        char *t = TrimWhitespace(line_copy);

        if (t && t[0] == '#') {
            char *hdr = TrimWhitespace(t + 1);

            if (cur_sec == SEC_WORKER && !claim_tm && !worker_handled) {
                AppendStr(out, out_max, user_name);
                AppendStr(out, out_max, "\n");
                worker_handled = true;
            }

            if (strncasecmp(hdr, "TASK MASTER", 11) == 0) {
                cur_sec = SEC_TM;
                has_tm_sec = true;
                AppendStr(out, out_max, line);
                AppendStr(out, out_max, "\n");
                if (claim_tm) {
                    AppendStr(out, out_max, user_name);
                    AppendStr(out, out_max, "\n");
                    tm_handled = true;
                }
                continue;
            } else if (strncasecmp(hdr, "WORKER", 6) == 0) {
                cur_sec = SEC_WORKER;
                has_worker_sec = true;
                AppendStr(out, out_max, line);
                AppendStr(out, out_max, "\n");
                continue;
            } else if (strncasecmp(hdr, "STATUS", 6) == 0) {
                cur_sec = SEC_STATUS;
                if (claim_tm && !has_tm_sec && !tm_handled) {
                    AppendStr(out, out_max, "# TASK MASTER\n");
                    AppendStr(out, out_max, user_name);
                    AppendStr(out, out_max, "\n");
                    tm_handled = true;
                    has_tm_sec = true;
                }
                if (!claim_tm && !has_worker_sec && !worker_handled) {
                    AppendStr(out, out_max, "# WORKER\n");
                    AppendStr(out, out_max, user_name);
                    AppendStr(out, out_max, "\n");
                    worker_handled = true;
                    has_worker_sec = true;
                }
                AppendStr(out, out_max, line);
                AppendStr(out, out_max, "\n");
                continue;
            } else {
                cur_sec = SEC_OTHER;
                AppendStr(out, out_max, line);
                AppendStr(out, out_max, "\n");
                continue;
            }
        }

        if (cur_sec == SEC_TM) {
            if (claim_tm) {
                continue;
            } else {
                AppendStr(out, out_max, line);
                AppendStr(out, out_max, "\n");
            }
        } else if (cur_sec == SEC_WORKER) {
            if (!claim_tm && !worker_handled && strcmp(t, "---") == 0) {
                AppendStr(out, out_max, user_name);
                AppendStr(out, out_max, "\n");
                worker_handled = true;
            } else {
                AppendStr(out, out_max, line);
                AppendStr(out, out_max, "\n");
            }
        } else {
            AppendStr(out, out_max, line);
            AppendStr(out, out_max, "\n");
        }
    }

    if (!claim_tm && !worker_handled) {
        if (!has_worker_sec) {
            AppendStr(out, out_max, "# WORKER\n");
        }
        AppendStr(out, out_max, user_name);
        AppendStr(out, out_max, "\n");
    } else if (claim_tm && !has_tm_sec) {
        AppendStr(out, out_max, "# TASK MASTER\n");
        AppendStr(out, out_max, user_name);
        AppendStr(out, out_max, "\n");
    }

    return true;
}

static bool PerformClaimTask(const char *repo_dir, const Task *task, const char *user_name,
                             bool claim_tm, char *out_status, size_t out_status_len) {
    if (!repo_dir || !task || !user_name) return false;

    char git_buf[1024] = "";
    char checkout_cmd[512];
    snprintf(checkout_cmd, sizeof(checkout_cmd), "checkout \"%s\"", task->branch);

    int chk_res = RunGit(repo_dir, checkout_cmd, git_buf, sizeof(git_buf));
    if (chk_res != 0) {
        char track_cmd[1024];
        snprintf(track_cmd, sizeof(track_cmd), "checkout -b \"%s\" \"origin/%s\"", task->branch, task->branch);
        if (RunGit(repo_dir, track_cmd, git_buf, sizeof(git_buf)) != 0) {
            snprintf(out_status, out_status_len, "Git checkout failed: %s", TrimWhitespace(git_buf));
            return false;
        }
    }

    char task_path[1024];
    snprintf(task_path, sizeof(task_path), "%s/TASK.md", repo_dir);

    char *file_content = (char *)malloc(MAX_CONTENT);
    if (!file_content) {
        snprintf(out_status, out_status_len, "Memory allocation error");
        return false;
    }
    file_content[0] = '\0';

    FILE *f = fopen(task_path, "rb");
    if (f) {
        size_t n = fread(file_content, 1, MAX_CONTENT - 1, f);
        file_content[n] = '\0';
        fclose(f);
    } else {
        snprintf(file_content, MAX_CONTENT,
                 "# NAME\n%s\n# DESCRIPTION\nTask branch %s\n# STATUS\navailable\n",
                 task->name, task->branch);
    }

    char *updated = (char *)malloc(MAX_CONTENT);
    if (!updated) {
        free(file_content);
        snprintf(out_status, out_status_len, "Memory allocation error");
        return false;
    }

    if (!UpdateTaskMdClaim(file_content, user_name, claim_tm, updated, MAX_CONTENT)) {
        free(file_content);
        free(updated);
        snprintf(out_status, out_status_len, "Failed to update TASK.md format");
        return false;
    }
    free(file_content);

    FILE *fw = fopen(task_path, "wb");
    if (!fw) {
        free(updated);
        snprintf(out_status, out_status_len, "Could not write to TASK.md");
        return false;
    }
    fputs(updated, fw);
    fclose(fw);
    free(updated);

    // git add -A
    RunGit(repo_dir, "add -A", git_buf, sizeof(git_buf));

    // git commit
    char commit_cmd[1024];
    snprintf(commit_cmd, sizeof(commit_cmd),
             "-c user.name=\"%s\" -c user.email=\"%s@taskfinder.local\" commit -m \"Claim %s on %s by %s\"",
             user_name, user_name,
             claim_tm ? "Task Master" : "Worker",
             task->branch, user_name);
    RunGit(repo_dir, commit_cmd, git_buf, sizeof(git_buf));

    // git push
    char push_cmd[512];
    snprintf(push_cmd, sizeof(push_cmd), "push origin \"%s\"", task->branch);
    int push_res = RunGit(repo_dir, push_cmd, git_buf, sizeof(git_buf));

    if (push_res == 0) {
        snprintf(out_status, out_status_len, "Claimed & pushed to origin/%s!", task->branch);
    } else {
        snprintf(out_status, out_status_len, "Claimed & committed! (Push skipped: offline/no origin)");
    }
    return true;
}

static bool AddSubtaskToContent(const char *content, const char *subtask_branch, char *out, size_t out_max) {
    if (!content || !subtask_branch || !out || out_max == 0) return false;
    out[0] = '\0';

    char search_pattern[256];
    snprintf(search_pattern, sizeof(search_pattern), "[%s/]", subtask_branch);

    if (strstr(content, search_pattern)) {
        snprintf(out, out_max, "%s", content);
        return true;
    }

    bool subtasks_sec_found = false;
    bool inserted = false;
    bool in_subtasks = false;

    const char *p = content;
    char line[1024];

    while (*p) {
        size_t idx = 0;
        while (*p && *p != '\n' && *p != '\r' && idx < sizeof(line) - 1) {
            line[idx++] = *p++;
        }
        line[idx] = '\0';
        while (*p == '\r' || *p == '\n') p++;

        char line_copy[1024];
        strncpy(line_copy, line, sizeof(line_copy) - 1);
        line_copy[sizeof(line_copy) - 1] = '\0';
        char *t = TrimWhitespace(line_copy);

        if (t && t[0] == '#') {
            if (in_subtasks && !inserted) {
                AppendStr(out, out_max, search_pattern);
                AppendStr(out, out_max, "\n");
                inserted = true;
            }
            char *hdr = TrimWhitespace(t + 1);
            if (strncasecmp(hdr, "SUBTASK", 7) == 0) {
                subtasks_sec_found = true;
                in_subtasks = true;
            } else {
                in_subtasks = false;
            }
        }

        AppendStr(out, out_max, line);
        AppendStr(out, out_max, "\n");
    }

    if (in_subtasks && !inserted) {
        AppendStr(out, out_max, search_pattern);
        AppendStr(out, out_max, "\n");
        inserted = true;
    } else if (!subtasks_sec_found && !inserted) {
        AppendStr(out, out_max, "# SUBTASKS\n");
        AppendStr(out, out_max, search_pattern);
        AppendStr(out, out_max, "\n");
        inserted = true;
    }

    return true;
}

static bool AddWorkerRequestToContent(const char *content, char *out, size_t out_max, char *err_msg, size_t err_msg_len) {
    out[0] = '\0';
    if (!content) return false;

    bool worker_sec_found = false;
    bool in_worker = false;
    bool inserted = false;
    bool already_has_dash = false;

    const char *p = content;
    char line[1024];

    while (*p) {
        size_t idx = 0;
        while (*p && *p != '\n' && *p != '\r' && idx < sizeof(line) - 1) {
            line[idx++] = *p++;
        }
        line[idx] = '\0';
        while (*p == '\r' || *p == '\n') p++;

        char line_copy[1024];
        strncpy(line_copy, line, sizeof(line_copy) - 1);
        line_copy[sizeof(line_copy) - 1] = '\0';
        char *t = TrimWhitespace(line_copy);

        if (t && t[0] == '#') {
            if (in_worker && !inserted) {
                if (!already_has_dash) {
                    AppendStr(out, out_max, "---\n");
                    inserted = true;
                }
            }
            char *hdr = TrimWhitespace(t + 1);
            if (strncasecmp(hdr, "WORKER", 6) == 0) {
                worker_sec_found = true;
                in_worker = true;
            } else {
                in_worker = false;
            }
        } else if (in_worker) {
            if (strcmp(t, "---") == 0) {
                already_has_dash = true;
            }
        }

        AppendStr(out, out_max, line);
        AppendStr(out, out_max, "\n");
    }

    if (in_worker && !inserted) {
        if (!already_has_dash) {
            AppendStr(out, out_max, "---\n");
            inserted = true;
        }
    }

    if (already_has_dash) {
        if (err_msg) snprintf(err_msg, err_msg_len, "Worker slot is already open ('---')");
        return false;
    }

    if (!worker_sec_found && !inserted) {
        out[0] = '\0';
        bool sec_inserted = false;
        p = content;
        while (*p) {
            size_t idx = 0;
            while (*p && *p != '\n' && *p != '\r' && idx < sizeof(line) - 1) {
                line[idx++] = *p++;
            }
            line[idx] = '\0';
            while (*p == '\r' || *p == '\n') p++;

            char line_copy[1024];
            strncpy(line_copy, line, sizeof(line_copy) - 1);
            line_copy[sizeof(line_copy) - 1] = '\0';
            char *t = TrimWhitespace(line_copy);

            if (t && t[0] == '#' && !sec_inserted) {
                char *hdr = TrimWhitespace(t + 1);
                if (strncasecmp(hdr, "STATUS", 6) == 0 || strncasecmp(hdr, "DEADLINE", 8) == 0 || strncasecmp(hdr, "PARENT", 6) == 0 || strncasecmp(hdr, "SUBTASK", 7) == 0) {
                    AppendStr(out, out_max, "# WORKER\n---\n\n");
                    sec_inserted = true;
                }
            }
            AppendStr(out, out_max, line);
            AppendStr(out, out_max, "\n");
        }
        if (!sec_inserted) {
            AppendStr(out, out_max, "\n# WORKER\n---\n");
        }
    }

    return true;
}

static bool UpdateStatusInContent(const char *content, const char *new_status, char *out, size_t out_max) {
    out[0] = '\0';
    if (!content || !new_status) return false;

    bool status_sec_found = false;
    bool in_status = false;
    bool replaced = false;

    const char *p = content;
    char line[1024];

    while (*p) {
        size_t idx = 0;
        while (*p && *p != '\n' && *p != '\r' && idx < sizeof(line) - 1) {
            line[idx++] = *p++;
        }
        line[idx] = '\0';
        while (*p == '\r' || *p == '\n') p++;

        char line_copy[1024];
        strncpy(line_copy, line, sizeof(line_copy) - 1);
        line_copy[sizeof(line_copy) - 1] = '\0';
        char *t = TrimWhitespace(line_copy);

        if (t && t[0] == '#') {
            if (in_status && !replaced) {
                AppendStr(out, out_max, new_status);
                AppendStr(out, out_max, "\n");
                replaced = true;
            }
            char *hdr = TrimWhitespace(t + 1);
            if (strncasecmp(hdr, "STATUS", 6) == 0) {
                status_sec_found = true;
                in_status = true;
            } else {
                in_status = false;
            }
            AppendStr(out, out_max, line);
            AppendStr(out, out_max, "\n");
        } else if (in_status) {
            if (!replaced) {
                AppendStr(out, out_max, new_status);
                AppendStr(out, out_max, "\n");
                replaced = true;
            }
        } else {
            AppendStr(out, out_max, line);
            AppendStr(out, out_max, "\n");
        }
    }

    if (in_status && !replaced) {
        AppendStr(out, out_max, new_status);
        AppendStr(out, out_max, "\n");
        replaced = true;
    } else if (!status_sec_found && !replaced) {
        AppendStr(out, out_max, "\n# STATUS\n");
        AppendStr(out, out_max, new_status);
        AppendStr(out, out_max, "\n");
    }

    return true;
}

static bool UpdateDescriptionInContent(const char *content, const char *new_desc, char *out, size_t out_max) {
    out[0] = '\0';
    if (!content) return false;

    bool desc_sec_found = false;
    bool in_desc = false;
    bool replaced = false;

    const char *p = content;
    char line[1024];

    while (*p) {
        size_t idx = 0;
        while (*p && *p != '\n' && *p != '\r' && idx < sizeof(line) - 1) {
            line[idx++] = *p++;
        }
        line[idx] = '\0';
        while (*p == '\r' || *p == '\n') p++;

        char line_copy[1024];
        strncpy(line_copy, line, sizeof(line_copy) - 1);
        line_copy[sizeof(line_copy) - 1] = '\0';
        char *t = TrimWhitespace(line_copy);

        if (t && t[0] == '#') {
            if (in_desc && !replaced) {
                if (new_desc && strlen(new_desc) > 0) {
                    AppendStr(out, out_max, new_desc);
                    AppendStr(out, out_max, "\n");
                }
                replaced = true;
            }
            char *hdr = TrimWhitespace(t + 1);
            if (strncasecmp(hdr, "DESCRIPTION", 11) == 0) {
                desc_sec_found = true;
                in_desc = true;
                AppendStr(out, out_max, line);
                AppendStr(out, out_max, "\n");
                if (new_desc && strlen(new_desc) > 0) {
                    AppendStr(out, out_max, new_desc);
                    AppendStr(out, out_max, "\n");
                }
                replaced = true;
            } else {
                in_desc = false;
                AppendStr(out, out_max, line);
                AppendStr(out, out_max, "\n");
            }
        } else if (in_desc) {
            // Skip old description lines
        } else {
            AppendStr(out, out_max, line);
            AppendStr(out, out_max, "\n");
        }
    }

    if (!desc_sec_found) {
        out[0] = '\0';
        bool sec_inserted = false;
        p = content;
        while (*p) {
            size_t idx = 0;
            while (*p && *p != '\n' && *p != '\r' && idx < sizeof(line) - 1) {
                line[idx++] = *p++;
            }
            line[idx] = '\0';
            while (*p == '\r' || *p == '\n') p++;

            char line_copy[1024];
            strncpy(line_copy, line, sizeof(line_copy) - 1);
            line_copy[sizeof(line_copy) - 1] = '\0';
            char *t = TrimWhitespace(line_copy);

            if (t && t[0] == '#' && !sec_inserted) {
                char *hdr = TrimWhitespace(t + 1);
                if (strncasecmp(hdr, "TASK MASTER", 11) == 0 || strncasecmp(hdr, "WORKER", 6) == 0 || strncasecmp(hdr, "STATUS", 6) == 0) {
                    AppendStr(out, out_max, "# DESCRIPTION\n");
                    if (new_desc && strlen(new_desc) > 0) {
                        AppendStr(out, out_max, new_desc);
                        AppendStr(out, out_max, "\n");
                    }
                    AppendStr(out, out_max, "\n");
                    sec_inserted = true;
                }
            }
            AppendStr(out, out_max, line);
            AppendStr(out, out_max, "\n");
        }
        if (!sec_inserted) {
            AppendStr(out, out_max, "\n# DESCRIPTION\n");
            if (new_desc && strlen(new_desc) > 0) {
                AppendStr(out, out_max, new_desc);
                AppendStr(out, out_max, "\n");
            }
        }
    }

    return true;
}

static bool PerformRequestWorker(const char *repo_dir, const char *user_name, const char *branch,
                                 char *out_status, size_t out_status_len) {
    if (!repo_dir || !branch || strlen(branch) == 0) {
        snprintf(out_status, out_status_len, "Invalid branch name");
        return false;
    }

    char git_buf[1024] = "";
    char checkout_cmd[512];
    snprintf(checkout_cmd, sizeof(checkout_cmd), "checkout \"%s\"", branch);
    if (RunGit(repo_dir, checkout_cmd, git_buf, sizeof(git_buf)) != 0) {
        snprintf(out_status, out_status_len, "Git checkout failed: %s", TrimWhitespace(git_buf));
        return false;
    }

    char task_path[1024];
    snprintf(task_path, sizeof(task_path), "%s/TASK.md", repo_dir);

    char *content = (char *)malloc(MAX_CONTENT);
    char *updated = (char *)malloc(MAX_CONTENT);
    if (!content || !updated) {
        if (content) free(content);
        if (updated) free(updated);
        snprintf(out_status, out_status_len, "Memory allocation error");
        return false;
    }
    content[0] = '\0';
    updated[0] = '\0';

    FILE *fp = fopen(task_path, "rb");
    if (fp) {
        size_t n = fread(content, 1, MAX_CONTENT - 1, fp);
        content[n] = '\0';
        fclose(fp);
    } else {
        snprintf(content, MAX_CONTENT, "# NAME\n%s\n# STATUS\navailable\n", branch);
    }

    char err[256] = "";
    if (!AddWorkerRequestToContent(content, updated, MAX_CONTENT, err, sizeof(err))) {
        free(content);
        free(updated);
        snprintf(out_status, out_status_len, "%s", strlen(err) > 0 ? err : "Failed to add worker request");
        return false;
    }
    free(content);

    FILE *fpw = fopen(task_path, "wb");
    if (fpw) {
        fputs(updated, fpw);
        fclose(fpw);
    }
    free(updated);

    RunGit(repo_dir, "add -A", git_buf, sizeof(git_buf));

    char commit_cmd[1024];
    snprintf(commit_cmd, sizeof(commit_cmd),
             "-c user.name=\"%s\" -c user.email=\"%s@taskfinder.local\" commit -m \"Request worker slot for %s\"",
             (user_name && strlen(user_name) > 0) ? user_name : "TaskFinder",
             (user_name && strlen(user_name) > 0) ? user_name : "taskfinder",
             branch);
    RunGit(repo_dir, commit_cmd, git_buf, sizeof(git_buf));

    char push_cmd[512];
    snprintf(push_cmd, sizeof(push_cmd), "push origin \"%s\"", branch);
    int p_res = RunGit(repo_dir, push_cmd, git_buf, sizeof(git_buf));

    if (p_res == 0) {
        snprintf(out_status, out_status_len, "Worker requested & pushed for '%s'!", branch);
    } else {
        snprintf(out_status, out_status_len, "Worker requested & committed for '%s'! (Push skipped: offline)", branch);
    }
    return true;
}

static bool PerformToggleStatus(const char *repo_dir, const char *user_name, const char *branch,
                                const char *current_status, char *out_status, size_t out_status_len) {
    if (!repo_dir || !branch || strlen(branch) == 0) {
        snprintf(out_status, out_status_len, "Invalid branch name");
        return false;
    }

    const char *new_status = (strcasecmp(current_status, "available") == 0) ? "completed" : "available";

    char git_buf[1024] = "";
    char checkout_cmd[512];
    snprintf(checkout_cmd, sizeof(checkout_cmd), "checkout \"%s\"", branch);
    if (RunGit(repo_dir, checkout_cmd, git_buf, sizeof(git_buf)) != 0) {
        snprintf(out_status, out_status_len, "Git checkout failed: %s", TrimWhitespace(git_buf));
        return false;
    }

    char task_path[1024];
    snprintf(task_path, sizeof(task_path), "%s/TASK.md", repo_dir);

    char *content = (char *)malloc(MAX_CONTENT);
    char *updated = (char *)malloc(MAX_CONTENT);
    if (!content || !updated) {
        if (content) free(content);
        if (updated) free(updated);
        snprintf(out_status, out_status_len, "Memory allocation error");
        return false;
    }
    content[0] = '\0';
    updated[0] = '\0';

    FILE *fp = fopen(task_path, "rb");
    if (fp) {
        size_t n = fread(content, 1, MAX_CONTENT - 1, fp);
        content[n] = '\0';
        fclose(fp);
    } else {
        snprintf(content, MAX_CONTENT, "# NAME\n%s\n# STATUS\n%s\n", branch, new_status);
    }

    UpdateStatusInContent(content, new_status, updated, MAX_CONTENT);
    free(content);

    FILE *fpw = fopen(task_path, "wb");
    if (fpw) {
        fputs(updated, fpw);
        fclose(fpw);
    }
    free(updated);

    RunGit(repo_dir, "add -A", git_buf, sizeof(git_buf));

    char commit_cmd[1024];
    snprintf(commit_cmd, sizeof(commit_cmd),
             "-c user.name=\"%s\" -c user.email=\"%s@taskfinder.local\" commit -m \"Set status to %s for %s\"",
             (user_name && strlen(user_name) > 0) ? user_name : "TaskFinder",
             (user_name && strlen(user_name) > 0) ? user_name : "taskfinder",
             new_status, branch);
    RunGit(repo_dir, commit_cmd, git_buf, sizeof(git_buf));

    char push_cmd[512];
    snprintf(push_cmd, sizeof(push_cmd), "push origin \"%s\"", branch);
    int p_res = RunGit(repo_dir, push_cmd, git_buf, sizeof(git_buf));

    if (p_res == 0) {
        snprintf(out_status, out_status_len, "Task '%s' status changed to '%s' & pushed!", branch, new_status);
    } else {
        snprintf(out_status, out_status_len, "Task '%s' status changed to '%s'! (Push skipped: offline)", branch, new_status);
    }
    return true;
}

static bool PerformUpdateDescription(const char *repo_dir, const char *user_name, const char *branch,
                                     const char *new_desc, char *out_status, size_t out_status_len) {
    if (!repo_dir || !branch || strlen(branch) == 0) {
        snprintf(out_status, out_status_len, "Invalid branch name");
        return false;
    }

    char git_buf[1024] = "";
    char checkout_cmd[512];
    snprintf(checkout_cmd, sizeof(checkout_cmd), "checkout \"%s\"", branch);
    if (RunGit(repo_dir, checkout_cmd, git_buf, sizeof(git_buf)) != 0) {
        snprintf(out_status, out_status_len, "Git checkout failed: %s", TrimWhitespace(git_buf));
        return false;
    }

    char task_path[1024];
    snprintf(task_path, sizeof(task_path), "%s/TASK.md", repo_dir);

    char *content = (char *)malloc(MAX_CONTENT);
    char *updated = (char *)malloc(MAX_CONTENT);
    if (!content || !updated) {
        if (content) free(content);
        if (updated) free(updated);
        snprintf(out_status, out_status_len, "Memory allocation error");
        return false;
    }
    content[0] = '\0';
    updated[0] = '\0';

    FILE *fp = fopen(task_path, "rb");
    if (fp) {
        size_t n = fread(content, 1, MAX_CONTENT - 1, fp);
        content[n] = '\0';
        fclose(fp);
    } else {
        snprintf(content, MAX_CONTENT, "# NAME\n%s\n# STATUS\navailable\n", branch);
    }

    UpdateDescriptionInContent(content, new_desc, updated, MAX_CONTENT);
    free(content);

    FILE *fpw = fopen(task_path, "wb");
    if (fpw) {
        fputs(updated, fpw);
        fclose(fpw);
    }
    free(updated);

    RunGit(repo_dir, "add -A", git_buf, sizeof(git_buf));

    char commit_cmd[1024];
    snprintf(commit_cmd, sizeof(commit_cmd),
             "-c user.name=\"%s\" -c user.email=\"%s@taskfinder.local\" commit -m \"Update description for %s\"",
             (user_name && strlen(user_name) > 0) ? user_name : "TaskFinder",
             (user_name && strlen(user_name) > 0) ? user_name : "taskfinder",
             branch);
    RunGit(repo_dir, commit_cmd, git_buf, sizeof(git_buf));

    char push_cmd[512];
    snprintf(push_cmd, sizeof(push_cmd), "push origin \"%s\"", branch);
    int p_res = RunGit(repo_dir, push_cmd, git_buf, sizeof(git_buf));

    if (p_res == 0) {
        snprintf(out_status, out_status_len, "Description updated & pushed for '%s'!", branch);
    } else {
        snprintf(out_status, out_status_len, "Description updated for '%s'! (Push skipped: offline)", branch);
    }
    return true;
}

static int GetUserManagedTasks(const TaskList *all_tasks, const char *user_name, Task out_tasks[MAX_TASKS]) {
    int count = 0;
    if (!all_tasks || !user_name || strlen(user_name) == 0) return 0;
    for (int i = 0; i < all_tasks->count; i++) {
        const Task *t = &all_tasks->items[i];
        if (NamesEqual(t->task_master, user_name)) {
            if (out_tasks && count < MAX_TASKS) {
                out_tasks[count] = *t;
            }
            count++;
        }
    }
    return count;
}

static bool PerformCreateProject(const char *repo_dir, const char *user_name,
                                 const char *parent_branch,
                                 const char *name, const char *branch, const char *desc,
                                 const char *tm, const char *deadline,
                                 char *out_status, size_t out_status_len) {
    if (!repo_dir || !branch || strlen(branch) == 0) {
        snprintf(out_status, out_status_len, "Invalid branch name");
        return false;
    }
    if (!parent_branch || strlen(parent_branch) == 0) {
        snprintf(out_status, out_status_len, "Parent task is required to create a subtask");
        return false;
    }

    char git_buf[1024] = "";

    // Check if new branch already exists
    char chk_cmd[512];
    snprintf(chk_cmd, sizeof(chk_cmd), "show-ref --verify \"refs/heads/%s\"", branch);
    if (RunGit(repo_dir, chk_cmd, git_buf, sizeof(git_buf)) == 0) {
        snprintf(out_status, out_status_len, "Branch '%s' already exists!", branch);
        return false;
    }

    // 1. Checkout parent branch
    char checkout_cmd[512];
    snprintf(checkout_cmd, sizeof(checkout_cmd), "checkout \"%s\"", parent_branch);
    if (RunGit(repo_dir, checkout_cmd, git_buf, sizeof(git_buf)) != 0) {
        snprintf(out_status, out_status_len, "Git checkout parent '%s' failed: %s", parent_branch, TrimWhitespace(git_buf));
        return false;
    }

    // 2. Read parent TASK.md and append subtask link
    char task_path[1024];
    snprintf(task_path, sizeof(task_path), "%s/TASK.md", repo_dir);

    char *parent_content = (char *)malloc(MAX_CONTENT);
    char *parent_updated = (char *)malloc(MAX_CONTENT);
    if (!parent_content || !parent_updated) {
        if (parent_content) free(parent_content);
        if (parent_updated) free(parent_updated);
        snprintf(out_status, out_status_len, "Memory allocation error");
        return false;
    }
    parent_content[0] = '\0';
    parent_updated[0] = '\0';

    FILE *fp = fopen(task_path, "rb");
    if (fp) {
        size_t n = fread(parent_content, 1, MAX_CONTENT - 1, fp);
        parent_content[n] = '\0';
        fclose(fp);
    } else {
        snprintf(parent_content, MAX_CONTENT,
                 "# NAME\n%s\n# STATUS\navailable\n# SUBTASKS\n", parent_branch);
    }

    AddSubtaskToContent(parent_content, branch, parent_updated, MAX_CONTENT);
    free(parent_content);

    FILE *fpw = fopen(task_path, "wb");
    if (fpw) {
        fputs(parent_updated, fpw);
        fclose(fpw);
    }
    free(parent_updated);

    RunGit(repo_dir, "add -A", git_buf, sizeof(git_buf));

    char p_commit[1024];
    snprintf(p_commit, sizeof(p_commit),
             "-c user.name=\"%s\" -c user.email=\"%s@taskfinder.local\" commit -m \"Add subtask %s to %s\"",
             (user_name && strlen(user_name) > 0) ? user_name : "TaskFinder",
             (user_name && strlen(user_name) > 0) ? user_name : "taskfinder",
             branch, parent_branch);
    RunGit(repo_dir, p_commit, git_buf, sizeof(git_buf));

    char p_push[512];
    snprintf(p_push, sizeof(p_push), "push origin \"%s\"", parent_branch);
    RunGit(repo_dir, p_push, git_buf, sizeof(git_buf));

    // 3. Checkout new subtask branch from parent
    char b_cmd[512];
    snprintf(b_cmd, sizeof(b_cmd), "checkout -b \"%s\"", branch);
    int b_res = RunGit(repo_dir, b_cmd, git_buf, sizeof(git_buf));
    if (b_res != 0) {
        snprintf(out_status, out_status_len, "Git checkout -b failed: %s", TrimWhitespace(git_buf));
        return false;
    }

    // 4. Create subtask TASK.md (Worker section should NOT be present)
    FILE *f = fopen(task_path, "wb");
    if (!f) {
        snprintf(out_status, out_status_len, "Could not create TASK.md file");
        return false;
    }

    fprintf(f, "# NAME\n%s\n", (name && strlen(name) > 0) ? name : branch);
    fprintf(f, "# DESCRIPTION\n%s\n", (desc && strlen(desc) > 0) ? desc : "...");
    fprintf(f, "# TASK MASTER\n%s\n", (tm && strlen(tm) > 0) ? tm : "");
    fprintf(f, "# STATUS\navailable\n");
    if (deadline && strlen(deadline) > 0) {
        fprintf(f, "# DEADLINE\n%s\n", deadline);
    }
    fprintf(f, "# PARENT\n[%s/]\n\n# SUBTASKS\n\n", parent_branch);
    fclose(f);

    // git add -A
    RunGit(repo_dir, "add -A", git_buf, sizeof(git_buf));

    // git commit
    char commit_cmd[1024];
    snprintf(commit_cmd, sizeof(commit_cmd),
             "-c user.name=\"%s\" -c user.email=\"%s@taskfinder.local\" commit -m \"Create subtask: %s\"",
             (user_name && strlen(user_name) > 0) ? user_name : "TaskFinder",
             (user_name && strlen(user_name) > 0) ? user_name : "taskfinder",
             (name && strlen(name) > 0) ? name : branch);
    RunGit(repo_dir, commit_cmd, git_buf, sizeof(git_buf));

    // git push -u origin <branch>
    char push_cmd[512];
    snprintf(push_cmd, sizeof(push_cmd), "push -u origin \"%s\"", branch);
    int push_res = RunGit(repo_dir, push_cmd, git_buf, sizeof(git_buf));

    if (push_res == 0) {
        snprintf(out_status, out_status_len, "Subtask '%s' created & pushed to origin/%s!", name, branch);
    } else {
        snprintf(out_status, out_status_len, "Subtask '%s' created & committed! (Push skipped: offline/no origin)", name);
    }
    return true;
}

static void EvaluateUserTasks(const TaskList *all_tasks, const char *user_name,
                              Task *out_active, char out_roles[][64], int *out_active_count,
                              Task *out_free, int *out_free_count) {
    *out_active_count = 0;
    *out_free_count = 0;

    for (int i = 0; i < all_tasks->count; i++) {
        const Task *t = &all_tasks->items[i];
        if (!t->is_available) continue;
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

        if (has_active && *out_active_count < MAX_TASKS) {
            out_active[*out_active_count] = *t;
            snprintf(out_roles[*out_active_count], sizeof(out_roles[0]), "%s", role);
            (*out_active_count)++;
        }
    }

    if (*out_active_count == 0) {
        for (int i = 0; i < all_tasks->count; i++) {
            const Task *t = &all_tasks->items[i];
            if (!t->is_available) continue;

            bool is_free = t->is_task_master_free || (t->requires_worker && t->is_worker_free);
            if (is_free && *out_free_count < MAX_TASKS) {
                out_free[(*out_free_count)++] = *t;
            }
        }
        qsort(out_free, *out_free_count, sizeof(Task), CompareTasksDescending);
    }
}

static void RunCliManageTasks(const char *repo_dir, const char *github_base, TaskList *all_tasks, const char *user_name) {
    static Task managed[MAX_TASKS];
    int m_count = GetUserManagedTasks(all_tasks, user_name, managed);
    if (m_count == 0) {
        printf("\n>> You are not Task Master of any task.\n\n");
        return;
    }

    printf("\n============================================================\n");
    printf("           MANAGE TASKS (Task Master: %s)\n", user_name);
    printf("============================================================\n");
    for (int mi = 0; mi < m_count; mi++) {
        printf("  [%d] %s\n", mi + 1, managed[mi].name);
        printf("      Branch: %s | Status: %s | Workers: %d%s\n",
               managed[mi].branch, managed[mi].status,
               managed[mi].worker_count,
               managed[mi].is_worker_free ? " (worker requested: ---)" : "");
    }
    printf("\nSelect task to manage [1-%d, c to cancel]: ", m_count);
    char m_in[64] = "";
    if (!fgets(m_in, sizeof(m_in), stdin)) return;
    char *tm_c = TrimWhitespace(m_in);
    if (strcasecmp(tm_c, "c") == 0) return;
    int m_idx = atoi(tm_c);
    if (m_idx < 1 || m_idx > m_count) return;

    Task *sel_t = &managed[m_idx - 1];
    printf("\n--- MANAGING: %s (%s) ---\n", sel_t->name, sel_t->branch);
    printf("Status:      %s\n", sel_t->status);
    printf("Description: %s\n", strlen(sel_t->description) > 0 ? sel_t->description : "(none)");
    printf("Workers:     %d%s\n", sel_t->worker_count, sel_t->is_worker_free ? " (slot open: ---)" : "");
    printf("\nOptions:\n");
    printf("  [1] Request worker slot (adds '---' to # WORKER)\n");
    printf("  [2] Toggle status (current: %s -> %s)\n",
           sel_t->status, (strcasecmp(sel_t->status, "available") == 0) ? "completed" : "available");
    printf("  [3] Update description\n");
    printf("  [c] Cancel\n");
    printf("Choose action [1-3, c]: ");
    char act_in[64] = "";
    if (!fgets(act_in, sizeof(act_in), stdin)) return;
    char *act = TrimWhitespace(act_in);

    char status_msg[512] = "";
    if (strcmp(act, "1") == 0) {
        if (PerformRequestWorker(repo_dir, user_name, sel_t->branch, status_msg, sizeof(status_msg))) {
            printf(">> %s\n", status_msg);
            LoadAllTasks(repo_dir, github_base, all_tasks);
        } else {
            printf(">> ERROR: %s\n", status_msg);
        }
    } else if (strcmp(act, "2") == 0) {
        if (PerformToggleStatus(repo_dir, user_name, sel_t->branch, sel_t->status, status_msg, sizeof(status_msg))) {
            printf(">> %s\n", status_msg);
            LoadAllTasks(repo_dir, github_base, all_tasks);
        } else {
            printf(">> ERROR: %s\n", status_msg);
        }
    } else if (strcmp(act, "3") == 0) {
        printf("Enter new description: ");
        char new_desc[512] = "";
        if (fgets(new_desc, sizeof(new_desc), stdin)) {
            char *td = TrimWhitespace(new_desc);
            if (PerformUpdateDescription(repo_dir, user_name, sel_t->branch, td, status_msg, sizeof(status_msg))) {
                printf(">> %s\n", status_msg);
                LoadAllTasks(repo_dir, github_base, all_tasks);
            } else {
                printf(">> ERROR: %s\n", status_msg);
            }
        }
    }
}

// -----------------------------------------------------------------------------
// TERMINAL MODE (CLI FALLBACK)
// -----------------------------------------------------------------------------
static void RunCliMode(const char *repo_dir, const char *github_base, TaskList *all_tasks, const char *initial_user) {
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

    while (1) {
        char *user_name = TrimWhitespace(user);
        if (strlen(user_name) == 0) {
            printf("No name entered.\n");
            return;
        }

        static Task active_tasks[MAX_TASKS];
        static char active_roles[MAX_TASKS][64];
        int active_count = 0;

        static Task free_tasks[MAX_TASKS];
        int free_count = 0;

        EvaluateUserTasks(all_tasks, user_name, active_tasks, active_roles, &active_count, free_tasks, &free_count);

        if (active_count > 0) {
            printf("\n============================================================\n");
            printf("  ACTIVE TASK ASSIGNED (%s)\n", user_name);
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
            int user_managed_count = GetUserManagedTasks(all_tasks, user_name, NULL);
            if (user_managed_count > 0) {
                printf("Options: [m] Manage task (worker/status/desc), [n] Create new subtask, [s] Switch user, [q] Quit: ");
            } else {
                printf("Options: [s] Switch user, [q] Quit: ");
            }
            char cmd[64] = "";
            if (!fgets(cmd, sizeof(cmd), stdin)) break;
            char *c = TrimWhitespace(cmd);
            if (strcasecmp(c, "q") == 0) break;
            if (strcasecmp(c, "s") == 0) {
                printf("Enter new user name: ");
                if (!fgets(user, sizeof(user), stdin)) break;
                continue;
            }
            if (strcasecmp(c, "m") == 0) {
                RunCliManageTasks(repo_dir, github_base, all_tasks, user_name);
                continue;
            }
            if (strcasecmp(c, "n") == 0) {
                static Task managed_parents[MAX_TASKS];
                int managed_count = GetUserManagedTasks(all_tasks, user_name, managed_parents);
                if (managed_count == 0) {
                    printf("\n>> You are not Task Master of any task.\n");
                    printf(">> A task can only be created as a subtask of a task you manage.\n\n");
                    continue;
                }

                printf("\n--- CREATE NEW SUBTASK ---\n");
                printf("Select parent task you manage:\n");
                for (int mi = 0; mi < managed_count; mi++) {
                    printf("  [%d] %s (branch: %s)\n", mi + 1, managed_parents[mi].name, managed_parents[mi].branch);
                }
                int parent_choice = 1;
                if (managed_count > 1) {
                    printf("Choose parent [1-%d]: ", managed_count);
                    char p_in[32] = "";
                    if (fgets(p_in, sizeof(p_in), stdin)) {
                        int pc = atoi(TrimWhitespace(p_in));
                        if (pc >= 1 && pc <= managed_count) parent_choice = pc;
                    }
                }
                const char *parent_branch = managed_parents[parent_choice - 1].branch;
                printf("Parent task: %s (%s)\n", managed_parents[parent_choice - 1].name, parent_branch);

                char p_name[MAX_STR] = "", p_desc[MAX_STR] = "", p_branch[MAX_STR] = "";
                char p_tm[MAX_STR] = "", p_dl[64] = "";

                printf("Subtask Name: ");
                if (!fgets(p_name, sizeof(p_name), stdin)) break;
                char *pn = TrimWhitespace(p_name);
                if (strlen(pn) == 0) {
                    printf("Creation cancelled: Name cannot be empty.\n");
                    continue;
                }

                char default_branch[MAX_STR] = "";
                GenerateBranchSlug(pn, default_branch, sizeof(default_branch));

                printf("Git Branch [%s]: ", default_branch);
                char b_in[MAX_STR] = "";
                if (fgets(b_in, sizeof(b_in), stdin)) {
                    char *tb = TrimWhitespace(b_in);
                    if (strlen(tb) > 0) strncpy(p_branch, tb, sizeof(p_branch) - 1);
                    else strcpy(p_branch, default_branch);
                } else {
                    strcpy(p_branch, default_branch);
                }

                printf("Description: ");
                if (fgets(p_desc, sizeof(p_desc), stdin)) TrimWhitespace(p_desc);

                printf("Task Master (leave blank for none) []: ");
                char tm_in[MAX_STR] = "";
                if (fgets(tm_in, sizeof(tm_in), stdin)) {
                    char *tt = TrimWhitespace(tm_in);
                    strncpy(p_tm, tt, sizeof(p_tm) - 1);
                }

                printf("Deadline (yy/MM/DD, optional): ");
                if (fgets(p_dl, sizeof(p_dl), stdin)) TrimWhitespace(p_dl);

                char status_msg[512] = "";
                printf(">> Creating subtask branch '%s' under '%s'...\n", p_branch, parent_branch);
                if (PerformCreateProject(repo_dir, user_name, parent_branch, pn, p_branch, p_desc, p_tm, p_dl, status_msg, sizeof(status_msg))) {
                    printf(">> %s\n", status_msg);
                    LoadAllTasks(repo_dir, github_base, all_tasks);
                } else {
                    printf(">> ERROR: %s\n", status_msg);
                }
                continue;
            }
            continue;
        }

        // No active task
        printf("\n============================================================\n");
        printf("  NO ACTIVE TASK - AVAILABLE TASKS (%s)\n", user_name);
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

        int user_managed_count = GetUserManagedTasks(all_tasks, user_name, NULL);
        if (user_managed_count > 0) {
            printf("Enter [1-%d] to claim task, [m] manage task, [n] create new subtask, [s] switch user, [q] quit: ", free_count > 0 ? free_count : 1);
        } else {
            printf("Enter [1-%d] to claim task, [s] switch user, [q] quit: ", free_count > 0 ? free_count : 1);
        }
        char input[64] = "";
        if (!fgets(input, sizeof(input), stdin)) break;
        char *cmd = TrimWhitespace(input);

        if (strcasecmp(cmd, "q") == 0) break;
        if (strcasecmp(cmd, "s") == 0) {
            printf("Enter new user name: ");
            if (!fgets(user, sizeof(user), stdin)) break;
            continue;
        }
        if (strcasecmp(cmd, "m") == 0) {
            RunCliManageTasks(repo_dir, github_base, all_tasks, user_name);
            continue;
        }
        if (strcasecmp(cmd, "n") == 0) {
            static Task managed_parents[MAX_TASKS];
            int managed_count = GetUserManagedTasks(all_tasks, user_name, managed_parents);
            if (managed_count == 0) {
                printf("\n>> You are not Task Master of any task.\n");
                printf(">> A task can only be created as a subtask of a task you manage.\n\n");
                continue;
            }

            printf("\n--- CREATE NEW SUBTASK ---\n");
            printf("Select parent task you manage:\n");
            for (int mi = 0; mi < managed_count; mi++) {
                printf("  [%d] %s (branch: %s)\n", mi + 1, managed_parents[mi].name, managed_parents[mi].branch);
            }
            int parent_choice = 1;
            if (managed_count > 1) {
                printf("Choose parent [1-%d]: ", managed_count);
                char p_in[32] = "";
                if (fgets(p_in, sizeof(p_in), stdin)) {
                    int pc = atoi(TrimWhitespace(p_in));
                    if (pc >= 1 && pc <= managed_count) parent_choice = pc;
                }
            }
            const char *parent_branch = managed_parents[parent_choice - 1].branch;
            printf("Parent task: %s (%s)\n", managed_parents[parent_choice - 1].name, parent_branch);

            char p_name[MAX_STR] = "", p_desc[MAX_STR] = "", p_branch[MAX_STR] = "";
            char p_tm[MAX_STR] = "", p_dl[64] = "";

            printf("Subtask Name: ");
            if (!fgets(p_name, sizeof(p_name), stdin)) break;
            char *pn = TrimWhitespace(p_name);
            if (strlen(pn) == 0) {
                printf("Creation cancelled: Name cannot be empty.\n");
                continue;
            }

            char default_branch[MAX_STR] = "";
            GenerateBranchSlug(pn, default_branch, sizeof(default_branch));

            printf("Git Branch [%s]: ", default_branch);
            char b_in[MAX_STR] = "";
            if (fgets(b_in, sizeof(b_in), stdin)) {
                char *tb = TrimWhitespace(b_in);
                if (strlen(tb) > 0) strncpy(p_branch, tb, sizeof(p_branch) - 1);
                else strcpy(p_branch, default_branch);
            } else {
                strcpy(p_branch, default_branch);
            }

            printf("Description: ");
            if (fgets(p_desc, sizeof(p_desc), stdin)) TrimWhitespace(p_desc);

            printf("Task Master (leave blank for none) []: ");
            char tm_in[MAX_STR] = "";
            if (fgets(tm_in, sizeof(tm_in), stdin)) {
                char *tt = TrimWhitespace(tm_in);
                strncpy(p_tm, tt, sizeof(p_tm) - 1);
            }

            printf("Deadline (yy/MM/DD, optional): ");
            if (fgets(p_dl, sizeof(p_dl), stdin)) TrimWhitespace(p_dl);

            char status_msg[512] = "";
            printf(">> Creating subtask branch '%s' under '%s'...\n", p_branch, parent_branch);
            if (PerformCreateProject(repo_dir, user_name, parent_branch, pn, p_branch, p_desc, p_tm, p_dl, status_msg, sizeof(status_msg))) {
                printf(">> %s\n", status_msg);
                LoadAllTasks(repo_dir, github_base, all_tasks);
            } else {
                printf(">> ERROR: %s\n", status_msg);
            }
            continue;
        }

        int choice = atoi(cmd);
        if (choice >= 1 && choice <= free_count) {
            Task *t = &free_tasks[choice - 1];
            bool claim_tm = false;

            if (t->is_task_master_free && (t->requires_worker && t->is_worker_free)) {
                printf("Both roles are open on this task.\nClaim as: [1] Task Master, [2] Worker, [c] Cancel: ");
                char r_in[32] = "";
                if (!fgets(r_in, sizeof(r_in), stdin)) break;
                char *rc = TrimWhitespace(r_in);
                if (strcmp(rc, "1") == 0) claim_tm = true;
                else if (strcmp(rc, "2") == 0) claim_tm = false;
                else {
                    printf("Cancelled.\n");
                    continue;
                }
            } else if (t->is_task_master_free) {
                claim_tm = true;
            } else {
                claim_tm = false;
            }

            printf(">> Claiming %s on branch %s for %s...\n", claim_tm ? "Task Master" : "Worker", t->branch, user_name);
            char status_msg[512] = "";
            if (PerformClaimTask(repo_dir, t, user_name, claim_tm, status_msg, sizeof(status_msg))) {
                printf(">> %s\n", status_msg);
                LoadAllTasks(repo_dir, github_base, all_tasks);
            } else {
                printf(">> ERROR: %s\n", status_msg);
            }
            continue;
        }

        printf("Invalid choice.\n");
    }
}

// -----------------------------------------------------------------------------
// HLAVNÍ GRAFICKÉ ROZHRANÍ (RAYLIB)
// -----------------------------------------------------------------------------
int main(int argc, char **argv) {
    putenv("GIT_TERMINAL_PROMPT=0");

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
        RunCliMode(repo_dir, github_base, &all_tasks, cli_user);
        return 0;
    }

    const int defaultWidth = 920;
    const int defaultHeight = 640;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(defaultWidth, defaultHeight, "Task Finder - Horror Project");

    if (!IsWindowReady()) {
        printf("[INFO] Graphical window could not be opened, falling back to CLI mode...\n");
        FILE *err_f = fopen("task_finder_error.log", "w");
        if (err_f) {
            fprintf(err_f, "Graphical window failed to initialize (OpenGL/driver issue). Running in fallback mode.\n");
            fclose(err_f);
        }
        RunCliMode(repo_dir, github_base, &all_tasks, cli_user);
        return 0;
    }

    SetTargetFPS(60);

    AppScreen current_screen = SCREEN_LOGIN;
    AppScreen screen_before_create = SCREEN_LOGIN;
    AppScreen screen_before_manage = SCREEN_LOGIN;

    char username_input[MAX_STR] = "";
    int username_len = 0;

    // Active & free tasks
    static Task active_tasks[MAX_TASKS];
    static char active_roles[MAX_TASKS][64];
    int active_task_count = 0;

    static Task free_tasks[MAX_TASKS];
    int free_task_count = 0;

    float scroll_y = 0.0f;

    // Create project form state
    enum {
        FIELD_NAME = 0,
        FIELD_BRANCH,
        FIELD_DESC,
        FIELD_TM,
        FIELD_DEADLINE,
        FIELD_COUNT
    };
    char form_name[MAX_STR] = "";
    char form_branch[MAX_STR] = "";
    char form_desc[MAX_STR] = "";
    char form_tm[MAX_STR] = "";
    char form_deadline[64] = "";
    int form_focus = FIELD_NAME;
    bool form_branch_manual = false;

    static Task managed_parents[MAX_TASKS];
    int managed_parent_count = 0;
    int form_parent_idx = 0;

    // Manage task state
    char manage_desc_buf[512] = "";
    int manage_parent_idx = 0;

    // Toast notification
    char toast_msg[256] = "";
    float toast_timer = 0.0f;
    bool toast_is_error = false;

    // UI Colors
    Color colBg        = (Color){ 20, 22, 28, 255 };
    Color colCard      = (Color){ 32, 36, 46, 255 };
    Color colCardHover = (Color){ 42, 48, 62, 255 };
    Color colAccent    = (Color){ 52, 120, 246, 255 };
    Color colAccentHov = (Color){ 70, 140, 255, 255 };
    Color colGreen     = (Color){ 40, 167, 69, 255 };
    Color colGreenHov  = (Color){ 50, 190, 80, 255 };
    Color colCyan      = (Color){ 23, 162, 184, 255 };
    Color colCyanHov   = (Color){ 30, 185, 210, 255 };
    Color colGold      = (Color){ 245, 175, 25, 255 };
    Color colRed       = (Color){ 220, 53, 69, 255 };

    while (!WindowShouldClose()) {
        int winW = GetScreenWidth();
        int winH = GetScreenHeight();
        Vector2 mouse = GetMousePosition();

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
                if (username_len > 0) {
                    EvaluateUserTasks(&all_tasks, username_input, active_tasks, active_roles, &active_task_count, free_tasks, &free_task_count);
                }
                current_screen = SCREEN_LOGIN;
            }
            UnloadDroppedFiles(dropped);
        }

        // Screen logic
        if (current_screen == SCREEN_LOGIN) {
            int key = GetCharPressed();
            while (key > 0) {
                if ((key >= 32) && (key <= 125) && (username_len < MAX_STR - 1)) {
                    username_input[username_len++] = (char)key;
                    username_input[username_len] = '\0';
                }
                key = GetCharPressed();
            }

            if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
                if (username_len > 0) {
                    username_input[--username_len] = '\0';
                }
            }

            bool submit = IsKeyPressed(KEY_ENTER);
            Rectangle btnCheck = { winW / 2.0f - 110, 360, 220, 44 };
            bool btnHover = CheckCollisionPointRec(mouse, btnCheck);

            if (btnHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                submit = true;
            }

            if (submit && username_len > 0) {
                EvaluateUserTasks(&all_tasks, username_input, active_tasks, active_roles, &active_task_count, free_tasks, &free_task_count);
                managed_parent_count = GetUserManagedTasks(&all_tasks, username_input, managed_parents);
                scroll_y = 0.0f;
                if (active_task_count > 0) {
                    current_screen = SCREEN_ACTIVE_TASK;
                } else {
                    current_screen = SCREEN_FREE_TASKS;
                }
            }
        } else if (current_screen == SCREEN_CREATE_PROJECT) {
            // Tab key navigates between fields
            if (IsKeyPressed(KEY_TAB)) {
                if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
                    form_focus = (form_focus - 1 + FIELD_COUNT) % FIELD_COUNT;
                } else {
                    form_focus = (form_focus + 1) % FIELD_COUNT;
                }
            }

            char *active_buf = NULL;
            size_t active_max = MAX_STR;
            switch (form_focus) {
                case FIELD_NAME: active_buf = form_name; break;
                case FIELD_BRANCH: active_buf = form_branch; break;
                case FIELD_DESC: active_buf = form_desc; break;
                case FIELD_TM: active_buf = form_tm; break;
                case FIELD_DEADLINE: active_buf = form_deadline; active_max = 64; break;
            }

            // Backspace handling
            if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
                if (active_buf) {
                    size_t len = strlen(active_buf);
                    if (len > 0) {
                        active_buf[len - 1] = '\0';
                        if (form_focus == FIELD_BRANCH) {
                            form_branch_manual = true;
                        } else if (form_focus == FIELD_NAME && !form_branch_manual) {
                            GenerateBranchSlug(form_name, form_branch, sizeof(form_branch));
                        }
                    }
                }
            }

            // Text input handling
            int key = GetCharPressed();
            while (key > 0) {
                if (key >= 32 && key <= 125 && active_buf) {
                    size_t len = strlen(active_buf);
                    if (len < active_max - 1) {
                        active_buf[len] = (char)key;
                        active_buf[len + 1] = '\0';
                        if (form_focus == FIELD_BRANCH) {
                            form_branch_manual = true;
                        } else if (form_focus == FIELD_NAME && !form_branch_manual) {
                            GenerateBranchSlug(form_name, form_branch, sizeof(form_branch));
                        }
                    }
                }
                key = GetCharPressed();
            }
        } else if (current_screen == SCREEN_MANAGE_TASK) {
            // Backspace handling for manage_desc_buf
            if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
                size_t len = strlen(manage_desc_buf);
                if (len > 0) {
                    manage_desc_buf[len - 1] = '\0';
                }
            }

            // Text input handling for manage_desc_buf
            int key = GetCharPressed();
            while (key > 0) {
                if (key >= 32 && key <= 125) {
                    size_t len = strlen(manage_desc_buf);
                    if (len < sizeof(manage_desc_buf) - 2) {
                        manage_desc_buf[len] = (char)key;
                        manage_desc_buf[len + 1] = '\0';
                    }
                }
                key = GetCharPressed();
            }
        } else {
            // Mouse wheel scrolling for task lists
            float wheel = GetMouseWheelMove();
            scroll_y += wheel * 35.0f;
            if (scroll_y > 0.0f) scroll_y = 0.0f;
        }

        // DRAWING
        BeginDrawing();
        ClearBackground(colBg);

        if (current_screen == SCREEN_LOGIN) {
            // Top right: + New Project button
            Rectangle btnNewLogin = { winW - 170.0f, 15, 145, 36 };
            bool newLogHov = CheckCollisionPointRec(mouse, btnNewLogin);
            DrawRectangleRounded(btnNewLogin, 0.15f, 4, newLogHov ? colGreenHov : colGreen);
            DrawText("+ New Project", (int)(btnNewLogin.x + btnNewLogin.width / 2 - MeasureText("+ New Project", 15) / 2), (int)btnNewLogin.y + 10, 15, RAYWHITE);
            if (newLogHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (username_len == 0) {
                    snprintf(toast_msg, sizeof(toast_msg), "Please enter your name first");
                    toast_timer = 3.5f;
                    toast_is_error = true;
                } else {
                    managed_parent_count = GetUserManagedTasks(&all_tasks, username_input, managed_parents);
                    if (managed_parent_count == 0) {
                        snprintf(toast_msg, sizeof(toast_msg), "You are not Task Master of any task. Only Task Masters can create subtasks.");
                        toast_timer = 4.0f;
                        toast_is_error = true;
                    } else {
                        screen_before_create = SCREEN_LOGIN;
                        current_screen = SCREEN_CREATE_PROJECT;
                        form_name[0] = '\0';
                        form_branch[0] = '\0';
                        form_desc[0] = '\0';
                        form_tm[0] = '\0';
                        form_deadline[0] = '\0';
                        form_parent_idx = 0;
                        form_focus = FIELD_NAME;
                        form_branch_manual = false;
                    }
                }
            }

            DrawText("TASK FINDER", winW / 2 - MeasureText("TASK FINDER", 34) / 2, 130, 34, RAYWHITE);
            DrawText("Git Branch Task Management & Finder", winW / 2 - MeasureText("Git Branch Task Management & Finder", 16) / 2, 175, 16, LIGHTGRAY);

            Rectangle cardRec = { winW / 2.0f - 240, 220, 480, 210 };
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

            Rectangle btnCheck = { winW / 2.0f - 110, cardRec.y + 130, 220, 44 };
            bool btnHover = CheckCollisionPointRec(mouse, btnCheck);
            DrawRectangleRounded(btnCheck, 0.15f, 4, btnHover ? colAccentHov : colAccent);
            const char *btnTxt = "Check Tasks";
            DrawText(btnTxt, (int)(btnCheck.x + btnCheck.width / 2 - MeasureText(btnTxt, 18) / 2), (int)(btnCheck.y + 13), 18, RAYWHITE);

            // Repository info at bottom
            char info_buf[512];
            if (!g_git_available) {
                snprintf(info_buf, sizeof(info_buf), "Git executable not found in PATH or standard install locations!");
                DrawText(info_buf, winW / 2 - MeasureText(info_buf, 14) / 2, winH - 35, 14, (Color){ 220, 80, 80, 255 });
            } else if (all_tasks.count > 0) {
                snprintf(info_buf, sizeof(info_buf), "Repository: %s (%d task branches)", repo_dir, all_tasks.count);
                DrawText(info_buf, winW / 2 - MeasureText(info_buf, 14) / 2, winH - 35, 14, DARKGRAY);
            } else {
                snprintf(info_buf, sizeof(info_buf), "Repository: %s (0 task branches)  [Tip: Drag & drop folder here]", repo_dir);
                DrawText(info_buf, winW / 2 - MeasureText(info_buf, 14) / 2, winH - 35, 14, (Color){ 220, 150, 50, 255 });
            }

        } else if (current_screen == SCREEN_ACTIVE_TASK) {
            // SCREEN: ACTIVE TASK ASSIGNED
            Rectangle topBar = { 0, 0, (float)winW, 65 };
            DrawRectangleRec(topBar, (Color){ 26, 30, 38, 255 });
            DrawText("TASK FINDER", 25, 20, 24, RAYWHITE);

            // Button: Manage Tasks (if user is Task Master of any tasks)
            int managed_count_top = GetUserManagedTasks(&all_tasks, username_input, NULL);
            if (managed_count_top > 0) {
                Rectangle btnManageTop = { winW - 510.0f, 15, 150, 36 };
                bool manHov = CheckCollisionPointRec(mouse, btnManageTop);
                DrawRectangleRounded(btnManageTop, 0.15f, 4, manHov ? colCyanHov : colCyan);
                char manTxt[64];
                snprintf(manTxt, sizeof(manTxt), "Manage Tasks (%d)", managed_count_top);
                DrawText(manTxt, (int)(btnManageTop.x + btnManageTop.width / 2 - MeasureText(manTxt, 14) / 2), (int)btnManageTop.y + 11, 14, RAYWHITE);
                if (manHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    managed_parent_count = GetUserManagedTasks(&all_tasks, username_input, managed_parents);
                    manage_parent_idx = 0;
                    if (managed_parent_count > 0) {
                        strncpy(manage_desc_buf, managed_parents[0].description, sizeof(manage_desc_buf) - 1);
                        manage_desc_buf[sizeof(manage_desc_buf) - 1] = '\0';
                    }
                    screen_before_manage = SCREEN_ACTIVE_TASK;
                    current_screen = SCREEN_MANAGE_TASK;
                }
            }

            // Button: + New Project
            Rectangle btnNew = { winW - 350.0f, 15, 150, 36 };
            bool newHov = CheckCollisionPointRec(mouse, btnNew);
            DrawRectangleRounded(btnNew, 0.15f, 4, newHov ? colGreenHov : colGreen);
            DrawText("+ New Project", (int)(btnNew.x + btnNew.width / 2 - MeasureText("+ New Project", 15) / 2), (int)btnNew.y + 10, 15, RAYWHITE);
            if (newHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                managed_parent_count = GetUserManagedTasks(&all_tasks, username_input, managed_parents);
                if (managed_parent_count == 0) {
                    snprintf(toast_msg, sizeof(toast_msg), "You are not Task Master of any task. Only Task Masters can create subtasks.");
                    toast_timer = 4.0f;
                    toast_is_error = true;
                } else {
                    screen_before_create = SCREEN_ACTIVE_TASK;
                    current_screen = SCREEN_CREATE_PROJECT;
                    form_name[0] = '\0';
                    form_branch[0] = '\0';
                    form_desc[0] = '\0';
                    form_tm[0] = '\0';
                    form_deadline[0] = '\0';
                    form_parent_idx = 0;
                    form_focus = FIELD_NAME;
                    form_branch_manual = false;
                }
            }

            // Button: Change User
            Rectangle btnBack = { winW - 180.0f, 15, 150, 36 };
            bool backHov = CheckCollisionPointRec(mouse, btnBack);
            DrawRectangleRounded(btnBack, 0.15f, 4, backHov ? colCardHover : colCard);
            DrawText("Change User", (int)(btnBack.x + btnBack.width / 2 - MeasureText("Change User", 15) / 2), (int)btnBack.y + 10, 15, RAYWHITE);
            if (backHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                current_screen = SCREEN_LOGIN;
            }

            float curY = 90 + scroll_y;

            DrawText("ACTIVE TASK ASSIGNED", 40, (int)curY, 22, colGold);
            curY += 35;
            DrawText("Complete your active task before picking new tasks.", 40, (int)curY, 15, LIGHTGRAY);
            curY += 35;

            for (int i = 0; i < active_task_count; i++) {
                Task *t = &active_tasks[i];
                Rectangle card = { 40, curY, (float)winW - 80, 165 };
                DrawRectangleRounded(card, 0.05f, 6, colCard);

                // Name
                DrawText(t->name, (int)card.x + 25, (int)card.y + 18, 22, RAYWHITE);

                // Role tag
                Rectangle roleTag = { card.x + 25, card.y + 48, (float)MeasureText(active_roles[i], 14) + 16, 24 };
                DrawRectangleRounded(roleTag, 0.3f, 4, (Color){ 200, 130, 10, 255 });
                DrawText(active_roles[i], (int)roleTag.x + 8, (int)roleTag.y + 5, 14, RAYWHITE);

                // Branch
                char branchTxt[256];
                snprintf(branchTxt, sizeof(branchTxt), "Branch: %s", t->branch);
                DrawText(branchTxt, (int)roleTag.x + (int)roleTag.width + 15, (int)card.y + 51, 15, LIGHTGRAY);

                // Description preview if exists
                if (strlen(t->description) > 0) {
                    char descTxt[120];
                    snprintf(descTxt, sizeof(descTxt), "Desc: %.60s%s", t->description, strlen(t->description) > 60 ? "..." : "");
                    DrawText(descTxt, (int)card.x + 25, (int)card.y + 82, 14, (Color){ 190, 195, 205, 255 });
                } else {
                    DrawText("Desc: (none)", (int)card.x + 25, (int)card.y + 82, 14, DARKGRAY);
                }

                // Deadline
                char dlTxt[256];
                if (t->has_deadline) {
                    if (t->days_to_deadline > 0) {
                        snprintf(dlTxt, sizeof(dlTxt), "Deadline: %04d/%02d/%02d (%d days left)", t->deadline_year, t->deadline_month, t->deadline_day, t->days_to_deadline);
                        DrawText(dlTxt, (int)card.x + 25, (int)card.y + 106, 14, (Color){ 200, 200, 200, 255 });
                    } else if (t->days_to_deadline == 0) {
                        DrawText("Deadline: TODAY!", (int)card.x + 25, (int)card.y + 106, 14, colRed);
                    } else {
                        snprintf(dlTxt, sizeof(dlTxt), "Deadline: %04d/%02d/%02d (OVERDUE: %d days!)", t->deadline_year, t->deadline_month, t->deadline_day, -t->days_to_deadline);
                        DrawText(dlTxt, (int)card.x + 25, (int)card.y + 106, 14, colRed);
                    }
                } else {
                    DrawText("Deadline: None", (int)card.x + 25, (int)card.y + 106, 14, DARKGRAY);
                }

                // Status & Workers line
                char stInfo[256];
                snprintf(stInfo, sizeof(stInfo), "Status: %s  |  Workers: %d%s",
                         t->status, t->worker_count,
                         t->is_worker_free ? " (slot open)" : "");
                DrawText(stInfo, (int)card.x + 25, (int)card.y + 130, 14, (strcasecmp(t->status, "available") == 0) ? colGreen : LIGHTGRAY);

                bool is_tm = NamesEqual(t->task_master, username_input);
                if (is_tm) {
                    float bW = 142.0f;
                    float bH = 34.0f;
                    float rX = card.x + card.width - 310.0f;

                    // Row 1: [ Open on GitHub ]  [ Manage Details ]
                    Rectangle btnGit = { rX, card.y + 25, bW, bH };
                    bool gitHov = CheckCollisionPointRec(mouse, btnGit);
                    DrawRectangleRounded(btnGit, 0.15f, 4, gitHov ? colAccentHov : colAccent);
                    const char *gtTxt = "Open on GitHub";
                    DrawText(gtTxt, (int)(btnGit.x + btnGit.width / 2 - MeasureText(gtTxt, 13) / 2), (int)btnGit.y + 10, 13, RAYWHITE);
                    if (gitHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        OpenURL(t->github_url);
                    }

                    Rectangle btnDetails = { rX + bW + 12, card.y + 25, bW, bH };
                    bool detHov = CheckCollisionPointRec(mouse, btnDetails);
                    DrawRectangleRounded(btnDetails, 0.15f, 4, detHov ? colCyanHov : colCyan);
                    const char *detTxt = "Manage Details";
                    DrawText(detTxt, (int)(btnDetails.x + btnDetails.width / 2 - MeasureText(detTxt, 13) / 2), (int)btnDetails.y + 10, 13, RAYWHITE);
                    if (detHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        managed_parent_count = GetUserManagedTasks(&all_tasks, username_input, managed_parents);
                        manage_parent_idx = 0;
                        for (int m = 0; m < managed_parent_count; m++) {
                            if (strcmp(managed_parents[m].branch, t->branch) == 0) {
                                manage_parent_idx = m;
                                break;
                            }
                        }
                        if (managed_parent_count > 0) {
                            strncpy(manage_desc_buf, managed_parents[manage_parent_idx].description, sizeof(manage_desc_buf) - 1);
                            manage_desc_buf[sizeof(manage_desc_buf) - 1] = '\0';
                        }
                        screen_before_manage = SCREEN_ACTIVE_TASK;
                        current_screen = SCREEN_MANAGE_TASK;
                    }

                    // Row 2: [ Finish Task / Reopen ]  [ Request Worker ]
                    bool is_avail = (strcasecmp(t->status, "available") == 0);
                    Rectangle btnToggle = { rX, card.y + 70, bW, bH };
                    bool togHov = CheckCollisionPointRec(mouse, btnToggle);
                    DrawRectangleRounded(btnToggle, 0.15f, 4, togHov ? (is_avail ? colGreenHov : (Color){ 220, 150, 40, 255 }) : (is_avail ? colGreen : (Color){ 195, 130, 25, 255 }));
                    const char *togTxt = is_avail ? "Finish Task" : "Reopen Task";
                    DrawText(togTxt, (int)(btnToggle.x + btnToggle.width / 2 - MeasureText(togTxt, 13) / 2), (int)btnToggle.y + 10, 13, RAYWHITE);
                    if (togHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        char st_msg[512] = "";
                        bool ok = PerformToggleStatus(repo_dir, username_input, t->branch, t->status, st_msg, sizeof(st_msg));
                        snprintf(toast_msg, sizeof(toast_msg), "%s", st_msg);
                        toast_timer = 4.0f;
                        toast_is_error = !ok;
                        if (ok) {
                            LoadAllTasks(repo_dir, github_base, &all_tasks);
                            managed_parent_count = GetUserManagedTasks(&all_tasks, username_input, managed_parents);
                            EvaluateUserTasks(&all_tasks, username_input, active_tasks, active_roles, &active_task_count, free_tasks, &free_task_count);
                            if (active_task_count == 0) current_screen = SCREEN_FREE_TASKS;
                        }
                    }

                    Rectangle btnReqW = { rX + bW + 12, card.y + 70, bW, bH };
                    bool reqHov = CheckCollisionPointRec(mouse, btnReqW);
                    Color colReqW = (Color){ 105, 70, 175, 255 };
                    Color colReqWHov = (Color){ 125, 90, 205, 255 };
                    DrawRectangleRounded(btnReqW, 0.15f, 4, reqHov ? colReqWHov : colReqW);
                    const char *reqTxt = "Request Worker";
                    DrawText(reqTxt, (int)(btnReqW.x + btnReqW.width / 2 - MeasureText(reqTxt, 13) / 2), (int)btnReqW.y + 10, 13, RAYWHITE);
                    if (reqHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        char st_msg[512] = "";
                        bool ok = PerformRequestWorker(repo_dir, username_input, t->branch, st_msg, sizeof(st_msg));
                        snprintf(toast_msg, sizeof(toast_msg), "%s", st_msg);
                        toast_timer = 4.0f;
                        toast_is_error = !ok;
                        if (ok) {
                            LoadAllTasks(repo_dir, github_base, &all_tasks);
                            managed_parent_count = GetUserManagedTasks(&all_tasks, username_input, managed_parents);
                            EvaluateUserTasks(&all_tasks, username_input, active_tasks, active_roles, &active_task_count, free_tasks, &free_task_count);
                        }
                    }
                } else {
                    // Button: Open on GitHub (Worker role)
                    Rectangle btnGit = { card.x + card.width - 210, card.y + card.height / 2 - 20, 190, 42 };
                    bool gitHov = CheckCollisionPointRec(mouse, btnGit);
                    DrawRectangleRounded(btnGit, 0.15f, 4, gitHov ? colAccentHov : colAccent);
                    const char *gtTxt = "Open on GitHub";
                    DrawText(gtTxt, (int)(btnGit.x + btnGit.width / 2 - MeasureText(gtTxt, 16) / 2), (int)btnGit.y + 12, 16, RAYWHITE);

                    if (gitHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        OpenURL(t->github_url);
                    }
                }

                curY += card.height + 20;
            }

        } else if (current_screen == SCREEN_FREE_TASKS) {
            // SCREEN: AVAILABLE FREE TASKS
            Rectangle topBar = { 0, 0, (float)winW, 65 };
            DrawRectangleRec(topBar, (Color){ 26, 30, 38, 255 });
            DrawText("TASK FINDER", 25, 20, 24, RAYWHITE);

            // Button: Manage Tasks (if user is Task Master of any tasks)
            int managed_count_free_top = GetUserManagedTasks(&all_tasks, username_input, NULL);
            if (managed_count_free_top > 0) {
                Rectangle btnManageTop = { winW - 510.0f, 15, 150, 36 };
                bool manHov = CheckCollisionPointRec(mouse, btnManageTop);
                DrawRectangleRounded(btnManageTop, 0.15f, 4, manHov ? colCyanHov : colCyan);
                char manTxt[64];
                snprintf(manTxt, sizeof(manTxt), "Manage Tasks (%d)", managed_count_free_top);
                DrawText(manTxt, (int)(btnManageTop.x + btnManageTop.width / 2 - MeasureText(manTxt, 14) / 2), (int)btnManageTop.y + 11, 14, RAYWHITE);
                if (manHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    managed_parent_count = GetUserManagedTasks(&all_tasks, username_input, managed_parents);
                    manage_parent_idx = 0;
                    if (managed_parent_count > 0) {
                        strncpy(manage_desc_buf, managed_parents[0].description, sizeof(manage_desc_buf) - 1);
                        manage_desc_buf[sizeof(manage_desc_buf) - 1] = '\0';
                    }
                    screen_before_manage = SCREEN_FREE_TASKS;
                    current_screen = SCREEN_MANAGE_TASK;
                }
            }

            // Button: + New Project
            Rectangle btnNew = { winW - 350.0f, 15, 150, 36 };
            bool newHov = CheckCollisionPointRec(mouse, btnNew);
            DrawRectangleRounded(btnNew, 0.15f, 4, newHov ? colGreenHov : colGreen);
            DrawText("+ New Project", (int)(btnNew.x + btnNew.width / 2 - MeasureText("+ New Project", 15) / 2), (int)btnNew.y + 10, 15, RAYWHITE);
            if (newHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                managed_parent_count = GetUserManagedTasks(&all_tasks, username_input, managed_parents);
                if (managed_parent_count == 0) {
                    snprintf(toast_msg, sizeof(toast_msg), "You are not Task Master of any task. Only Task Masters can create subtasks.");
                    toast_timer = 4.0f;
                    toast_is_error = true;
                } else {
                    screen_before_create = SCREEN_FREE_TASKS;
                    current_screen = SCREEN_CREATE_PROJECT;
                    form_name[0] = '\0';
                    form_branch[0] = '\0';
                    form_desc[0] = '\0';
                    form_tm[0] = '\0';
                    form_deadline[0] = '\0';
                    form_parent_idx = 0;
                    form_focus = FIELD_NAME;
                    form_branch_manual = false;
                }
            }

            // Button: Change User
            Rectangle btnBack = { winW - 180.0f, 15, 150, 36 };
            bool backHov = CheckCollisionPointRec(mouse, btnBack);
            DrawRectangleRounded(btnBack, 0.15f, 4, backHov ? colCardHover : colCard);
            DrawText("Change User", (int)(btnBack.x + btnBack.width / 2 - MeasureText("Change User", 15) / 2), (int)btnBack.y + 10, 15, RAYWHITE);
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
                Rectangle card = { 40, curY, (float)winW - 80, 115 };
                bool cardHover = CheckCollisionPointRec(mouse, card);
                DrawRectangleRounded(card, 0.06f, 6, cardHover ? colCardHover : colCard);

                // Index and title
                char title[MAX_STR];
                snprintf(title, sizeof(title), "%d. %.200s", i + 1, t->name);
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

                // Action Buttons on Card (Right Aligned)
                float btnRight = card.x + card.width - 20;

                // Button: Open on GitHub
                Rectangle btnGit = { btnRight - 140, card.y + card.height / 2 - 18, 140, 36 };
                bool gitHov = CheckCollisionPointRec(mouse, btnGit);
                DrawRectangleRounded(btnGit, 0.15f, 4, gitHov ? colAccentHov : colAccent);
                const char *gtTxt = "Open on GitHub";
                DrawText(gtTxt, (int)(btnGit.x + btnGit.width / 2 - MeasureText(gtTxt, 14) / 2), (int)btnGit.y + 10, 14, RAYWHITE);
                if (gitHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    OpenURL(t->github_url);
                }
                btnRight -= 150;

                // Button: Claim Worker
                if (t->requires_worker && t->is_worker_free) {
                    Rectangle btnClaimW = { btnRight - 125, card.y + card.height / 2 - 18, 125, 36 };
                    bool wHov = CheckCollisionPointRec(mouse, btnClaimW);
                    DrawRectangleRounded(btnClaimW, 0.15f, 4, wHov ? colGreenHov : colGreen);
                    DrawText("Claim Worker", (int)(btnClaimW.x + btnClaimW.width / 2 - MeasureText("Claim Worker", 14) / 2), (int)btnClaimW.y + 10, 14, RAYWHITE);
                    if (wHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        char status_buf[512] = "";
                        bool ok = PerformClaimTask(repo_dir, t, username_input, false, status_buf, sizeof(status_buf));
                        snprintf(toast_msg, sizeof(toast_msg), "%s", status_buf);
                        toast_timer = 4.0f;
                        toast_is_error = !ok;
                        if (ok) {
                            LoadAllTasks(repo_dir, github_base, &all_tasks);
                            managed_parent_count = GetUserManagedTasks(&all_tasks, username_input, managed_parents);
                            EvaluateUserTasks(&all_tasks, username_input, active_tasks, active_roles, &active_task_count, free_tasks, &free_task_count);
                            current_screen = SCREEN_ACTIVE_TASK;
                        }
                    }
                    btnRight -= 135;
                }

                // Button: Claim TM
                if (t->is_task_master_free) {
                    Rectangle btnClaimTM = { btnRight - 110, card.y + card.height / 2 - 18, 110, 36 };
                    bool tmHov = CheckCollisionPointRec(mouse, btnClaimTM);
                    DrawRectangleRounded(btnClaimTM, 0.15f, 4, tmHov ? colCyanHov : colCyan);
                    DrawText("Claim TM", (int)(btnClaimTM.x + btnClaimTM.width / 2 - MeasureText("Claim TM", 14) / 2), (int)btnClaimTM.y + 10, 14, RAYWHITE);
                    if (tmHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        char status_buf[512] = "";
                        bool ok = PerformClaimTask(repo_dir, t, username_input, true, status_buf, sizeof(status_buf));
                        snprintf(toast_msg, sizeof(toast_msg), "%s", status_buf);
                        toast_timer = 4.0f;
                        toast_is_error = !ok;
                        if (ok) {
                            LoadAllTasks(repo_dir, github_base, &all_tasks);
                            managed_parent_count = GetUserManagedTasks(&all_tasks, username_input, managed_parents);
                            EvaluateUserTasks(&all_tasks, username_input, active_tasks, active_roles, &active_task_count, free_tasks, &free_task_count);
                            current_screen = SCREEN_ACTIVE_TASK;
                        }
                    }
                    btnRight -= 120;
                }

                curY += card.height + 15;
            }

        } else if (current_screen == SCREEN_CREATE_PROJECT) {
            // SCREEN: CREATE NEW SUBTASK
            Rectangle topBar = { 0, 0, (float)winW, 65 };
            DrawRectangleRec(topBar, (Color){ 26, 30, 38, 255 });
            DrawText("TASK FINDER", 25, 20, 24, RAYWHITE);

            // Button: Cancel / Back
            Rectangle btnBack = { winW - 170.0f, 15, 145, 36 };
            bool backHov = CheckCollisionPointRec(mouse, btnBack);
            DrawRectangleRounded(btnBack, 0.15f, 4, backHov ? colCardHover : colCard);
            DrawText("Back to Tasks", (int)(btnBack.x + btnBack.width / 2 - MeasureText("Back to Tasks", 15) / 2), (int)btnBack.y + 10, 15, RAYWHITE);
            if (backHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                current_screen = screen_before_create;
            }

            Rectangle card = { winW / 2.0f - 380, 72, 760, 485 };
            DrawRectangleRounded(card, 0.04f, 6, colCard);

            DrawText("Create New Subtask", (int)card.x + 35, (int)card.y + 16, 22, RAYWHITE);
            DrawText("Creates a subtask linked to your parent task, commits all changes and pushes.", (int)card.x + 35, (int)card.y + 42, 14, LIGHTGRAY);

            bool cursor_on = (((int)(GetTime() * 2)) % 2 == 0);

            // 0. Parent Task Selection (Required)
            DrawText("Parent Task (You manage as Task Master) *", (int)card.x + 35, (int)card.y + 68, 14, colGold);
            Rectangle recParent = { card.x + 35, card.y + 88, card.width - 70, 38 };
            DrawRectangleRounded(recParent, 0.12f, 4, (Color){ 26, 30, 40, 255 });
            DrawRectangleRoundedLines(recParent, 0.12f, 4, (Color){ 70, 80, 105, 255 });

            if (managed_parent_count > 0) {
                char p_label[256];
                snprintf(p_label, sizeof(p_label), "Parent: %s  [Branch: %s]",
                         managed_parents[form_parent_idx].name, managed_parents[form_parent_idx].branch);
                DrawText(p_label, (int)recParent.x + 12, (int)recParent.y + 11, 14, RAYWHITE);

                if (managed_parent_count > 1) {
                    Rectangle btnPrevP = { recParent.x + recParent.width - 150, recParent.y + 4, 68, 30 };
                    Rectangle btnNextP = { recParent.x + recParent.width - 76, recParent.y + 4, 68, 30 };
                    bool prevHov = CheckCollisionPointRec(mouse, btnPrevP);
                    bool nextHov = CheckCollisionPointRec(mouse, btnNextP);

                    DrawRectangleRounded(btnPrevP, 0.15f, 4, prevHov ? colCardHover : (Color){ 45, 52, 68, 255 });
                    DrawText("< Prev", (int)(btnPrevP.x + btnPrevP.width / 2 - MeasureText("< Prev", 13) / 2), (int)btnPrevP.y + 8, 13, RAYWHITE);
                    DrawRectangleRounded(btnNextP, 0.15f, 4, nextHov ? colCardHover : (Color){ 45, 52, 68, 255 });
                    DrawText("Next >", (int)(btnNextP.x + btnNextP.width / 2 - MeasureText("Next >", 13) / 2), (int)btnNextP.y + 8, 13, RAYWHITE);

                    if (prevHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        form_parent_idx = (form_parent_idx - 1 + managed_parent_count) % managed_parent_count;
                    }
                    if (nextHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        form_parent_idx = (form_parent_idx + 1) % managed_parent_count;
                    }
                } else {
                    const char *badge = "[ Only managed task ]";
                    DrawText(badge, (int)(recParent.x + recParent.width - MeasureText(badge, 13) - 12), (int)recParent.y + 11, 13, LIGHTGRAY);
                }
            } else {
                DrawText("No managed parent task available!", (int)recParent.x + 12, (int)recParent.y + 11, 14, colRed);
            }

            // 1. Subtask Name
            DrawText("Subtask Name *", (int)card.x + 35, (int)card.y + 134, 14, (form_focus == FIELD_NAME) ? colAccent : RAYWHITE);
            Rectangle recName = { card.x + 35, card.y + 154, card.width - 70, 34 };
            if (CheckCollisionPointRec(mouse, recName) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) form_focus = FIELD_NAME;
            DrawRectangleRounded(recName, 0.12f, 4, (Color){ 22, 24, 30, 255 });
            DrawRectangleRoundedLines(recName, 0.12f, 4, (form_focus == FIELD_NAME) ? colAccent : (Color){ 60, 66, 80, 255 });
            if (strlen(form_name) == 0) {
                DrawText("e.g. Monster AI Pathfinding", (int)recName.x + 12, (int)recName.y + 9, 14, DARKGRAY);
            } else {
                DrawText(form_name, (int)recName.x + 12, (int)recName.y + 9, 14, RAYWHITE);
            }
            if (form_focus == FIELD_NAME && cursor_on) {
                int tw = MeasureText(form_name, 14);
                DrawRectangle((int)recName.x + 13 + tw, (int)recName.y + 8, 2, 18, colAccent);
            }

            // 2. Git Branch
            DrawText("Git Branch Name *", (int)card.x + 35, (int)card.y + 196, 14, (form_focus == FIELD_BRANCH) ? colAccent : RAYWHITE);
            Rectangle recBranch = { card.x + 35, card.y + 216, card.width - 70, 34 };
            if (CheckCollisionPointRec(mouse, recBranch) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                form_focus = FIELD_BRANCH;
                form_branch_manual = true;
            }
            DrawRectangleRounded(recBranch, 0.12f, 4, (Color){ 22, 24, 30, 255 });
            DrawRectangleRoundedLines(recBranch, 0.12f, 4, (form_focus == FIELD_BRANCH) ? colAccent : (Color){ 60, 66, 80, 255 });
            if (strlen(form_branch) == 0) {
                DrawText("task/your-task-slug", (int)recBranch.x + 12, (int)recBranch.y + 9, 14, DARKGRAY);
            } else {
                DrawText(form_branch, (int)recBranch.x + 12, (int)recBranch.y + 9, 14, RAYWHITE);
            }
            if (form_focus == FIELD_BRANCH && cursor_on) {
                int tw = MeasureText(form_branch, 14);
                DrawRectangle((int)recBranch.x + 13 + tw, (int)recBranch.y + 8, 2, 18, colAccent);
            }

            // 3. Description
            DrawText("Description", (int)card.x + 35, (int)card.y + 258, 14, (form_focus == FIELD_DESC) ? colAccent : RAYWHITE);
            Rectangle recDesc = { card.x + 35, card.y + 278, card.width - 70, 34 };
            if (CheckCollisionPointRec(mouse, recDesc) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) form_focus = FIELD_DESC;
            DrawRectangleRounded(recDesc, 0.12f, 4, (Color){ 22, 24, 30, 255 });
            DrawRectangleRoundedLines(recDesc, 0.12f, 4, (form_focus == FIELD_DESC) ? colAccent : (Color){ 60, 66, 80, 255 });
            if (strlen(form_desc) == 0) {
                DrawText("Short task overview...", (int)recDesc.x + 12, (int)recDesc.y + 9, 14, DARKGRAY);
            } else {
                DrawText(form_desc, (int)recDesc.x + 12, (int)recDesc.y + 9, 14, RAYWHITE);
            }
            if (form_focus == FIELD_DESC && cursor_on) {
                int tw = MeasureText(form_desc, 14);
                DrawRectangle((int)recDesc.x + 13 + tw, (int)recDesc.y + 8, 2, 18, colAccent);
            }

            // 4. Two columns: Task Master & Deadline (Defaults: Empty!)
            float halfW = (card.width - 90) / 2.0f;

            DrawText("Task Master (leave empty for open)", (int)card.x + 35, (int)card.y + 320, 14, (form_focus == FIELD_TM) ? colAccent : RAYWHITE);
            Rectangle recTM = { card.x + 35, card.y + 340, halfW, 34 };
            if (CheckCollisionPointRec(mouse, recTM) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) form_focus = FIELD_TM;
            DrawRectangleRounded(recTM, 0.12f, 4, (Color){ 22, 24, 30, 255 });
            DrawRectangleRoundedLines(recTM, 0.12f, 4, (form_focus == FIELD_TM) ? colAccent : (Color){ 60, 66, 80, 255 });
            if (strlen(form_tm) == 0) {
                DrawText("Leave empty if open", (int)recTM.x + 12, (int)recTM.y + 9, 14, DARKGRAY);
            } else {
                DrawText(form_tm, (int)recTM.x + 12, (int)recTM.y + 9, 14, RAYWHITE);
            }
            if (form_focus == FIELD_TM && cursor_on) {
                int tw = MeasureText(form_tm, 14);
                DrawRectangle((int)recTM.x + 13 + tw, (int)recTM.y + 8, 2, 18, colAccent);
            }

            DrawText("Deadline (yy/MM/DD, optional)", (int)(card.x + 55 + halfW), (int)card.y + 320, 14, (form_focus == FIELD_DEADLINE) ? colAccent : RAYWHITE);
            Rectangle recDL = { card.x + 55 + halfW, card.y + 340, halfW, 34 };
            if (CheckCollisionPointRec(mouse, recDL) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) form_focus = FIELD_DEADLINE;
            DrawRectangleRounded(recDL, 0.12f, 4, (Color){ 22, 24, 30, 255 });
            DrawRectangleRoundedLines(recDL, 0.12f, 4, (form_focus == FIELD_DEADLINE) ? colAccent : (Color){ 60, 66, 80, 255 });
            if (strlen(form_deadline) == 0) {
                DrawText("e.g. 26/10/31", (int)recDL.x + 12, (int)recDL.y + 9, 14, DARKGRAY);
            } else {
                DrawText(form_deadline, (int)recDL.x + 12, (int)recDL.y + 9, 14, RAYWHITE);
            }
            if (form_focus == FIELD_DEADLINE && cursor_on) {
                int tw = MeasureText(form_deadline, 14);
                DrawRectangle((int)recDL.x + 13 + tw, (int)recDL.y + 8, 2, 18, colAccent);
            }

            // 5. Initial Status & Help
            DrawText("Initial Status:", (int)card.x + 35, (int)card.y + 388, 14, LIGHTGRAY);
            DrawText("available", (int)(card.x + 35 + MeasureText("Initial Status: ", 14)), (int)card.y + 388, 14, colGreen);
            DrawText("TAB to cycle fields, ENTER to submit", (int)(card.x + 55 + halfW), (int)card.y + 388, 12, DARKGRAY);

            // 6. Action buttons
            Rectangle btnCreate = { card.x + 35, card.y + 422, 250, 44 };
            bool createHov = CheckCollisionPointRec(mouse, btnCreate);
            DrawRectangleRounded(btnCreate, 0.15f, 4, createHov ? colGreenHov : colGreen);
            DrawText("Create & Push Subtask", (int)(btnCreate.x + btnCreate.width / 2 - MeasureText("Create & Push Subtask", 16) / 2), (int)btnCreate.y + 14, 16, RAYWHITE);

            Rectangle btnCancelForm = { card.x + 305, card.y + 422, 130, 44 };
            bool canHov = CheckCollisionPointRec(mouse, btnCancelForm);
            DrawRectangleRounded(btnCancelForm, 0.15f, 4, canHov ? colCardHover : (Color){ 45, 50, 65, 255 });
            DrawText("Cancel", (int)(btnCancelForm.x + btnCancelForm.width / 2 - MeasureText("Cancel", 16) / 2), (int)btnCancelForm.y + 14, 16, RAYWHITE);

            bool submit_create = (createHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) || (IsKeyPressed(KEY_ENTER) && form_focus != FIELD_DESC);
            if (canHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                current_screen = screen_before_create;
            }

            if (submit_create) {
                char *pn = TrimWhitespace(form_name);
                char *pb = TrimWhitespace(form_branch);
                if (managed_parent_count == 0) {
                    snprintf(toast_msg, sizeof(toast_msg), "No managed parent task selected");
                    toast_timer = 3.5f;
                    toast_is_error = true;
                } else if (strlen(pn) == 0) {
                    snprintf(toast_msg, sizeof(toast_msg), "Please enter a subtask name");
                    toast_timer = 3.5f;
                    toast_is_error = true;
                } else if (strlen(pb) == 0) {
                    snprintf(toast_msg, sizeof(toast_msg), "Please enter a branch name");
                    toast_timer = 3.5f;
                    toast_is_error = true;
                } else {
                    const char *p_branch = managed_parents[form_parent_idx].branch;
                    char status_msg[512] = "";
                    bool ok = PerformCreateProject(repo_dir, username_input, p_branch, pn, pb,
                                                   form_desc, form_tm, form_deadline,
                                                   status_msg, sizeof(status_msg));
                    snprintf(toast_msg, sizeof(toast_msg), "%s", status_msg);
                    toast_timer = 4.0f;
                    toast_is_error = !ok;
                    if (ok) {
                        LoadAllTasks(repo_dir, github_base, &all_tasks);
                        managed_parent_count = GetUserManagedTasks(&all_tasks, username_input, managed_parents);
                        EvaluateUserTasks(&all_tasks, username_input, active_tasks, active_roles, &active_task_count, free_tasks, &free_task_count);
                        if (active_task_count > 0) {
                            current_screen = SCREEN_ACTIVE_TASK;
                        } else {
                            current_screen = SCREEN_FREE_TASKS;
                        }
                    }
                }
            }
        } else if (current_screen == SCREEN_MANAGE_TASK) {
            // SCREEN: MANAGE TASKS (Task Master controls)
            Rectangle topBar = { 0, 0, (float)winW, 65 };
            DrawRectangleRec(topBar, (Color){ 26, 30, 38, 255 });
            DrawText("TASK FINDER", 25, 20, 24, RAYWHITE);
            DrawText("TASK MASTER CONTROLS", 185, 25, 16, colCyan);

            // Button: Back to Tasks
            Rectangle btnBack = { winW - 170.0f, 15, 145, 36 };
            bool backHov = CheckCollisionPointRec(mouse, btnBack);
            DrawRectangleRounded(btnBack, 0.15f, 4, backHov ? colCardHover : colCard);
            DrawText("Back to Tasks", (int)(btnBack.x + btnBack.width / 2 - MeasureText("Back to Tasks", 15) / 2), (int)btnBack.y + 10, 15, RAYWHITE);
            if (backHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                current_screen = screen_before_manage;
            }

            Rectangle card = { winW / 2.0f - 390, 75, 780, 500 };
            DrawRectangleRounded(card, 0.04f, 6, colCard);

            DrawText("Manage Task (Task Master Controls)", (int)card.x + 35, (int)card.y + 16, 22, RAYWHITE);
            char subhead[256];
            snprintf(subhead, sizeof(subhead), "Task Master: %s  |  Control status, worker requests, and description", username_input);
            DrawText(subhead, (int)card.x + 35, (int)card.y + 42, 14, LIGHTGRAY);

            if (managed_parent_count == 0) {
                DrawText("You do not manage any tasks as Task Master.", (int)card.x + 35, (int)card.y + 90, 16, colRed);
            } else {
                if (manage_parent_idx >= managed_parent_count) manage_parent_idx = managed_parent_count - 1;
                Task *cur_m = &managed_parents[manage_parent_idx];

                // 1. Task Switcher Box
                Rectangle recSwitch = { card.x + 35, card.y + 68, card.width - 70, 42 };
                DrawRectangleRounded(recSwitch, 0.12f, 4, (Color){ 24, 28, 38, 255 });
                DrawRectangleRoundedLines(recSwitch, 0.12f, 4, (Color){ 70, 80, 105, 255 });

                char task_label[320];
                snprintf(task_label, sizeof(task_label), "[%d/%d] %s   (Branch: %s)",
                         manage_parent_idx + 1, managed_parent_count, cur_m->name, cur_m->branch);
                DrawText(task_label, (int)recSwitch.x + 12, (int)recSwitch.y + 12, 15, RAYWHITE);

                if (managed_parent_count > 1) {
                    Rectangle btnPrev = { recSwitch.x + recSwitch.width - 150, recSwitch.y + 5, 68, 32 };
                    Rectangle btnNext = { recSwitch.x + recSwitch.width - 76, recSwitch.y + 5, 68, 32 };
                    bool prevHov = CheckCollisionPointRec(mouse, btnPrev);
                    bool nextHov = CheckCollisionPointRec(mouse, btnNext);
                    DrawRectangleRounded(btnPrev, 0.15f, 4, prevHov ? colCardHover : (Color){ 45, 52, 68, 255 });
                    DrawText("< Prev", (int)(btnPrev.x + btnPrev.width / 2 - MeasureText("< Prev", 13) / 2), (int)btnPrev.y + 9, 13, RAYWHITE);
                    DrawRectangleRounded(btnNext, 0.15f, 4, nextHov ? colCardHover : (Color){ 45, 52, 68, 255 });
                    DrawText("Next >", (int)(btnNext.x + btnNext.width / 2 - MeasureText("Next >", 13) / 2), (int)btnNext.y + 9, 13, RAYWHITE);

                    if (prevHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        manage_parent_idx = (manage_parent_idx - 1 + managed_parent_count) % managed_parent_count;
                        strncpy(manage_desc_buf, managed_parents[manage_parent_idx].description, sizeof(manage_desc_buf) - 1);
                        manage_desc_buf[sizeof(manage_desc_buf) - 1] = '\0';
                    }
                    if (nextHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        manage_parent_idx = (manage_parent_idx + 1) % managed_parent_count;
                        strncpy(manage_desc_buf, managed_parents[manage_parent_idx].description, sizeof(manage_desc_buf) - 1);
                        manage_desc_buf[sizeof(manage_desc_buf) - 1] = '\0';
                    }
                }

                // 2. Control Cards: Left = Status, Right = Worker
                float halfW = (card.width - 90) / 2.0f;

                // Box Left: Status
                Rectangle boxStatus = { card.x + 35, card.y + 122, halfW, 115 };
                DrawRectangleRounded(boxStatus, 0.1f, 4, (Color){ 24, 28, 38, 255 });
                DrawRectangleRoundedLines(boxStatus, 0.1f, 4, (Color){ 60, 68, 88, 255 });
                DrawText("TASK STATUS", (int)boxStatus.x + 14, (int)boxStatus.y + 12, 13, colGold);

                bool is_avail = (strcasecmp(cur_m->status, "available") == 0);
                char stShow[64];
                snprintf(stShow, sizeof(stShow), "Current: %s", cur_m->status);
                DrawText(stShow, (int)boxStatus.x + 14, (int)boxStatus.y + 32, 15, is_avail ? colGreen : LIGHTGRAY);

                Rectangle btnTogStat = { boxStatus.x + 14, boxStatus.y + 62, boxStatus.width - 28, 38 };
                bool togHov = CheckCollisionPointRec(mouse, btnTogStat);
                DrawRectangleRounded(btnTogStat, 0.15f, 4, togHov ? (is_avail ? colGreenHov : (Color){ 220, 150, 40, 255 }) : (is_avail ? colGreen : (Color){ 195, 130, 25, 255 }));
                const char *togLabel = is_avail ? "Finish Task (Set Completed)" : "Reopen Task (Set Available)";
                DrawText(togLabel, (int)(btnTogStat.x + btnTogStat.width / 2 - MeasureText(togLabel, 13) / 2), (int)btnTogStat.y + 12, 13, RAYWHITE);

                if (togHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    char st_msg[512] = "";
                    bool ok = PerformToggleStatus(repo_dir, username_input, cur_m->branch, cur_m->status, st_msg, sizeof(st_msg));
                    snprintf(toast_msg, sizeof(toast_msg), "%s", st_msg);
                    toast_timer = 4.0f;
                    toast_is_error = !ok;
                    if (ok) {
                        LoadAllTasks(repo_dir, github_base, &all_tasks);
                        managed_parent_count = GetUserManagedTasks(&all_tasks, username_input, managed_parents);
                        EvaluateUserTasks(&all_tasks, username_input, active_tasks, active_roles, &active_task_count, free_tasks, &free_task_count);
                        if (managed_parent_count > 0) {
                            if (manage_parent_idx >= managed_parent_count) manage_parent_idx = managed_parent_count - 1;
                            strncpy(manage_desc_buf, managed_parents[manage_parent_idx].description, sizeof(manage_desc_buf) - 1);
                            manage_desc_buf[sizeof(manage_desc_buf) - 1] = '\0';
                        }
                    }
                }

                // Box Right: Worker
                Rectangle boxWorker = { card.x + 55 + halfW, card.y + 122, halfW, 115 };
                DrawRectangleRounded(boxWorker, 0.1f, 4, (Color){ 24, 28, 38, 255 });
                DrawRectangleRoundedLines(boxWorker, 0.1f, 4, (Color){ 60, 68, 88, 255 });
                DrawText("WORKER SLOTS", (int)boxWorker.x + 14, (int)boxWorker.y + 12, 13, colCyan);

                char wInfo[128];
                if (!cur_m->requires_worker) {
                    snprintf(wInfo, sizeof(wInfo), "No worker requested (# WORKER absent)");
                } else if (cur_m->is_worker_free) {
                    snprintf(wInfo, sizeof(wInfo), "Slot open (waiting for worker)");
                } else {
                    snprintf(wInfo, sizeof(wInfo), "Assigned: %d worker(s)", cur_m->worker_count);
                }
                DrawText(wInfo, (int)boxWorker.x + 14, (int)boxWorker.y + 32, 14, LIGHTGRAY);

                Rectangle btnReqWorker = { boxWorker.x + 14, boxWorker.y + 62, boxWorker.width - 28, 38 };
                bool reqHov = CheckCollisionPointRec(mouse, btnReqWorker);
                Color colReqW = (Color){ 105, 70, 175, 255 };
                Color colReqWHov = (Color){ 125, 90, 205, 255 };
                DrawRectangleRounded(btnReqWorker, 0.15f, 4, reqHov ? colReqWHov : colReqW);
                const char *reqLabel = "Request Worker Slot (adds ---)";
                DrawText(reqLabel, (int)(btnReqWorker.x + btnReqWorker.width / 2 - MeasureText(reqLabel, 13) / 2), (int)btnReqWorker.y + 12, 13, RAYWHITE);

                if (reqHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    char st_msg[512] = "";
                    bool ok = PerformRequestWorker(repo_dir, username_input, cur_m->branch, st_msg, sizeof(st_msg));
                    snprintf(toast_msg, sizeof(toast_msg), "%s", st_msg);
                    toast_timer = 4.0f;
                    toast_is_error = !ok;
                    if (ok) {
                        LoadAllTasks(repo_dir, github_base, &all_tasks);
                        managed_parent_count = GetUserManagedTasks(&all_tasks, username_input, managed_parents);
                        EvaluateUserTasks(&all_tasks, username_input, active_tasks, active_roles, &active_task_count, free_tasks, &free_task_count);
                        if (managed_parent_count > 0) {
                            if (manage_parent_idx >= managed_parent_count) manage_parent_idx = managed_parent_count - 1;
                            strncpy(manage_desc_buf, managed_parents[manage_parent_idx].description, sizeof(manage_desc_buf) - 1);
                            manage_desc_buf[sizeof(manage_desc_buf) - 1] = '\0';
                        }
                    }
                }

                // 3. Description Editor
                DrawText("Task Description (Editable) *", (int)card.x + 35, (int)card.y + 250, 14, colAccent);
                Rectangle recDescBox = { card.x + 35, card.y + 272, card.width - 70, 75 };
                DrawRectangleRounded(recDescBox, 0.1f, 4, (Color){ 20, 23, 30, 255 });
                DrawRectangleRoundedLines(recDescBox, 0.1f, 4, colAccent);

                bool cursor_on = (((int)(GetTime() * 2)) % 2 == 0);
                int max_line_len = 80;
                int d_len = (int)strlen(manage_desc_buf);
                if (d_len == 0) {
                    DrawText("Type task description here...", (int)recDescBox.x + 12, (int)recDescBox.y + 12, 14, DARKGRAY);
                    if (cursor_on) {
                        DrawRectangle((int)recDescBox.x + 12, (int)recDescBox.y + 10, 2, 18, colAccent);
                    }
                } else if (d_len <= max_line_len) {
                    DrawText(manage_desc_buf, (int)recDescBox.x + 12, (int)recDescBox.y + 12, 14, RAYWHITE);
                    if (cursor_on) {
                        int tw = MeasureText(manage_desc_buf, 14);
                        DrawRectangle((int)recDescBox.x + 13 + tw, (int)recDescBox.y + 10, 2, 18, colAccent);
                    }
                } else {
                    char line1[128];
                    int split = max_line_len;
                    if (split > d_len) split = d_len;
                    strncpy(line1, manage_desc_buf, split);
                    line1[split] = '\0';
                    DrawText(line1, (int)recDescBox.x + 12, (int)recDescBox.y + 10, 14, RAYWHITE);
                    DrawText(manage_desc_buf + split, (int)recDescBox.x + 12, (int)recDescBox.y + 32, 14, RAYWHITE);
                    if (cursor_on) {
                        int tw = MeasureText(manage_desc_buf + split, 14);
                        DrawRectangle((int)recDescBox.x + 13 + tw, (int)recDescBox.y + 30, 2, 18, colAccent);
                    }
                }

                DrawText("Type to edit description. ENTER or click 'Save Description' to commit & push.", (int)card.x + 35, (int)card.y + 355, 12, DARKGRAY);

                // 4. Action Buttons at Bottom
                Rectangle btnSaveDesc = { card.x + 35, card.y + 385, 200, 44 };
                bool saveHov = CheckCollisionPointRec(mouse, btnSaveDesc);
                DrawRectangleRounded(btnSaveDesc, 0.15f, 4, saveHov ? colAccentHov : colAccent);
                const char *svTxt = "Save Description";
                DrawText(svTxt, (int)(btnSaveDesc.x + btnSaveDesc.width / 2 - MeasureText(svTxt, 15) / 2), (int)btnSaveDesc.y + 14, 15, RAYWHITE);

                Rectangle btnGitMan = { card.x + 250, card.y + 385, 170, 44 };
                bool gmHov = CheckCollisionPointRec(mouse, btnGitMan);
                DrawRectangleRounded(btnGitMan, 0.15f, 4, gmHov ? (Color){ 65, 78, 105, 255 } : (Color){ 50, 60, 80, 255 });
                const char *gmTxt = "Open on GitHub";
                DrawText(gmTxt, (int)(btnGitMan.x + btnGitMan.width / 2 - MeasureText(gmTxt, 15) / 2), (int)btnGitMan.y + 14, 15, RAYWHITE);
                if (gmHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    OpenURL(cur_m->github_url);
                }

                Rectangle btnDone = { card.x + card.width - 205, card.y + 385, 170, 44 };
                bool doneHov = CheckCollisionPointRec(mouse, btnDone);
                DrawRectangleRounded(btnDone, 0.15f, 4, doneHov ? colCardHover : (Color){ 45, 52, 68, 255 });
                const char *dnTxt = "Done / Back";
                DrawText(dnTxt, (int)(btnDone.x + btnDone.width / 2 - MeasureText(dnTxt, 15) / 2), (int)btnDone.y + 14, 15, RAYWHITE);
                if (doneHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    current_screen = screen_before_manage;
                }

                bool submit_save_desc = (saveHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) || IsKeyPressed(KEY_ENTER);
                if (submit_save_desc) {
                    char st_msg[512] = "";
                    bool ok = PerformUpdateDescription(repo_dir, username_input, cur_m->branch, manage_desc_buf, st_msg, sizeof(st_msg));
                    snprintf(toast_msg, sizeof(toast_msg), "%s", st_msg);
                    toast_timer = 4.0f;
                    toast_is_error = !ok;
                    if (ok) {
                        LoadAllTasks(repo_dir, github_base, &all_tasks);
                        managed_parent_count = GetUserManagedTasks(&all_tasks, username_input, managed_parents);
                        EvaluateUserTasks(&all_tasks, username_input, active_tasks, active_roles, &active_task_count, free_tasks, &free_task_count);
                        if (managed_parent_count > 0) {
                            if (manage_parent_idx >= managed_parent_count) manage_parent_idx = managed_parent_count - 1;
                            strncpy(manage_desc_buf, managed_parents[manage_parent_idx].description, sizeof(manage_desc_buf) - 1);
                            manage_desc_buf[sizeof(manage_desc_buf) - 1] = '\0';
                        }
                    }
                }
            }
        }

        // Render floating Toast notification if active
        if (toast_timer > 0.0f) {
            toast_timer -= GetFrameTime();
            int toastW = MeasureText(toast_msg, 15) + 50;
            if (toastW < 320) toastW = 320;
            if (toastW > winW - 60) toastW = winW - 60;
            Rectangle toastRec = { (winW - toastW) / 2.0f, (float)(winH - 56), (float)toastW, 40 };
            DrawRectangleRounded(toastRec, 0.3f, 4, toast_is_error ? (Color){ 180, 30, 30, 240 } : (Color){ 25, 135, 60, 240 });
            DrawRectangleRoundedLines(toastRec, 0.3f, 4, RAYWHITE);
            DrawText(toast_msg, (int)(toastRec.x + toastRec.width / 2 - MeasureText(toast_msg, 15) / 2), (int)(toastRec.y + 12), 15, RAYWHITE);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}


