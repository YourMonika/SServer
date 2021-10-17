REQUIRMENTS:
--------------------------
lib Boost
gcc (9+) 
(- lower version doesn't contain work with std::filesystem) 
ninja


MAKE:
--------------------------
cmake -G Ninja


BUILD:
--------------------------
ninja

USAGE:
--------------------------
./SServer <p> <t>
<p> - Port you use to work
<t> - Num of threads you use in work

Also any other FTP-client. e.g. FileZilla 