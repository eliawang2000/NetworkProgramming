#include <boost/asio.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <stdio.h>
#include <iomanip>
#include <sstream>
#include <string>
#include <string.h>
#include <array>
#include <iostream>
#include <fstream>
#include <vector>
#include <regex>
#include <boost/algorithm/string.hpp> 

using boost::asio::ip::tcp;
using namespace std;

class Bind : public enable_shared_from_this<Bind> {
public:
    tcp::socket client_socket;
    tcp::socket server_socket;
    tcp::acceptor bind_acceptor;
    enum { max_length = 10240 };
    unsigned char client_buf[max_length];
    unsigned char server_buf[max_length];
    unsigned short port;
    bool bind_flag;

    Bind(tcp::socket client_sock, tcp::socket server_sock, boost::asio::io_context& io_context)
        : client_socket(move(client_sock)), server_socket(move(server_sock)), bind_acceptor(io_context, tcp::endpoint(tcp::v4(), 0)) {
        memset(client_buf, 0x00, max_length);
        memset(server_buf, 0x00, max_length);
        bind_flag = false;
    }

    void start(){
        bind_reply();
    }

private:
    void bind_reply() {
        auto self(shared_from_this());
        port = bind_acceptor.local_endpoint().port();
        unsigned char p1 = port / 256;
        unsigned char p2 = port % 256;
        unsigned char reply_array[8] = {0, 90, p1, p2, 0, 0, 0, 0};
        boost::asio::async_write(
            client_socket, boost::asio::buffer(reply_array, 8),
            [this, self](boost::system::error_code ec, size_t /*length*/) {
                if (!ec) {
                    if (bind_flag == false) 
                        do_accept();
                    else {
                        read_client();
                        read_server();
                    }
                }
            }
        );
    }

    void do_accept() {
        auto self(shared_from_this());
        // tcp::socket server_socket;
        bind_acceptor.async_accept(
            server_socket, [this, self](boost::system::error_code ec) {
                if (!ec) {
                    bind_flag = true;
                    bind_reply();
                    // make_shared<BindSocket>(move(client_socket), move(socket), port)->bind_reply();
                    bind_acceptor.close();
                }
                
            }
        );
    }

    void read_client() {
        auto self(shared_from_this());
        client_socket.async_read_some(
            boost::asio::buffer(client_buf, max_length),
            [this, self](boost::system::error_code ec, size_t length) {
                if (!ec) {
                    write_server(length);
                }
                // else{
                //     client_socket.close();
                // }
                else {
                    if(ec == boost::asio::error::eof){
                        boost::system::error_code ect;
                        client_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_receive, ect);
                        server_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ect);

                    }
                    else {
                        read_client();
                    }
                }
            }
        );
    }

    void write_server(size_t length) {
        auto self(shared_from_this());
        boost::asio::async_write(
            server_socket, boost::asio::buffer(client_buf, length),
            [this, self](boost::system::error_code ec, size_t length) {
                if (!ec) {
                    read_client();
                }
                else{
                    // client_socket.close();
                }
            }
        );
    }

    void read_server() {
        auto self(shared_from_this());
        server_socket.async_read_some(
            boost::asio::buffer(server_buf, max_length),
            [this, self](boost::system::error_code ec, size_t length) {
                if (!ec) {
                    write_client(length);
                }
                // else{
                //     server_socket.close();
                // }
                else {
                    if(ec == boost::asio::error::eof) {
                        boost::system::error_code ect;
                        server_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_receive, ect);
                        client_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ect);
                    }
                    else {
                        read_server();
                    }
                }
            }
        );
    }

    void write_client(size_t length) {
        auto self(shared_from_this());
        boost::asio::async_write(
            client_socket, boost::asio::buffer(server_buf, length),
            [this, self](boost::system::error_code ec, size_t length) {
                if (!ec) {
                    read_server();
                }
                else {
                    // server_socket.close();
                }
            }
        );
    }
};

class conn : public enable_shared_from_this<conn> {
public:
    tcp::socket client_socket;
    tcp::socket server_socket;
    tcp::endpoint endpoint;
    enum { max_length = 10240 };
    unsigned char client_buf[max_length];
    unsigned char server_buf[max_length];

