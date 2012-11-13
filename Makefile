CC=g++ -g
BOOST_INC=/usr/local/boost/include/
BOOST_LIB=/usr/local/boost/lib
BOOST_LIBS=$(BOOST_LIB)/libboost_system.a $(BOOST_LIB)/libboost_filesystem.a

backup: backup.cpp
	$(CC) -o backup backup.cpp -I$(BOOST_INC) $(BOOST_LIBS)

clean:
	rm backup
