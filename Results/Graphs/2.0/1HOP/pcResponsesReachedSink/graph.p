set xlabel "Network Size"
set ylabel "Percentage Correctly Evaluated"
set pointsize 1
set key right top
set title "Response Reached Sink"
set xrange [14:49]
set xtics (15,30,48)
set yrange [0:1]
set ytics auto
set terminal pdf enhanced
set output "graph.pdf" 
plot "graph.dat" u 1:2:3 w errorlines ti "PELE",\
"graph.dat" u 1:4:5 w errorlines ti "PELP"
