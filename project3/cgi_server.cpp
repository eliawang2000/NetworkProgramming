#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <boost/algorithm/string/classification.hpp>  // Include boost::for is_any_of
#include <boost/algorithm/string/split.hpp>  // Include for boost::split
#include <boost/algorithm/string.hpp> // include Boost, a C++ library
#include <boost/asio.hpp>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <fstream>
// #include <boost/filesystem.hpp>


using boost::asio::ip::tcp;
using namespace std;
boost::asio::io_context io_context;
string HTML_panel();
// string HTML_console();


class User_info {
public:
    User_info(int port, std::string addr, std::string file)
	: serverPort(port), serverAddr(addr), testFile(file){}

public:
	int serverPort;
	string serverAddr;
	string testFile;
};
// vector<User_info> client_vec;


class session : public enable_shared_from_this<session> {
   public:
    tcp::socket socket_;
    tcp::endpoint endpoint_;
    enum { max_length = 10240 };
    char input_array[max_length];
    vector<string> file_vec;
    string index;
    shared_ptr<tcp::socket> old_socket_;

    session(tcp::socket socket, tcp::endpoint endpoint, string filename, string idx, shared_ptr<tcp::socket> old_socket)
    : socket_(move(socket)), old_socket_(old_socket) {
        endpoint_ = endpoint;
        index = idx;
        file_vec = get_file(filename);
        cout << "0 use_cnt:" << old_socket_.use_count() << endl;
    }