    conn(tcp::socket client_sock, tcp::socket server_sock, tcp::endpoint endpoint)
        : client_socket(move(client_sock)), server_socket(move(server_sock)) {
        this->endpoint = endpoint;
        memset(client_buf, 0x00, max_length);
        memset(server_buf, 0x00, max_length);
    }
    void start() {
        auto self(shared_from_this());
        server_socket.async_connect(
            endpoint,
            [this, self](const boost::system::error_code& ec){
                if(!ec) {
                    conn_reply();
                }
            }
        );
    }
    
private:
    void conn_reply() {
        auto self(shared_from_this());
        unsigned char reply_array[8] = {0, 90, 0, 0, 0, 0, 0, 0};
        boost::asio::async_write(
            client_socket, boost::asio::buffer(reply_array, 8),
            [this, self](boost::system::error_code ec, size_t /*length*/) {
                if (!ec) {
                    read_client();
                    read_server();
                }
            }
        );
    }

    void read_client() {
        auto self(shared_from_this());
        client_socket.async_read_some(
            boost::asio::buffer(client_buf, max_length),
            [this, self](boost::system::error_code ec, size_t length) {
                if (!ec) {
                    write_server(length);
                }
                // else{
                //     client_socket.close();
                // }
                else {
                    if(ec == boost::asio::error::eof) {
                        boost::system::error_code ect;
                        client_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_receive, ect);
                        server_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ect);

                    }
                    else {
                        read_client();
                    }
                }
            }
        );
    }

    void write_server(size_t length) {
        auto self(shared_from_this());
        boost::asio::async_write(
            server_socket, boost::asio::buffer(client_buf, length),
            [this, self](boost::system::error_code ec, size_t length) {
                if (!ec) {
                    read_client();
                }
                else{
                    // client_socket.close();
                }
            }
        );
    }

    void read_server() {
        auto self(shared_from_this());
        server_socket.async_read_some(
            boost::asio::buffer(server_buf, max_length),
            [this, self](boost::system::error_code ec, size_t length) {
                if (!ec) {
                    write_client(length);
                }
                // else{
                //     server_socket.close();
                // }
                else
                {
                    if(ec == boost::asio::error::eof){
                        boost::system::error_code ect;
                        server_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_receive, ect);
                        client_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ect);
                    }
                    else {
                        read_server();
                    }
                }
            }
        );
    }

    void write_client(size_t length) {
        auto self(shared_from_this());
        boost::asio::async_write(
            client_socket, boost::asio::buffer(server_buf, length),
            [this, self](boost::system::error_code ec, size_t length) {
                if (!ec) {
                    read_server();
                }
                else {
                    // server_socket.close();
                }
            }
        );
    }

};

class socks : public enable_shared_from_this<socks> {
public:
    tcp::socket client_socket;
    enum { max_length = 10240 };
    unsigned char client_buf[max_length];
    boost::asio::io_context& io_context;
    string VN;
    string CD;
    string DSTPORT;
    string DSTIP = "";
    string domain = "";
    string D_IP;
    string reply;
    string command;
    tcp::endpoint endpoint;

    socks(tcp::socket socket, boost::asio::io_context& io_context) 
        : client_socket(move(socket)), io_context(io_context) {}

    void start() {
        parse();
    }

private:
    void parse() {
        auto self(shared_from_this());
        client_socket.async_read_some(
            boost::asio::buffer(client_buf, max_length),
            [this, self](boost::system::error_code ec, size_t length) {
                if (!ec) {
                    VN = to_string(client_buf[0]);
                    CD = to_string(client_buf[1]);
                    DSTPORT = to_string(((int)client_buf[2]*256) + (int)client_buf[3]);
                    get_DSTIP();
                    get_domain(length);
                    create_sock();
                }
            }
        );
    }

    void create_sock() {
        /*connect*/
        if(CD == "1") {
            string host;
            if(domain != "")
                host = domain;
            else
                host = DSTIP;
            tcp::resolver resolver(io_context);
            tcp::resolver::query query(host, DSTPORT);
            tcp::resolver::iterator iter = resolver.resolve(query);
            endpoint = iter->endpoint();
            // tcp::endpoint endpoint = iter->endpoint();
            // tcp::socket server(io_context);
            D_IP = endpoint.address().to_string();
            command = "CONNECT";
        }
        /*bind*/
        else if(CD == "2") {
            D_IP = DSTIP;
            command = "BIND";
        }
        confirm_valid();
    }

