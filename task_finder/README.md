# Task Finder

Nástroj v jazyce C s grafickým rozhraním v **Raylib** pro správu a výběr úkolů v Git větvích projektu.
Všechny texty a popisky v uživatelském rozhraní jsou v čisté angličtině (ASCII bez diakritiky), aby se předešlo jakýmkoliv problémům s fonty ve vizuálním rozhraní Raylibu.

---

## Vlastnosti
- **Nulová instalace pro uživatele:** Stačí spustit hotovou binárku:
  - Na **Linuxu**: `./task_finder`
  - Na **Windows**: `task_finder.exe` (spustitelné i dvojklikem)
- **100% kompatibilní s výchozím fontem Raylibu:** Všechny texty a popisky jsou bez diakritiky (anglicky). Navíc je implementován automatický převodník (`StripDiacritics`), takže i kdyby se v `TASK.md` objevil český název úkolu, zobrazí se ve vizuálu čistě bez otazníků a chybějících znaků.
- **100% lokální a offline:** Čte data přímo z lokálního repozitáře Gitu (`git show <branch>:TASK.md`), nepotřebuje internet ani GitHub API tokeny.
- **Detekce aktivního úkolu:**
  - Pokud je člověk zapsán pod `# WORKER`, nebo je `# TASK MASTER` na úkolu bez subtasků, program ho upozorní a zobrazí jeho rozdělaný úkol.
- **Seřazená nabídka volných úkolů:**
  - Pokud člověk aktivní úkol nemá, nabídne mu dostupné úkoly (se statusem `available`) seřazené sestupně podle skóre.
- **Přímé odkazy na GitHub:**
  - U každého úkolu je tlačítko `Open on GitHub`, které otevře konkrétní větev ve webovém prohlížeči.
- **Podpora grafického (GUI) i textového (CLI) režimu:**
  - Výchozí je okenní grafické UI (Raylib).
  - V případě spuštění s `--cli` nebo na PC bez displeje se automaticky přepne do terminálového režimu.

---

## Kde upravit vlastní scoring funkci?
Tvá scoring funkce se nachází v samostatném souboru:
[`src/score.h`](src/score.h)

```c
float CalculateScore(int days_to_deadline) {
    if (days_to_deadline >= 900000) {
        return -999999.0f; // Úkoly bez termínu na konec
    }

    // Tvoje vlastní logika výpočtu skóre:
    // Seznam je řazen SESTUPNĚ (vyšší skóre = vyšší priorita v seznamu)
    return (float)(-days_to_deadline);
}
```
Kdykoliv funkci upravíš, stačí spustit `make all`.

---

## Kompilace
Projekt se kompiluje pomocí standardního `make`:
- **Obě platformy najednou (Linux + Windows):**
  ```bash
  make all
  ```
- **Pouze Linux:**
  ```bash
  make linux
  ```
- **Pouze Windows (`task_finder.exe` přes MinGW):**
  ```bash
  make windows
  ```

---

## Testovací offline prostředí
Pro otestování bez nutnosti zásahu do reálného repozitáře je k dispozici skript:
```bash
./setup_test_env.sh
```
Tento skript vytvoří lokální offline repozitář ve složce `test_repo/` se 7 ukázkovými větvemi a různými stavy úkolů:
- Pepa (`task/monster-ai` -> Task Master bez subtasků -> aktivní úkol)
- Jirka (`task/inventory` -> Worker -> aktivní úkol)
- Volné úkoly pro ostatní uživatele (`task/cutscene-intro`, `task/audio-system`, `task/level-design`)
- Dormant úkol (`task/multiplayer-proto` -> správně ignorován)

Testování programu proti testovacímu repozitáři:
```bash
./task_finder --repo ./test_repo --cli Pepa
./task_finder --repo ./test_repo --cli Jirka
./task_finder --repo ./test_repo --cli Karel
```
