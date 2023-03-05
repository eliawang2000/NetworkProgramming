#include <iostream>
#include <string>
#include <unistd.h>
#include <cstring>
#include <string.h>
#include <vector>
#include <signal.h>
#include <sstream>
#include <queue>
#include <map>
#include <utility>
#include <sys/wait.h>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <netdb.h>
using namespace std;

int passiveTCP(const char* service, int qlen);
int passivesock(const char *service, const char *protocol, int qlen);
void welcome(int n);
void broadcast_login(int n);
void create_pipe(vector <string> single_job, int job_cnt, map <int, pair <int, int> > &pipe_map, int ID, string input);
vector<string> command_split(string command);
void parse(string str1, queue<string> &q);
void close_pipe(pair <int, int> p);
void execvp_work(string final_job, pair <int, int> left_pipe_id, pair <int, int> right_pipe_id, bool send_error_to_other, bool file_redirection, string redirection_file_name, bool number_pipe, int ID, bool not_send, bool not_recv);
int get_to_client(string str);
int get_from_client(string str);
string del_redir_client_info(string str);
void who(int me);
void yell(int me, string yell_msg);
void tell(int me, int tellwho, string tell_msg);
void change_name(int me, string name_msg);
void broadcast_msg(string send_pipe_msg);
bool check_from_client(int &from_which_client, int ID, pair <int, int> &left_pipe_id, pair <int, int> &right_pipe_id, string input);
bool check_to_client(int &to_which_client, int ID, pair <int, int> &left_pipe_id, pair <int, int> &right_pipe_id, string input);
void logout(int ID);
void initenv(int ID);
void set_env(int ID);
#define MAX_CLIENTS 31
#define QLENS 30

class user_info {
public:
    int ID;
    int fd;
    int port;
    char name[21];
    char ip[25];
    map <string, string> env_map;
    map <int, pair <int, int> > pipe_map;
    int client_pipe[MAX_CLIENTS][2]; //to whom
    int job_cnt;
} user[MAX_CLIENTS];

fd_set rfds;
fd_set afds;
int numfd;