    void start() {
        memset(input_array, '\0', 10240);
        // ptrr = &socket_;
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
        string cmd = file_vec[0];
        file_vec.erase(file_vec.begin());
        boost::asio::async_write(
            socket_, boost::asio::buffer(output_array, strlen(output_array)),
            [this, self, cmd](boost::system::error_code ec, size_t /*length*/) {
                if (!ec) {
                    do_read();

                  if (cmd.find("exit") != string::npos) {
                        socket_.close();
                        cout << "close remote" << endl;
                        cout << "1 use_cnt:" << old_socket_.use_count() << endl;
                        if(old_socket_.use_count() == 2){
                            old_socket_->close();
                        }
                  }
                  else {
                      do_read();
                  }


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
        string new_str = "<script>document.getElementById(\'s" + index + "\').innerHTML += \'" + str + "\';</script>&NewLine;";
        print_output(new_str);
    }

    void output_command(string str) {
        boost::replace_all(str, "&", "&amp;");
        boost::replace_all(str, "<", "&lt;");
        boost::replace_all(str, ">", "&gt;");
        boost::replace_all(str, "\"", "&quot;");
        boost::replace_all(str, "\'", "&apos;");
        boost::replace_all(str, "\n", "&NewLine;");
        boost::replace_all(str, "\r", "");
        string new_str =  "<script>document.getElementById(\'s" + index + "\').innerHTML += \'<b>" + str + "</b>\';</script>&NewLine;";
        print_output(new_str);
    }

    void print_output(std::string msg){
      auto self(shared_from_this());
      boost::asio::async_write(*old_socket_, boost::asio::buffer(msg, msg.length()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/){
          if (ec) {
            cerr << ec << endl;
          }
        });
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


class console : public std::enable_shared_from_this<console> {
private:
    enum { max_length = 10240 };
    tcp::socket socket_;
    char data_[max_length];
    bool is_cgi = true;
    vector<User_info> client_vec;
    string REQUEST_METHOD, REQUEST_URI, QUERY_STRING, SERVER_PROTOCOL, HTTP_HOST;
    string SERVER_ADDR, SERVER_PORT, REMOTE_ADDR, REMOTE_PORT/*, EXEC_FILE*/;

public:
    console(tcp::socket socket) : socket_(std::move(socket)){}
    string panel_html_str;
    string console_html_str;
    shared_ptr<tcp::socket> ptrr;
    void start(){
        // ptrr = shared_ptr<tcp::socket>(&socket_);
        do_read();
        // cout << ptrr->get() << endl;
    }

private:
    void do_read() {
        auto self(shared_from_this()); // 等價於 auto self = std::make_shared<session>(shared_from_this());
        socket_.async_read_some(boost::asio::buffer(data_, max_length), 
            [this, self](boost::system::error_code ec, std::size_t length) {
            if (!ec) {
              parse();
              do_write(length);
              if(REQUEST_URI.find("panel.cgi") != string::npos) {
                cout << "panel" << endl;
                panel_html_str = HTML_panel();
                printHTML(panel_html_str);
              }
              else if(REQUEST_URI.find("console.cgi") != string::npos) {
                cout << "console" << endl;
                ptrr = shared_ptr<tcp::socket>(&socket_);
                InitClients();
                console_html_str = HTML_console();
                printHTML(console_html_str);
                io_context.run();
              }
            }
        });
    }

  string HTML_console(){
	string str;
  str = "<!DOCTYPE html>\
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

	for (long unsigned int i = 0; i < client_vec.size(); ++i)
		str +=  "<th scope=\"col\">" + client_vec[i].serverAddr + ":" + to_string(client_vec[i].serverPort) + "</th>";
	
	str +=   "</tr>\
                        </thead>\
                        <tbody>\
                          <tr>";

	for (long unsigned int i = 0; i < client_vec.size(); ++i)
		str += "<td><pre id = \"s" + to_string(i) + "\" class=\"mb-0\"></pre></td>";

	str +=     "</tr>\
      				</tbody>\
    			</table>\
  			</body>\
		</html>";
  return str;
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

		// EXEC_FILE = boost::filesystem::current_path().string() + REQUEST_URI;
		
	}

    void do_write(std::size_t length){
      auto self(shared_from_this());
      string status_string = "HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\n";
      boost::asio::async_write(socket_, boost::asio::buffer(status_string, status_string.length()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/){});
    }

    void printHTML(std::string str){
      auto self(shared_from_this());
      boost::asio::async_write(socket_, boost::asio::buffer(str, str.length()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/){});
    }

    void InitClients(){
      string addr, port, file;
      replace(QUERY_STRING.begin(), QUERY_STRING.end(), '&', ' ');
      if (QUERY_STRING.length() == 0) return;
      istringstream iss(QUERY_STRING);
      // shared_ptr<tcp::socket> ptrr(&socket_);
      // cout << "hihi use_cnt:" << ptrr.use_count() << endl;
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
              client_vec.push_back(User_info(stoi(port), addr, file));
              cout  << "elia1" << endl;
              Link2Server(i);
          }
          else break;
      }
   }

    void Link2Server(int id){
        // shared_ptr<tcp::socket> ptrr(&socket_);
        tcp::resolver resolver(io_context);
        tcp::resolver::query query(client_vec[id].serverAddr, to_string(client_vec[id].serverPort));
        tcp::resolver::iterator iter = resolver.resolve(query);
        tcp::endpoint endpoint = iter->endpoint();
        tcp::socket socket(io_context);
        // cout << "111  use_cnt:" << ptrr.use_count() << endl;
        make_shared<session>(move(socket), endpoint, client_vec[id].testFile, to_string(id), ptrr)->start();
        // cout << "222  use_cnt:" << ptrr.use_count() << endl;
        cout << "elia2" << endl;
    }

};

class server {
public:
    server(boost::asio::io_context& io_context, short port) : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)){
        do_accept();
    }

private:
    void do_accept() {
      cout << "accept" << endl;
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<console>(std::move(socket))->start();
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

string HTML_panel(){
	string str =  R""""(<!DOCTYPE html>
<html lang="en">
  <head>
    <title>NP Project 3 Panel</title>
    <link
      rel="stylesheet"
      href="https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css"
      integrity="sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2"
      crossorigin="anonymous"
    />
    <link
      href="https://fonts.googleapis.com/css?family=Source+Code+Pro"
      rel="stylesheet"
    />
    <link
      rel="icon"
      type="image/png"
      href="https://cdn4.iconfinder.com/data/icons/iconsimple-setting-time/512/dashboard-512.png"
    />
    <style>
      * {
        font-family: 'Source Code Pro', monospace;
      }
    </style>
  </head>
  <body class="bg-secondary pt-5">
    <form action="console.cgi" method="GET">
      <table class="table mx-auto bg-light" style="width: inherit">
        <thead class="thead-dark">
          <tr>
            <th scope="col">#</th>
            <th scope="col">Host</th>
            <th scope="col">Port</th>
            <th scope="col">Input File</th>
          </tr>
        </thead>
        <tbody>
          <tr>
            <th scope="row" class="align-middle">Session 1</th>
            <td>
              <div class="input-group">
                <select name="h0" class="custom-select">
                  <option></option><option value="nplinux1.cs.nctu.edu.tw">nplinux1</option><option value="nplinux2.cs.nctu.edu.tw">nplinux2</option><option value="nplinux3.cs.nctu.edu.tw">nplinux3</option><option value="nplinux4.cs.nctu.edu.tw">nplinux4</option><option value="nplinux5.cs.nctu.edu.tw">nplinux5</option><option value="nplinux6.cs.nctu.edu.tw">nplinux6</option><option value="nplinux7.cs.nctu.edu.tw">nplinux7</option><option value="nplinux8.cs.nctu.edu.tw">nplinux8</option><option value="nplinux9.cs.nctu.edu.tw">nplinux9</option><option value="nplinux10.cs.nctu.edu.tw">nplinux10</option><option value="nplinux11.cs.nctu.edu.tw">nplinux11</option><option value="nplinux12.cs.nctu.edu.tw">nplinux12</option>
                </select>
                <div class="input-group-append">
                  <span class="input-group-text">.cs.nctu.edu.tw</span>
                </div>
              </div>
            </td>
            <td>
              <input name="p0" type="text" class="form-control" size="5" />
            </td>
            <td>
              <select name="f0" class="custom-select">
                <option></option>
                <option value="t1.txt">t1.txt</option><option value="t2.txt">t2.txt</option><option value="t3.txt">t3.txt</option><option value="t4.txt">t4.txt</option><option value="t5.txt">t5.txt</option>
              </select>
            </td>
          </tr>
          <tr>
            <th scope="row" class="align-middle">Session 2</th>
            <td>
              <div class="input-group">
                <select name="h1" class="custom-select">
                  <option></option><option value="nplinux1.cs.nctu.edu.tw">nplinux1</option><option value="nplinux2.cs.nctu.edu.tw">nplinux2</option><option value="nplinux3.cs.nctu.edu.tw">nplinux3</option><option value="nplinux4.cs.nctu.edu.tw">nplinux4</option><option value="nplinux5.cs.nctu.edu.tw">nplinux5</option><option value="nplinux6.cs.nctu.edu.tw">nplinux6</option><option value="nplinux7.cs.nctu.edu.tw">nplinux7</option><option value="nplinux8.cs.nctu.edu.tw">nplinux8</option><option value="nplinux9.cs.nctu.edu.tw">nplinux9</option><option value="nplinux10.cs.nctu.edu.tw">nplinux10</option><option value="nplinux11.cs.nctu.edu.tw">nplinux11</option><option value="nplinux12.cs.nctu.edu.tw">nplinux12</option>
                </select>
                <div class="input-group-append">
                  <span class="input-group-text">.cs.nctu.edu.tw</span>
                </div>
              </div>
            </td>
            <td>
              <input name="p1" type="text" class="form-control" size="5" />
            </td>
            <td>
              <select name="f1" class="custom-select">
                <option></option>
                <option value="t1.txt">t1.txt</option><option value="t2.txt">t2.txt</option><option value="t3.txt">t3.txt</option><option value="t4.txt">t4.txt</option><option value="t5.txt">t5.txt</option>
              </select>
            </td>
          </tr>
          <tr>
            <th scope="row" class="align-middle">Session 3</th>
            <td>
              <div class="input-group">
                <select name="h2" class="custom-select">
                  <option></option><option value="nplinux1.cs.nctu.edu.tw">nplinux1</option><option value="nplinux2.cs.nctu.edu.tw">nplinux2</option><option value="nplinux3.cs.nctu.edu.tw">nplinux3</option><option value="nplinux4.cs.nctu.edu.tw">nplinux4</option><option value="nplinux5.cs.nctu.edu.tw">nplinux5</option><option value="nplinux6.cs.nctu.edu.tw">nplinux6</option><option value="nplinux7.cs.nctu.edu.tw">nplinux7</option><option value="nplinux8.cs.nctu.edu.tw">nplinux8</option><option value="nplinux9.cs.nctu.edu.tw">nplinux9</option><option value="nplinux10.cs.nctu.edu.tw">nplinux10</option><option value="nplinux11.cs.nctu.edu.tw">nplinux11</option><option value="nplinux12.cs.nctu.edu.tw">nplinux12</option>
                </select>
                <div class="input-group-append">
                  <span class="input-group-text">.cs.nctu.edu.tw</span>
                </div>
              </div>
            </td>
            <td>
              <input name="p2" type="text" class="form-control" size="5" />
            </td>
            <td>
              <select name="f2" class="custom-select">
                <option></option>
                <option value="t1.txt">t1.txt</option><option value="t2.txt">t2.txt</option><option value="t3.txt">t3.txt</option><option value="t4.txt">t4.txt</option><option value="t5.txt">t5.txt</option>
              </select>
            </td>
          </tr>
          <tr>
            <th scope="row" class="align-middle">Session 4</th>
            <td>
              <div class="input-group">
                <select name="h3" class="custom-select">
                  <option></option><option value="nplinux1.cs.nctu.edu.tw">nplinux1</option><option value="nplinux2.cs.nctu.edu.tw">nplinux2</option><option value="nplinux3.cs.nctu.edu.tw">nplinux3</option><option value="nplinux4.cs.nctu.edu.tw">nplinux4</option><option value="nplinux5.cs.nctu.edu.tw">nplinux5</option><option value="nplinux6.cs.nctu.edu.tw">nplinux6</option><option value="nplinux7.cs.nctu.edu.tw">nplinux7</option><option value="nplinux8.cs.nctu.edu.tw">nplinux8</option><option value="nplinux9.cs.nctu.edu.tw">nplinux9</option><option value="nplinux10.cs.nctu.edu.tw">nplinux10</option><option value="nplinux11.cs.nctu.edu.tw">nplinux11</option><option value="nplinux12.cs.nctu.edu.tw">nplinux12</option>
                </select>
                <div class="input-group-append">
                  <span class="input-group-text">.cs.nctu.edu.tw</span>
                </div>
              </div>
            </td>
            <td>
              <input name="p3" type="text" class="form-control" size="5" />
            </td>
            <td>
              <select name="f3" class="custom-select">
                <option></option>
                <option value="t1.txt">t1.txt</option><option value="t2.txt">t2.txt</option><option value="t3.txt">t3.txt</option><option value="t4.txt">t4.txt</option><option value="t5.txt">t5.txt</option>
              </select>
            </td>
          </tr>
          <tr>
            <th scope="row" class="align-middle">Session 5</th>
            <td>
              <div class="input-group">
                <select name="h4" class="custom-select">
                  <option></option><option value="nplinux1.cs.nctu.edu.tw">nplinux1</option><option value="nplinux2.cs.nctu.edu.tw">nplinux2</option><option value="nplinux3.cs.nctu.edu.tw">nplinux3</option><option value="nplinux4.cs.nctu.edu.tw">nplinux4</option><option value="nplinux5.cs.nctu.edu.tw">nplinux5</option><option value="nplinux6.cs.nctu.edu.tw">nplinux6</option><option value="nplinux7.cs.nctu.edu.tw">nplinux7</option><option value="nplinux8.cs.nctu.edu.tw">nplinux8</option><option value="nplinux9.cs.nctu.edu.tw">nplinux9</option><option value="nplinux10.cs.nctu.edu.tw">nplinux10</option><option value="nplinux11.cs.nctu.edu.tw">nplinux11</option><option value="nplinux12.cs.nctu.edu.tw">nplinux12</option>
                </select>
                <div class="input-group-append">
                  <span class="input-group-text">.cs.nctu.edu.tw</span>
                </div>
              </div>
            </td>
            <td>
              <input name="p4" type="text" class="form-control" size="5" />
            </td>
            <td>
              <select name="f4" class="custom-select">
                <option></option>
                <option value="t1.txt">t1.txt</option><option value="t2.txt">t2.txt</option><option value="t3.txt">t3.txt</option><option value="t4.txt">t4.txt</option><option value="t5.txt">t5.txt</option>
              </select>
            </td>
          </tr>
          <tr>
            <td colspan="3"></td>
            <td>
              <button type="submit" class="btn btn-info btn-block">Run</button>
            </td>
          </tr>
        </tbody>
      </table>
    </form>
  </body>
</html>)"""";
return str;

}

