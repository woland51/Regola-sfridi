"""
Calcolo taglio barre da grezzo
-------------------------------
Dato un grezzo di partenza (mm) e la lunghezza minima di barra richiesta (mm),
calcola in quanti pezzi UGUALI conviene dividere il grezzo in modo da ottenere
il pezzo di lunghezza "subito maggiore" a quella richiesta.

Esempio: barra 1180 mm, grezzo 6000 mm -> 5 pezzi da 1200 mm (1,2 m)

Per creare un eseguibile .exe che gira su qualsiasi PC Windows SENZA
richiedere Python installato:

    1. Su un PC Windows con Python installato, apri il prompt dei comandi
       nella cartella dove hai salvato questo file e lancia:

           pip install pyinstaller
           pyinstaller --onefile --windowed --name TaglioBarre taglio_barre.py

    2. Troverai l'eseguibile in: dist\\TaglioBarre.exe

    3. Copia SOLO quel file .exe su qualsiasi altro PC Windows: funzionerà
       senza bisogno di installare Python o altre librerie.
"""
from numpy import sqrt
import tkinter as tk
from tkinter import ttk, messagebox
import sv_ttk
import darkdetect

LUNGHEZZE_STANDARD = [1000, 1500, 2000, 3000, 4000, 6000, 12000]
INDICE_DEFAULT = LUNGHEZZE_STANDARD.index(6000)


def calcola_taglio(lunghezza_barra_mm: float, lunghezza_grezzo_mm: float):
    """
    Restituisce (numero_pezzi, lunghezza_pezzo_mm).
    Cerca il massimo numero intero di pezzi n tale che grezzo/n >= barra,
    cioè il pezzo più vicino "per eccesso" alla lunghezza richiesta.
    """
    if lunghezza_barra_mm <= 0 or lunghezza_grezzo_mm <= 0:
        raise ValueError("Le lunghezze devono essere maggiori di zero.")

    if lunghezza_barra_mm > lunghezza_grezzo_mm:
        raise ValueError("La barra richiesta è più lunga del grezzo di partenza.")

    divisori = []

    for i in range(1, round(sqrt(lunghezza_grezzo_mm))+1 ):
        if lunghezza_grezzo_mm % i == 0:
        # Trova il co-fattore
            cofattore = lunghezza_grezzo_mm // i
            divisori.append(i)
            if i != cofattore:
                divisori.append(cofattore)
    divisori.sort()
    
    for divisore in divisori:
        if lunghezza_barra_mm <= divisore:
            lunghezza_pezzo_mm = divisore
            break
    
    n = lunghezza_grezzo_mm/lunghezza_pezzo_mm
    return n, lunghezza_pezzo_mm


                
    divisori.sort()
def format_num(x: float) -> str:

    """Formatta un numero eliminando gli zeri decimali superflui (1.200 -> 1.2)."""
    s = f"{x:.3f}".rstrip("0").rstrip(".")
    return s if s else "0"