int main(int argc, const char* argv[]) {
    // user initiation
    for(int i = 0; i < MAX_CLIENTS; ++i) {
        user[i].ID = -1;
        user[i].fd = -1;
        user[i].port = -1;
        memset(user[i].name, '\0', 21);
        strcpy(user[i].name, "(no name)");
        memset(user[i].ip, '\0', 25);
        user[i].env_map["PATH"] = "bin:.";
        for (int j = 0; j < MAX_CLIENTS; ++j){
            user[i].client_pipe[j][0] = -1;
            user[i].client_pipe[j][1] = -1;
        }
        user[i].job_cnt = 0;
    }

    int msock = passiveTCP(argv[1], QLENS);
    cout << "master sock number:" << msock << endl;
    FD_ZERO(&afds);
    FD_SET(msock, &afds);
    while(1){
        setenv("PATH", "bin:.", 1);
        // memcpy(&rfds, &afds, sizeof(rfds));
        numfd = getdtablesize();
        
        // if (select(numfd, &rfds, NULL, NULL, NULL)< 0){
        //     // cout<< "select fault " << endl;
        //     continue;
        // }
        // cout << "select_val:" << select_val << endl;
        if (FD_ISSET(msock, &rfds)) {
            int ssock;
            struct sockaddr_in addr;
            int addrlen = sizeof(addr);
            ssock = accept(msock, (struct sockaddr *)&addr, (socklen_t*)&addrlen);
            cout << "ssock:" << ssock << endl;
            if(ssock < 0) {
                continue;
            }
            for(int i = 1; i < MAX_CLIENTS; ++i) {
                if(user[i].fd == -1) {
                    user[i].ID = i;
                    user[i].fd = ssock;
                    user[i].port = ntohs(addr.sin_port);
                    strcpy(user[i].name, "(no name)");
                    strcpy(user[i].ip, inet_ntoa(addr.sin_addr));
                    // clearenv();
                    setenv("PATH", user[i].env_map["PATH"].c_str(), 1);
                    // cout << "PATH = " << user[i].env_map["PATH"] << endl;
                    user[i].job_cnt = 0;
                    if (ssock <= numfd)
                        numfd = ssock + 1;
                    FD_SET(ssock, &afds);
                    welcome(i);
                    broadcast_login(i);
                    string server_str = "% ";
                    send(ssock, server_str.c_str(), strlen(server_str.c_str()), 0);
                    break;
                }
                else if(i == MAX_CLIENTS - 1) {
                    string max_user_string = "max user online\n";
                    send(ssock, max_user_string.c_str(), strlen(max_user_string.c_str()), 0);
                    close(ssock);
                }
            }
        }    
        memcpy(&rfds, &afds, sizeof(rfds));
        if (select(numfd, &rfds, NULL, NULL, NULL)< 0){
            // cout<< "select fault " << endl;
            continue;
        }
        
        for(int i = 1; i < MAX_CLIENTS; ++i) {
            if(user[i].fd != -1 && FD_ISSET(user[i].fd, &rfds)) {
                int readlen;
                char input_array[15001];
                memset(input_array, '\0', 15001);
                // cout << "user id:" << i << endl;
                readlen = recv(user[i].fd, input_array, 15000, 0);
                // cout << "readlen:" << readlen << endl;
                if (readlen < 0){
                    continue;
                }
                else if (readlen == 0){
                    //logout
                    logout(i);
                }
                else {
                    char *bp;
                    if((bp = strchr(input_array, '\n')) != NULL)
                        *bp = '\0';
                    if((bp = strchr(input_array, '\r')) != NULL)
                        *bp = '\0';
                }
                string input = string(input_array);
                cout << "input:" << input << endl;
                if (input == "exit"){
                    logout(i);
                    continue;
                }
                queue<string> job_queue;
                parse(input, job_queue);
                set_env(i);
                // setenv("PATH", user[i].env_map["PATH"].c_str(), 1);
                while (job_queue.size() != 0) {
                    string command;
                    command = job_queue.front();
                    // cout << "command:" << command << endl; 
                    job_queue.pop();
                    user[i].job_cnt++;
                    vector<string> work_vec;
                    vector<string> single_job;
                    string tmp_str = "";
                    work_vec = command_split(command);
                    for (int k = 0; k < work_vec.size(); k++) {
                        // cout << work_vec[k] << endl;
                        if (work_vec[k][0] == '|' || work_vec[k][0] == '!' || work_vec[k] == ">") {
                            single_job.push_back(tmp_str);
                            single_job.push_back(work_vec[k]);
                            tmp_str = "";
                        }
                        else {
                            tmp_str += work_vec[k];
                            tmp_str += " ";
                        }
                    }
                    if (tmp_str != "")
                        single_job.push_back(tmp_str);
                    create_pipe(single_job, user[i].job_cnt, user[i].pipe_map, user[i].ID, input);
                }
                initenv(i);
                string server_str = "% ";
                send(user[i].fd, server_str.c_str(), strlen(server_str.c_str()), 0);
            }
        }
    }
    close(msock);
}

