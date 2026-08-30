/*
 * Calcolo Taglio Barre - versione C con GUI Win32 nativa
 * --------------------------------------------------------
 * Nessuna libreria esterna, nessun runtime da installare: una volta
 * compilato produce un singolo .exe che gira su qualsiasi Windows
 * (XP/7/10/11), a 32 o 64 bit.
 *
 * COMPILAZIONE
 *
 * Con MinGW (gcc) installato su Windows, da prompt dei comandi:
 *
 *     gcc -O2 -mwindows -o TaglioBarre.exe taglio_barre.c
 *
 * Con Visual Studio (Developer Command Prompt):
 *
 *     cl /O2 taglio_barre.c /link user32.lib gdi32.lib
 *
 * Il file TaglioBarre.exe risultante e' autonomo: copialo su qualsiasi
 * PC Windows e funzionera' senza installare nulla.
 */

#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#define ID_EDIT_BARRA      101
#define ID_COMBO_GREZZO    102
#define ID_EDIT_CUSTOM     103
#define ID_BUTTON_CALCOLA  104
#define ID_BUTTON_PULISCI  107
#define ID_EDIT_RISULTATO  105
#define ID_LABEL_INFO      106

static const int LUNGHEZZE_STANDARD[] = {1000, 1500, 2000, 3000, 4000, 6000, 12000};
static const int N_STANDARD = 7;      /* numero di voci standard nella tendina */
static const int INDICE_DEFAULT = 5;  /* indice di 6000 nell'array sopra */

HWND hEditBarra, hComboGrezzo, hEditCustom, hEditRisultato, hLabelInfo;
WNDPROC OrigEditBarraProc;

/* Calcola il numero di pezzi e la lunghezza di ciascun pezzo.
   Ritorna 1 se OK, 0 se errore (mette il messaggio in errMsg). */
int CalcolaTaglio(double barra_mm, double grezzo_mm,
                   int *n_pezzi, double *pezzo_mm, char *errMsg, size_t errMsgLen) {
    if (barra_mm <= 0 || grezzo_mm <= 0) {
        snprintf(errMsg, errMsgLen, "Le lunghezze devono essere maggiori di zero.");
        return 0;
    }
    if (barra_mm > grezzo_mm) {
        snprintf(errMsg, errMsgLen, "La barra richiesta e' piu' lunga del grezzo.");
        return 0;
    }

    int n = (int)floor(grezzo_mm / barra_mm);
    if (n < 1) n = 1;

    *n_pezzi = n;
    *pezzo_mm = grezzo_mm / n;
    return 1;
}

/* Formatta un numero eliminando gli zeri decimali superflui (1.200 -> 1.2) */
void FormatNum(double x, char *out, size_t outLen) {
    snprintf(out, outLen, "%.3f", x);
    size_t len = strlen(out);
    while (len > 0 && out[len - 1] == '0') { out[--len] = '\0'; }
    if (len > 0 && out[len - 1] == '.') { out[--len] = '\0'; }
}

/* Conta le cifre del valore attualmente selezionato/digitato per il grezzo */
int CifreGrezzoCorrente(void) {
    int sel = (int)SendMessageA(hComboGrezzo, CB_GETCURSEL, 0, 0);
    char buf[64];

    if (sel == N_STANDARD) { /* Personalizzato... */
        GetWindowTextA(hEditCustom, buf, sizeof(buf));
    } else if (sel >= 0) {
        SendMessageA(hComboGrezzo, CB_GETLBTEXT, sel, (LPARAM)buf);
    } else {
        return 6; /* fallback */
    }

    int cifre = 0;
    for (char *p = buf; *p; p++) if (*p >= '0' && *p <= '9') cifre++;
    return cifre > 0 ? cifre : 6;
}

/* Verifica se il TESTO COMPLETO proposto per il campo barra e' valido:
   solo cifre ed al massimo UN separatore decimale (',' o '.'), e la
   parte intera (prima del separatore) non deve superare maxDigits. */
