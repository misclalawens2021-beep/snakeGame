/*
 * ============================================================
 *  SNIKE - Jeu du serpent en C (Windows, biblioth�que conio.h)
 * ============================================================
 *  R�gles :
 *   - 3 pommes apparaissent � chaque fois.
 *   - Le serpent doit manger les 3 pommes ; ensuite il grandit
 *     d'une case et 3 nouvelles pommes apparaissent.
 *   - La partie s'arr�te si le serpent touche un mur ou se mord
 *     la queue.
 *   - Sauvegarde / reprise d'une partie dans un fichier.
 *   - Deux vitesses (lente / rapide).
 *   - Deux modes : joueur humain ou ordinateur (le serpent joue
 *     tout seul).
 *
 *  Compilation (MinGW / gcc sous Windows) :
 *      gcc snike.c -o snike.exe
 *
 *  Touches :
 *      Fl�ches ou Z/Q/S/D : diriger le serpent
 *      K : sauvegarder   P : pause   X : quitter la partie
 * ============================================================
 */

#include <stdio.h>      /* printf, fopen, fprintf, fscanf...        */
#include <stdlib.h>     /* rand, srand, system                      */
#include <conio.h>      /* _kbhit (touche press�e ?), _getch (lire) */
#include <ctype.h>      /* tolower                                  */
#include <time.h>       /* time (pour initialiser le hasard)        */
#include <windows.h>    /* Sleep, gestion du curseur de la console  */

/* ---------- Constantes ---------- */
#define LARGEUR      40                    /* largeur du terrain        */
#define HAUTEUR      20                    /* hauteur du terrain        */
#define MAX_SERPENT  (LARGEUR * HAUTEUR)   /* taille max du serpent     */
#define NB_POMMES    4                     /* pommes par vague          */
#define FICHIER_SAUVEGARDE "snike_save.txt"

#define VITESSE_LENTE  150   /* pause entre deux d�placements (ms) */
#define VITESSE_RAPIDE  60

/* ---------- Types ---------- */

/* Une position sur le terrain */
typedef struct {
    int x;
    int y;
} Point;

/* Une pomme : sa position + est-elle encore sur le terrain ? */
typedef struct {
    Point pos;
    int   active;   /* 1 = visible, 0 = d�j� mang�e */
} Pomme;

/* Les 4 directions. L'ordre est choisi pour que  d ^ 1  donne
 * la direction oppos�e : HAUT(0)<->BAS(1), GAUCHE(2)<->DROITE(3) */
enum { HAUT, BAS, GAUCHE, DROITE };

/* D�placement en x et en y pour chaque direction */
const int DX[4] = { 0, 0, -1, 1 };
const int DY[4] = { -1, 1, 0, 0 };

/* Toute la partie dans une seule structure : c'est pratique
 * pour la sauvegarder / la recharger. */
typedef struct {
    Point serpent[MAX_SERPENT]; /* serpent[0] = la t�te               */
    int   longueur;             /* nombre de cases du serpent         */
    int   direction;            /* direction actuelle                 */
    Pomme pommes[NB_POMMES];    /* les n pommes de la vague en cours  */
    int   pommesMangees;        /* combien mang�es dans la vague (0-2)*/
    int   score;                /* nombre total de pommes mang�es     */
    int   vitesse;              /* 0 = lente, 1 = rapide              */
    int   modeOrdi;             /* 0 = humain, 1 = ordinateur         */
} Jeu;

/* ============================================================
 *  Fonctions d'affichage (console Windows)
 * ============================================================ */

/* Place le curseur de la console en (x, y) : on r��crit l'�cran
 * "par-dessus" au lieu de l'effacer, ce qui �vite le clignotement. */
void gotoxy(int x, int y) {
    COORD c;
    c.X = (SHORT)x;
    c.Y = (SHORT)y;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), c);
}

/* Cache le curseur clignotant pour un affichage plus propre */
void cacherCurseur(void) {
    CONSOLE_CURSOR_INFO info;
    info.dwSize = 1;
    info.bVisible = FALSE;
    SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
}

