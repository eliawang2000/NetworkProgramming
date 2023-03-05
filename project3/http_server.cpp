#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sstream>
#include <regex>
#include <boost/filesystem.hpp>

using namespace std;
using boost::asio::ip::tcp;
boost::asio::io_context io_context;

class session : public std::enable_shared_from_this<session> {
private:
    enum { max_length = 1024 };
    tcp::socket socket_;
    char data_[max_length];
    bool is_cgi = true;
	string REQUEST_METHOD, REQUEST_URI, QUERY_STRING, SERVER_PROTOCOL, HTTP_HOST;
	string SERVER_ADDR, SERVER_PORT, REMOTE_ADDR, REMOTE_PORT, EXEC_FILE;
public:
    session(tcp::socket socket) : socket_(std::move(socket)){}
    void start(){
        do_read();
    }

private:
    void do_read() {
        auto self(shared_from_this()); // 等價於 auto self = std::make_shared<session>(shared_from_this());
        socket_.async_read_some(boost::asio::buffer(data_, max_length), 
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
					parse();
					set_env();
					do_write(length);
                }
        });
    }

	void parse() {
		string temp = string(data_);
		istringstream iss(temp);
		iss >> REQUEST_METHOD >> REQUEST_URI >> SERVER_PROTOCOL >> temp >> HTTP_HOST;

		SERVER_ADDR = socket_.local_endpoint().address().to_string();
		REMOTE_ADDR = socket_.remote_endpoint().address().to_string();

		SERVER_PORT = to_string(socket_.local_endpoint().port());
		REMOTE_PORT = to_string(socket_.remote_endpoint().port());

		iss = istringstream(REQUEST_URI);
		getline(iss, REQUEST_URI, '?');
		getline(iss, QUERY_STRING);

		EXEC_FILE = boost::filesystem::current_path().string() + REQUEST_URI;
		
	}

    void set_env() {
		setenv("REQUEST_METHOD", REQUEST_METHOD.c_str(), 1);
		setenv("REQUEST_URI", REQUEST_URI.c_str(), 1);
		setenv("QUERY_STRING", QUERY_STRING.c_str(), 1);
		setenv("SERVER_PROTOCOL", SERVER_PROTOCOL.c_str(), 1);
		setenv("HTTP_HOST", HTTP_HOST.c_str(), 1);
		setenv("SERVER_ADDR", SERVER_ADDR.c_str(), 1);
		setenv("SERVER_PORT", SERVER_PORT.c_str(), 1);
		setenv("REMOTE_ADDR", REMOTE_ADDR.c_str(), 1);
		setenv("REMOTE_PORT", REMOTE_PORT.c_str(), 1);
	}

    void do_write(std::size_t length){
		auto self(shared_from_this());
		// writes a certain amount of data to a stream before completion.
		string status_string = "HTTP/1.1 200 OK\n";
		boost::asio::async_write(socket_, boost::asio::buffer(status_string, status_string.length()),
			[this, self](boost::system::error_code ec, std::size_t /*length*/){
			if (!ec){
				do_cgi();
			}
			});
    }

	void do_cgi() {
		// pid_t pid;
		io_context.notify_fork(boost::asio::io_context::fork_prepare); //告知接下來將要fork

		// while((pid = fork()) < 0);

		int status;
		pid_t pid = fork();
		while (pid < 0) {
			wait(&status);
			pid = fork();
		}

		if(pid == 0) {

			io_context.notify_fork(boost::asio::io_context::fork_child); 
			int sock = socket_.native_handle();
			dup2(sock, STDERR_FILENO);
			dup2(sock, STDIN_FILENO);
			dup2(sock, STDOUT_FILENO);
			socket_.close();

			if (execlp(EXEC_FILE.c_str(), EXEC_FILE.c_str(), NULL) < 0) {
				cout << "execlp failed" << endl;
				fflush(stdout);
			}
			else {cout << "execlp success" << endl;}
		}
		else {
			io_context.notify_fork(boost::asio::io_context::fork_parent);
			socket_.close();
			signal(SIGCHLD,SIG_IGN);
		}
	}
};

class server {
public:
    server(boost::asio::io_context& io_context, short port) : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)){
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<session>(std::move(socket))->start();
                }

                do_accept();
            });
    }

    tcp::acceptor acceptor_;
};

int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: async_tcp_echo_server <port>\n";
            return 1;
        }
        server HttpServer(io_context, std::atoi(argv[1]));
        io_context.run();
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}