void create_pipe(vector <string> single_job, int job_cnt, map <int, pair <int, int> > &pipe_map, int ID, string input){
    bool receive = false;
    pair <int, int> left_pipe_id;
    pair <int, int> right_pipe_id;
    left_pipe_id.first = -1;
    left_pipe_id.second = -1;
    right_pipe_id.first = -1;
    right_pipe_id.second = -1;
    bool send_error_to_other = false;
    bool file_redirection = false;
    bool number_pipe = false;
    int to_which_client = -1;
    int from_which_client = -1;
    string redirection_file_name = "";
    for (int i = 0; i < single_job.size(); i++) {
        bool not_send = false;
        bool not_recv = false;
        cout << "single_job:" << single_job[i] << endl;
        // cout << "job_cnt:" << job_cnt << endl;
        // if (i == 0){
        if (pipe_map.find(job_cnt) != pipe_map.end()) {
            // cout << "someone sent msg to you" << job_cnt<< endl;
            receive = true;
            left_pipe_id.first = pipe_map[job_cnt].first;
            left_pipe_id.second = pipe_map[job_cnt].second;
            pipe_map.erase(job_cnt);
        }
        // }
        bool next = false;
        if (i != single_job.size()-1)    
            next = true;
        vector <string> env_vec;
        env_vec = command_split(single_job[i]);
        if (env_vec[0] == "printenv") {
            if (user[ID].env_map.find(env_vec[1].c_str()) == user[ID].env_map.end()){
                char *env_str = getenv(env_vec[1].c_str());
                if (env_str != NULL) 
                    send(user[ID].fd, env_str, strlen(env_str), 0);
            }
            else {
                string env_str = user[ID].env_map[env_vec[1]] + "\n";
                send(user[ID].fd, env_str.c_str(), strlen(env_str.c_str()), 0);
            } 
        }
        else if (env_vec[0] == "setenv") {
            user[ID].env_map[env_vec[1]] = env_vec[2]; 
            setenv(env_vec[1].c_str(), env_vec[2].c_str(), 1);
            char *env_str = getenv(env_vec[1].c_str());
            if (env_str != NULL) 
                cout << env_str << endl;
        }
        else if (env_vec[0] == "who" && env_vec.size() == 1) {
            who(ID);
        }
        else if (env_vec[0] == "yell") {
            yell(ID, input);
            break;
        }
        else if (env_vec[0] == "tell"){
            int tellwho = stoi(env_vec[1]);
            if (user[tellwho].fd == -1){
                string err_msg =  "*** Error: user #" + to_string(tellwho) + " does not exist yet. ***\n";
                send(user[ID].fd, err_msg.c_str(), strlen(err_msg.c_str()), 0);
            }
            else tell(ID, tellwho, input);
            break;
        }
        else if (env_vec[0] == "name") {
            // input += " ";
            // cout << "name:" << env_vec[0] << endl;
            change_name(ID, input);
            break;
        }
        else if (i%2 == 0) {
            if (next == true && single_job[i+1] == "|") {
                int p[2];
                if (pipe(p) < 0) {
                    // cout << "pipe failed" << endl;
                    exit(-1);
                }
                if (i == 0) {
                    right_pipe_id.first = p[0];
                    right_pipe_id.second = p[1];
                }
                else if (i != single_job.size()-1){
                    // cout << "middle child" << endl;
                    left_pipe_id = right_pipe_id;
                    right_pipe_id.first = p[0];
                    right_pipe_id.second = p[1];
                }  
            }
            else if (i == single_job.size()-1) {
                if (single_job.size() != 1) {
                    left_pipe_id = right_pipe_id;
                    right_pipe_id.first = -1;
                    right_pipe_id.second = -1;
                }
            }
            else if (single_job[i+1] == ">") {
                if (i != 0) {
                    left_pipe_id = right_pipe_id;
                    right_pipe_id.first = -1;
                    right_pipe_id.second = -1;
                }
                file_redirection = true;
                redirection_file_name = single_job[i+2];
                redirection_file_name.pop_back();
                cout << "file name :" << redirection_file_name << endl;
            }
            else {
                if (single_job[i+1][0] == '|' || single_job[i+1][0] == '!') {
                    // cout << "number pipe is: " << single_job[i+1] << endl;
                    number_pipe = true;
                    if (single_job[i+1][0] == '!') 
                        send_error_to_other = true;                    
                    int pass_line;
                    stringstream ss;
                    ss << single_job[i+1].substr(1);
                    ss >> pass_line;
                    int target_id;
                    target_id = user[ID].job_cnt + pass_line;
                    // cout << "job " << job_cnt << " sent to " << target_id << endl;
                    if (pipe_map.find(target_id) != pipe_map.end()) {
                        // cout << "you are not first one to send to target" << endl;
                        if (receive == false && i != 0)
                            left_pipe_id = right_pipe_id;
                        right_pipe_id.first = pipe_map[target_id].first;
                        right_pipe_id.second = pipe_map[target_id].second;
                    }
                    else {
                        int p[2];
                        if (pipe(p) < 0) {
                            // cout << "pipe failed" << endl;
                            exit(-1);
                        }
                        // cout << "you are first one to send to target" << endl;
                        pipe_map[target_id].first = p[0];
                        pipe_map[target_id].second = p[1];
                        if (receive == false)
                            left_pipe_id = right_pipe_id;
                        
                        right_pipe_id.first = p[0];
                        right_pipe_id.second = p[1];
                    }
                }
                
            }
            // file_redirection = false;
            from_which_client = get_from_client(single_job[i]);
            to_which_client = get_to_client(single_job[i]);
            if (from_which_client != -1 || to_which_client != -1){
                single_job[i] = del_redir_client_info(single_job[i]);
            }
            if (from_which_client != -1)
                not_recv = check_from_client(from_which_client, ID, left_pipe_id, right_pipe_id, input);
            if (to_which_client != -1)
                not_send = check_to_client(to_which_client, ID, left_pipe_id, right_pipe_id, input); 
            execvp_work(single_job[i], left_pipe_id, right_pipe_id, send_error_to_other, file_redirection, redirection_file_name, number_pipe, ID, not_send, not_recv);
            send_error_to_other = false;
            number_pipe = false;
            redirection_file_name = "";
            receive = false;
        }     
        if (file_redirection == true)
            break;
    }
}