/* Dessine tout le terrain, le serpent, les pommes et le score */
void afficher(const Jeu *j) {
    char grille[HAUTEUR][LARGEUR];
    int x, y, i;

    /* 1) On remplit la grille avec des espaces */
    for (y = 0; y < HAUTEUR; y++)
        for (x = 0; x < LARGEUR; x++)
            grille[y][x] = ' ';

    /* 2) On place les pommes actives */
    for (i = 0; i < NB_POMMES; i++)
        if (j->pommes[i].active)
            grille[j->pommes[i].pos.y][j->pommes[i].pos.x] = '*';

    /* 3) On place le corps ('o') puis la t�te ('O') */
    for (i = 1; i < j->longueur; i++)
        grille[j->serpent[i].y][j->serpent[i].x] = 'o';
    grille[j->serpent[0].y][j->serpent[0].x] = 'O';

    /* 4) On affiche la grille avec les murs autour */
    gotoxy(0, 0);
    printf("+");
    for (x = 0; x < LARGEUR; x++) printf("-");
    printf("+\n");
    for (y = 0; y < HAUTEUR; y++) {
        printf("|");
        for (x = 0; x < LARGEUR; x++) printf("%c", grille[y][x]);
        printf("|\n");
    }
    printf("+");
    for (x = 0; x < LARGEUR; x++) printf("-");
    printf("+\n");

    /* 5) Infos et rappel des touches */
    printf("Score : %d   Longueur : %d   Vague : %d/%d   Mode : %s   Vitesse : %s   \n",
           j->score, j->longueur, j->pommesMangees, NB_POMMES,
           j->modeOrdi ? "ORDI " : "HUMAIN",
           j->vitesse ? "RAPIDE" : "LENTE ");
    printf("Fleches/ZQSD : diriger | K : sauver | P : pause | X : quitter   \n");
}

/* ============================================================
 *  Logique du jeu
 * ============================================================ */

/* Renvoie 1 si la case (x, y) est occup�e par le serpent.
 * On regarde les 'n' premi�res cases du serpent seulement. */
int estSurSerpent(const Jeu *j, int x, int y, int n) {
    int i;
    for (i = 0; i < n; i++)
        if (j->serpent[i].x == x && j->serpent[i].y == y)
            return 1;
    return 0;
}

/* Fait appara�tre 3 nouvelles pommes sur des cases libres
 * (ni sur le serpent, ni sur une autre pomme). */
void genererPommes(Jeu *j) {
    int i, k, x, y, ok;
    for (i = 0; i < NB_POMMES; i++) {
        do {
            x = rand() % LARGEUR;
            y = rand() % HAUTEUR;
            ok = !estSurSerpent(j, x, y, j->longueur);
            /* on v�rifie aussi les pommes d�j� plac�es */
            for (k = 0; k < i; k++)
                if (j->pommes[k].pos.x == x && j->pommes[k].pos.y == y)
                    ok = 0;
        } while (!ok);
        j->pommes[i].pos.x = x;
        j->pommes[i].pos.y = y;
        j->pommes[i].active = 1;
    }
    j->pommesMangees = 0;
}

/* Pr�pare une nouvelle partie : serpent de 3 cases au milieu */
void nouvelleParti(Jeu *j, int modeOrdi, int vitesse) {
    int i;
    j->longueur  = 3;
    j->direction = DROITE;
    j->score     = 0;
    j->vitesse   = vitesse;
    j->modeOrdi  = modeOrdi;
    for (i = 0; i < j->longueur; i++) {
        j->serpent[i].x = LARGEUR / 2 - i;   /* t�te � droite, queue � gauche */
        j->serpent[i].y = HAUTEUR / 2;
    }
    genererPommes(j);
}

/* V�rifie si la case (x, y) est "s�re" pour la t�te :
 * ni mur, ni corps. On ignore la queue car elle va se d�caler. */
int caseSure(const Jeu *j, int x, int y) {
    if (x < 0 || x >= LARGEUR || y < 0 || y >= HAUTEUR) return 0;
    if (estSurSerpent(j, x, y, j->longueur - 1)) return 0;
    return 1;
}

/* ---------- Intelligence de l'ordinateur ----------
 * Principe simple : on cherche la pomme la plus proche, puis on
 * choisit parmi les directions possibles (pas de demi-tour, pas de
 * collision) celle qui rapproche le plus de cette pomme. */
