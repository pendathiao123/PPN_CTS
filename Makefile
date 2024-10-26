CC = g++
CFLAGS = -Wall -g

# Définition des fichiers objets
OBJ = Client.o Server.o Crypto.o

all: Main_Cli Main_Serv Main

Main_Cli: Main_Cli.o $(OBJ)
	$(CC) $(CFLAGS) -o Main_Cli Main_Cli.o $(OBJ)

Main_Serv: Main_Serv.o $(OBJ)
	$(CC) $(CFLAGS) -o Main_Serv Main_Serv.o $(OBJ)

Main: Main.o $(OBJ)
	$(CC) $(CFLAGS) -o Main Main.o $(OBJ)

# Règle pour compiler les fichiers .cpp en fichiers .o
%.o: %.cpp
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o Main_Cli Main_Serv Main