void execvp_work(string final_job, pair <int, int> left_pipe_id, pair <int, int> right_pipe_id, bool send_error_to_other, bool file_redirection, string redirection_file_name, bool number_pipe, int ID, bool not_send, bool not_recv) {
    
    int status;
    pid_t pid = fork();
    while (pid < 0) {
        wait(&status);
        pid = fork();
    }
    if (pid == 0) {
        cout << "l1:" << left_pipe_id.first << " l2:" << left_pipe_id.second << endl;
        cout << "r1:" << right_pipe_id.first << " r2:" << right_pipe_id.second << endl;
        if (send_error_to_other == true) {
            // cout << "send_error_to_other == true" << endl;
            dup2(right_pipe_id.second, STDERR_FILENO);
        }
        else if (send_error_to_other == false) {
            // cout << "send_error_to_other == false" << endl;
            close(STDERR_FILENO);
            dup2(user[ID].fd, STDERR_FILENO);
        }
        
        if ((left_pipe_id.first == left_pipe_id.second) && (right_pipe_id.first == right_pipe_id.second)) {
            //ex: ls
            // cout << "ID:" << ID << endl;
            close(STDOUT_FILENO); // Close stdout
            close(STDERR_FILENO);
            dup2(user[ID].fd, STDOUT_FILENO);
            dup2(user[ID].fd, STDERR_FILENO);
        } 
        else if (left_pipe_id.first == left_pipe_id.second) {
            // cout << "first in execvp_work" << endl;
            dup2(right_pipe_id.second, STDOUT_FILENO);
            close_pipe(right_pipe_id);
        }
        else if (right_pipe_id.first == right_pipe_id.second){
            // cout << "last in execvp_work" << endl;
            dup2(left_pipe_id.first, STDIN_FILENO);
            close_pipe(left_pipe_id);
            close(STDOUT_FILENO); // close stdout
            close(STDERR_FILENO);
            dup2(user[ID].fd, STDOUT_FILENO);
            dup2(user[ID].fd, STDERR_FILENO);
        }
        else {
            // cout << "middle in execvp_work" << endl;
            dup2(left_pipe_id.first, STDIN_FILENO);
            dup2(right_pipe_id.second, STDOUT_FILENO);
            close_pipe(left_pipe_id);
            close_pipe(right_pipe_id);
        }

        if (file_redirection == true) {
            //file redirection
            freopen(redirection_file_name.c_str(), "w", stdout);
        }

        if (not_send)
            freopen("/dev/null", "w", stdout);
        if (not_recv)
            freopen("/dev/null", "r", stdin);

        vector <string> tmp_vec;
        tmp_vec = command_split(final_job);
        char** arg;
        arg = new char* [tmp_vec.size()+1];
        for (int j = 0; j < tmp_vec.size(); j++) {
            arg[j] = (char*)tmp_vec[j].c_str();
        }
        arg[tmp_vec.size()] = NULL;
        
        if (execvp(arg[0], arg) < 0) {
            string unknown_command;
            unknown_command = "";
            for (int j = 0; j < final_job.size(); j++){
                if (final_job[j] != ' ') {
                    unknown_command += final_job[j];
                }
                else {
                    break;
                }
            }
            cerr << "Unknown command: [" << unknown_command << "]." << endl;
            exit(0);
        }
    }
    else {
        if (right_pipe_id.first != right_pipe_id.second) {
            if (left_pipe_id.first != left_pipe_id.second) {
                close_pipe(left_pipe_id);
                cout << "parent of middle" << endl;
            }
            else {
                cout << "parent of first" << endl;
            }
            signal(SIGCHLD,SIG_IGN);
        }
        else {
            // last child
            if (left_pipe_id.first != left_pipe_id.second)
                close_pipe(left_pipe_id);
            cout << "parent of last" << endl;
            if (number_pipe == false){
                cout << "not number pipe" << endl;
                wait(NULL);
            }
        }
    }
}