int choisirDirectionOrdi(const Jeu *j) {
    int i, d, meilleure = j->direction, meilleureDist = 1000000;
    int cibleX = j->serpent[0].x, cibleY = j->serpent[0].y;
    int distCible = 1000000;

    /* 1) Trouver la pomme active la plus proche (distance de Manhattan) */
    for (i = 0; i < NB_POMMES; i++) {
        if (!j->pommes[i].active) continue;
        int dist = abs(j->pommes[i].pos.x - j->serpent[0].x)
                 + abs(j->pommes[i].pos.y - j->serpent[0].y);
        if (dist < distCible) {
            distCible = dist;
            cibleX = j->pommes[i].pos.x;
            cibleY = j->pommes[i].pos.y;
        }
    }

    /* 2) Tester les 4 directions */
    for (d = 0; d < 4; d++) {
        if (d == (j->direction ^ 1)) continue;          /* pas de demi-tour */
        int nx = j->serpent[0].x + DX[d];
        int ny = j->serpent[0].y + DY[d];
        if (!caseSure(j, nx, ny)) continue;             /* �vite mur/corps  */
        int dist = abs(cibleX - nx) + abs(cibleY - ny);
        if (dist < meilleureDist) {
            meilleureDist = dist;
            meilleure = d;
        }
    }
    return meilleure;
}

/* ---------- Sauvegarde / chargement ---------- */

/* �crit toute la partie dans un fichier texte.
 * Renvoie 1 si OK, 0 en cas d'erreur. */
int sauvegarder(const Jeu *j) {
    int i;
    FILE *f = fopen(FICHIER_SAUVEGARDE, "w");
    if (f == NULL) return 0;

    fprintf(f, "%d %d %d %d %d %d\n", j->longueur, j->direction, j->score,
            j->vitesse, j->modeOrdi, j->pommesMangees);
    for (i = 0; i < NB_POMMES; i++)
        fprintf(f, "%d %d %d\n", j->pommes[i].pos.x, j->pommes[i].pos.y,
                j->pommes[i].active);
    for (i = 0; i < j->longueur; i++)
        fprintf(f, "%d %d\n", j->serpent[i].x, j->serpent[i].y);

    fclose(f);
    return 1;
}

/* Relit une partie depuis le fichier.
 * Renvoie 1 si OK, 0 si le fichier n'existe pas ou est invalide. */
int charger(Jeu *j) {
    int i;
    FILE *f = fopen(FICHIER_SAUVEGARDE, "r");
    if (f == NULL) return 0;

    if (fscanf(f, "%d %d %d %d %d %d", &j->longueur, &j->direction, &j->score,
               &j->vitesse, &j->modeOrdi, &j->pommesMangees) != 6
        || j->longueur < 1 || j->longueur > MAX_SERPENT) {
        fclose(f);
        return 0;
    }
    for (i = 0; i < NB_POMMES; i++)
        if (fscanf(f, "%d %d %d", &j->pommes[i].pos.x, &j->pommes[i].pos.y,
                   &j->pommes[i].active) != 3) { fclose(f); return 0; }
    for (i = 0; i < j->longueur; i++)
        if (fscanf(f, "%d %d", &j->serpent[i].x, &j->serpent[i].y) != 2) {
            fclose(f);
            return 0;
        }

    fclose(f);
    return 1;
}

/* ---------- Lecture du clavier ----------
 * Renvoie 1 si le joueur veut quitter la partie, 0 sinon.
 * _kbhit() dit s'il y a une touche en attente (sans bloquer),
 * _getch() lit cette touche. */
int lireClavier(Jeu *j) {
    while (_kbhit()) {
        int c = _getch();
        int nouvelle = -1;

        /* Les fl�ches envoient 2 codes : 0 ou 224, puis le vrai code */
        if (c == 0 || c == 224) {
            c = _getch();
            if (c == 72) nouvelle = HAUT;
            else if (c == 80) nouvelle = BAS;
            else if (c == 75) nouvelle = GAUCHE;
            else if (c == 77) nouvelle = DROITE;
        } else {
            switch (tolower(c)) {
                case 'z': nouvelle = HAUT;    break;
                case 's': nouvelle = BAS;     break;
                case 'q': nouvelle = GAUCHE;  break;
                case 'd': nouvelle = DROITE;  break;
                case 'k':                     /* sauvegarde */
                    gotoxy(0, HAUTEUR + 5);
                    printf(sauvegarder(j) ? "Partie sauvegardee !          "
                                          : "Erreur de sauvegarde !        ");
                    break;
                case 'p':                     /* pause : on attend une touche */
                    gotoxy(0, HAUTEUR + 5);
                    printf("PAUSE - appuyez sur une touche...");
                    _getch();
                    gotoxy(0, HAUTEUR + 5);
                    printf("                                 ");
                    break;
                case 'x':
                    return 1;                 /* quitter */
            }
        }

        /* En mode humain, on change de direction sauf demi-tour */
        if (!j->modeOrdi && nouvelle != -1 && nouvelle != (j->direction ^ 1))
            j->direction = nouvelle;
    }
    return 0;
}

