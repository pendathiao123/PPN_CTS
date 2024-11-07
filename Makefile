CC = g++
CFLAGS = -Wall -g

# Répertoires des fichiers source
SRC_DIR = src/code
INCLUDE_DIR = src/headers

# Fichiers d'en-tête
CPPFLAGS = -I$(INCLUDE_DIR)

# Cibles et règles
all: main_cli main_serv

main_cli: $(SRC_DIR)/Main_Cli.o $(SRC_DIR)/Server.o $(SRC_DIR)/Crypto.o $(SRC_DIR)/Client.o $(SRC_DIR)/ClientHandler.o
	$(CC) -o main_cli $(SRC_DIR)/Main_Cli.o $(SRC_DIR)/Server.o $(SRC_DIR)/Crypto.o $(SRC_DIR)/Client.o $(SRC_DIR)/ClientHandler.o

main_serv: $(SRC_DIR)/Main_Serv.o $(SRC_DIR)/Server.o $(SRC_DIR)/Crypto.o $(SRC_DIR)/ClientHandler.o
	$(CC) -o main_serv $(SRC_DIR)/Main_Serv.o $(SRC_DIR)/Server.o $(SRC_DIR)/Crypto.o $(SRC_DIR)/ClientHandler.o

$(SRC_DIR)/Main_Cli.o: $(SRC_DIR)/Main_Cli.cpp $(INCLUDE_DIR)/Server.h $(INCLUDE_DIR)/Crypto.h $(INCLUDE_DIR)/Client.h
	$(CC) -c $(SRC_DIR)/Main_Cli.cpp $(CPPFLAGS) -o $(SRC_DIR)/Main_Cli.o

$(SRC_DIR)/Main_Serv.o: $(SRC_DIR)/Main_Serv.cpp $(INCLUDE_DIR)/Server.h $(INCLUDE_DIR)/Crypto.h
	$(CC) -c $(SRC_DIR)/Main_Serv.cpp $(CPPFLAGS) -o $(SRC_DIR)/Main_Serv.o

$(SRC_DIR)/Server.o: $(SRC_DIR)/Server.cpp $(INCLUDE_DIR)/Server.h
	$(CC) -c $(SRC_DIR)/Server.cpp $(CPPFLAGS) -o $(SRC_DIR)/Server.o

$(SRC_DIR)/Crypto.o: $(SRC_DIR)/Crypto.cpp $(INCLUDE_DIR)/Crypto.h
	$(CC) -c $(SRC_DIR)/Crypto.cpp $(CPPFLAGS) -o $(SRC_DIR)/Crypto.o

$(SRC_DIR)/Client.o: $(SRC_DIR)/Client.cpp $(INCLUDE_DIR)/Client.h
	$(CC) -c $(SRC_DIR)/Client.cpp $(CPPFLAGS) -o $(SRC_DIR)/Client.o

$(SRC_DIR)/ClientHandler.o: $(SRC_DIR)/ClientHandler.cpp $(INCLUDE_DIR)/ClientHandler.h
	$(CC) -c $(SRC_DIR)/ClientHandler.cpp $(CPPFLAGS) -o $(SRC_DIR)/ClientHandler.o

clean:
	rm -f $(SRC_DIR)/*.o main_cli main_serv