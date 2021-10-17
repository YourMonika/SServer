#include "shserver.hpp"

void Session::start(){
    auto self(shared_from_this());

    spawn(this->socket.get_executor(),
          [self](yield_context yield){
              self->write_("220 SESSION STARTED", yield);

              self->echo_(yield);
          });
}

void Session::write_( std::string context, yield_context yield ) {
    auto self(shared_from_this());
    context.append("\n");
    boost::system::error_code error;

    self -> socket.async_send(buffer(context.c_str(), context.length()), yield[error]);
    if(error) { std::cerr << "SUS WRITING\n"; self->active = false; }

    std::cout << "RESPONSE" << context << "SENT TO" << &self->socket << "\n";
    std::cout << ".\n.\n.\n";
}

void Session::read_(yield_context yield) {
    auto self(shared_from_this());

    memset(self->rd_buffer, 0, 4096);

    boost::system::error_code error;

    self->socket.async_read_some(buffer(self->rd_buffer, 4096), yield[error]);
    if(error){ return;}
    else { std::cout << "READ " << std::string(self->rd_buffer) << "\n"; }
}

void Session::echo_(yield_context yield){
  auto self(shared_from_this());

  spawn(context, [self](yield_context yield){
    for(;;){
      if(!self->active) { break; }
      self->read_(yield);

      self->write_(self->parseIn(yield), yield);
    }
  });
}

bool Session::isEqual(std::string it, std::string that){
  std::transform(it.begin(), it.end(), it.begin(), tolower); // !!! std::tolower
  return (it.compare(that) == 0);
}

std::string Session::handleLIST(yield_context yield){
    auto self(shared_from_this());
    auto data_sock = std::make_shared<tcp::socket>(context);

    std::string toSend = "";

    for (const auto & file : directory_iterator(this->curpath)){
        toSend.append(file.path());
        toSend.append("\r\n");
    }

    write_("150 SENDING LIST DATA", yield);

    boost::system::error_code error;

    spawn(context, [self, data_sock, toSend](yield_context yield){
        boost::system::error_code error;
        self->harvest.async_accept(*data_sock, yield[error]);
        data_sock->async_write_some(buffer(toSend.c_str(), toSend.size()), yield[error]);

        if(error){ std::cerr << "ERROR SENDING LIST DIRECTORY\n"; }
      });
      return("226 DIRECTORY DATA SENT");
}

std::string Session::handleCWD(std::string goTo){
        auto newWD = current_path() / goTo;

        if(!exists(newWD)){
          return("450 ACTION NOT TAKEN, GIVEN DIRECTORY DOESN'T EXIST");
        }
        if(status(newWD).type() != file_type::directory){
          return("450 ACTION NOT TAKEN, NOT A DIRECTORY!");
        }
        this->curpath.assign(newWD);
        current_path(this->curpath);
        return("250 CHANGE DIRECTORY OK");
}

std::string Session::handleRETR(std::string fname, yield_context yield){
    auto self(shared_from_this());

    auto data_sock = std::make_shared<tcp::socket>(context);

    std::ifstream local_file;
    std::filesystem::path local_file_path(fname);

    local_file.open(local_file_path, std::ios_base::binary|std::ios_base::ate);

    std::size_t filesize = local_file.tellg();
    local_file.seekg(0);

    boost::system::error_code error;

    if(!exists(local_file_path)){
        return("450 ACTION NOT TAKEN, FILE DOESN'T EXIST");
    }
    if(status(local_file_path).type() != file_type::regular){
        return("450 ACTION NOT TAKEN, THIS IS NOT A FILE");
    }

    char * buff = new char [filesize];
    if(local_file){
        local_file.read(buff, (std::streamsize) filesize);

        std::cout << "SENDING " << local_file.gcount() << " BYTES\n";
        write_("150 OPENING BINARY MODE TO TRANSFER DATA", yield);

        spawn(context,
            [self, data_sock, buff, buffsize = local_file.gcount()](yield_context yield){
            boost::system::error_code error;
            self->harvest.async_accept(*data_sock, yield[error]);
            std::cout << data_sock->local_endpoint() << " --- " << data_sock->remote_endpoint() << "\n";
            data_sock->async_write_some(buffer(buff, buffsize), yield[error]);

    });
  }
  return("226 FILE TRANSFER SUCCESFUL");
}

