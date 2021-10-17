#ifndef SHSERVER_HPP
#define SHSERVER_HPP
#define BOOST_COROUTINES_NO_DEPRECATION_WARNING

#include <boost/smart_ptr/enable_shared_from_this.hpp>
#include <cstdio>
#include <boost/asio/io_context.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/shared_ptr.hpp>
#include <cstdlib>
#include <unistd.h>
#include <string>
#include <iostream>
#include <vector>
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <regex>
#include <iterator>
#include <algorithm>
#include <list>
#include <memory>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <boost/array.hpp>

using namespace boost::asio;
using namespace boost::asio::ip;
using namespace std::filesystem;

//

class Session:
        public std::enable_shared_from_this<Session>
{
public:
    Session(tcp::socket socket, io_context& iocontext):
        socket(std::move(socket)),
        context(iocontext),
        harvest(iocontext){
        curpath = current_path();
        active = true; // mode
    }
    void start();
    void fstart();
    bool active;
private:
    path curpath;
    // FTP - comm
    std::string handleRETR(std::string file, yield_context yield); // RETR - client handler
    std::string handleCWD(std::string dir); // CHDIR - client handler
    std::string habdleLIST(yield_context yield); // LIST - client handler
    std::string handleSTOR(std::string, yield_context); // STOR - client handler

    std::string parseIn(yield_context yield); // input parser
    bool isEqual(std::string it, std::string that);
    void echo_(yield_context yield);

    void read_(yield_context); // read data from socket, use rd_buffer
    void write_(std::string message, yield_context); // write data to socket, use wr_buffer
    char wr_buffer[4096];
    char rd_buffer[4096];

    tcp::socket socket;     // communication socket
    tcp::acceptor harvest;  // connection acceptor
    io_context& context;    // IO-context, just all u need

};

// SERVER

class SHserver: public std::enable_shared_from_this<SHserver>{

public:
    SHserver(io_context& iocontext, int port):
        context(iocontext),
        harvest(context, tcp::endpoint(tcp::v4(), port))
    {};

    void accept_(tcp::socket& socket){
        harvest.set_option(socket_base::reuse_address{true});

        spawn(context,
              [this, &socket](yield_context yield){
            for(;;){
                boost::system::error_code error;

                harvest.async_accept(socket, yield[error]);
                if(error){return;}
                std::make_shared<Session>(std::move(socket), context)->start();

            }
        });
    }


private:

    io_context& context;
    tcp::acceptor harvest;

};


#endif // SHSERVER_HPP