void close_pipe(pair <int, int> p) {
    close(p.first);
    close(p.second);
}

vector<string> command_split(string command) {
    vector <string> command_splited;
    vector<string> arr;
    istringstream ss(command);
    string word;
    while(ss >> word) {
        arr.push_back(word);
    }
    return arr;
} 

void parse(string str1, queue<string> &q) {
    const char* d = " ";
    char *p;
    char str2[str1.size()+1];
    strcpy(str2, str1.c_str()); 

    p = strtok(str2, d);
    string tmp_str;
    tmp_str = "";
    while (p != NULL) {
        if ((p[0] == '|' || p[0] == '!') && (strlen(p) != 1)) { //number pipe
            tmp_str += p;
            q.push(tmp_str);
            tmp_str = "";
        }
        else {
            tmp_str += p;
            tmp_str += " ";
        }
        p = strtok(NULL, d);           
    }
    if (tmp_str != "") {
        q.push(tmp_str);
    }
    return;
}

void broadcast_msg(string send_pipe_msg){
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (user[i].fd != -1) {
            send(user[i].fd, send_pipe_msg.c_str(), strlen(send_pipe_msg.c_str()), 0);
        }
    }
}

void broadcast_login(int n) {
    string s = "*** User '";
    s += user[n].name;
    s += "' entered from ";
    s += string(user[n].ip) + ":" + to_string(user[n].port);
    s += ". ***\n";

    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (user[i].fd != -1) {
            send(user[i].fd, s.c_str(), strlen(s.c_str()), 0);
        }
    }
}

int get_to_client(string str){
    string s = "";
    vector<string> vec = command_split(str);
    for (int i = 0; i < vec.size(); i++) {
        // cout << vec[i];
        if (vec[i][0] == '>') {
            s = vec[i].substr(1);
            break;
        }
    }
    if (s == "")
        return -1;
    else return stoi(s);
}

int get_from_client(string str){
    string s = "";
    string new_str = "";
    vector<string> vec = command_split(str);
    for (int i = 0; i < vec.size(); i++) {
        if (vec[i][0] == '<') {
            s = vec[i].substr(1);
            break;
        }
    }
    if (s == "")
        return -1;
    else return stoi(s);
}

string del_redir_client_info(string str){
    string new_str = "";
    for (int i = 0; i < str.length(); i++) {
        if (str[i] == '>' || str[i] == '<') 
            break;
        else new_str += str[i];
    }
    // new_str = new_str.substr(0, new_str.length()-1);
    return new_str;
}

void initenv(int ID){
    map <string, string>::iterator iter;
    for (iter = user[ID].env_map.begin(); iter != user[ID].env_map.end(); iter++){
        unsetenv(iter->first.c_str());
    }
}

void set_env(int ID){
    map <string, string>::iterator iter;
    for (iter = user[ID].env_map.begin(); iter != user[ID].env_map.end(); iter++){
        setenv(iter->first.c_str(), iter->second.c_str(), 1);
    }
        
}

void who(int me){
    string str = "<ID>    <nickname>    <IP:port>    <indicate me>\n";   
    send(user[me].fd, str.c_str(), strlen(str.c_str()), 0);
    for (int i = 1; i < MAX_CLIENTS; i++){
        if (user[i].fd != -1){  
            str = (to_string(user[i].ID) + "    " + string(user[i].name) + "    " + string(user[i].ip) + ":" + to_string(user[i].port)+ "    ");
            if (me == user[i].ID)
                str += "<-me";
            str += "\n";
            send(user[me].fd, str.c_str(), strlen(str.c_str()), 0);
        }
    }
}