    void get_domain(size_t length) {
        int pos1 = -1, pos2 = -1;
        for(size_t i = 8; i < length; i++) {
            if(client_buf[i] == 0x00){
                if (pos1 == -1)
                    pos1 = i;
                else pos2 = i;
            }
        }
        if (pos2 != -1){
            for(int j = pos1+1; j < pos2; j++){
                domain += client_buf[j];
            }
        }
    }

    void get_DSTIP() {
        for(int i = 4; i < 8; i++) {
            DSTIP += to_string(client_buf[i]);
            if(i != 7) 
                DSTIP += ".";
        }
    }

    vector<string> get_file_content(string filename) {
        vector<string> result;
        string myText;
        ifstream MyReadFile(filename);
        while (getline (MyReadFile, myText)) {
            result.push_back(myText+"\n");
        }
        MyReadFile.close(); 
        return result;
    }

    void confirm_valid() {
        vector<string> rules = get_file_content("socks.conf"); // 140.114.*.*
        string permission, bind_or_connect, pattern;
        for(long unsigned int i = 0; i < rules.size(); i++) {
            istringstream iss(rules[i]);
            iss >> permission >> bind_or_connect >> pattern;
            if((bind_or_connect == "c" && CD == "1") || (bind_or_connect == "b" && CD == "2")) {
                string client_ip = client_socket.remote_endpoint().address().to_string();
                boost::replace_all(pattern, ".", "\\.");
                boost::replace_all(pattern, "*", ".*");
                regex reg(pattern);
                if(regex_match(client_ip, reg)) {
                    reply = "Accept";
                    print_msg();
                    tcp::socket server_socket(io_context);
                    if (CD == "1"){
                        // tcp::socket server_socket(io_context);
                        make_shared<conn>(move(client_socket), move(server_socket), endpoint)->start();
                    }  
                    else make_shared<Bind>(move(client_socket), move(server_socket), io_context)->start();
                    return;
                }
            }
        }
        reply = "Reject";
        print_msg();
        send_reject();
        return;
    }

    void print_msg(){
        cout << "<S_IP>: " << client_socket.remote_endpoint().address().to_string() << endl;
        cout << "<S_PORT>: " << client_socket.remote_endpoint().port() << endl;
        cout << "<D_IP>: " << D_IP << endl;
        cout << "<D_PORT>: " << DSTPORT << endl;
        cout << "<Command>: " << command << endl;
        cout << "<Reply>: " << reply << endl << endl;
    }

    void send_reject() {
        auto self(shared_from_this());
        unsigned char reply_array[8] = {0, 91, 0, 0, 0, 0, 0, 0};
        boost::asio::async_write(
            client_socket, boost::asio::buffer(reply_array, 8),
            [this, self](boost::system::error_code ec, size_t /*length*/) {
                if (!ec) {
                    client_socket.close();
                }
            }
        );
    }


};

class server {
public:
    tcp::acceptor acceptor_;
    boost::asio::io_context& io_context;
    server(boost::asio::io_context& io_context, short port) : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)), io_context(io_context){
        do_accept();
    }

    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    io_context.notify_fork(boost::asio::io_context::fork_prepare);

                    pid_t pid = fork();
                    while (pid < 0) {
                        pid = fork();
                    }
                    if (pid == 0) { 
                        io_context.notify_fork(boost::asio::io_context::fork_child);
                        acceptor_.close();
                        make_shared<socks>(move(socket), io_context)->start();
                    }
                    else{ 
                        signal(SIGCHLD, SIG_IGN);
                        io_context.notify_fork(boost::asio::io_context::fork_parent);
                        socket.close();
                        do_accept();
                    }
                }
            });
    }
};

int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            // std::cerr << "Usage: async_tcp_echo_server <port>\n";
            return 1;
        }
        boost::asio::io_context io_context;
        server HttpServer(io_context, std::atoi(argv[1]));
        io_context.run();
    }
    catch (std::exception& e) {
        // std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}
