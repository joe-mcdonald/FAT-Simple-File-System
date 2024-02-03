p3

diskinfo: displays the infomation about the file system
disklist: displays a list of files and their directories at the directoru specified (otherwise it shows the root)
diskget: copies a file from the file system to the current linux directory
diskput: copies a file from the current linux directory to the file system

to compile, run:
make

to run each of the programs, run:
diskinfo:
./diskinfo [file system image]

disklist:
./disklist [file system image] [/subdirectory - optional]

diskget:
./diskget [file system image] [source path] [destination path]

diskput:
./diskput [file system image] [source path] [destination path]


Note about diskput.c:
- the program runs, it can push a file into the file system, but there 
are some errors that should be noted. One error is that it will not 
push to sub directories, if you try to push to a subdirectory it will 
push it to the root. The other issue is that the name of the file copy 
is always stuck as the same name from the linux file system. For 
instance, if you wanted to copy 'foo_joe.txt' to the file system as 
'foo_joe_p3.txt', it will copy correctly but it will be called 
'foo_joe.txt'. Those are the 2 errors I observed. The other 3 files 
should work fine and have been tested vigorously.