void tell(int me, int tellwho, string tell_msg){
    int space =2;
    string str = "*** " + string(user[me].name) + " told you ***: ";
    for (int i = 0; i < tell_msg.length(); ++i){
        if(space == 0)
            str += tell_msg[i];
        else if (tell_msg[i] == ' ') space--;
    }
    str += "\n";
    send(user[tellwho].fd, str.c_str(), strlen(str.c_str()), 0);
}

void yell(int me, string yell_msg) {
    string str = "*** " + string(user[me].name) + " yelled ***:" + yell_msg.substr(4) + "\n";
    for (int i = 1; i < MAX_CLIENTS; ++i){
        if (user[i].fd != -1)
            send(user[i].fd, str.c_str(), strlen(str.c_str()), 0);
    }
}

void change_name(int me, string name_msg){
    string del_space_name = "";
    name_msg = name_msg.substr(5, name_msg.length()-5);
    for (int k = 0; k < name_msg.length(); k++){
        if (name_msg[k] != ' ')
            del_space_name += name_msg[k];
        else break;
    }
    name_msg = del_space_name;
    string str = "";
    for (int i = 1; i < MAX_CLIENTS; i++){
        if (name_msg == string(user[i].name)){
            cout << "test" << endl;
            str = "*** User '" + name_msg + "' already exists. ***\n";
            send(user[me].fd, str.c_str(), strlen(str.c_str()), 0);
            return;
        }
    }
    str = "*** User from " + string(user[me].ip) + ":" + to_string(user[me].port) + " is named '" + name_msg + "'. ***\n";
    for (int i = 1; i < MAX_CLIENTS; ++i){
        if (user[i].fd != -1)
            send(user[i].fd, str.c_str(), strlen(str.c_str()), 0);
    }
    strcpy(user[me].name, name_msg.c_str());
}

void logout(int n){
    FD_CLR(user[n].fd, &afds);
    string logout_msg = "*** User '" + string(user[n].name) + "' left. ***\n";
    close(user[n].fd);
    user[n].ID = -1;
    user[n].fd = -1;
    user[n].port = -1;
    memset(user[n].name, '\0', 21);
    strcpy(user[n].name, "(no name)");
    memset(user[n].ip, '\0', 25);
    user[n].env_map.clear();
    user[n].env_map["PATH"] = "bin:.";
    // memset(user[n].env, '\0', 200);
    // strcpy(user[n].env, "PATH=bin:.");
    user[n].job_cnt = 0;
    user[n].pipe_map.clear();
    pair<int, int> p;
    for (int i = 1; i < MAX_CLIENTS; i++){
        //someone send you file
        if (user[i].client_pipe[n][0] != user[i].client_pipe[n][1]){
            p.first = user[i].client_pipe[n][0];
            p.second = user[i].client_pipe[n][1];
            close_pipe(p);
            user[i].client_pipe[n][0] = -1;
            user[i].client_pipe[n][1] = -1;
        }
        //you send someone file
        if (user[n].client_pipe[i][0] != user[n].client_pipe[i][1]){
            p.first = user[n].client_pipe[i][0];
            p.second = user[n].client_pipe[i][1];
            close_pipe(p);
            user[n].client_pipe[i][0] = -1;
            user[n].client_pipe[i][1] = -1;
        }
    }
    broadcast_msg(logout_msg);
}
 

void welcome(int n) {
    string welcome_msg = "****************************************\n** Welcome to the information server. **\n****************************************\n";
    send(user[n].fd, welcome_msg.c_str(), strlen(welcome_msg.c_str()), 0);
}

