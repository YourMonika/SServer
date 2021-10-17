#include "shserver.hpp"

int main(int argc, char* argv[]){
    if(argc != 3){
        std::cerr << "WRONG USAGE! USE: SServer <PORT> <NUM_THREADS>\n";
       return(-1);
    }

    
    //initializing session
    //try exceptions
    unsigned int port;
    std::size_t t_num;

    try{
        port = std::stoi(argv[1]);
    }
    catch(std::invalid_argument exception){
        std::cerr << "VALUE CAN'T BE USED TO INITIALIZE PORT. ./SServer >>error<<  <t>.\n";
        return(-1);
    }

    try{
        t_num = std::stoi(argv[2]);
    }
    catch(std::invalid_argument exception){
        std::cerr << "VALUE CAN'T BE USED TO INITIALIZE THREADS. ./SServer <n> >>error<<.\n";
        return(-1);
    }

    io_context context;
    thread_pool threadpool(t_num);
    SHserver server(context, port);
    tcp::socket socket(context);

    server.accept_(socket);

    for(int i = 0; i < t_num; ++i){
        post(threadpool, [&context](){
            context.run();
            });
    }

    threadpool.join();

    return 0;
}