int ValidaCandidatoBarra(const char *testo, int maxDigits) {
    int nSep = 0;
    for (const char *p = testo; *p; p++) {
        if (*p == ',' || *p == '.') nSep++;
        else if (*p < '0' || *p > '9') return 0;
    }
    if (nSep > 1) return 0;

    int integerLen = 0;
    for (const char *p = testo; *p; p++) {
        if (*p == ',' || *p == '.') break;
        integerLen++;
    }
    if (integerLen > maxDigits) return 0;
    return 1;
}

/* Tronca SOLO la parte intera del testo del campo barra al limite corrente,
   preservando l'eventuale separatore e la parte decimale. */
void TroncaBarraALimite(void) {
    int max = CifreGrezzoCorrente();
    char buf[128];
    GetWindowTextA(hEditBarra, buf, sizeof(buf));

    int sepIndex = -1;
    for (int i = 0; buf[i]; i++) {
        if (buf[i] == ',' || buf[i] == '.') { sepIndex = i; break; }
    }
    int integerLen = (sepIndex >= 0) ? sepIndex : (int)strlen(buf);

    if (integerLen > max) {
        char nuovo[128];
        int p = 0;
        for (int i = 0; i < max && p < (int)sizeof(nuovo) - 1; i++) nuovo[p++] = buf[i];
        if (sepIndex >= 0) {
            for (int i = sepIndex; buf[i] && p < (int)sizeof(nuovo) - 1; i++) nuovo[p++] = buf[i];
        }
        nuovo[p] = '\0';
        SetWindowTextA(hEditBarra, nuovo);
    }
}

/* Subclass del campo "lunghezza barra": filtra digitazione e incolla,
   accettando cifre e un separatore decimale (',' o '.'), con il limite
   di cifre applicato solo alla parte intera. */