bool check_from_client(int &from_which_client, int ID, pair <int, int> &left_pipe_id, pair <int, int> &right_pipe_id, string input) {
    if (from_which_client > 30 || user[from_which_client].fd == -1) {
        string err_msg =  "*** Error: user #" + to_string(from_which_client) + " does not exist yet. ***\n";
        send(user[ID].fd, err_msg.c_str(), strlen(err_msg.c_str()), 0);
        from_which_client = -1;
        return true;
        // break;
    }
    //檢查這個人有沒有傳東西給你
    else if (user[from_which_client].client_pipe[ID][0] == user[from_which_client].client_pipe[ID][1]){
        string err_msg = "*** Error: the pipe #" + to_string(from_which_client) + "->#" + to_string(ID) + " does not exist yet. ***\n";
        send(user[ID].fd, err_msg.c_str(), strlen(err_msg.c_str()), 0);
        from_which_client = -1;
        return true;
        // break;
    }
    else {
        left_pipe_id.first = user[from_which_client].client_pipe[ID][0];
        left_pipe_id.second = user[from_which_client].client_pipe[ID][1];
        user[from_which_client].client_pipe[ID][0] = -1;
        user[from_which_client].client_pipe[ID][1] = -1;
        string public_recv_msg = "*** " + string(user[ID].name) + " (#" + to_string(ID) + ") just received from " + string(user[from_which_client].name) + " (#" + to_string(from_which_client) + ") by '" + input + "' ***\n";
        broadcast_msg(public_recv_msg);
        return false;
    }
}

bool check_to_client(int &to_which_client, int ID, pair <int, int> &left_pipe_id, pair <int, int> &right_pipe_id, string input) {
    if (to_which_client > 30 || user[to_which_client].fd == -1) {
        string err_msg =  "*** Error: user #" + to_string(to_which_client) + " does not exist yet. ***\n";
        send(user[ID].fd, err_msg.c_str(), strlen(err_msg.c_str()), 0);
        to_which_client = -1;
        return true;
        // break;
    }
    //檢查上次傳給這個人他收了沒
    else if (user[ID].client_pipe[to_which_client][0] != user[ID].client_pipe[to_which_client][1]){
        string err_msg = "*** Error: the pipe #" + to_string(ID) + "->#" + to_string(to_which_client) + " already exists. ***\n";
        send(user[ID].fd, err_msg.c_str(), strlen(err_msg.c_str()), 0);
        to_which_client = -1;
        return true;
        // break;
    }
    else {
        //真正傳送給別的client->create pipe
        int p[2];
        if (pipe(p) < 0) {
            exit(-1);
        }
        // left_pipe_id = right_pipe_id;
        right_pipe_id.first = p[0];
        right_pipe_id.second = p[1];
        user[ID].client_pipe[to_which_client][0] = p[0];
        user[ID].client_pipe[to_which_client][1] = p[1];
        string public_send_msg = "*** " + string(user[ID].name) +  " (#" + to_string(ID) +") just piped '" + input +  "' to " + string(user[to_which_client].name) +  " (#" + to_string(to_which_client) + ") ***\n";
        broadcast_msg(public_send_msg);
        return false;
    }
}

int passiveTCP(const char* service, int qlen) {
    return passivesock(service, "tcp", qlen);
}

int passivesock(const char *service, const char *protocol, int qlen) {
    struct servent *pse;            // pointer to service info entry
    struct protoent *ppe;           // pointer to protocol info entry
    struct sockaddr_in sin;         // an endpoint addr
    int sock, type;                 // socketfd & socket type

    bzero((char *)&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    
    //map service name to port number
    if(pse = getservbyname(service, protocol))
        sin.sin_port = htons(ntohs((u_short) pse -> s_port));
    else if((sin.sin_port = htons((u_short)atoi(service))) == 0) {
        exit(-1);
    }

    //map protocol name to protocol number
    if((ppe = getprotobyname(protocol)) == 0) { 
        exit(-1);
    }
    
    //use protocol to choose a socket type
    if(strcmp(protocol, "tcp") == 0)
        type = SOCK_STREAM;
    else
        type = SOCK_DGRAM;

    // allocate a socket
    sock = socket(PF_INET, type, ppe->p_proto);
    if(sock < 0) {
        exit(-1);
    }
    const int enable = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));

    //bind the socket
    if(bind(sock, (struct sockaddr*) &sin, sizeof(sin)) < 0) {
        exit(-1);
    }
    if(type == SOCK_STREAM && listen(sock, qlen) < 0) {
        exit(-1);
    }
    return sock;
}
