#include <boost/asio/io_service.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/algorithm/string/classification.hpp>  
#include <boost/algorithm/string/split.hpp> 
#include <boost/algorithm/string.hpp> 
#include <array>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <boost/filesystem.hpp>

using boost::asio::ip::tcp;
using namespace std;

boost::asio::io_context io_context;

void InitClients();
void SendInitialHTML();
void Link2Server(int id);

class Client {
public:
    Client(int port, std::string addr, std::string file)
	: serverPort(port), serverAddr(addr), testFile(file){}

public:
	int serverPort;
	string serverAddr;
	string testFile;
};

class session : public enable_shared_from_this<session> {
   public:
    tcp::socket socket_;
    tcp::endpoint endpoint_;
    enum { max_length = 10240 };
    char input_array[max_length];
    vector<string> file_vec;
    string index;

    session(tcp::socket socket, tcp::endpoint endpoint, string filename, string idx)
    : socket_(move(socket)) {
        endpoint_ = endpoint;
        index = idx;
        file_vec = get_file(filename);
    }

    void start() {
        memset(input_array, '\0', 10240);
        auto self(shared_from_this());
        socket_.async_connect(
            endpoint_,
            [this, self](const boost::system::error_code& error){
                if(!error) {
                    do_read();
                }
            }
        );
    }

    void do_read() {
        auto self(shared_from_this());
        socket_.async_read_some(
            boost::asio::buffer(input_array, max_length),
            [this, self](boost::system::error_code ec, size_t length) {
                if (!ec) {
                    string data = string(input_array);
                    memset(input_array, '\0', 10240);
                    output_shell(data);
                    if(data.find("%") != string::npos) { // contain %
                        do_write();
                    }
                    else{
                        do_read();
                    }
                }
            }
        );
    }

    void do_write() {
        auto self(shared_from_this());
        char output_array[max_length];
        strcpy(output_array, file_vec[0].c_str());
        output_command(file_vec[0]);
        file_vec.erase(file_vec.begin());
        boost::asio::async_write(
            socket_, boost::asio::buffer(output_array, strlen(output_array)),
            [this, self](boost::system::error_code ec, size_t /*length*/) {
                if (!ec) {
                    do_read();
                }
            }
        );
    }

    void output_shell(string str) {
        boost::replace_all(str, "&", "&amp;");
        boost::replace_all(str, "<", "&lt;");
        boost::replace_all(str, ">", "&gt;");
        boost::replace_all(str, "\"", "&quot;");
        boost::replace_all(str, "\'", "&apos;");
        boost::replace_all(str, "\n", "&NewLine;");
        boost::replace_all(str, "\r", "");
        cout << "<script>document.getElementById(\'s" + index + "\').innerHTML += \'" + str + "\';</script>&NewLine;";
    }

    void output_command(string str) {
        boost::replace_all(str, "&", "&amp;");
        boost::replace_all(str, "<", "&lt;");
        boost::replace_all(str, ">", "&gt;");
        boost::replace_all(str, "\"", "&quot;");
        boost::replace_all(str, "\'", "&apos;");
        boost::replace_all(str, "\n", "&NewLine;");
        boost::replace_all(str, "\r", "");
        cout << "<script>document.getElementById(\'s" + index + "\').innerHTML += \'<b>" + str + "</b>\';</script>&NewLine;";
    }

    vector<string> get_file(string filename) {
        vector<string> input_vec;
        string line;
        ifstream ReadFile;
        ReadFile.open(filename);
        if (!ReadFile.is_open()) {
            cerr << "Could not open the file - " << filename << endl;
        }
        while (getline(ReadFile, line)){
            input_vec.push_back(line+"\n");
        }
        ReadFile.close();
        return input_vec;
    }
};

vector<Client> client_vec;

int main(){
    try {
        cout << "Content-type: text/html\r\n\r\n";
        InitClients();
        SendInitialHTML();
        io_context.run();
    }
    catch (exception& e) {
        cerr << e.what() << endl;
    }
}

void InitClients(){
    string query_string = string(getenv("QUERY_STRING"));
	string addr, port, file;
    replace(query_string.begin(), query_string.end(), '&', ' ');
    if (query_string.length() == 0) return;
	istringstream iss(query_string);
    for (int i = 0; i < 5; ++i){
        iss >> addr >> port >> file;
        if (addr.length() > 3) 
            addr = addr.substr(3, addr.length()-3);
        else addr = "";

		if (port.length() > 3) 
            port = port.substr(3, port.length()-3);
        else port = "";
		
		if (file.length() > 3) {
            string dir = "test_case/";
            file = file.substr(3, file.length()-3);
            file = dir + file;
        }
        else file = "";

		if (addr.length() != 0 && port.length() != 0 && file.length() != 0) {
			client_vec.push_back(Client(stoi(port), addr, file));
            Link2Server(i);
        }
        else break;
	}
}


void Link2Server(int id){
    tcp::resolver resolver(io_context);
    tcp::resolver::query query(client_vec[id].serverAddr, to_string(client_vec[id].serverPort));
    tcp::resolver::iterator iter = resolver.resolve(query);
    tcp::endpoint endpoint = iter->endpoint();
    tcp::socket socket(io_context);
    make_shared<session>(move(socket), endpoint, client_vec[id].testFile, to_string(id))->start();
}

void SendInitialHTML(){
	string HTMLContent;
    cout << "<!DOCTYPE html>\
            <html lang=\"en\">\
            <head>\
                <meta charset=\"UTF-8\" />\
                <title>NP Project 3 Sample Console</title>\
                <link\
                rel=\"stylesheet\"\
                href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\"\
                integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\"\
                crossorigin=\"anonymous\"\
                />\
                <link\
                href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\
                rel=\"stylesheet\"\
                />\
                <link\
                rel=\"icon\"\
                type=\"image/png\"\
                href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\"\
                />\
                <style>\
                * {\
                    font-family: 'Source Code Pro', monospace;\
                    font-size: 1rem !important;\
                }\
                body {\
                    background-color: #212529;\
                }\
                pre {\
                    color: #cccccc;\
                }\
                b {\
                    color: #d29ed4;\
                }\
                </style>\
            </head>\
            <body>\
                <table class=\"table table-dark table-bordered\">\
                <thead>\
                    <tr>";

	for (int i = 0; i < client_vec.size(); ++i)
		cout <<  "<th scope=\"col\">" + client_vec[i].serverAddr + ":" + to_string(client_vec[i].serverPort) + "</th>";
	
	cout <<    "</tr>\
      				</thead>\
      				<tbody>\
        				<tr>";

	for (int i = 0; i < client_vec.size(); ++i)
		cout << "<td><pre id = \"s" + to_string(i) + "\" class=\"mb-0\"></pre></td>";

	cout <<     "</tr>\
      				</tbody>\
    			</table>\
  			</body>\
		</html>";

    cout << endl;
}