LRESULT CALLBACK EditBarraSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_CHAR) {
        unsigned char ch = (unsigned char)wParam;
        if (ch >= 32) { /* ignora caratteri di controllo (backspace, ecc.) */
            char testo[128];
            GetWindowTextA(hwnd, testo, sizeof(testo));
            DWORD selStart = 0, selEnd = 0;
            SendMessageA(hwnd, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
            int lenTesto = (int)strlen(testo);
            if ((int)selStart > lenTesto) selStart = lenTesto;
            if ((int)selEnd > lenTesto) selEnd = lenTesto;

            char candidato[160];
            int p = 0;
            for (DWORD i = 0; i < selStart && p < (int)sizeof(candidato) - 1; i++) candidato[p++] = testo[i];
            if (p < (int)sizeof(candidato) - 1) candidato[p++] = (char)ch;
            for (DWORD i = selEnd; i < (DWORD)lenTesto && p < (int)sizeof(candidato) - 1; i++) candidato[p++] = testo[i];
            candidato[p] = '\0';

            if (!ValidaCandidatoBarra(candidato, CifreGrezzoCorrente())) {
                return 0; /* blocca il carattere */
            }
        }
        return CallWindowProcA(OrigEditBarraProc, hwnd, msg, wParam, lParam);
    }

    if (msg == WM_PASTE) {
        int consentito = 0;
        if (OpenClipboard(hwnd)) {
            HANDLE hData = GetClipboardData(CF_TEXT);
            if (hData) {
                char *clip = (char *)GlobalLock(hData);
                if (clip) {
                    char testo[128];
                    GetWindowTextA(hwnd, testo, sizeof(testo));
                    DWORD selStart = 0, selEnd = 0;
                    SendMessageA(hwnd, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
                    int lenTesto = (int)strlen(testo);
                    if ((int)selStart > lenTesto) selStart = lenTesto;
                    if ((int)selEnd > lenTesto) selEnd = lenTesto;

                    char candidato[256];
                    int p = 0;
                    for (DWORD i = 0; i < selStart && p < (int)sizeof(candidato) - 1; i++) candidato[p++] = testo[i];
                    for (const char *c = clip; *c && p < (int)sizeof(candidato) - 1; c++) candidato[p++] = *c;
                    for (DWORD i = selEnd; i < (DWORD)lenTesto && p < (int)sizeof(candidato) - 1; i++) candidato[p++] = testo[i];
                    candidato[p] = '\0';

                    GlobalUnlock(hData);
                    consentito = ValidaCandidatoBarra(candidato, CifreGrezzoCorrente());
                }
            }
            CloseClipboard();
        }
        if (!consentito) return 0; /* blocca l'incollaggio non valido */
        return CallWindowProcA(OrigEditBarraProc, hwnd, msg, wParam, lParam);
    }

    return CallWindowProcA(OrigEditBarraProc, hwnd, msg, wParam, lParam);
}

void EseguiCalcolo(HWND hwnd) {
    char bufBarra[64], bufCustom[64], bufCombo[64];
    GetWindowTextA(hEditBarra, bufBarra, sizeof(bufBarra));
    for (char *p = bufBarra; *p; p++) if (*p == ',') *p = '.';
    double barra_mm = atof(bufBarra);

    int sel = (int)SendMessageA(hComboGrezzo, CB_GETCURSEL, 0, 0);
    double grezzo_mm;

    if (sel == N_STANDARD) { /* ultima voce = "Personalizzato..." */
        GetWindowTextA(hEditCustom, bufCustom, sizeof(bufCustom));
        for (char *p = bufCustom; *p; p++) if (*p == ',') *p = '.';
        grezzo_mm = atof(bufCustom);
    } else {
        SendMessageA(hComboGrezzo, CB_GETLBTEXT, sel, (LPARAM)bufCombo);
        grezzo_mm = atof(bufCombo);
    }

    int n_pezzi;
    double pezzo_mm;
    char errMsg[128];

    if (!CalcolaTaglio(barra_mm, grezzo_mm, &n_pezzi, &pezzo_mm, errMsg, sizeof(errMsg))) {
        MessageBoxA(hwnd, errMsg, "Errore", MB_ICONERROR | MB_OK);
        SetWindowTextA(hEditRisultato, "");
        SetWindowTextA(hLabelInfo, "");
        return;
    }

    double pezzo_m = pezzo_mm / 1000.0;
    char numFormattato[32];
    FormatNum(pezzo_m, numFormattato, sizeof(numFormattato));

    /* Nel campo di output va SOLO il numero, senza l'unita' di misura */
    SetWindowTextA(hEditRisultato, numFormattato);
    SendMessageA(hEditRisultato, EM_SETSEL, 0, -1); /* seleziona tutto per copia rapida */

    char info[128];
    snprintf(info, sizeof(info), "(%d pezzi da %.0f mm)", n_pezzi, pezzo_mm);
    SetWindowTextA(hLabelInfo, info);
}

void Pulisci(void) {
    SetWindowTextA(hEditBarra, "");
    SetWindowTextA(hEditCustom, "");
    SetWindowTextA(hEditRisultato, "");
    SetWindowTextA(hLabelInfo, "");
    SetFocus(hEditBarra);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        CreateWindowA("STATIC", "Lunghezza barra richiesta (mm):",
            WS_VISIBLE | WS_CHILD, 20, 20, 250, 20, hwnd, NULL, NULL, NULL);
        hEditBarra = CreateWindowA("EDIT", "",
            WS_VISIBLE | WS_CHILD | WS_BORDER,
            280, 18, 100, 24, hwnd, (HMENU)ID_EDIT_BARRA, NULL, NULL);
        SendMessageA(hEditBarra, EM_SETLIMITTEXT, 32, 0); /* limite generico di sicurezza */
        /* subclass per filtrare cifre + un separatore decimale (',' o '.') */
        OrigEditBarraProc = (WNDPROC)SetWindowLongPtrA(
            hEditBarra, GWLP_WNDPROC, (LONG_PTR)EditBarraSubclassProc);

        CreateWindowA("STATIC", "Lunghezza grezzo di partenza (mm):",
            WS_VISIBLE | WS_CHILD, 20, 60, 250, 20, hwnd, NULL, NULL, NULL);
        hComboGrezzo = CreateWindowA("COMBOBOX", "",
            WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST,
            280, 58, 100, 200, hwnd, (HMENU)ID_COMBO_GREZZO, NULL, NULL);

        char buf[16];
        for (int i = 0; i < N_STANDARD; i++) {
            snprintf(buf, sizeof(buf), "%d", LUNGHEZZE_STANDARD[i]);
            SendMessageA(hComboGrezzo, CB_ADDSTRING, 0, (LPARAM)buf);
        }
        SendMessageA(hComboGrezzo, CB_ADDSTRING, 0, (LPARAM)"Personalizzato...");
        SendMessageA(hComboGrezzo, CB_SETCURSEL, INDICE_DEFAULT, 0); /* default 6000 */

        hEditCustom = CreateWindowA("EDIT", "", WS_CHILD | WS_BORDER | ES_NUMBER,
            280, 88, 100, 24, hwnd, (HMENU)ID_EDIT_CUSTOM, NULL, NULL);
        /* nascosto finche' non si seleziona "Personalizzato..." */

        CreateWindowA("BUTTON", "Calcola",
            WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            110, 130, 100, 32, hwnd, (HMENU)ID_BUTTON_CALCOLA, NULL, NULL);

        CreateWindowA("BUTTON", "Pulisci",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            220, 130, 100, 32, hwnd, (HMENU)ID_BUTTON_PULISCI, NULL, NULL);

        CreateWindowA("STATIC", "Lunghezza minima:",
            WS_VISIBLE | WS_CHILD, 20, 180, 150, 20, hwnd, NULL, NULL, NULL);

        /* Campo di output: read-only ma selezionabile/copiabile (Ctrl+C).
           Contiene SOLO il numero, l'unita' "m" e' un'etichetta a parte. */
        hEditRisultato = CreateWindowA("EDIT", "",
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_READONLY | ES_CENTER,
            180, 178, 70, 24, hwnd, (HMENU)ID_EDIT_RISULTATO, NULL, NULL);

        CreateWindowA("STATIC", "m", WS_VISIBLE | WS_CHILD,
            258, 181, 20, 20, hwnd, NULL, NULL, NULL);

        hLabelInfo = CreateWindowA("STATIC", "",
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            20, 215, 360, 20, hwnd, (HMENU)ID_LABEL_INFO, NULL, NULL);
        break;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == ID_BUTTON_CALCOLA) {
            EseguiCalcolo(hwnd);
        } else if (LOWORD(wParam) == ID_BUTTON_PULISCI) {
            Pulisci();
        } else if (LOWORD(wParam) == ID_COMBO_GREZZO && HIWORD(wParam) == CBN_SELCHANGE) {
            int sel = (int)SendMessageA(hComboGrezzo, CB_GETCURSEL, 0, 0);
            ShowWindow(hEditCustom, sel == N_STANDARD ? SW_SHOW : SW_HIDE);
            TroncaBarraALimite();
        } else if (LOWORD(wParam) == ID_EDIT_CUSTOM && HIWORD(wParam) == EN_CHANGE) {
            TroncaBarraALimite();
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance; (void)lpCmdLine;

    const char CLASS_NAME[] = "TaglioBarreWindow";
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(0, CLASS_NAME, "Calcolo Taglio Barre",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 420, 300,
        NULL, NULL, hInstance, NULL);

    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    /* Tabella di acceleratori: il tasto Invio equivale a premere "Calcola",
       indipendentemente da quale campo ha il focus in quel momento. */
    ACCEL accels[] = {
        { FVIRTKEY, VK_RETURN, ID_BUTTON_CALCOLA }
    };
    HACCEL hAccel = CreateAcceleratorTable(accels, 1);

    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (!TranslateAccelerator(hwnd, hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    DestroyAcceleratorTable(hAccel);
    return 0;
}
