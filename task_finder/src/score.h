#ifndef SCORE_H
#define SCORE_H

// ==============================================================================
// TVOJE SCORING FUNKCE
// ==============================================================================
// Tuto funkci si můžeš upravit / přepsat přesně podle svých potřeb.
//
// Vstup:
//   days_to_deadline:
//     > 0 : tolik dní zbývá do termínu (v budoucnu)
//     = 0 : deadline je dnes
//     < 0 : úkol je po termínu (např. -5 znamená 5 dní po termínu)
//     999999 : úkol nemá platně zadaný termín (nebo je placeholder)
//
// Výstup:
//   float: výsledné skóre.
//   Seznam volných úkolů se řadí SESTUPNĚ (od nejvyššího skóre po nejnižší).
//   Úkol s nejvyšším skóre bude na 1. místě.
// ==============================================================================
static inline float CalculateScore(int days_to_deadline) {
    if (days_to_deadline >= 900000) {
        // Úkoly bez termínu dostanou nejnižší skóre a zařadí se na konec seznamu
        return -999999.0f;
    }

    // Příklad výchozí logiky (uprav dle svých preferencí):
    // Čím méně dní zbývá (případně čím více je po termínu), tím vyšší skóre:
    // Např.:
    //   5 dní po termínu (-5)  -> skóre = 5.0  (nejvyšší priorita)
    //   termín dnes (0)        -> skóre = 0.0
    //   zbývá 10 dní (10)      -> skóre = -10.0
    return (float)(-days_to_deadline);
}

#endif // SCORE_H
