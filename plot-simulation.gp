set title "Évolution de la valeur du Bitcoin"
set xlabel "Temps"
set ylabel "Prix"
set grid
set xdata time
set timefmt "%s"        # Interpréter l'axe X comme des timestamps UNIX
set format x "%H:%M:%S" # Afficher les heures:minutes:secondes
set term wxt persist    # Garder la fenêtre ouverte après exécution
plot "SRD-BTC.dat" using 1:2 with lines title "SRD-BTC"

while(1) {
    replot
    pause 1
}