/* ---------- Un pas de jeu ----------
 * D�place le serpent d'une case. Renvoie 1 si le serpent est mort. */
int avancer(Jeu *j) {
    int i, grandit = 0, mange = -1;

    /* Nouvelle position de la t�te */
    Point tete;
    tete.x = j->serpent[0].x + DX[j->direction];
    tete.y = j->serpent[0].y + DY[j->direction];

    /* 1) Collision avec un mur -> fin de partie */
    if (tete.x < 0 || tete.x >= LARGEUR || tete.y < 0 || tete.y >= HAUTEUR)
        return 1;

    /* 2) La t�te tombe-t-elle sur une pomme ? */
    for (i = 0; i < NB_POMMES; i++)
        if (j->pommes[i].active && j->pommes[i].pos.x == tete.x
                                && j->pommes[i].pos.y == tete.y)
            mange = i;

    /* Si c'est la 3e pomme de la vague, le serpent grandit */
    if (mange != -1 && j->pommesMangees + 1 == NB_POMMES)
        grandit = 1;

    /* 3) Collision avec le corps (le serpent se mord la queue).
     * Si le serpent ne grandit pas, la queue va se d�caler : on
     * ne la teste pas. S'il grandit, la queue reste : on la teste. */
    if (estSurSerpent(j, tete.x, tete.y, grandit ? j->longueur : j->longueur - 1))
        return 1;

    /* 4) D�placement : chaque case prend la place de la pr�c�dente */
    Point ancienneQueue = j->serpent[j->longueur - 1];
    for (i = j->longueur - 1; i > 0; i--)
        j->serpent[i] = j->serpent[i - 1];
    j->serpent[0] = tete;

    /* 5) Gestion de la pomme mang�e */
    if (mange != -1) {
        j->pommes[mange].active = 0;
        j->pommesMangees++;
        j->score++;
    }
    if (grandit) {
        j->serpent[j->longueur] = ancienneQueue;   /* on rallonge */
        j->longueur++;
        genererPommes(j);                          /* nouvelle vague */
    }
    return 0;
}

/* ============================================================
 *  Boucle d'une partie
 * ============================================================ */
void jouer(Jeu *j) {
    int fini = 0;

    system("cls");        /* efface l'�cran une seule fois au d�but */
    cacherCurseur();

    while (!fini) {
        afficher(j);

        /* Lecture clavier (quitter, sauvegarder, direction...) */
        if (lireClavier(j)) break;

        /* En mode ordinateur, c'est le programme qui d�cide */
        if (j->modeOrdi)
            j->direction = choisirDirectionOrdi(j);

        /* On avance ; si le serpent meurt, on sort de la boucle */
        fini = avancer(j);

        /* Pause entre deux images : d�finit la vitesse du jeu */
        Sleep(j->vitesse ? VITESSE_RAPIDE : VITESSE_LENTE);
    }

    if (fini) {
        afficher(j);
        gotoxy(0, HAUTEUR + 5);
        printf("*** GAME OVER ***  Score final : %d              \n", j->score);
    }
    printf("Appuyez sur une touche pour revenir au menu...\n");
    _getch();
}

/* ============================================================
 *  Programme principal : menu
 * ============================================================ */
int main(void) {
    Jeu jeu;
    int choix, mode, vitesse;

    srand((unsigned)time(NULL));   /* hasard diff�rent � chaque lancement */

    do {
        system("cls");
        printf("=========== SNIKE ===========\n");
        printf(" 1. Nouvelle partie\n");
        printf(" 2. Reprendre la partie sauvegardee\n");
        printf(" 3. Quitter\n");
        printf("Votre choix : ");
        choix = _getch() - '0';

        if (choix == 1) {
            printf("\n\n Mode : 1. Humain   2. Ordinateur : ");
            mode = (_getch() - '0') == 2;
            printf("\n Vitesse : 1. Lente   2. Rapide : ");
            vitesse = (_getch() - '0') == 2;
            nouvelleParti(&jeu, mode, vitesse);
            jouer(&jeu);
        } else if (choix == 2) {
            if (charger(&jeu))
                jouer(&jeu);
            else {
                printf("\n\nAucune sauvegarde valide. Appuyez sur une touche.");
                _getch();
            }
        }
    } while (choix != 3);

    return 0;
}
