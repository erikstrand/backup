# backup

I wrote this command line tool to backup large collections of infrequently changed files. It uses the boost filesystem library (http://www.boost.org/doc/libs/1_52_0/libs/filesystem/doc/index.htm) for filesystem traversal, and C++ streams for the actual business of copying. It does not do any version tracking, and generates no auxiliary files.

