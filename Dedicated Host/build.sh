
FILES='main.c ../bmap.c ../bmap_client.c ../bmap_server.c ../bolo.c ../buf.c ../client.c ../errchk.c ../images.c ../io.c ../list.c ../rect.c ../resolver.c ../server.c ../terrain.c ../tiles.c ../timing.c ../vector.c'

gcc -I.. -lm -lpthread -o dedicatedhost $FILES
