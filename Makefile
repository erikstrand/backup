CC=clang++ -std=c++11
BOOST_INC=/usr/local/boost/include/
BOOST_LIB=/usr/local/boost/lib
BOOST_LIBS=$(BOOST_LIB)/libboost_system.a $(BOOST_LIB)/libboost_filesystem.a $(BOOST_LIB)/libboost_program_options.a

all: bin/backup

bin/backup: src/main.cpp bin/Backup.o bin/FileSize.o
	$(CC) -o bin/backup src/main.cpp bin/Backup.o bin/FileSize.o -I$(BOOST_INC) $(BOOST_LIBS)

bin/Backup.o: src/Backup.cpp src/Backup.h src/FileSize.h
	$(CC) -c src/Backup.cpp -o bin/Backup.o -I$(BOOST_INC) 

bin/FileSize.o: src/FileSize.cpp src/FileSize.h
	$(CC) -c src/FileSize.cpp -o bin/FileSize.o

clean:
	rm -rf bin/*