def _trova_separatore(testo: str):
    """Ritorna l'indice del primo separatore decimale (',' o '.') oppure None."""
    for i, c in enumerate(testo):
        if c in ",.":
            return i
    return None


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        # sv_ttk.set_theme(darkdetect.theme())
        self.title("Regola dello sfrido")
        self.resizable(False, False)
        self.geometry("400x340")

        self.max_digits_barra = 4  # aggiornato in base al grezzo selezionato

        pad = {"padx": 10, "pady": 6}

        frame = ttk.Frame(self, padding=15)
        frame.pack(fill="both", expand=True)

        # --- Lunghezza barra ---
        ttk.Label(frame, text="Lunghezza barra richiesta (mm):").grid(
            row=0, column=0, sticky="w", **pad
        )
        vcmd = (self.register(self._valida_barra), "%P")
        self.entry_barra = ttk.Entry(
            frame, width=15, validate="key", validatecommand=vcmd
        )
        self.entry_barra.grid(row=0, column=1, **pad)

        # --- Lunghezza grezzo ---
        ttk.Label(frame, text="Lunghezza grezzo di partenza (mm):").grid(
            row=1, column=0, sticky="w", **pad
        )

        self.combo_grezzo = ttk.Combobox(
            frame,
            values=[str(v) for v in LUNGHEZZE_STANDARD] + ["Personalizzato..."],
            width=13,
            state="readonly",
        )
        self.combo_grezzo.current(INDICE_DEFAULT)  # default 6000
        self.combo_grezzo.grid(row=1, column=1, **pad)
        self.combo_grezzo.bind("<<ComboboxSelected>>", self._on_combo_change)

        self.entry_grezzo_custom = ttk.Entry(frame, width=15)
        self.entry_grezzo_custom.grid(row=2, column=1, **pad)
        self.entry_grezzo_custom.grid_remove()  # nascosto finché non serve
        self.entry_grezzo_custom.bind("<KeyRelease>", lambda e: self._aggiorna_limite_barra())

        # --- Pulsanti Calcola / Pulisci ---
        pulsanti_frame = ttk.Frame(frame)
        pulsanti_frame.grid(row=3, column=0, columnspan=2, pady=15)

        btn_calcola = ttk.Button(pulsanti_frame, text="Calcola", command=self.on_calcola)
        btn_calcola.grid(row=0, column=0, padx=5)

        btn_pulisci = ttk.Button(pulsanti_frame, text="Pulisci", command=self.on_pulisci)
        btn_pulisci.grid(row=0, column=1, padx=5)

        # --- Output: campo selezionabile/copiabile, solo il numero in metri ---
        ttk.Label(frame, text="Lunghezza minima:").grid(
            row=4, column=0, sticky="w", **pad
        )
        output_frame = ttk.Frame(frame)
        output_frame.grid(row=4, column=1, sticky="w", **pad)

        self.entry_risultato = ttk.Entry(output_frame, width=10, justify="center")
        self.entry_risultato.pack(side="left")
        self.entry_risultato.configure(state="readonly")

        ttk.Label(output_frame, text="m").pack(side="left", padx=(6, 0))

        self.info_var = tk.StringVar(value="")
        info_label = ttk.Label(
            frame, textvariable=self.info_var, foreground="#444444"
        )
        info_label.grid(row=5, column=0, columnspan=2, pady=(0, 10))

        self._aggiorna_limite_barra()
        self.entry_barra.focus()
        self.bind("<Return>", lambda e: self.on_calcola())

    # ---------------- Limite cifre dinamico sul campo barra ----------------
    # Il limite riguarda SOLO la parte intera: la barra accetta valori
    # decimali con virgola o punto (es. 1234,6 oppure 1234.7), e questi
    # vengono interpretati come numeri decimali senza contare le cifre
    # dopo il separatore.

    def _cifre_grezzo_corrente(self) -> int:
        if self.combo_grezzo.get() == "Personalizzato...":
            testo = self.entry_grezzo_custom.get()
        else:
            testo = self.combo_grezzo.get()
        cifre = "".join(ch for ch in testo if ch.isdigit())
        return len(cifre) if cifre else 6  # fallback generoso se vuoto

    def _tronca_a_limite(self, testo: str) -> str:
        sep_index = _trova_separatore(testo)
        parte_intera = testo[:sep_index] if sep_index is not None else testo
        resto = testo[sep_index:] if sep_index is not None else ""
        if len(parte_intera) > self.max_digits_barra:
            parte_intera = parte_intera[: self.max_digits_barra]
        return parte_intera + resto

    def _aggiorna_limite_barra(self):
        self.max_digits_barra = self._cifre_grezzo_corrente()
        attuale = self.entry_barra.get()
        nuovo = self._tronca_a_limite(attuale)
        if nuovo != attuale:
            self.entry_barra.delete(0, tk.END)
            self.entry_barra.insert(0, nuovo)

    def _valida_barra(self, P: str) -> bool:
        if P == "":
            return True
        separatori = [c for c in P if c in ",."]
        if len(separatori) > 1:
            return False  # un solo separatore decimale ammesso
        for c in P:
            if not (c.isdigit() or c in ",."):
                return False
        sep_index = _trova_separatore(P)
        parte_intera = P[:sep_index] if sep_index is not None else P
        if len(parte_intera) > self.max_digits_barra:
            return False
        return True

    def _on_combo_change(self, event=None):
        if self.combo_grezzo.get() == "Personalizzato...":
            self.entry_grezzo_custom.grid()
            self.entry_grezzo_custom.focus()
        else:
            self.entry_grezzo_custom.grid_remove()
        self._aggiorna_limite_barra()

    # ---------------- Calcolo ----------------

    def _get_lunghezza_grezzo(self) -> float:
        if self.combo_grezzo.get() == "Personalizzato...":
            valore = self.entry_grezzo_custom.get().strip().replace(",", ".")
        else:
            valore = self.combo_grezzo.get().strip()
        return float(valore)

    def _set_risultato(self, testo: str):
        self.entry_risultato.configure(state="normal")
        self.entry_risultato.delete(0, tk.END)
        self.entry_risultato.insert(0, testo)
        self.entry_risultato.configure(state="readonly")
        self.entry_risultato.selection_range(0, tk.END)

    def on_calcola(self):
        try:
            barra_txt = self.entry_barra.get().strip().replace(",", ".")
            if not barra_txt:
                raise ValueError("Inserisci la lunghezza della barra.")
            barra_mm = float(barra_txt)
            grezzo_mm = self._get_lunghezza_grezzo()
            n, pezzo_mm = calcola_taglio(barra_mm, grezzo_mm)
            pezzo_m = pezzo_mm / 1000

            self._set_risultato(format_num(pezzo_m))
            self.info_var.set(f"({n} pezzi da {pezzo_mm:.0f} mm)")
        except ValueError as e:
            messagebox.showerror("Errore", str(e))
            self._set_risultato("")
            self.info_var.set("")
        except Exception:
            messagebox.showerror("Errore", "Inserisci valori numerici validi.")
            self._set_risultato("")
            self.info_var.set("")

    def on_pulisci(self):
        """Svuota i campi di input e di output, senza toccare la selezione del grezzo."""
        self.entry_barra.delete(0, tk.END)
        self.entry_grezzo_custom.delete(0, tk.END)
        self._set_risultato("")
        self.info_var.set("")
        self._aggiorna_limite_barra()
        self.entry_barra.focus()


if __name__ == "__main__":
    app = App()
    app.mainloop()