std::string Session::handleSTOR(std::string toGet, yield_context yield){
    auto self(shared_from_this());
    path filename(toGet);
    filename = curpath / filename;

    std::cout << std::string(filename);
    if(exists(filename)){
        return("450 FILE ALREADY EXISTS");
    }

    auto data_sock = std::make_shared<tcp::socket>(context);


    write_("150 READY TO ACCEPT DATA", yield);
    boost::system::error_code error;

    spawn(context, [self, data_sock, toGet, filename](yield_context yield){
    boost::system::error_code error;
    self->harvest.async_accept(*data_sock, yield[error]);

    std::shared_ptr<std::vector<char>> buff = std::make_shared<std::vector<char>>(1024*1024*1);

    std::fstream fs(filename, std::ios::out | std::ios::binary);

    async_read(*data_sock, boost::asio::buffer(*buff), transfer_at_least(buff->size()), yield[error]);
    buff->shrink_to_fit();
    fs.write(buff->data(), static_cast<std::streamsize>(buff->size()));

    });
    return("250 UPLOAD SUCCESFUL");
}

std::string Session::parseIn(yield_context yield){
    auto self(shared_from_this());

    std::string input = std::string(rd_buffer);

    std::cout << "ECHO INPUT IS: " << input << '\n';

    std::string response = "250 BAD COMMAND";
    std::regex word_regex("(\\S+)");

    if(input.size() == 0){
        return response;
    }
    auto words_begin = std::sregex_iterator(input.begin(), input.end(), word_regex);
    auto words_end = std::sregex_iterator();

    std::smatch match = *words_begin;
    std::string command = match.str();

    if (std::distance(words_begin, words_end) == 1){
        if (isEqual(command, "PWD")){
            response = "257 \"" + std::string(this->curpath) + "\" IS CURRENT DIRECTORY";
        }
        else if (isEqual(command, "LIST")) {
            return(handleLIST(yield));
        }
        else if (isEqual(command, "PASV")){
            if(harvest.is_open()){
                harvest.close();
            }
            boost::system::error_code error;

            tcp::endpoint ep(tcp::v4(), 0);

            harvest.open(ep.protocol(), error);
            harvest.bind(ep);
            harvest.listen(socket_base::max_connections, error);

            auto ip_bytes = socket.local_endpoint().address().to_v4().to_bytes();
            auto port     = harvest.local_endpoint().port();

            std::stringstream stream;
            stream << "227 ";
            for (size_t i = 0; i < 4; i++)
            {
                stream << static_cast<int>(ip_bytes[i]) << ",";
            }
            stream << ((port >> 8) & 0xff) << "," << (port & 0xff);

            return(stream.str());
        }
        else if (isEqual(command, "QUIT")){
            response = "221 CONNECTION CLOSE BY CLIENT";
            self->socket.close();
            self->active = false;
        }
        else if (isEqual(command, "FEAT")){
            response = "211 END";
        }
        else if (isEqual(command, "SYST")) {
            response = "215 UNIX TYPE: SUS";
        }
        else if (isEqual(command, "CDUP")){
            if(curpath == "/"){
                return("450 CAN'T GO UP FROM ROOT DIRECTORY");
            } else{
                curpath = current_path().parent_path();
                current_path(curpath);
                return("200 CHDIR OK");
            }
        }
    }
    else{
        std::vector<std::string> queryParams;

        for(std::sregex_iterator i = ++words_begin; i !=  words_end; ++i){
            queryParams.push_back((*i).str());
        }

        if(isEqual(command, "CWD")){
            if(queryParams.size() > 1){
                response = "450 ACTION NOT TAKEN, NO DIRECTORY SPECIFIED";
            }
            else {
                return(handleCWD(queryParams.front()));
            }
        }
        else if (isEqual(command, "PORT")){
            response = "451 ACTION NOT TAKEN, USE PASSIVE MODE";
        }
        else if (isEqual(command, "RETR")){
            if(queryParams.size() > 1){
                response = "450 ACTION NOT TAKEN, NO FILE SPECIFIED";
            }
            else {
                return(handleRETR(queryParams.front(), yield));
            }
        }
        else if (isEqual(command, "USER")) {
            response = "331 PLEASE SPECIFY THE PASSWORD";
        }
        else if (isEqual(command, "PASS")) {
            response = "230 LOGIN SUCCESFUL";
        }
        else if (isEqual(command, "TYPE")) {
            response = "200 SWITCHING TO BINARY MODE";
        }
        else if (isEqual(command, "DELE")){
            auto file = queryParams.front();

            if(!exists(file)){
                return("450 ACTION NOT TAKEN, FILE DOESN'T EXIST");
            }
            if(status(file).type() != file_type::regular){
                return("450 ACTION NOT TAKEN, THIS IS NOT A FILE");
            }

            remove(file);
            response = "250 DELETED SUCCESFULLY";
        }
        else if (isEqual(command, "STOR")){
            return(handleSTOR(queryParams.front(), yield));
        }
        else if (isEqual(command, "RMD")){

            if(rmdir(queryParams.front().c_str()) == 0){
                return("250 DELETED SUCCESFULLY");
            }
            else { return("450 ACTION NOT TAKEN"); }
        }
    }
    return response;
}
